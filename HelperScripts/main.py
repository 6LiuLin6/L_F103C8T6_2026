import serial
import time


# ============================================================
# Configuration
# ============================================================

PORT = "COM5"          # Change to your USB-RS485 COM port
BAUDRATE = 115200     # Must match STM32 UART configuration

SLAVE_ADDRESS = 1

# Values that the STM32 master will read
REGISTER_0 = 0x1234
REGISTER_1 = 0x5678


# ============================================================
# Modbus CRC16
# ============================================================

def modbus_crc(data):
    crc = 0xFFFF

    for byte in data:
        crc ^= byte

        for _ in range(8):
            if crc & 0x0001:
                crc >>= 1
                crc ^= 0xA001
            else:
                crc >>= 1

    return crc


def add_crc(data):
    crc = modbus_crc(data)

    # Modbus sends CRC low byte first
    return data + bytes([
        crc & 0xFF,
        (crc >> 8) & 0xFF
    ])


def check_crc(frame):
    if len(frame) < 3:
        return False

    received_crc = frame[-2] | (frame[-1] << 8)
    calculated_crc = modbus_crc(frame[:-2])

    return received_crc == calculated_crc


# ============================================================
# Print frame
# ============================================================

def print_frame(prefix, frame):
    print(
        f"{prefix}: "
        + " ".join(f"{b:02X}" for b in frame)
    )


# ============================================================
# Build response to Function 03
# ============================================================

def build_read_holding_response(slave, start_address, quantity):
    if quantity != 2:
        # Illegal data value
        response = bytes([
            slave,
            0x83,
            0x03
        ])

        return add_crc(response)

    # Two 16-bit registers
    response = bytes([
        slave,
        0x03,
        0x04,

        (REGISTER_0 >> 8) & 0xFF,
        REGISTER_0 & 0xFF,

        (REGISTER_1 >> 8) & 0xFF,
        REGISTER_1 & 0xFF
    ])

    return add_crc(response)


# ============================================================
# Main
# ============================================================

def main():

    print("======================================")
    print(" Modbus RTU Python Slave Test")
    print("======================================")
    print(f"Port       : {PORT}")
    print(f"Baudrate   : {BAUDRATE}")
    print(f"Slave ID   : {SLAVE_ADDRESS}")
    print(f"Register 0 : 0x{REGISTER_0:04X}")
    print(f"Register 1 : 0x{REGISTER_1:04X}")
    print()

    try:
        ser = serial.Serial(
            port=PORT,
            baudrate=BAUDRATE,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.1
        )

    except serial.SerialException as e:
        print(f"Cannot open serial port: {e}")
        return

    print("Serial port opened.")
    print("Waiting for Modbus requests...")
    print()

    buffer = bytearray()

    try:

        while True:

            data = ser.read(1)

            if data:
                buffer += data

            # ------------------------------------------------
            # Your master request is exactly 8 bytes:
            #
            # 01 03 00 00 00 02 C4 0B
            # ------------------------------------------------

            if len(buffer) >= 8:

                # Find slave address
                if buffer[0] != SLAVE_ADDRESS:
                    buffer.pop(0)
                    continue

                # Function code
                if buffer[1] != 0x03:
                    print_frame("Unknown request", buffer[:8])
                    buffer.pop(0)
                    continue

                request = bytes(buffer[:8])
                del buffer[:8]

                print_frame("RX", request)

                # Check CRC
                if not check_crc(request):
                    print("ERROR: CRC error")
                    continue

                # Decode request
                start_address = (
                    (request[2] << 8) |
                    request[3]
                )

                quantity = (
                    (request[4] << 8) |
                    request[5]
                )

                print(
                    f"  Slave      = {request[0]}"
                )
                print(
                    f"  Function   = 0x{request[1]:02X}"
                )
                print(
                    f"  Start addr = {start_address}"
                )
                print(
                    f"  Quantity   = {quantity}"
                )

                # Build response
                response = build_read_holding_response(
                    SLAVE_ADDRESS,
                    start_address,
                    quantity
                )

                # Small delay to simulate slave processing
                time.sleep(0.005)

                ser.write(response)
                ser.flush()

                print_frame("TX", response)

                print(
                    f"  REG0 = 0x{REGISTER_0:04X}"
                )
                print(
                    f"  REG1 = 0x{REGISTER_1:04X}"
                )

                value32 = (
                    (REGISTER_0 << 16) |
                    REGISTER_1
                )

                print(
                    f"  32-bit = 0x{value32:08X}"
                )

                print()

    except KeyboardInterrupt:
        print("\nStopped.")

    finally:
        ser.close()
        print("Serial port closed.")


if __name__ == "__main__":
    main()