# IIS2ICLX

| 항목         | 값                                                                         |
| ------------ | -------------------------------------------------------------------------- |
| 제조사·품목 | STMicroelectronics · 2축 고정밀 MEMS 경사계 (X·Y + 온도)                 |
| 인터페이스   | SPI 4-wire ≤10 MHz (mode 3) / I2C ≤1 MHz / MIPI I3C                      |
| 주소 폭      | 7-bit (0x00~0x7E), 레지스터 폭 전부 1바이트                                |
| 데이터시트   | DS13005 Rev 7 (119 p)                                                      |
| PDF 경로     | `D:\0_Document\0_데이터시트\1_IC\MEMS\Datasheet_STMicronics_IIS2ICLX.pdf`  |
| 드라이버     | `oThirdParty/Inc/MEMS_IIS2ICLXTR.h` · `oThirdParty/Src/MEMS_IIS2ICLXTR.c` |
| 섀도 이미지  | `N = 0x7F = 127` 바이트, 그중 예약 67바이트(53%) 낭비                      |

**동기화 분류** — `1` 매 주기 부품→호스트 · `2` 호스트→부품 설정값 · `3` 1회성(ID·초기화·뱅크)

## 레지스터 맵

| 주소  | 이름                     | 폭  | 접근     | 예약비트           | 자기클리어 | 휘발 | 동기화 | AutoSync | POR    | 비고                              |
| ----- | ------------------------ | --- | -------- | ------------------ | ---------- | ---- | ------ | -------- | ------ | --------------------------------- |
| 00    | RESERVED                 | 1   | RESERVED | —                 | —         | —   | —     | 금지     | —     | 예약 — 쓰면 영구 손상            |
| 01    | FUNC_CFG_ACCESS          | 1   | RW       | 0x3F               | —         | —   | 3      | 금지     | 0x00   | 뱅크 전환 — 주소 의미가 바뀜     |
| 02    | PIN_CTRL                 | 1   | RW       | 0x80 · 1고정 0x3F | —         | —   | 3      | 금지     | 0x3F   | 1고정 비트 — RMW 필수            |
| 03-06 | RESERVED                 | 4   | RESERVED | —                 | —         | —   | —     | 금지     | —     | 예약 — 쓰면 영구 손상            |
| 07    | FIFO_CTRL1               | 1   | RW       | —                 | —         | —   | 2      | 허용     | 0x00   | WTM[7:0]                          |
| 08    | FIFO_CTRL2               | 1   | RW       | 0x4E               | —         | —   | 2      | 허용     | 0x00   | STOP_ON_WTM·ODRCHG_EN·WTM8      |
| 09    | FIFO_CTRL3               | 1   | RW       | 0xF0               | —         | —   | 2      | 허용     | 0x00   | BDR_XL[3:0]                       |
| 0A    | FIFO_CTRL4               | 1   | RW       | 미확인             | —         | —   | 2      | 허용     | 0x00   | FIFO mode·ODR_T_BATCH            |
| 0B    | COUNTER_BDR_REG1         | 1   | RW       | 미확인             | —         | —   | 2      | 허용     | 0x00   | dataready_pulsed 포함             |
| 0C    | COUNTER_BDR_REG2         | 1   | RW       | —                 | —         | —   | 2      | 허용     | 0x00   | CNT_BDR_TH[7:0]                   |
| 0D    | INT1_CTRL                | 1   | RW       | 미확인             | —         | —   | 2      | 허용     | 0x00   | INT1 라우팅                       |
| 0E    | INT2_CTRL                | 1   | RW       | 미확인             | —         | —   | 2      | 허용     | 0x00   | INT2 라우팅                       |
| 0F    | WHO_AM_I                 | 1   | RO       | —                 | —         | —   | 3      | 금지     | 0x6B   | RO — 쓰기 불가                   |
| 10    | CTRL1_XL                 | 1   | RW       | 0x01               | —         | —   | 2      | 허용     | 0x00   | ODR[7:4]·FS[3:2]·LPF2_XL_EN(b1) |
| 11    | RESERVED                 | 1   | RESERVED | —                 | —         | —   | —     | 금지     | —     | 예약 — 쓰면 영구 손상            |
| 12    | CTRL3_C                  | 1   | RW       | 0x02               | 0x81       | ✓    | 3      | 금지     | 0x04   | 쓰면 리셋·부팅 재발행            |
| 13    | CTRL4_C                  | 1   | RW       | 0xD3               | —         | —   | 2      | 허용     | 0x00   | I2C_disable·DRDY_MASK            |
| 14    | CTRL5_C                  | 1   | RW       | 0xFC               | —         | —   | 2      | 허용     | 0x00   | ST[1:0]_XL self-test              |
| 15    | CTRL6_C                  | 1   | RW       | 0x17               | —         | —   | 2      | 허용     | 0x00   | DEN trig·USR_OFF_W(b3)           |
| 16    | CTRL7_XL                 | 1   | RW       | 0xFD               | —         | —   | 2      | 허용     | 0x00   | USR_OFF_ON_OUT(b1)만 유효         |
| 17    | CTRL8_XL                 | 1   | RW       | 0x03               | —         | —   | 2      | 허용     | 0x00   | HPCF[7:5]·HP_SLOPE(b2)           |
| 18    | CTRL9_XL                 | 1   | RW       | 0x01 · 1고정 0x30 | —         | —   | 3      | 금지     | 0xE0   | DEVICE_CONF — 리셋 후 부품만 0   |
| 19    | CTRL10_C                 | 1   | RW       | 0xDF               | —         | —   | 2      | 허용     | 0x00   | TIMESTAMP_EN(b5)만 유효           |
| 1A    | ALL_INT_SRC              | 1   | RO       | —                 | —         | ✓    | 1      | 금지     | output | 읽으면 클리어 (LIR=1)             |
| 1B    | WAKE_UP_SRC              | 1   | RO       | —                 | —         | ✓    | 1      | 금지     | output | 읽으면 클리어 (LIR=1)             |
| 1C    | TAP_SRC                  | 1   | RO       | —                 | —         | ✓    | 1      | 금지     | output | 읽으면 클리어 (LIR=1)             |
| 1D    | DEN_SRC                  | 1   | RO       | —                 | —         | ✓    | 1      | 금지     | output | latched/pulsed 선택               |
| 1E    | STATUS_REG               | 1   | RO       | 0xFA               | —         | ✓    | 1      | 금지     | output | 출력 읽으면 하강                  |
| 1F    | RESERVED                 | 1   | RESERVED | —                 | —         | —   | —     | 금지     | —     | 예약 — 쓰면 영구 손상            |
| 20-21 | OUT_TEMP_L/H             | 2   | RO       | —                 | —         | ✓    | 1      | 금지     | output | BDU=1 시 2바이트 일괄 읽기        |
| 22-27 | RESERVED                 | 6   | RESERVED | —                 | —         | —   | —     | 금지     | —     | 자이로 자리 — 본 부품 미존재     |
| 28-2B | OUTX/OUTY_L/H_A          | 4   | RO       | —                 | —         | ✓    | 1      | 금지     | output | 2's complement 16-bit ×2축       |
| 2C-34 | RESERVED                 | 9   | RESERVED | —                 | —         | —   | —     | 금지     | —     | 예약 — 쓰면 영구 손상            |
| 35    | EMB_FUNC_STATUS_MAINPAGE | 1   | RO       | —                 | —         | ✓    | 1      | 금지     | output | FSM long counter 인터럽트         |
| 36-37 | FSM_STATUS_A/B_MAINPAGE  | 2   | RO       | —                 | —         | ✓    | 1      | 금지     | output | FSM 1~16 상태                     |
| 38    | MLC_STATUS_MAINPAGE      | 1   | RO       | —                 | —         | ✓    | 1      | 금지     | output | MLC 0~7 상태                      |
| 39    | STATUS_MASTER_MAINPAGE   | 1   | RO       | —                 | —         | ✓    | 1      | 금지     | output | sensor hub 상태                   |
| 3A    | FIFO_STATUS1             | 1   | RO       | —                 | —         | ✓    | 1      | 금지     | output | DIFF_FIFO[7:0]                    |
| 3B    | FIFO_STATUS2             | 1   | RO       | —                 | —         | ✓    | 1      | 금지     | output | 읽으면 클리어 (p.45 각주)         |
| 3C-3F | RESERVED                 | 4   | RESERVED | —                 | —         | —   | —     | 금지     | —     | 예약 — 쓰면 영구 손상            |
| 40-43 | TIMESTAMP0~3             | 4   | RO       | —                 | —         | ✓    | 1      | 금지     | output | 32-bit 카운터                     |
| 44-55 | RESERVED                 | 18  | RESERVED | —                 | —         | —   | —     | 금지     | —     | 예약 — 쓰면 영구 손상            |
| 56    | TAP_CFG0                 | 1   | RW       | 미확인             | —         | —   | 2      | 허용     | 0x00   | LIR·INT_CLR_ON_READ 포함         |
| 57    | TAP_CFG1                 | 1   | RW       | 미확인             | —         | —   | 2      | 허용     | 0x00   | tap 우선순위·X축 임계            |
| 58    | TAP_CFG2                 | 1   | RW       | 미확인             | —         | —   | 2      | 허용     | 0x00   | INTERRUPTS_ENABLE                 |
| 59    | RESERVED                 | 1   | RESERVED | —                 | —         | —   | —     | 금지     | —     | 6D 자리 — 본 부품 미존재         |
| 5A    | INT_DUR2                 | 1   | RW       | 미확인             | —         | —   | 2      | 허용     | 0x00   | tap SHOCK·QUIET·DUR             |
| 5B    | WAKE_UP_THS              | 1   | RW       | 미확인             | —         | —   | 2      | 허용     | 0x00   | wake-up 임계                      |
| 5C    | WAKE_UP_DUR              | 1   | RW       | 미확인             | —         | —   | 2      | 허용     | 0x00   | wake-up·sleep 지속               |
| 5D    | RESERVED                 | 1   | RESERVED | —                 | —         | —   | —     | 금지     | —     | free-fall 자리 — 미존재          |
| 5E-5F | MD1_CFG · MD2_CFG       | 2   | RW       | 미확인             | —         | —   | 2      | 허용     | 0x00   | 이벤트 → INT1/INT2 라우팅        |
| 60-62 | RESERVED                 | 3   | RESERVED | —                 | —         | —   | —     | 금지     | —     | 예약 — 쓰면 영구 손상            |
| 63    | INTERNAL_FREQ_FINE       | 1   | RO       | —                 | —         | —   | 3      | 금지     | output | 공장 트림값 — RO                 |
| 64-72 | RESERVED                 | 15  | RESERVED | —                 | —         | —   | —     | 금지     | —     | OIS 자리 — 본 부품 미존재        |
| 73    | X_OFS_USR                | 1   | RW       | —                 | —         | —   | 2      | 허용     | 0x00   | 2's comp, 범위 -127~127           |
| 74    | Y_OFS_USR                | 1   | RW       | —                 | —         | —   | 2      | 허용     | 0x00   | 2's comp, 범위 -127~127           |
| 75-77 | RESERVED                 | 3   | RESERVED | —                 | —         | —   | —     | 금지     | —     | 예약 — 쓰면 영구 손상            |
| 78    | FIFO_DATA_OUT_TAG        | 1   | RO       | —                 | —         | ✓    | 1      | 금지     | output | 읽으면 FIFO pop                   |
| 79-7E | FIFO_DATA_OUT_X/Y/Z_L/H  | 6   | RO       | —                 | —         | ✓    | 1      | 금지     | output | 읽으면 FIFO pop                   |

