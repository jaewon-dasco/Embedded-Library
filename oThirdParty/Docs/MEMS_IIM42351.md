# IIM-42351

| 항목       | 값                                                                |
| ---------- | ----------------------------------------------------------------- |
| 제조사     | TDK InvenSense                                                    |
| 구성       | 3축 가속도 전용 (자이로 없음), 16bit 출력                         |
| 인터페이스 | SPI 4-wire/3-wire (max 24MHz) · I2C · I3C                       |
| 주소폭     | 7bit (SPI 1바이트 헤더 = R/W[7] + A[6:0]), 뱅크 0~4               |
| WHO_AM_I   | `0x6C` (0x75)                                                     |
| 데이터시트 | DS-000441 Rev 1.4                                                 |
| PDF        | `D:\0_Document\0_데이터시트\1_IC\MEMS\Datasheet_TDK_IIM42351.pdf` |
| 드라이버   | `oThirdParty/{Inc,Src}/MEMS_IIM42351.{h,c}`                       |

ICM-42688-P와 레지스터 맵이 대체로 호환되나 **자이로 관련 주소가 통째로 비어 있고**(0x25~0x2A, 0x4F), 0x51·0x52의 이름·예약 마스크가 다르다.

동기화 열 표기: `1` 매 주기 부품→호스트 · `2` 호스트→부품 설정값 · `3` 1회성.

## 레지스터 맵

