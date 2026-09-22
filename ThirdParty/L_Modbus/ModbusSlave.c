#include "mb.h"
#include "port_internal.h"
#include "main.h"
#include <stdio.h>

#define MODBUS_SLAVE_ADDRESS 1U
#define MODBUS_BAUDRATE 115200UL
#define INPUT_START 1000U
#define INPUT_COUNT 4U
#define HOLDING_START 0U
#define HOLDING_COUNT 4U

static USHORT input_registers[INPUT_COUNT];
static USHORT holding_registers[HOLDING_COUNT];
static uint32_t last_register_print_tick;

void modbusInit(void)
{
    eMBErrorCode status;

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
    value32 = ((uint32_t)holding_registers[0] << 16)
            | holding_registers[1];

    if (HAL_GetTick() - last_register_print_tick >= 1000U)
    {
        printf("holding32=0x%08lX (%lu)\r\n",
               (unsigned long)value32, (unsigned long)value32);
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
    (void)buffer;
    (void)address;
    (void)count;
    (void)mode;
    return MB_ENOREG;
}

eMBErrorCode eMBRegDiscreteCB(UCHAR *buffer, USHORT address, USHORT count)
{
    (void)buffer;
    (void)address;
    (void)count;
    return MB_ENOREG;
}