## 동기화 설계 주의

**섀도 이미지 diff 는 이 IC 에서 레지스터 속성 테이블 없이는 성립하지 않는다.** 아래 다섯 가지가 각각 독립적으로 설계를 깨뜨린다.

| 함정                  | 내용                                                                                                                                                      |
| --------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 예약 주소 blind write | p.32 원문 *"Reserved registers must not be changed. Writing to those registers may cause permanent damage to the device."* — 67바이트가 대상             |
| 2단 뱅크              | `FUNC_CFG_ACCESS(01h).FUNC_CFG_EN=1` → 0x02~0x7E 가 embedded-functions 뱅크로 치환, 그 안에서 다시 `PAGE_SEL(02h)` 로 페이지 분기 (§12·§13)           |
| 자기클리어            | `CTRL3_C.SW_RESET(b0)`·`BOOT(b7)` 는 Tab.41 *"automatically cleared"* — 섀도가 1을 기억하면 매 Sync 마다 리셋 재발행                                    |
| read-to-clear         | `FIFO_STATUS2` 는 p.45 *"reset when this register is read"*. LIR=1 이면 `ALL_INT_SRC`·`WAKE_UP_SRC`·`TAP_SRC` 도 동일. `STATUS_REG` 는 출력 읽기로 하강 |
| 역방향 사각지대       | `CTRL9_XL.DEVICE_CONF` 는 SW reset 으로 부품만 0이 되고 섀도는 1로 남는다 — diff 가 차이를 못 봐 재설정이 영원히 발행되지 않는다                         |

