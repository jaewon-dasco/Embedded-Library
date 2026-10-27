# ISM330DHCX

| 항목         | 값                                                                             |
| ------------ | ------------------------------------------------------------------------------ |
| 제조사·품목 | STMicroelectronics · 6축 산업용 IMU (3축 가속도 + 3축 자이로 + 온도)          |
| 인터페이스   | SPI 4-wire ≤10 MHz / I2C ≤1 MHz / MIPI I3C + **보조 SPI(SPI2, OIS 전용)**    |
| 주소 폭      | 7-bit (0x00~0x7E), 레지스터 폭 전부 1바이트                                    |
| 데이터시트   | DS13012 Rev 7 (167 p)                                                          |
| PDF 경로     | `D:\0_Document\0_데이터시트\1_IC\MEMS\Datasheet_STMicronics_ISM330DHCX.pdf`    |
| 드라이버     | `oThirdParty/Inc/MEMS_ISM330DHCXTR.h` · `oThirdParty/Src/MEMS_ISM330DHCXTR.c` |
| 섀도 이미지  | `N = 0x7F = 127` 바이트, 그중 예약 48바이트(38%) 낭비                          |

**동기화 분류** — `1` 매 주기 부품→호스트 · `2` 호스트→부품 설정값 · `3` 1회성(ID·초기화·뱅크)

## 레지스터 맵

