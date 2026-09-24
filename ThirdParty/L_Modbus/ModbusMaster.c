#include "main.h"
#include "usart.h"
#include "mb.h"
#include "mbcrc.h"
#include <stdbool.h>
#include <stdio.h>

#define MODBUS_MASTER_SLAVE_ADDRESS 1U
#define MODBUS_MASTER_POLL_PERIOD_MS 1000U
#define MODBUS_MASTER_RESPONSE_TIMEOUT_MS 250U
#define MODBUS_MASTER_READ_HOLDING 0x03U
#define MODBUS_MASTER_REGISTER_ADDRESS 0U
#define MODBUS_MASTER_REGISTER_COUNT 2U
#define MODBUS_MASTER_MAX_FRAME 32U

static uint32_t last_master_poll_tick;

static uint16_t modbusMasterBuildReadRequest(uint8_t *request)
{
    uint16_t crc;

    request[0] = MODBUS_MASTER_SLAVE_ADDRESS;
    request[1] = MODBUS_MASTER_READ_HOLDING;
    request[2] = (uint8_t)(MODBUS_MASTER_REGISTER_ADDRESS >> 8);
    request[3] = (uint8_t)(MODBUS_MASTER_REGISTER_ADDRESS & 0xFFU);
    request[4] = (uint8_t)(MODBUS_MASTER_REGISTER_COUNT >> 8);
    request[5] = (uint8_t)(MODBUS_MASTER_REGISTER_COUNT & 0xFFU);
    crc = usMBCRC16(request, 6U);
    request[6] = (uint8_t)(crc & 0xFFU);
    request[7] = (uint8_t)(crc >> 8);
    return 8U;
}

static bool modbusMasterFrameIsValid(const uint8_t *frame, uint16_t length)
{
    return length >= 5U && usMBCRC16(frame, length) == 0U;
}

static bool modbusMasterFindResponse(const uint8_t *stream, uint16_t stream_length,
                                     uint8_t *response, uint16_t *response_length)
{
    uint16_t offset;
    uint16_t frame_length;
    uint8_t byte_count;
    uint8_t function;

    for (offset = 0U; offset + 2U < stream_length; offset++)
    {
        if (stream[offset] != MODBUS_MASTER_SLAVE_ADDRESS)
        {
            continue;
        }

        function = stream[offset + 1U];
        if ((function & 0x80U) != 0U)
        {
            frame_length = 5U;
        }
        else if (function == MODBUS_MASTER_READ_HOLDING)
        {
            byte_count = stream[offset + 2U];
            frame_length = (uint16_t)byte_count + 5U;
        }
        else
        {
            continue;
        }

        if (offset + frame_length <= stream_length &&
            modbusMasterFrameIsValid(&stream[offset], frame_length))
        {
            uint16_t index;
            for (index = 0U; index < frame_length; index++)
            {
                response[index] = stream[offset + index];
            }
            *response_length = frame_length;
            return true;
        }
    }
    return false;
}

static void modbusMasterDrainByte(uint8_t *stream, uint16_t *stream_length)
{
    if ((huart1.Instance->SR & USART_SR_RXNE) != 0U)
    {
        uint8_t byte = (uint8_t)(huart1.Instance->DR & 0xFFU);
        if (*stream_length < MODBUS_MASTER_MAX_FRAME)
        {
            stream[(*stream_length)++] = byte;
        }
    }
}