주소 자동증가는 `IF_INC=1` 에서 예외 없는 +1 이며(§6.2·§6.3), 레지스터 폭도 전부 1바이트라 오프셋 항등 자체는 성립한다. 다만 **0x7E 를 넘어선 자동증가는 데이터시트에 규정이 없다** — FIFO 다중 엔트리를 한 트랜잭션으로 읽는 것은 근거가 없다.

16-bit 값(`OUT_TEMP`·`OUT_*_A`·`TIMESTAMP`)은 인접 8-bit 2개로 오버레이 가능하나, `BDU=1` 일 때는 MSB/LSB 를 한 트랜잭션으로 읽어야 하므로 부분 memcpy 분할을 금지한다.

**권고 구조**: `Image[127]` 과 같은 크기의 `const Attr[127]`(`RESERVED_ADDR`·`RO`·`SELF_CLEAR_MASK`·`RES_BIT_MASK`·`SYNCABLE`)을 짝지어 두고, diff 는 `SYNCABLE` 주소에만, 쓰기는 `RES_BIT_MASK` 기준 read-modify-write. 뱅크 접근은 이미지 밖 별도 경로로 격리한다.

## 현행 드라이버 결함

| 위치                                      | 현행                                     | 데이터시트                                                                                              | 판정                                                             |
| ----------------------------------------- | ---------------------------------------- | ------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------- |
| `MEMS_IIS2ICLXTR.h` `CTRL8_XL` bit0·bit1 | `LOW_PASS_ON_6D`·`XL_FS_MODE` 기능 비트 | Tab.52 p.43 — 둘 다 *"must be set to 0"*. 6D·XL_FS_MODE 기능 자체가 없음                              | **결함** — 1을 쓰면 미정의 동작                                 |
| 구조체에 `CTRL9_XL` 없음                  | 미정의                                   | p.28 — SPI 사용 시 `I2C_disable=1` + `DEVICE_CONF=1` 필수                                              | **누락** — 초기화·SW reset 후 재설정 불가                      |
| `Register.Data` 가 0x22~0x27 관통 읽기    | `int16_t Reserved[3]`                    | 0x22-0x27 은 예약 주소                                                                                  | 읽기는 무해, 같은 구조를 write 에 재사용하면 p.32 손상 경고 대상 |
| 헤더 상단 "⚠ 미검증 상수"                 | WHO_AM_I·FS 코드·감도·ODR 미검증 표기 | Tab.17 p.31 WHO_AM_I=`01101011`=0x6B, Tab.39 p.39 FS 코드, Tab.2 p.10 감도, Tab.38 ODR — **전부 일치** | 경고문 갱신 대상 (값은 정확)                                     |