| 뱅크 | 주소      | 이름                             | 폭   | 접근 | 예약비트  | 자기클리어 | 휘발 | 동기화 | AutoSync | POR  | 비고                                            |
| ---- | --------- | -------------------------------- | ---- | ---- | --------- | ---------- | ---- | ------ | -------- | ---- | ----------------------------------------------- |
| 0    | 0x11      | DEVICE_CONFIG                    | 8    | RW   | 0xEE      | 0x01       | —   | 2/3    | 부분허용 | 0x00 | bit0 소프트리셋은 1회성 — diff 마스크 제외     |
| 0    | 0x13      | DRIVE_CONFIG                     | 8    | RW   | 0xC0      | —         | —   | 2      | 허용     | ?    | I2C/SPI slew rate                               |
| 0    | 0x14      | INT_CONFIG                       | 8    | RW   | 0xC0      | —         | —   | 2      | 허용     | 0x00 | INT1/INT2 극성·구동·모드                      |
| 0    | 0x16      | FIFO_CONFIG                      | 8    | RW   | 0x3F      | —         | —   | 2      | 허용     | ?    | FIFO_MODE[7:6]                                  |
| 0    | 0x1D~0x1E | TEMP_DATA1/0                     | 16   | RO   | —        | —         | ✓    | 1      | 금지     | 0x80 | SYNCR — 부품이 갱신                            |
| 0    | 0x1F~0x24 | ACCEL_DATA_X/Y/Z                 | 48   | RO   | —        | —         | ✓    | 1      | 금지     | 0x80 | SYNCR                                           |
| 0    | 0x25~0x2A | (없음)                           | —   | —   | —        | —         | —   | —     | 금지     | —   | 자이로 부재로 맵 공백 — 버스트 통과 금지       |
| 0    | 0x2B~0x2C | TMST_FSYNCH/L                    | 16   | RO   | —        | —         | ✓    | 1      | 금지     | ?    | FSYNC 타임스탬프                                |
| 0    | 0x2D      | INT_STATUS                       | 8    | R/C  | 0xA0      | 0x5F       | ✓    | 1      | 금지     | 0x10 | 읽으면 클리어. b5는 예약(PLL_RDY 없음)          |
| 0    | 0x2E      | FIFO_COUNTH                      | 8    | RO   | —        | —         | ✓    | 1      | 금지     | 0x00 | 읽으면 H/L 동시 latch                           |
| 0    | 0x2F      | FIFO_COUNTL                      | 8    | RO   | —        | —         | ✓    | 1      | 금지     | 0x00 | H를 먼저 읽어야 갱신                            |
| 0    | 0x30      | FIFO_DATA                        | 8    | RO   | —        | —         | ✓    | 1      | 금지     | 0xFF | FIFO pop — 반복 읽기마다 다른 값               |
| 0    | 0x31~0x36 | APEX_DATA0~5                     | 48   | RO   | —        | —         | ✓    | 1      | 금지     | ?    | 보수계·탭·자유낙하(FF_DUR) 결과               |
| 0    | 0x37      | INT_STATUS2                      | 8    | R/C  | 0xF0      | 0x0F       | ✓    | 1      | 금지     | ?    | 읽으면 클리어 (SMD/WOM)                         |
| 0    | 0x38      | INT_STATUS3                      | 8    | R/C  | 0xC8      | 0x37       | ✓    | 1      | 금지     | ?    | 읽으면 클리어 (APEX·FF)                        |
| 0    | 0x4B      | SIGNAL_PATH_RESET                | 8    | W/C  | 0x91      | 0x0E       | —   | 3      | 금지     | 0x00 | 자기클리어 커맨드 — 섀도가 1을 기억하면 재발행 |
| 0    | 0x4C      | INTF_CONFIG0                     | 8    | RW   | 0x06      | —         | —   | 2      | 허용     | 0x30 | 엔디안·UI_SIFS_CFG                             |
| 0    | 0x4D      | INTF_CONFIG1                     | 8    | RW   | 0xF0      | —         | —   | 2      | 허용     | ?    | CLKSEL·RTC_MODE                                |
| 0    | 0x4E      | PWR_MGMT0                        | 8    | RW   | 0xCC      | —         | —   | 2      | 조건부   | 0x00 | 단독·최후 기록 + 200µs 홀드                    |
| 0    | 0x4F      | (없음)                           | —   | —   | —        | —         | —   | —     | 금지     | —   | ICM의 GYRO_CONFIG0 자리 — 맵 공백              |
| 0    | 0x50      | ACCEL_CONFIG0                    | 8    | RW   | 0x10      | —         | —   | 2      | 허용     | 0x03 | FS[7:5] + ODR[3:0]. POR 표기 모순 있음          |
| 0    | 0x51      | TEMP_FILT_CONFIG                 | 8    | RW   | 0x1F      | —         | —   | 2      | 허용     | 0x16 | 예약 기본값 1 포함 — RMW 필수                  |
| 0    | 0x52      | ACCEL_FILT_CONFIG                | 8    | RW   | 0x0F      | —         | —   | 2      | 허용     | 0x11 | 하위니블 예약 기본값 1 — RMW 필수              |
| 0    | 0x53      | ACCEL_CONFIG1                    | 8    | RW   | 0xE1      | —         | —   | 2      | 허용     | 0x0D | 예약 기본값 1 포함 — RMW 필수                  |
| 0    | 0x54      | TMST_CONFIG                      | 8    | RW   | 0xE0      | —         | —   | 2      | 허용     | ?    | 타임스탬프 enable                               |
| 0    | 0x56      | APEX_CONFIG0                     | 8    | RW   | 0x08      | —         | —   | 2      | 허용     | ?    | DMP_ODR·FF_ENABLE                              |
| 0    | 0x57      | SMD_CONFIG                       | 8    | RW   | 0xE0      | —         | —   | 2      | 허용     | ?    | WOM/SMD 모드                                    |
| 0    | 0x5F      | FIFO_CONFIG1                     | 8    | RW   | 0x80      | —         | —   | 2      | 허용     | ?    | FIFO 항목 선택                                  |
| 0    | 0x60~0x61 | FIFO_CONFIG2/3                   | 16   | RW   | 0xF0(상)  | —         | —   | 2      | 허용     | ?    | FIFO_WM[11:0]                                   |
| 0    | 0x62      | FSYNC_CONFIG                     | 8    | RW   | 0x8C      | 0x02       | —   | 2      | 조건부   | ?    | bit1 FSYNC_UI_FLAG_CLEAR_SEL 주의               |
| 0    | 0x63      | INT_CONFIG0                      | 8    | RW   | 0xC0      | —         | —   | 2      | 허용     | ?    | 상태비트 클리어 정책                            |
| 0    | 0x64      | INT_CONFIG1                      | 8    | RW   | 0x8F      | —         | —   | 2      | 허용     | ?    | 펄스 폭·비동기 리셋                            |
| 0    | 0x65~0x66 | INT_SOURCE0/1                    | 16   | RW   | 0x80/0xB0 | —         | —   | 2      | 허용     | 0x00 | INT1 라우팅 (0x66 POR 확인)                     |
| 0    | 0x68~0x69 | INT_SOURCE3/4                    | 16   | RW   | 0x80/0xB0 | —         | —   | 2      | 허용     | ?    | INT2 라우팅                                     |
| 0    | 0x6C~0x6D | FIFO_LOST_PKT0/1                 | 16   | RO   | —        | —         | ✓    | 1      | 금지     | ?    | 손실 패킷 카운터                                |
| 0    | 0x70      | SELF_TEST_CONFIG                 | 8    | RW   | 0x87      | —         | —   | 3      | 금지     | ?    | 가속도 3축만 (b2:0 예약)                        |
| 0    | 0x75      | WHO_AM_I                         | 8    | RO   | —        | —         | —   | 3      | 금지     | 0x6C | 고정 ID                                         |
| all  | 0x76      | REG_BANK_SEL                     | 8    | RW   | 0xF8      | —         | —   | 3      | 금지     | 0x00 | 뱅크 전환 — 건드리면 전 주소 의미 변경         |
| 1    | 0x03      | SENSOR_CONFIG0                   | 8    | RW   | 0xF8      | —         | —   | 3      | 금지     | ?    | XA/YA/ZA_DISABLE                                |
| 1    | 0x62~0x64 | TMSTVAL0~2                       | 24   | RO   | 0xF0(상)  | —         | ✓    | 1      | 금지     | ?    | TMST_VALUE[19:0]                                |
| 1    | 0x7A~0x7C | INTF_CONFIG4~6                   | 24   | RW   | 상이      | —         | —   | 3      | 금지     | ?    | SPI_AP_4WIRE·PIN9_FUNCTION·I3C                |
| 2    | 0x03~0x05 | ACCEL_CONFIG_STATIC2~4           | 24   | RW   | 상이      | —         | —   | 3      | 금지     | ?    | 가속도 AAF 계수                                 |
| 2    | 0x3B~0x3D | XA/YA/ZA_ST_DATA                 | 24   | RW   | —        | —         | —   | 3      | 금지     | ?    | 자가진단 기준값                                 |
| 3    | 0x06      | PU_PD_CONFIG1                    | 8    | RW   | 상이      | —         | —   | 3      | 금지     | ?    | 핀 풀업/풀다운                                  |
| 3    | 0x0E      | PU_PD_CONFIG2                    | 8    | RW   | 상이      | —         | —   | 3      | 금지     | ?    | 핀 풀업/풀다운                                  |
| 4    | 0x09      | FDR_CONFIG                       | 8    | RW   | 상이      | —         | —   | 3      | 금지     | ?    | FDR_SEL                                         |
| 4    | 0x40~0x47 | APEX_CONFIG1~10                  | 상이 | RW   | 상이      | —         | —   | 3      | 금지     | ?    | APEX·자유낙하 임계값                           |
| 4    | 0x4A~0x51 | ACCEL_WOM_*_THR / INT_SOURCE6~10 | 상이 | RW   | 상이      | —         | —   | 3      | 금지     | ?    | WOM 임계·IBI 라우팅                            |

