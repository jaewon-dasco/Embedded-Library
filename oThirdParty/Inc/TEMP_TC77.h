/*
 * TEMP_TC77.h
 *
 *  Microchip TC77-3.3MCTTR thermal sensor, 13 bit, SPI compatible.
 *  Datasheet: Microchip TC77-3.3 (DS21462) - section/table numbers below.
 *
 *
 *  Bus
 *  ---
 *  Three wires: CS, SCK, and SI/O. SI/O is a SINGLE BIDIRECTIONAL line, so the
 *  SPI peripheral runs in HALF DUPLEX MASTER mode, where the single data line
 *  is MOSI - which is where SI/O sits on this board, through a 100 ohm series
 *  resistor (R10). Receive-only master would not work here: that mode samples
 *  MISO, and MISO is not wired to the part. CS is a plain GPIO.
 *
 *  Read sequence (datasheet p.9)
 *  ----------------------------
 *   1. CS low - the falling edge starts the transaction.
 *   2. 16 SCK pulses, MSB first. The part puts the first bit out on the
 *      rising edge and clocks the rest out on the falling edge, so the host
 *      samples on rising edges: CPOL 0 with CPHA 0.
 *   3. CS high, which ends it.
 *
 *  Only 14 of the 16 bits carry data (13 temperature bits plus the ready
 *  flag); the part tri-states SI/O after them and the last two bits read back
 *  as 1. That is why the low three bits are masked off before the shift.
 *
 *  Constraints
 *  -----------
 *  - The part powers up in Continuous Conversion mode, so no configuration
 *    write is needed to start it. This driver never writes the Configuration
 *    register.
 *  - TC77_READY_Msk stays 0 until the first conversion finishes. A read taken
 *    before that carries no temperature, so it is reported as RESULT_RUN and
 *    not as an error.
 *  - The Manufacturer ID register answers only in Shutdown mode, and entering
 *    Shutdown stops conversion. There is no cheap identity check on a running
 *    part, so Open cannot confirm the device the way a WHO_AM_I register does.
 */

#ifndef INC_TEMP_TC77_H_
#define INC_TEMP_TC77_H_

#include "ONE_Common.h"
#include "ONE_Signal.h"

#define TEMP_TC77_VERSION        0.3

/* Configuration register codes, datasheet p.10 "XX00 / XXFF" */
#define TC77_CONFIG_CONTINUOUS   0x0000
#define TC77_CONFIG_SHUTDOWN     0xFFFF

/* Manufacturer ID, readable in Shutdown mode only, bits 15:8 (p.11) */
#define TC77_MANUFACTURER_ID     0x54

/* Temperature register layout (p.9)
 *   bit 15..3 : 13 bit two's complement temperature
 *   bit 2     : 1 once the first conversion has completed
 *   bit 1..0  : tri-stated, read back as 1 */
#define TC77_READY_Msk           0x0004
#define TC77_TEMPERATURE_Msk     0xFFF8
#define TC77_TEMPERATURE_Pos     3

/* Transfer width in bits; 14 carry data but the frame is 16 clocks (p.9) */
#define TC77_FRAME_BIT_COUNT     16

/* Conversion time tCT, 300ms typical / 400ms maximum (p.2). The maximum is
 * used so a poll never reports a stale sample as fresh. */
#define TC77_CONVERSION_TIME_MS  400

/* 0.0625 degC per LSB = 512/8192 (p.2) */
#define TC77_LSB_DEGREE          0.0625f

/* HAL_SPI_Receive timeout. The frame is 16 bits at 125kHz, so 128us; the
 * margin covers a blocked bus rather than a slow transfer. */
#define TC77_RECEIVE_TIMEOUT_MS  50

typedef struct TC77_s{
	SPI_HandleTypeDef *pSPI;
	oIO_t CS;

	struct{
		uint16_t TEMP;     /* raw 16 bit frame as read */
	}Register;

	float  Temperature;    /* Adc converted to 0.01 degC */

	struct{
		uint8_t  IsReady;  /* ready flag seen at least once */
		uint8_t  IsOpen;
		uint8_t  CountOfError;
	}State;

}TC77_t;

extern oResult_t TC77_Open(SPI_HandleTypeDef *pSPI, oIO_t CS, TC77_t *pTC77);
extern oResult_t TC77_GetData(TC77_t *pTC77);

/* History

2026-10-15 | v0.1
    - baseline: bit banged 16 bit read, ready flag gate

2026-10-15 | v0.2
    - SPI half duplex master replaces the bit bang; GPIO mode juggling gone
    - sign handled by the signed cast on the left aligned field, fixing
      sub-zero readings that the manual bit-15 test could never reach
    - Close removed, no caller

*/

#endif /* INC_TEMP_TC77_H_ 
2026-10-20 | v0.3
	- CS handed over by value; CS.Port == NULL means no CS line, Open no longer rejects it and every CS toggle is skipped for a NULL Port
*/