| 주소  | 이름                     | 폭  | 접근     | 예약비트   | 자기클리어 | 휘발 | 동기화 | AutoSync | POR    | 비고                                    |
| ----- | ------------------------ | --- | -------- | ---------- | ---------- | ---- | ------ | -------- | ------ | --------------------------------------- |
| 00    | RESERVED                 | 1   | RESERVED | —         | —         | —   | —     | 금지     | —     | 예약 — 쓰면 영구 손상                  |
| 01    | FUNC_CFG_ACCESS          | 1   | RW       | 0x3F       | —         | —   | 3      | 금지     | 0x00   | 뱅크 전환 — 주소 의미가 바뀜           |
| 02    | PIN_CTRL                 | 1   | RW       | 1고정 0x3F | —         | —   | 3      | 금지     | 0x3F   | 1고정 비트 — RMW 필수                  |
| 03-06 | RESERVED                 | 4   | RESERVED | —         | —         | —   | —     | 금지     | —     | 예약 — 쓰면 영구 손상                  |
| 07    | FIFO_CTRL1               | 1   | RW       | —         | —         | —   | 2      | 허용     | 0x00   | WTM[7:0]                                |
| 08    | FIFO_CTRL2               | 1   | RW       | 미확인     | —         | —   | 2      | 허용     | 0x00   | STOP_ON_WTM·WTM8·압축 설정            |
| 09    | FIFO_CTRL3               | 1   | RW       | —         | —         | —   | 2      | 허용     | 0x00   | BDR_XL[3:0]·BDR_GY[3:0]                |
| 0A    | FIFO_CTRL4               | 1   | RW       | 미확인     | —         | —   | 2      | 허용     | 0x00   | FIFO mode·ODR_T_BATCH·DEC_TS          |
| 0B    | COUNTER_BDR_REG1         | 1   | RW       | 미확인     | —         | —   | 2      | 허용     | 0x00   | dataready_pulsed 포함                   |
| 0C    | COUNTER_BDR_REG2         | 1   | RW       | —         | —         | —   | 2      | 허용     | 0x00   | CNT_BDR_TH[7:0]                         |
| 0D    | INT1_CTRL                | 1   | RW       | 미확인     | —         | —   | 2      | 허용     | 0x00   | INT1 라우팅                             |
| 0E    | INT2_CTRL                | 1   | RW       | 미확인     | —         | —   | 2      | 허용     | 0x00   | INT2 라우팅                             |
| 0F    | WHO_AM_I                 | 1   | RO       | —         | —         | —   | 3      | 금지     | 0x6B   | RO — 쓰기 불가                         |
| 10    | CTRL1_XL                 | 1   | RW       | 0x01       | —         | —   | 2      | 허용     | 0x00   | ODR[7:4]·FS[3:2]·LPF2_XL_EN(b1)       |
| 11    | CTRL2_G                  | 1   | RW       | —         | —         | —   | 2      | 허용     | 0x00   | ODR[7:4]·FS_G[3:2]·FS_125·FS_4000    |
| 12    | CTRL3_C                  | 1   | RW       | 0x02       | 0x81       | ✓    | 3      | 금지     | 0x04   | 쓰면 리셋·부팅 재발행                  |
| 13    | CTRL4_C                  | 1   | RW       | 0x91       | —         | —   | 2      | 허용     | 0x00   | I2C_disable·DRDY_MASK·SLEEP_G         |
| 14    | CTRL5_C                  | 1   | RW       | 0x90       | —         | —   | 2      | 허용     | 0x00   | ROUNDING[6:5]·self-test                |
| 15    | CTRL6_C                  | 1   | RW       | —         | —         | —   | 2      | 허용     | 0x00   | DEN trig·XL_HM_MODE·FTYPE(자이로)     |
| 16    | CTRL7_G                  | 1   | RW       | 0x08       | —         | —   | 2      | 허용     | 0x00   | G_HM_MODE·HPM_G·OIS_ON_EN             |
| 17    | CTRL8_XL                 | 1   | RW       | 0x02       | —         | —   | 2      | 허용     | 0x00   | HPCF[7:5]·LOW_PASS_ON_6D(b0)           |
| 18    | CTRL9_XL                 | 1   | RW       | 0x01       | —         | —   | 3      | 금지     | 0xE0   | DEVICE_CONF — 리셋 후 부품만 0         |
| 19    | CTRL10_C                 | 1   | RW       | 0xDF       | —         | —   | 2      | 허용     | 0x00   | TIMESTAMP_EN(b5)만 유효                 |
| 1A    | ALL_INT_SRC              | 1   | RO       | —         | —         | ✓    | 1      | 금지     | output | 읽으면 클리어 (LIR=1)                   |
| 1B    | WAKE_UP_SRC              | 1   | RO       | —         | —         | ✓    | 1      | 금지     | output | 읽으면 클리어 (LIR=1)                   |
| 1C    | TAP_SRC                  | 1   | RO       | —         | —         | ✓    | 1      | 금지     | output | 읽으면 클리어 (LIR=1)                   |
| 1D    | D6D_SRC                  | 1   | RO       | —         | —         | ✓    | 1      | 금지     | output | 읽으면 클리어 (LIR=1)                   |
| 1E    | STATUS_REG               | 1   | RO       | 0xF8       | —         | ✓    | 1      | 금지     | output | 출력 읽으면 하강. SPI2 는 STATUS_SPIAux |
| 1F    | RESERVED                 | 1   | RESERVED | —         | —         | —   | —     | 금지     | —     | 예약 — 쓰면 영구 손상                  |
| 20-21 | OUT_TEMP_L/H             | 2   | RO       | —         | —         | ✓    | 1      | 금지     | output | BDU=1 시 2바이트 일괄 읽기              |
| 22-27 | OUTX/Y/Z_L/H_G           | 6   | RO       | —         | —         | ✓    | 1      | 금지     | output | 자이로 3축 2's comp                     |
| 28-2D | OUTX/Y/Z_L/H_A           | 6   | RO       | —         | —         | ✓    | 1      | 금지     | output | 가속도 3축 2's comp                     |
| 2E-34 | RESERVED                 | 7   | RESERVED | —         | —         | —   | —     | 금지     | —     | 예약 — 쓰면 영구 손상                  |
| 35    | EMB_FUNC_STATUS_MAINPAGE | 1   | RO       | —         | —         | ✓    | 1      | 금지     | output | FSM long counter 인터럽트               |
| 36-37 | FSM_STATUS_A/B_MAINPAGE  | 2   | RO       | —         | —         | ✓    | 1      | 금지     | output | FSM 1~16 상태                           |
| 38    | MLC_STATUS_MAINPAGE      | 1   | RO       | —         | —         | ✓    | 1      | 금지     | output | MLC 0~7 상태                            |
| 39    | STATUS_MASTER_MAINPAGE   | 1   | RO       | —         | —         | ✓    | 1      | 금지     | output | sensor hub 상태                         |
| 3A    | FIFO_STATUS1             | 1   | RO       | —         | —         | ✓    | 1      | 금지     | output | DIFF_FIFO[7:0]                          |
| 3B    | FIFO_STATUS2             | 1   | RO       | —         | —         | ✓    | 1      | 금지     | output | 읽으면 클리어 (p.63 각주)               |
| 3C-3F | RESERVED                 | 4   | RESERVED | —         | —         | —   | —     | 금지     | —     | 예약 — 쓰면 영구 손상                  |
| 40-43 | TIMESTAMP0~3             | 4   | RO       | —         | —         | ✓    | 1      | 금지     | output | 32-bit 카운터                           |
| 44-55 | RESERVED                 | 18  | RESERVED | —         | —         | —   | —     | 금지     | —     | 예약 — 쓰면 영구 손상                  |
| 56    | TAP_CFG0                 | 1   | RW       | 미확인     | —         | —   | 2      | 허용     | 0x00   | LIR·INT_CLR_ON_READ 포함               |
| 57    | TAP_CFG1                 | 1   | RW       | 미확인     | —         | —   | 2      | 허용     | 0x00   | tap 우선순위·X축 임계                  |
| 58    | TAP_CFG2                 | 1   | RW       | 미확인     | —         | —   | 2      | 허용     | 0x00   | INTERRUPTS_ENABLE                       |
| 59    | TAP_THS_6D               | 1   | RW       | 미확인     | —         | —   | 2      | 허용     | 0x00   | 6D 임계·Z축 tap 임계                   |
| 5A    | INT_DUR2                 | 1   | RW       | 미확인     | —         | —   | 2      | 허용     | 0x00   | tap SHOCK·QUIET·DUR                   |
| 5B    | WAKE_UP_THS              | 1   | RW       | 미확인     | —         | —   | 2      | 허용     | 0x00   | wake-up 임계                            |
| 5C    | WAKE_UP_DUR              | 1   | RW       | 미확인     | —         | —   | 2      | 허용     | 0x00   | wake-up·sleep 지속                     |
| 5D    | FREE_FALL                | 1   | RW       | 미확인     | —         | —   | 2      | 허용     | 0x00   | free-fall 임계·지속                    |
| 5E-5F | MD1_CFG · MD2_CFG       | 2   | RW       | 미확인     | —         | —   | 2      | 허용     | 0x00   | 이벤트 → INT1/INT2 라우팅              |
| 60-62 | RESERVED                 | 3   | RESERVED | —         | —         | —   | —     | 금지     | —     | 예약 — 쓰면 영구 손상                  |
| 63    | INTERNAL_FREQ_FINE       | 1   | RO       | —         | —         | —   | 3      | 금지     | output | 공장 트림값 — RO                       |
| 64-6E | RESERVED                 | 11  | RESERVED | —         | —         | —   | —     | 금지     | —     | 예약 — 쓰면 영구 손상                  |
| 6F    | INT_OIS                  | 1   | RW/RO    | 미확인     | —         | —   | 3      | 금지     | 0x00   | OIS 체인 ON 시 primary 는 RO            |
| 70-72 | CTRL1~3_OIS              | 3   | RW/RO    | 미확인     | —         | —   | 3      | 금지     | 0x00   | OIS 체인 ON 시 primary 는 RO            |
| 73-75 | X/Y/Z_OFS_USR            | 3   | RW       | —         | —         | —   | 2      | 허용     | 0x00   | 2's comp, 가중치는 `CTRL6_C.USR_OFF_W`  |
| 76-77 | RESERVED                 | 2   | RESERVED | —         | —         | —   | —     | 금지     | —     | 예약 — 쓰면 영구 손상                  |
| 78    | FIFO_DATA_OUT_TAG        | 1   | RO       | —         | —         | ✓    | 1      | 금지     | output | 읽으면 FIFO pop                         |
| 79-7E | FIFO_DATA_OUT_X/Y/Z_L/H  | 6   | RO       | —         | —         | ✓    | 1      | 금지     | output | 읽으면 FIFO pop                         |

