/*
 * TEMP_TC77.c
 *
 *  Version: 0.3 (2026-10-20)
 */

#include "TEMP_TC77.h"

/* Reads the 16 bit frame. CS frames the transfer while the SPI peripheral in
 * half duplex master mode drives SCK and samples SI/O. The frame is returned
 * exactly as it arrives, flag bits included, because the ready flag lives in
 * it and right aligning here would move the sign bit out from under the mask
 * GetData applies. */
static uint16_t TC77_ReadFrame(TC77_t *pTC77)
{
	uint8_t Buffer[TC77_FRAME_BIT_COUNT / 8] = {0};

	if(pTC77->CS.Port != NULL){
		IO_WRITE(pTC77->CS, IO_HIGH);
	}

	HAL_SPI_Receive(pTC77->pSPI, Buffer, sizeof(Buffer), TC77_RECEIVE_TIMEOUT_MS);

	if(pTC77->CS.Port != NULL){
		IO_WRITE(pTC77->CS, IO_LOW);
	}

	return (uint16_t)((Buffer[0] << 8) | Buffer[1]);
}

oResult_t TC77_Open(SPI_HandleTypeDef *pSPI, oIO_t CS, TC77_t *pTC77)
{
	if(pTC77 == NULL || pSPI == NULL){
		return RESULT_ERROR;
	}

	if(pTC77->State.IsOpen){
		return RESULT_OK;
	}

	pTC77->pSPI = pSPI;
	pTC77->CS   = CS;

	if(pSPI->State == HAL_SPI_STATE_RESET){
		if(HAL_SPI_Init(pSPI) != HAL_OK){
			return RESULT_ERROR;
		}
	}

	pTC77->Register.TEMP      = 0;
	pTC77->Temperature        = 0;
	pTC77->State.IsReady      = 0;
	pTC77->State.CountOfError = 0;

	/* The part is already converting out of reset, so there is nothing to
	 * write. CS is released and the first sample is simply awaited. */
	if(CS.Port != NULL){
		IO_WRITE(CS, IO_LOW);
	}

	pTC77->State.IsOpen = 1;

	return RESULT_OK;
}

/* RESULT_RUN while the first conversion is still running, which is a wait and
 * not a failure. RESULT_ERROR is kept for a frame that cannot be a reading. */
oResult_t TC77_GetData(TC77_t *pTC77)
{
	uint16_t Frame;
	int16_t Value;

	if(pTC77 == NULL || !pTC77->State.IsOpen){
		return RESULT_ERROR;
	}

	Frame = TC77_ReadFrame(pTC77);
	pTC77->Register.TEMP = Frame;

	/* An open or shorted SI/O reads as all ones or all zeros. Neither can be a
	 * real frame: the low two bits are tri-stated and always read 1, so 0x0000
	 * is impossible, and 0xFFFF is -0.0625 degC with the ready flag set, which
	 * a line stuck high imitates exactly. */
	if(Frame == 0x0000 || Frame == 0xFFFF){
		if(pTC77->State.CountOfError < UINT8_MAX){
			pTC77->State.CountOfError++;
		}
		return RESULT_ERROR;
	}

	if(!(Frame & TC77_READY_Msk)){
		return RESULT_RUN;
	}

	pTC77->State.IsReady = 1;

	/* The 13 bit word is left aligned in the frame, so masking the flag bits
	 * away and casting to a signed 16 bit value sign-extends it at no cost;
	 * the divide then right aligns it. Testing a sign bit by hand is what
	 * broke sub-zero readings: once the frame had been shifted right the sign
	 * sat at bit 12 while the test still looked at bit 15, so the negative
	 * branch was unreachable and -1 degC came out as +511 degC. */
	Value = (int16_t)(Frame & TC77_TEMPERATURE_Msk) / (1 << TC77_TEMPERATURE_Pos);

	pTC77->Temperature = (float)Value * TC77_LSB_DEGREE;

	return RESULT_OK;
}