검증 통과 항목: 감도 `0.015/0.031/0.061/0.122 mg/LSB`(Tab.2 p.10) · FS 코드 `00=±0.5g, 01=±3g, 10=±1g, 11=±2g`(Tab.39) · 온도 `raw/256+25`(Tab.4 p.11, Tsens 256 LSB/℃) · ODR 최대 833 Hz, `1xxx=RESERVED`(Tab.38) · `CTRL1_XL`·`CTRL3_C`·`CTRL4_C`·`STATUS_REG` 비트 배치.

## 미확인 사항

| 대상                                                          | 사유                                            |
| ------------------------------------------------------------- | ----------------------------------------------- |
| `FIFO_CTRL4`·`COUNTER_BDR_REG1`·`INT1/INT2_CTRL` 예약비트   | 해당 레지스터 상세 표(§11.6~11.10) 미열람      |
| `TAP_CFG0~2`·`INT_DUR2`·`WAKE_UP_*`·`MD1/MD2_CFG` 예약비트 | 이벤트 검출 계열 상세 표(§11.37~11.44) 미열람  |
| 0x7E 초과 주소 자동증가 동작                                  | 데이터시트에 규정 없음 — 미정의                |
| embedded-functions 뱅크·sensor hub 뱅크 내부 맵              | §12·§13·§16·§17 미열람 (본 검토 범위 밖) |