근거: §13.1~13.5 p.56-58 (맵), §14.1~14.52 p.59-78 (Bank 0 상세), §15~18 p.79-86 (Bank 1/2/3/4).

## 동기화 설계 주의

### 뱅크 — 주소→오프셋 항등이 깨지는 지점

`REG_BANK_SEL`(0x76)은 **모든 뱅크에서 보이며** `BANK_SEL[2:0]`으로 0/1/2/3/4를 고른다(§14.52 p.78). 같은 주소가 뱅크마다 다른 레지스터다 — 0x03은 Bank1 `SENSOR_CONFIG0`·Bank2 `ACCEL_CONFIG_STATIC2`. 단일 128B 이미지는 성립하지 않는다.

권장: 인터페이스 키는 `(bank << 8) | addr`, 내부 오프셋은 `BankBase[bank] + addr`. 실제 이미지는 **Bank 0 하나만**(N=0x77) 두고 Bank 1/2/3/4는 `BankSel → RMW → BankSel(0)` 명시 시퀀스로 처리한다 — Bank 0 밖은 AAF 계수·자가진단·핀 풀업·APEX 임계값처럼 초기화 시 한 번 쓰는 값이라 주기 diff 대상이 아니다.

| 뱅크 | 최대 주소 | 이미지 N | 실사용 | 낭비 | 판정                   |
| ---- | --------- | -------- | ------ | ---- | ---------------------- |
| 0    | 0x76      | 119      | ~48    | ~71  | 이미지 필요            |
| 1    | 0x7C      | 125      | ~8     | ~117 | 1회성 — 이미지 불필요 |
| 2    | 0x3D      | 62       | 6      | 56   | 1회성 — 이미지 불필요 |
| 3    | 0x0E      | 15       | 2      | 13   | 1회성 — 이미지 불필요 |
| 4    | 0x7F      | 128      | ~25    | ~103 | 1회성 — 이미지 불필요 |

