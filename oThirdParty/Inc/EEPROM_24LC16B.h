/*
 * EEPROM_24LC16B.h
 *
 *  Microchip 24LC16B I2C Serial EEPROM 드라이버.
 *
 *  호환 부품: 24AA16T — 동일 구성(8블록 x 256B, 8bit 워드주소, 16B 페이지)에
 *              전원 범위만 다른 저전압 버전이라 본 드라이버를 그대로 쓴다.
 *
 *  칩 사양
 *   - 용량      : 16 Kbit = 2 KByte
 *   - 구성      : 8 블록 × 256 byte, 페이지 16 byte
 *   - I2C 주소  : 상위 4비트 1010 고정. 블록(0~7)이 디바이스 주소 하위 3비트로 들어감
 *                 → 블록0=0xA0, 블록1=0xA2, … 블록7=0xAE
 *   - 워드주소  : 8bit (블록 내 0~255). ※ 24LC32A(16bit 주소)와 다름 — 혼동 주의
 *   - 페이지쓰기: 16 byte 경계. 쓰기 후 내부 write cycle ~5ms 대기 필요
 *
 *  HW 연결 (IPI HW1.4)
 *   - 버스      : I2C1 (hi2c1) — TMP1075와 공유. SCL=PA9, SDA=PA10
 *   - WP        : DO_EEPROM_WP(PA8) — VIH=쓰기 금지 / VIL=쓰기 허용 (보호가 active-high).
 *                 WP 제어는 호출측(Mi_Storage)
 *
 *  구조: 디바이스 핸들(EEPROM_24LC16B_t) + Init/DeInit/Read/Write/Recovery (TMP1075·NAND 드라이버 패턴).
 *  전송은 HAL_I2C_Mem(8bit 워드주소), 블로킹. Read/Write는 블록·페이지 경계 자동 분할.
 */

#ifndef INC_EEPROM_24LC16B_H_
#define INC_EEPROM_24LC16B_H_

#define EEPROM_24LC16B_VERSION       0.4

#include "main.h"
#include "ONE_Common.h"
#include "ONE_Signal.h"

#define EEPROM_24LC16B_TX_BASE       0xA0                                                       /* 블록0 주소 (1010 000 0) — 블록은 <<1로 가산 */
#define EEPROM_24LC16B_PAGE_SIZE     16
#define EEPROM_24LC16B_BLOCK_SIZE    256
#define EEPROM_24LC16B_TOTAL_BLOCKS  8
#define EEPROM_24LC16B_CAPACITY      (EEPROM_24LC16B_BLOCK_SIZE * EEPROM_24LC16B_TOTAL_BLOCKS)  /* 2048 */

/* WP 는 드라이버가 소유한다 — Write 진입 시 해제, 모든 이탈 경로에서 보호로 되돌린다.
   호출측이 관리하면 에러 리턴 경로에서 복구를 빠뜨려 보호가 풀린 채 남는다.
   WP.Port == NULL 이면 제어를 건너뛴다 (WP 미배선 보드 대응) */
typedef struct {
	I2C_HandleTypeDef *pI2C;
	oIO_t   WP;               /* 보호 active-high — IO_HIGH=쓰기 금지, IO_LOW=허용 */
	uint8_t IsOpen;
	uint8_t CountOfI2CError;  /* HAL_ERROR 연속 누적 — recovery 트리거 */
} EEPROM_24LC16B_t;

extern oResult_t EEPROM_24LC16B_Init  (EEPROM_24LC16B_t *pDev, I2C_HandleTypeDef *pI2C, oIO_t WP);
extern oResult_t EEPROM_24LC16B_DeInit(EEPROM_24LC16B_t *pDev);
extern oResult_t EEPROM_24LC16B_Read  (EEPROM_24LC16B_t *pDev, uint16_t Addr, uint8_t *pData, uint16_t Size);
extern oResult_t EEPROM_24LC16B_Write (EEPROM_24LC16B_t *pDev, uint16_t Addr, const uint8_t *pData, uint16_t Size);
extern oResult_t EEPROM_24LC16B_Recovery(EEPROM_24LC16B_t *pDev);

#endif /* INC_EEPROM_24LC16B_H_ */

/* History

2026-06-29 | v0.1
	- baseline — 범용 24LCxx (24LC32A/16B, 구조체·논블로킹, HAL_I2C_Mem)
2026-07-10 | v0.2
	- 24LC16B 전용화: 블록선택·블로킹 무상태 API·8bit 워드주소. 기존 EEPROM_24LC16B 통합
2026-07-10 | v0.3
	- 구조 재작성(TMP1075·NAND 패턴): 디바이스 핸들 EEPROM_24LC16B_t + Init/DeInit/Recovery,
	  반환 oResult_t, I2C 에러 누적 시 Recovery(I2C DeInit/Init). 전송 로직(청킹·delay) 불변
2026-07-10 | v0.31
	- Init: I2C RESET 상태면 HAL_I2C_Init (미초기화 버스 자동 init). DeInit: 무조건 HAL_I2C_DeInit
2026-07-10 | v0.32
	- 파일·심볼 개명 EEPROM_24LC → EEPROM_24LC16B — 원본 라이브러리 범용 EEPROM_24LC(24LCxx)와 분리.
	  로직 불변, 이름만 변경 (원본 범용 드라이버는 별개 유지)
2026-07-10 | v0.33
	- Read/Write 전 버스 RESET이면 Recovery(DeInit/Init) 호출 — 같은 I2C 버스 공유 드라이버(TMP1075)가
	  Close 시 HAL_I2C_DeInit(RESET)로 남기면 이후 전송이 즉시 실패하던 문제 해소(런타임 write 실패 → 영속 실패)
2026-09-03 | v0.4
	- WP 핀 소유권을 드라이버로 이관 — 핸들에 oIO_t WP 추가, Init 인자로 받는다(시그니처 변경).
	  Write 가 진입 시 해제하고 goto EXIT 단일 복귀점에서 보호로 되돌려, 에러 이탈에서도 WP 가 열린 채
	  남지 않는다. 종전에는 호출측(Mi_Storage)이 부팅 때 한 번 내리고 끝이라 보호가 상시 해제였다.
	  24xx16 WP 는 보호가 active-high (VIH=쓰기 금지 / VIL=허용). WP.Port==NULL 이면 제어 생략

*/