static bool modbusMasterTransmit(const uint8_t *request, uint16_t request_length,
                                 uint8_t *stream, uint16_t *stream_length)
{
    uint16_t index;
    uint32_t deadline = HAL_GetTick() + 100U;

    *stream_length = 0U;
    __HAL_UART_CLEAR_OREFLAG(&huart1);
    __HAL_UART_CLEAR_FEFLAG(&huart1);
    __HAL_UART_CLEAR_NEFLAG(&huart1);
    __HAL_UART_CLEAR_PEFLAG(&huart1);

    for (index = 0U; index < request_length; index++)
    {
        while ((huart1.Instance->SR & USART_SR_TXE) == 0U)
        {
            modbusMasterDrainByte(stream, stream_length);
            if ((int32_t)(deadline - HAL_GetTick()) <= 0)
            {
                return false;
            }
        }

        huart1.Instance->DR = request[index];
        modbusMasterDrainByte(stream, stream_length);
    }

    while ((huart1.Instance->SR & USART_SR_TC) == 0U)
    {
        modbusMasterDrainByte(stream, stream_length);
        if ((int32_t)(deadline - HAL_GetTick()) <= 0)
        {
            return false;
        }
    }
    modbusMasterDrainByte(stream, stream_length);
    return true;
}

static bool modbusMasterReadResponse(uint8_t *stream, uint16_t stream_length,
                                     uint8_t *response, uint16_t *response_length)
{
    uint32_t deadline = HAL_GetTick() + MODBUS_MASTER_RESPONSE_TIMEOUT_MS;

    while ((int32_t)(deadline - HAL_GetTick()) > 0 &&
           stream_length < MODBUS_MASTER_MAX_FRAME)
    {
        uint8_t byte;
        uint32_t remaining = deadline - HAL_GetTick();

        if (HAL_UART_Receive(&huart1, &byte, 1U, remaining) != HAL_OK)
        {
            if ((huart1.Instance->SR & USART_SR_RXNE) == 0U)
            {
                continue;
            }
            byte = (uint8_t)(huart1.Instance->DR & 0xFFU);
        }

        stream[stream_length++] = byte;
        if (modbusMasterFindResponse(stream, stream_length,
                                     response, response_length))
        {
            return true;
        }
    }
    return false;
}

static bool modbusMasterReadHolding(uint16_t *first_register,
                                    uint16_t *second_register)
{
    uint8_t request[8];
    uint8_t stream[MODBUS_MASTER_MAX_FRAME];
    uint8_t response[MODBUS_MASTER_MAX_FRAME];
    uint16_t stream_length;
    uint16_t response_length;

    (void)modbusMasterBuildReadRequest(request);
    if (!modbusMasterTransmit(request, sizeof(request), stream, &stream_length))
    {
        return false;
    }

    if (!modbusMasterReadResponse(stream, stream_length, response, &response_length) ||
        response_length != 9U || response[1] != MODBUS_MASTER_READ_HOLDING ||
        response[2] != 4U)
    {
        return false;
    }

    *first_register = ((uint16_t)response[3] << 8) | response[4];
    *second_register = ((uint16_t)response[5] << 8) | response[6];
    return true;
}

void modbusInit(void)
{
    __HAL_UART_DISABLE_IT(&huart1, UART_IT_RXNE);
    __HAL_UART_DISABLE_IT(&huart1, UART_IT_TXE);
    __HAL_UART_DISABLE_IT(&huart1, UART_IT_TC);
    HAL_NVIC_DisableIRQ(USART1_IRQn);
    last_master_poll_tick = HAL_GetTick() - MODBUS_MASTER_POLL_PERIOD_MS;
    printf("Modbus RTU master ready\r\n");
}

void modbusPoll(void)
{
    uint16_t high_word;
    uint16_t low_word;
    uint32_t value32;

    if (HAL_GetTick() - last_master_poll_tick < MODBUS_MASTER_POLL_PERIOD_MS)
    {
        return;
    }
    last_master_poll_tick = HAL_GetTick();

    if (modbusMasterReadHolding(&high_word, &low_word))
    {
        value32 = ((uint32_t)high_word << 16) | low_word;
        printf("master read: reg0=0x%04X reg1=0x%04X value32=0x%08lX\r\n",
               high_word, low_word, (unsigned long)value32);
    }
    else
    {
        printf("master read timeout or invalid response\r\n");
    }
}