0x76 미러 바이트는 이미지에 두되 diff에서 제외한다 — 이 한 바이트가 바뀌면 나머지 모든 오프셋의 의미가 동시에 바뀐다.

### SPI는 Burst Read만 지원 — Write는 Single만

§9.6 항목 7: *"Supports Single or Burst Reads and Single Writes."*

읽기는 주소 자동증가로 연속 가능하지만 **쓰기는 레지스터 하나당 한 트랜잭션**이다. `WriteReg(adr,size)`는 내부에서 반드시 바이트당 개별 전송으로 분해해야 하며, diff 결과로 연속 변경 바이트를 묶어 한 번에 쓰는 최적화는 불가능하다. 현행 `IIM42351_WriteReg`는 주소 1바이트 뒤에 N바이트를 연속 송신해 이미 이 규격을 위반한다(현재 `size=1` 호출뿐이라 잠복 중).

자동증가는 **선택된 뱅크 안에서만** 일어난다. 뱅크를 넘는 버스트는 규정이 없으므로 시도 금지. 이 부품은 자이로 부재로 **0x25~0x2A와 0x4F가 통째로 비어 있고** 그 밖에도 0x00~0x10·0x17~0x1C·0x39~0x4A 등이 공백이다 — 버스트가 통과하더라도 그 바이트는 값이 정의되지 않으므로 이미지 반영·diff 금지. 현행 `GetData`의 8바이트 버스트(0x1D~0x24)는 공백 직전에서 끝나므로 안전하다.

### PWR_MGMT0 타이밍

§14.30 p.68: OFF에서 다른 모드로 전환한 뒤 **200µs 동안 어떤 레지스터 쓰기도 금지**. 자이로가 없어 45ms 유지 규정은 본 부품에 적용되지 않으나(ICM-42688-P 계열 공통 규정), 드라이버를 공유할 경우 분기해야 한다. diff가 변경 바이트를 연속 전송하면 200µs 금지 구간을 침범한다 → 0x4E는 항상 **마지막에 단독으로** 쓰고 홀드한 뒤 다음 주기로 넘어가야 한다.

### 예약 필드 보존 (데이터시트 원문)

§13.5 말미 p.58:

> Register fields marked as Reserved must not be modified by the user. The Reset Value of the register can be used to determine the default value of reserved register fields, and unless otherwise noted this default value must be maintained even if the values of other register fields are modified by the user.

예약비트 기본값이 1인 레지스터(0x51=0x16, 0x52 하위니블=0x1, 0x53=0x0D)에서 통짜 바이트 쓰기는 실제 손상을 낸다. 쓰기 값은 `(image[i] & ~ReservedMask[i]) | (lastRead[i] & ReservedMask[i])`.

### 소프트리셋 후 섀도 무효화