## 동기화 설계 주의

IIS2ICLX 와 공통인 다섯 함정(예약 주소 손상 · 2단 뱅크 · 자기클리어 · read-to-clear · `DEVICE_CONF` 역방향 사각지대)에 더해, **이 IC 에는 보조 SPI 로 인한 소유권 함정이 하나 더 있다.**

| 함정                  | 내용                                                                                                                                                                                                         |
| --------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| 예약 주소 blind write | p.41 — 예약 레지스터는 변경 금지, 쓰면 영구 손상 가능. 48바이트가 대상                                                                                                                                      |
| 2단 뱅크              | `FUNC_CFG_ACCESS(01h)` 의 `FUNC_CFG_EN`/`SHUB_REG_ACCESS` → embedded-functions·sensor hub 뱅크, 그 안에서 다시 `PAGE_SEL` 로 페이지 분기                                                                   |
| 자기클리어            | `CTRL3_C.SW_RESET(b0)`·`BOOT(b7)` 는 Tab.48 p.51 *"automatically cleared"* — 섀도가 1을 기억하면 매 Sync 마다 리셋 재발행                                                                                  |
| read-to-clear         | `FIFO_STATUS2` 는 p.63 *"reset when this register is read"*. LIR=1 이면 `ALL_INT_SRC`·`WAKE_UP_SRC`·`TAP_SRC`·`D6D_SRC` 동일. `STATUS_REG` 는 출력 읽기로 하강                                            |
| 역방향 사각지대       | `CTRL9_XL.DEVICE_CONF` — SW reset 으로 부품만 0, 섀도는 1로 남아 diff 가 차이를 못 본다                                                                                                                     |
| **SPI2 소유권 전환**  | Tab.19 p.40 Comment 열 — OIS 체인이 SPI2 로 켜져 있으면 `INT_OIS(6F)`·`CTRL1~3_OIS(70-72)` 가 primary 인터페이스에서 **RO 로 바뀐다**. diff 가 write 를 내도 조용히 무시되어 섀도와 부품이 영구히 어긋난다 |

