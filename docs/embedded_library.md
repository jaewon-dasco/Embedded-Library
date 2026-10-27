# Embedded Library List

Root: `D:\1_Embedded Project\100_Library\`

## OneLibrary

| File                      | Description                                    | Path                                 |
| ------------------------- | ---------------------------------------------- | ------------------------------------ |
| ONE_Common.c/h            | Common types, macros, JSON parser              | `OneLibrary/Src/`, `OneLibrary/Inc/` |
| ONE_Time.c/h              | Timer, RTC, tick management                    | `OneLibrary/Src/`, `OneLibrary/Inc/` |
| ONE_Math.c/h              | Moving average, 1st-order LPF, polynomial, RPY | `OneLibrary/Src/`, `OneLibrary/Inc/` |
| ONE_Filter.c/h            | 2nd-order Biquad IIR (LPF, HPF, BPF, Shelf)    | `OneLibrary/Src/`, `OneLibrary/Inc/` |
| ONE_Signal.c/h            | Threshold, debounce, trigger, blink, delay     | `OneLibrary/Src/`, `OneLibrary/Inc/` |
| ONE_Memory.c/h            | Memory utility (IsAllZero, etc.)               | `OneLibrary/Src/`, `OneLibrary/Inc/` |
| ONE_CAN.c/h               | CAN bus interface                              | `OneLibrary/Src/`, `OneLibrary/Inc/` |
| ONE_Serial.c/h            | Serial debug output (variadic printf)          | `OneLibrary/Src/`, `OneLibrary/Inc/` |
| ONE_Task.c/h              | Task scheduling and management                 | `OneLibrary/Src/`, `OneLibrary/Inc/` |
| ONE_RingBuffer.c/h        | Ring/circular buffer                           | `OneLibrary/Src/`, `OneLibrary/Inc/` |
| ONE_ATCommend.c/h         | AT command parser (UART)                       | `OneLibrary/Src/`, `OneLibrary/Inc/` |
| ONE_SerialTransceiver.c/h | Serial packet transceiver (0xFE protocol)      | `OneLibrary/Src/`, `OneLibrary/Inc/` |
| ONE_SevenSegment.c/h      | Seven-segment display driver                   | `OneLibrary/Src/`, `OneLibrary/Inc/` |
| ONE_SlidingHistogram.c/h  | Sliding histogram (mode detection, EMA)        | `OneLibrary/Src/`, `OneLibrary/Inc/` |

## oThirdParty - Sensor

| File                   | Description                                 | Path                                   |
| ---------------------- | ------------------------------------------- | -------------------------------------- |
| MEMS_IIM42351.c/h      | IIM-42351 3-axis accelerometer (SPI, 24MHz) | `oThirdParty/Src/`, `oThirdParty/Inc/` |
| MEMS_ICM42688P.c/h     | ICM-42688-P 6-axis IMU (SPI, 24MHz)         | `oThirdParty/Src/`, `oThirdParty/Inc/` |
| MEMS_ADXL355BEZRL7.c/h | ADXL355 precision accelerometer (SPI)       | `oThirdParty/Src/`, `oThirdParty/Inc/` |
| TEMP_TMP1075NDRLR.c/h  | TMP1075 temperature sensor (I2C, 0x48)      | `oThirdParty/Src/`, `oThirdParty/Inc/` |
| ADC_NAU7802.c/h        | NAU7802 24-bit ADC (I2C)                    | `oThirdParty/Src/`, `oThirdParty/Inc/` |

## oThirdParty - Communication

| File             | Description                   | Path                                   |
| ---------------- | ----------------------------- | -------------------------------------- |
| LoRa_NML35.c/h   | NML35 LoRa transceiver (UART) | `oThirdParty/Src/`, `oThirdParty/Inc/` |
| LoRa_RAK3172.c/h | RAK3172 LoRa module (UART)    | `oThirdParty/Src/`, `oThirdParty/Inc/` |
| LTE_TYPE1SC.c/h  | TYPE1SC LTE cellular modem    | `oThirdParty/Src/`, `oThirdParty/Inc/` |
| LAN_WIZ750SR.c/h | WIZ750SR Ethernet (TCP/UDP)   | `oThirdParty/Src/`, `oThirdParty/Inc/` |

## oThirdParty - Peripheral

| File                         | Description                               | Path                                   |
| ---------------------------- | ----------------------------------------- | -------------------------------------- |
| EEPROM_24LC.c/h              | 24LC EEPROM driver (I2C)                  | `oThirdParty/Src/`, `oThirdParty/Inc/` |
| NAND_MT29F2G01ABAGDWB_IT.c/h | MT29F NAND Flash (QSPI Dual)              | `oThirdParty/Src/`, `oThirdParty/Inc/` |
| MCP23S17.c/h                 | MCP23S17 16-bit I/O Expander (SPI, 10MHz) | `oThirdParty/Src/`, `oThirdParty/Inc/` |
