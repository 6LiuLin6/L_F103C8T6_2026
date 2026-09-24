cmake_minimum_required(VERSION 3.22)

get_filename_component(MODULE_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

set(FREEMODBUS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/freemodbus)

option(MODBUS_MASTER_MODE "Build the Modbus RTU master example" ON)

if(MODBUS_MASTER_MODE)
    set(MODBUS_APP_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/ModbusMaster.c)
else()
    set(MODBUS_APP_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/ModbusSlave.c)
endif()

file(GLOB MODULE_SOURCES CONFIGURE_DEPENDS
    ${FREEMODBUS_DIR}/modbus/*.c
    ${FREEMODBUS_DIR}/modbus/functions/*.c
    ${FREEMODBUS_DIR}/modbus/rtu/*.c
    ${FREEMODBUS_DIR}/demo/STM32_CMAKE/port/*.c
    ${MODBUS_APP_SOURCE}
)

add_library(${MODULE_NAME}
        ${MODULE_SOURCES}
)

target_link_libraries(${MODULE_NAME} PRIVATE
    stm32cubemx
)
target_include_directories(${MODULE_NAME} PUBLIC
    ${FREEMODBUS_DIR}/modbus/include
    ${FREEMODBUS_DIR}/modbus/rtu
    ${FREEMODBUS_DIR}/demo/STM32_CMAKE/port
)

target_compile_options(${MODULE_NAME} INTERFACE
    "$<$<COMPILE_LANGUAGE:C>:-include${FREEMODBUS_DIR}/demo/STM32_CMAKE/port/port.h>"
)

target_compile_definitions(${MODULE_NAME} PRIVATE
    MB_USART_NR=1
    MB_ASCII_ENABLED=0
)