**상호의존 필드**: `CTRL2_G.FS_4000(b0)=1` 이면 `FS_G[3:2]`·`FS_125(b1)` 가 무시된다(Tab.45 p.50). 바이트 단위 diff 자체는 성립하나, 호스트 측 감도 환산은 이 우선순위를 반영한 별도 로직이 필요하다.

주소 자동증가는 `IF_INC=1` 에서 예외 없는 +1 이고(§6.2 p.21) 레지스터 폭도 전부 1바이트라 오프셋 항등 자체는 성립한다. **0x7E 를 넘어선 자동증가는 규정이 없다 — 미정의.** `BDU=1` 일 때 16-bit 값은 MSB/LSB 를 한 트랜잭션으로 읽어야 하므로 부분 memcpy 분할을 금지한다.

**권고 구조**: `Image[127]` + 동일 크기 `const Attr[127]`(`RESERVED_ADDR`·`RO`·`SELF_CLEAR_MASK`·`RES_BIT_MASK`·`SYNCABLE`). diff 는 `SYNCABLE` 주소에만, 쓰기는 `RES_BIT_MASK` 기준 read-modify-write. 뱅크·OIS 접근은 이미지 밖 별도 경로로 격리한다.

## 현행 드라이버 결함

| 위치                                     | 현행                                             | 데이터시트                                                                 | 판정                                                                       |
| ---------------------------------------- | ------------------------------------------------ | -------------------------------------------------------------------------- | -------------------------------------------------------------------------- |
| `MEMS_ISM330DHCXTR.h` `CTRL2_G` bit0     | 이름 없는 예약 비트 (`uint8_t : 1;`)             | Tab.44 p.50 — bit0 = `FS_4000`                                            | **결함** — 실재 기능 비트를 예약 처리, 섀도 오버레이로 접근 불가          |
| 동 헤더 gyro FS enum 주석                | *"별도 CTRL6_C.FS_4000=1 → ±4000 dps"*         | Tab.44/45 — `FS_4000` 은 `CTRL2_G(11h)` bit0. `CTRL6_C` 에 해당 비트 없음 | **결함** — 레지스터 오기                                                  |
| `MEMS_ISM330DHCXTR.c` `FIFO_ReadBurst()` | 0x78 에서 `EntryCount × 7` 바이트를 한 트랜잭션 | 자동증가는 +1, 맵은 0x7E 종료. wrap 규정 없음                              | **결함** — `EntryCount>1` 이면 미정의 동작. 엔트리마다 0x78 재시작이 안전 |
| 동 헤더 `CTRL6_C.FTYPE_XL : 3`           | 이름이 XL(가속도)                                | Tab.56 p.54 — `FTYPE[2:0]` 는 **자이로 LPF1** 대역폭                      | **명명 결함** — 비트 위치는 정확                                          |
| 동 헤더 `CTRL6_C.DEN_MODE : 3` (b7:5)    | 단일 3비트 필드                                  | Tab.55 — bit7 `TRIG_EN`, bit6 `LVL1_EN`, bit5 `LVL2_EN`                   | 경미 — 위치 일치, 세분 명명만 다름                                        |