0x11 bit0 소프트리셋은 **전 레지스터를 POR로 되돌린다**. 리셋 실행 시 이미지 전체를 POR 테이블로 재시드하지 않으면 diff 기준이 영구히 어긋난다.

## 현행 드라이버 결함

| #   | 대상                  | 현행                                                                                 | 데이터시트                                                                                         | 근거         |
| --- | --------------------- | ------------------------------------------------------------------------------------ | -------------------------------------------------------------------------------------------------- | ------------ |
| 1   | `INT_STATUS` 비트필드 | b1 예약, b2 FIFO_FULL, b3 FIFO_THS, b4 DATA_RDY, b5 RESET_DONE, b6 예약, b7 UI_FSYNC | b1 FIFO_FULL, b2 FIFO_THS, b3 DATA_RDY, b4 RESET_DONE, b5 예약(PLL_RDY 없음), b6 UI_FSYNC, b7 예약 | §14.15 p.62 |
| 2   | 0x52 이름·마스크     | `GYRO_ACCEL_CONFIG0`, 하위 4비트를 무명 예약으로 두고 통짜 기록                      | 실명 `ACCEL_FILT_CONFIG`, b3:0 예약이며 POR이 `0x1`                                                | §14.33 p.70 |
| 3   | `PWR_MGMT0` 비트필드  | b2~b4를 `:3` 통짜 예약으로 처리                                                      | b3:2 예약, **b4 IDLE(기능 비트)**                                                                  | §14.30 p.68 |
| 4   | `WriteReg`            | 주소 1바이트 + N바이트 연속 송신                                                     | SPI는 Single Write만 지원                                                                          | §9.6-7      |
| 5   | `ACCEL_CONFIG0` POR   | 코드에 POR 상수 없음                                                                 | POR `0x03`(±16g/8kHz)인데 같은 절 본문은 "0110: 1kHz (default)" — 데이터시트 자체 모순           | §14.31 p.69 |

**1의 파급**: `Open` case 6이 검사하는 `.FIFO_THS`(헤더 b3)는 실제로 DATA_RDY를 읽어 우연히 동작한다. `.RESET_DONE`(헤더 b5)은 존재하지 않는 예약 비트를 읽는다. b1 이상 전부 1비트 시프트.

**3의 파급**: 비트 위치 자체는 어긋나지 않으나 IDLE 비트에 접근할 수 없다(기능 누락).

정상 확인: 가속도 감도(±2g 61.035µG), 온도식(`/132.48 + 25`), `ACCEL_CONFIG0`의 `ODR[3:0] + 예약[4] + FS[7:5]` 배치, ODR enum이 0b0001/0b0010을 제외한 점, 8바이트 버스트 범위(0x1D~0x24), `DEVICE_CONFIG` 배치, `WHO_AM_I` 0x6C.

## 미확인 사항

- POR `?` 표기 레지스터 — 데이터시트 §13 레지스터 맵 장과 §14의 일부 절만 발췌해 읽었으므로 리셋값 미확보. 섀도 POR 테이블 확정 전에 §14.2~14.50(p.59-77)에서 개별 확인 필요
- 0x5F~0x70 구간의 주소·이름 — 발췌에서 직접 확인한 것은 0x64(INT_CONFIG1)·0x66(INT_SOURCE1)뿐이며 나머지는 ICM-42688-P 계열 공통 맵을 근거로 기입. 사용 전 §13.1 p.56 하단 확인 필요
- 예약비트 마스크 `?`/`상이` 표기 행 — 비트 배치는 §13 맵에서 읽었으나 예약 위치를 절 단위로 대조하지 않음
- `ACCEL_CONFIG0` POR — 0x03과 "1kHz default" 서술이 충돌한다. 실물 read-back으로 확정할 것
- 버스트 읽기가 뱅크 경계·맵 공백(0x25~0x2A, 0x4F)을 만났을 때의 동작 — 데이터시트에 규정 없음
- 소프트리셋 비트(0x11 bit0)의 하드웨어 자동 클리어 여부 — 데이터시트는 "1ms 대기"만 명시
