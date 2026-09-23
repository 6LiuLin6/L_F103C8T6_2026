#include "mb.h"
#include "port_internal.h"
#include "main.h"
#include <stdio.h>

#define MODBUS_SLAVE_ADDRESS 1U
#define MODBUS_BAUDRATE 115200UL
#define INPUT_START 1U
#define INPUT_COUNT 1000U
#define HOLDING_START 0U
#define HOLDING_COUNT 1000U
#define COIL_START 0U
#define COIL_COUNT 8U
#define COIL_LED_INDEX 0U
#define DISCRETE_START 0U
#define DISCRETE_COUNT 8U
#define DISCRETE_EXAMPLE_PIN GPIO_PIN_0
#define DISCRETE_EXAMPLE_PORT GPIOA

static USHORT input_registers[INPUT_COUNT];
static USHORT holding_registers[HOLDING_COUNT];
static UCHAR coils[COIL_COUNT];
static UCHAR discrete_inputs[DISCRETE_COUNT];
static uint32_t last_register_print_tick;

static void updateCoilOutputs(void)
{
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13,
                      coils[COIL_LED_INDEX] ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void modbusInit(void)
{
    eMBErrorCode status;
    GPIO_InitTypeDef gpio_init = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    gpio_init.Pin = GPIO_PIN_13;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &gpio_init);
    coils[COIL_LED_INDEX] = FALSE;
    updateCoilOutputs();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    gpio_init.Pin = DISCRETE_EXAMPLE_PIN;
    gpio_init.Mode = GPIO_MODE_INPUT;
    gpio_init.Pull = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DISCRETE_EXAMPLE_PORT, &gpio_init);

    discrete_inputs[0] = HAL_GPIO_ReadPin(DISCRETE_EXAMPLE_PORT, DISCRETE_EXAMPLE_PIN) ? 1U : 0U;

    MB_Uart_Init();
    status = eMBInit(MB_RTU, MODBUS_SLAVE_ADDRESS, 0, MODBUS_BAUDRATE,
                     MB_PAR_NONE, 1);
    if (status != MB_ENOERR)
    {
        Error_Handler();
    }

    status = eMBEnable();
    if (status != MB_ENOERR)
    {
        Error_Handler();
    }

    holding_registers[0] = 0x1234U;
    holding_registers[1] = 0x5678U;
}

void modbusPoll(void)
{
    uint32_t value32;

    (void)eMBPoll();
    input_registers[0]++;
    discrete_inputs[0] = HAL_GPIO_ReadPin(DISCRETE_EXAMPLE_PORT, DISCRETE_EXAMPLE_PIN) ? 1U : 0U;
    value32 = ((uint32_t)holding_registers[0] << 16)
            | holding_registers[1];

    if (HAL_GetTick() - last_register_print_tick >= 1000U)
    {
        printf("holding32=0x%08lX (%lu) discrete0=%u\r\n",
               (unsigned long)value32, (unsigned long)value32,
               (unsigned int)discrete_inputs[0]);
        last_register_print_tick = HAL_GetTick();
    }
}

eMBErrorCode eMBRegInputCB(UCHAR *buffer, USHORT address, USHORT count)
{
    USHORT index;

    address--;

    if (address < INPUT_START || count > INPUT_COUNT ||
        address - INPUT_START > INPUT_COUNT - count)
    {
        return MB_ENOREG;
    }

    index = address - INPUT_START;
    while (count-- > 0)
    {
        *buffer++ = (UCHAR)(input_registers[index] >> 8);
        *buffer++ = (UCHAR)(input_registers[index] & 0xFFU);
        index++;
    }
    return MB_ENOERR;
}

eMBErrorCode eMBRegHoldingCB(UCHAR *buffer, USHORT address, USHORT count,
                             eMBRegisterMode mode)
{
    USHORT index;

    address--;

    if (address < HOLDING_START || count > HOLDING_COUNT ||
        address - HOLDING_START > HOLDING_COUNT - count)
    {
        return MB_ENOREG;
    }

    index = address - HOLDING_START;
    while (count-- > 0)
    {
        if (mode == MB_REG_READ)
        {
            *buffer++ = (UCHAR)(holding_registers[index] >> 8);
            *buffer++ = (UCHAR)(holding_registers[index] & 0xFFU);
        }
        else
        {
            holding_registers[index] = ((USHORT)buffer[0] << 8) | buffer[1];
            buffer += 2;
        }
        index++;
    }
    return MB_ENOERR;
}

eMBErrorCode eMBRegCoilsCB(UCHAR *buffer, USHORT address, USHORT count,
                           eMBRegisterMode mode)
{
    USHORT index;
    USHORT offset;
    USHORT byte_count;

    address--;

    if (address < COIL_START || count > COIL_COUNT ||
        address - COIL_START > COIL_COUNT - count)
    {
        return MB_ENOREG;
    }

    index = address - COIL_START;
    byte_count = (count + 7U) / 8U;

    if (mode == MB_REG_READ)
    {
        for (offset = 0; offset < byte_count; offset++)
        {
            buffer[offset] = 0U;
        }

        for (offset = 0; offset < count; offset++)
        {
            if (coils[index + offset])
            {
                buffer[offset / 8U] |= (UCHAR)(1U << (offset % 8U));
            }
        }
    }
    else
    {
        for (offset = 0; offset < count; offset++)
        {
            coils[index + offset] =
                (UCHAR)((buffer[offset / 8U] >> (offset % 8U)) & 1U);
        }
        updateCoilOutputs();
    }

    return MB_ENOERR;
}

eMBErrorCode eMBRegDiscreteCB(UCHAR *buffer, USHORT address, USHORT count)
{
    USHORT index;
    USHORT bit_index;
    USHORT offset;

    address--;

    if (address < DISCRETE_START || count > DISCRETE_COUNT ||
        address - DISCRETE_START > DISCRETE_COUNT - count)
    {
        return MB_ENOREG;
    }

    index = address - DISCRETE_START;
    for (offset = 0; offset < count; offset++)
    {
        bit_index = index + offset;
        if (bit_index < DISCRETE_COUNT)
        {
            buffer[offset / 8U] |= (UCHAR)(discrete_inputs[bit_index] ? (1U << (offset % 8U)) : 0U);
        }
    }
    return MB_ENOERR;
}