검증 통과 항목: 가속도 감도 `0.061/0.122/0.244/0.488 mg/LSB` · 자이로 `4.375/8.75/17.50/35/70/140 mdps/LSB`(Tab.2 p.9-10) · `CTRL1_XL` FS `00=±2g, 01=±16g, 10=±4g, 11=±8g`(Tab.42) · ODR `1011=1.6 Hz LP`(Tab.43) · `CTRL4_C`(Tab.49) · `CTRL8_XL`(Tab.60) · `STATUS_REG`(Tab.74) 비트 배치 · 온도 `raw/256+25`(TSen 256 LSB/℃ p.13) · `WHO_AM_I=0x6B`(Tab.19 p.39).

## 미확인 사항

| 대상                                                                                     | 사유                                                   |
| ---------------------------------------------------------------------------------------- | ------------------------------------------------------ |
| `FIFO_CTRL2/4`·`COUNTER_BDR_REG1`·`INT1/INT2_CTRL` 예약비트                            | 해당 레지스터 상세 표(§9.3~9.10) 미열람               |
| `TAP_CFG0~2`·`TAP_THS_6D`·`INT_DUR2`·`WAKE_UP_*`·`FREE_FALL`·`MD1/MD2_CFG` 예약비트 | 이벤트 검출 계열 상세 표(§9.42~9.50) 미열람           |
| `INT_OIS`·`CTRL1~3_OIS` 예약비트, RO 전환 정확한 조건                                   | OIS 절(§9.52~9.55) 미열람 — Comment 열 표기로만 확인 |
| 0x7E 초과 주소 자동증가 동작                                                             | 데이터시트에 규정 없음 — 미정의                       |
| embedded-functions 뱅크·sensor hub 뱅크 내부 맵                                         | §10·§11·§14·§15 미열람 (본 검토 범위 밖)        |
