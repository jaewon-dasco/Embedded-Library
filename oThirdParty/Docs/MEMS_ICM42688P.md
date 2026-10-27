# ICM-42688-P

| 항목       | 값                                                                |
| ---------- | ----------------------------------------------------------------- |
| 제조사     | TDK InvenSense                                                    |
| 구성       | 3축 가속도 + 3축 자이로 (6-Axis IMU), 16bit 출력                  |
| 인터페이스 | SPI 4-wire/3-wire (max 24MHz) · I2C · I3C                       |
| 주소폭     | 7bit (SPI 1바이트 헤더 = R/W[7] + A[6:0]), 뱅크 0~4               |
| WHO_AM_I   | `0x47` (0x75)                                                     |
| 데이터시트 | DS-000347 Rev 1.6                                                 |
| PDF        | `D:\0_Document\0_데이터시트\1_IC\MEMS\Datasheet_TDK_ICM42688.pdf` |
| 드라이버   | `oThirdParty/{Inc,Src}/MEMS_ICM42688P.{h,c}`                      |

동기화 열 표기: `1` 매 주기 부품→호스트 · `2` 호스트→부품 설정값 · `3` 1회성.

## 레지스터 맵

| 뱅크 | 주소      | 이름                   | 폭   | 접근 | 예약비트  | 자기클리어 | 휘발 | 동기화 | AutoSync | POR  | 비고                                            |
| ---- | --------- | ---------------------- | ---- | ---- | --------- | ---------- | ---- | ------ | -------- | ---- | ----------------------------------------------- |
| 0    | 0x11      | DEVICE_CONFIG          | 8    | RW   | 0xEE      | 0x01       | —   | 2/3    | 부분허용 | 0x00 | bit0 소프트리셋은 1회성 — diff 마스크 제외     |
| 0    | 0x13      | DRIVE_CONFIG           | 8    | RW   | 0xC0      | —         | —   | 2      | 허용     | 0x05 | I2C/SPI slew rate                               |
| 0    | 0x14      | INT_CONFIG             | 8    | RW   | 0xC0      | —         | —   | 2      | 허용     | 0x00 | INT1/INT2 극성·구동·모드                      |
| 0    | 0x16      | FIFO_CONFIG            | 8    | RW   | 0x3F      | —         | —   | 2      | 허용     | 0x00 | FIFO_MODE[7:6]                                  |
| 0    | 0x1D~0x1E | TEMP_DATA1/0           | 16   | RO   | —        | —         | ✓    | 1      | 금지     | 0x80 | SYNCR — 부품이 갱신                            |
| 0    | 0x1F~0x24 | ACCEL_DATA_X/Y/Z       | 48   | RO   | —        | —         | ✓    | 1      | 금지     | 0x80 | SYNCR                                           |
| 0    | 0x25~0x2A | GYRO_DATA_X/Y/Z        | 48   | RO   | —        | —         | ✓    | 1      | 금지     | 0x80 | SYNCR                                           |
| 0    | 0x2B~0x2C | TMST_FSYNCH/L          | 16   | RO   | —        | —         | ✓    | 1      | 금지     | ?    | FSYNC 타임스탬프                                |
| 0    | 0x2D      | INT_STATUS             | 8    | R/C  | 0x80      | 0x7F       | ✓    | 1      | 금지     | 0x10 | 읽으면 클리어                                   |
| 0    | 0x2E      | FIFO_COUNTH            | 8    | RO   | —        | —         | ✓    | 1      | 금지     | 0x00 | 읽으면 H/L 동시 latch                           |
| 0    | 0x2F      | FIFO_COUNTL            | 8    | RO   | —        | —         | ✓    | 1      | 금지     | 0x00 | H를 먼저 읽어야 갱신                            |
| 0    | 0x30      | FIFO_DATA              | 8    | RO   | —        | —         | ✓    | 1      | 금지     | 0xFF | FIFO pop — 반복 읽기마다 다른 값               |
| 0    | 0x31~0x36 | APEX_DATA0~5           | 48   | RO   | —        | —         | ✓    | 1      | 금지     | ?    | 보수계·탭 결과                                 |
| 0    | 0x37      | INT_STATUS2            | 8    | R/C  | 0xF0      | 0x0F       | ✓    | 1      | 금지     | ?    | 읽으면 클리어 (SMD/WOM)                         |
| 0    | 0x38      | INT_STATUS3            | 8    | R/C  | 0xC0      | 0x3F       | ✓    | 1      | 금지     | ?    | 읽으면 클리어 (APEX)                            |
| 0    | 0x4B      | SIGNAL_PATH_RESET      | 8    | W/C  | 0x91      | 0x0E       | —   | 3      | 금지     | 0x00 | 자기클리어 커맨드 — 섀도가 1을 기억하면 재발행 |
| 0    | 0x4C      | INTF_CONFIG0           | 8    | RW   | 0x06      | —         | —   | 2      | 허용     | ?    | 엔디안·UI_SIFS_CFG                             |
| 0    | 0x4D      | INTF_CONFIG1           | 8    | RW   | 0xF0      | —         | —   | 2      | 허용     | 0x91 | CLKSEL·RTC_MODE                                |
| 0    | 0x4E      | PWR_MGMT0              | 8    | RW   | 0xC0      | —         | —   | 2      | 조건부   | 0x00 | 단독·최후 기록 + 200µs 홀드                    |
| 0    | 0x4F      | GYRO_CONFIG0           | 8    | RW   | 0x10      | —         | —   | 2      | 허용     | 0x06 | FS[7:5] + ODR[3:0]                              |
| 0    | 0x50      | ACCEL_CONFIG0          | 8    | RW   | 0x10      | —         | —   | 2      | 허용     | 0x06 | FS[7:5] + ODR[3:0]                              |
| 0    | 0x51      | GYRO_CONFIG1           | 8    | RW   | 0x10      | —         | —   | 2      | 허용     | 0x16 | 예약 기본값 1 포함 — RMW 필수                  |
| 0    | 0x52      | GYRO_ACCEL_CONFIG0     | 8    | RW   | —        | —         | —   | 2      | 허용     | 0x11 | ACCEL_BW[7:4] + GYRO_BW[3:0]                    |
| 0    | 0x53      | ACCEL_CONFIG1          | 8    | RW   | 0xE1      | —         | —   | 2      | 허용     | 0x0D | 예약 기본값 1 포함 — RMW 필수                  |
| 0    | 0x54      | TMST_CONFIG            | 8    | RW   | 0xE0      | —         | —   | 2      | 허용     | ?    | 타임스탬프 enable                               |
| 0    | 0x56      | APEX_CONFIG0           | 8    | RW   | 0x08      | —         | —   | 2      | 허용     | ?    | DMP_ODR·보수계 enable                          |
| 0    | 0x57      | SMD_CONFIG             | 8    | RW   | 0xE0      | —         | —   | 2      | 허용     | ?    | WOM/SMD 모드                                    |
| 0    | 0x5F      | FIFO_CONFIG1           | 8    | RW   | 0x80      | —         | —   | 2      | 허용     | ?    | FIFO 항목 선택                                  |
| 0    | 0x60~0x61 | FIFO_CONFIG2/3         | 16   | RW   | 0xF0(상)  | —         | —   | 2      | 허용     | ?    | FIFO_WM[11:0]                                   |
| 0    | 0x62      | FSYNC_CONFIG           | 8    | RW   | 0x8C      | 0x02       | —   | 2      | 조건부   | ?    | bit1 FSYNC_UI_FLAG_CLEAR_SEL 주의               |
| 0    | 0x63      | INT_CONFIG0            | 8    | RW   | 0xC0      | —         | —   | 2      | 허용     | ?    | 상태비트 클리어 정책                            |
| 0    | 0x64      | INT_CONFIG1            | 8    | RW   | 0x8F      | —         | —   | 2      | 허용     | ?    | 펄스 폭·비동기 리셋                            |
| 0    | 0x65~0x66 | INT_SOURCE0/1          | 16   | RW   | 0x80/0xB0 | —         | —   | 2      | 허용     | ?    | INT1 라우팅                                     |
| 0    | 0x68~0x69 | INT_SOURCE3/4          | 16   | RW   | 0x80/0xB0 | —         | —   | 2      | 허용     | ?    | INT2 라우팅                                     |
| 0    | 0x6C~0x6D | FIFO_LOST_PKT0/1       | 16   | RO   | —        | —         | ✓    | 1      | 금지     | ?    | 손실 패킷 카운터                                |
| 0    | 0x70      | SELF_TEST_CONFIG       | 8    | RW   | 0x80      | —         | —   | 3      | 금지     | ?    | 자가진단 전용                                   |
| 0    | 0x75      | WHO_AM_I               | 8    | RO   | —        | —         | —   | 3      | 금지     | 0x47 | 고정 ID                                         |
| all  | 0x76      | REG_BANK_SEL           | 8    | RW   | 0xF8      | —         | —   | 3      | 금지     | 0x00 | 뱅크 전환 — 건드리면 전 주소 의미 변경         |
| 1    | 0x03      | SENSOR_CONFIG0         | 8    | RW   | 0xC0      | —         | —   | 3      | 금지     | ?    | 축 disable                                      |
| 1    | 0x0B~0x11 | GYRO_CONFIG_STATIC2~10 | 72   | RW   | 상이      | —         | —   | 3      | 금지     | ?    | 자이로 AAF·노치 계수                           |
| 1    | 0x5F~0x61 | XG/YG/ZG_ST_DATA       | 24   | RW   | —        | —         | —   | 3      | 금지     | ?    | 자가진단 기준값                                 |
| 1    | 0x62~0x64 | TMSTVAL0~2             | 24   | RO   | 0xF0(상)  | —         | ✓    | 1      | 금지     | ?    | TMST_VALUE[19:0]                                |
| 1    | 0x7A~0x7C | INTF_CONFIG4~6         | 24   | RW   | 상이      | —         | —   | 3      | 금지     | ?    | SPI_AP_4WIRE·I3C                               |
| 2    | 0x03~0x05 | ACCEL_CONFIG_STATIC2~4 | 24   | RW   | 상이      | —         | —   | 3      | 금지     | ?    | 가속도 AAF 계수                                 |
| 2    | 0x3B~0x3D | XA/YA/ZA_ST_DATA       | 24   | RW   | —        | —         | —   | 3      | 금지     | ?    | 자가진단 기준값                                 |
| 4    | 0x40~0x4D | APEX_CONFIG1~9         | 상이 | RW   | 상이      | —         | —   | 3      | 금지     | ?    | APEX 임계값                                     |
| 4    | 0x4E~0x51 | INT_SOURCE6~10         | 상이 | RW   | 상이      | —         | —   | 3      | 금지     | ?    | APEX/I3C IBI 라우팅                             |
| 4    | 0x77~0x7F | OFFSET_USER0~8         | 72   | RW   | —        | —         | —   | 3      | 금지     | ?    | 가속도·자이로 오프셋                           |

근거: §13.1~13.4 p.61-63 (맵), §14.1~14.59 p.64-88 (Bank 0 상세), §15~17 p.89-98 (Bank 1/2/4).

## 동기화 설계 주의

### 뱅크 — 주소→오프셋 항등이 깨지는 지점

`REG_BANK_SEL`(0x76)은 **모든 뱅크에서 보이며** `BANK_SEL[2:0]`으로 0/1/2/3/4를 고른다(§14.59 p.88). 같은 주소가 뱅크마다 다른 레지스터다 — 0x03은 Bank1 `SENSOR_CONFIG0`·Bank2 `ACCEL_CONFIG_STATIC2`, 0x4B은 Bank0 `SIGNAL_PATH_RESET`·Bank4 `INT_SOURCE6`. 단일 128B 이미지는 성립하지 않는다.

권장: 인터페이스 키는 `(bank << 8) | addr`, 내부 오프셋은 `BankBase[bank] + addr`. 실제 이미지는 **Bank 0 하나만**(N=0x77) 두고 Bank 1/2/4는 `BankSel → RMW → BankSel(0)` 명시 시퀀스로 처리한다 — Bank 0 밖은 전부 AAF 계수·자가진단·오프셋·APEX 임계값처럼 초기화 시 한 번 쓰는 값이라 주기 diff 대상이 아니다.

| 뱅크 | 최대 주소 | 이미지 N | 실사용 | 낭비 | 판정                   |
| ---- | --------- | -------- | ------ | ---- | ---------------------- |
| 0    | 0x76      | 119      | ~57    | ~62  | 이미지 필요            |
| 1    | 0x7C      | 125      | 20     | 105  | 1회성 — 이미지 불필요 |
| 2    | 0x3D      | 62       | 6      | 56   | 1회성 — 이미지 불필요 |
| 4    | 0x7F      | 128      | ~30    | ~98  | 1회성 — 이미지 불필요 |

0x76 미러 바이트는 이미지에 두되 diff에서 제외한다 — 이 한 바이트가 바뀌면 나머지 모든 오프셋의 의미가 동시에 바뀐다.

### SPI는 Burst Read만 지원 — Write는 Single만

§9.6 항목 7 (p.53): *"Supports Single or Burst Reads and Single Writes."*

읽기는 주소 자동증가로 연속 가능하지만 **쓰기는 레지스터 하나당 한 트랜잭션**이다. `WriteReg(adr,size)`는 내부에서 반드시 바이트당 개별 전송으로 분해해야 하며, diff 결과로 연속 변경 바이트를 묶어 한 번에 쓰는 최적화는 불가능하다. 현행 `ICM42688P_WriteReg`는 주소 1바이트 뒤에 N바이트를 연속 송신해 이미 이 규격을 위반한다(현재 `size=1` 호출뿐이라 잠복 중).

자동증가는 **선택된 뱅크 안에서만** 일어난다. 뱅크를 넘는 버스트는 규정이 없으므로 시도 금지. 맵 공백 구간(0x00~0x10, 0x17~0x1C, 0x39~0x4A, 0x58~0x5E 등)은 값이 정의되지 않으므로 버스트가 통과하더라도 이미지 반영·diff 금지.

### PWR_MGMT0 타이밍

§14.36 p.76: OFF에서 다른 모드로 전환한 뒤 **200µs 동안 어떤 레지스터 쓰기도 금지**, 자이로는 켠 뒤 **최소 45ms 유지**. diff가 변경 바이트를 연속 전송하면 이 금지 구간을 침범한다 → 0x4E는 항상 **마지막에 단독으로** 쓰고 200µs 홀드한 뒤 다음 주기로 넘어가야 한다.

### 예약 필드 보존 (데이터시트 원문)

§13.4 말미 p.63:

> Register fields marked as Reserved must not be modified by the user. The Reset Value of the register can be used to determine the default value of reserved register fields, and unless otherwise noted this default value must be maintained even if the values of other register fields are modified by the user.

예약비트 기본값이 1인 레지스터(0x51=0x16, 0x53=0x0D, 0x4D=0x91)에서 통짜 바이트 쓰기는 실제 손상을 낸다. 쓰기 값은 `(image[i] & ~ReservedMask[i]) | (lastRead[i] & ReservedMask[i])`.

### 소프트리셋 후 섀도 무효화

0x11 bit0 소프트리셋은 **전 레지스터를 POR로 되돌린다**. 리셋 실행 시 이미지 전체를 POR 테이블로 재시드하지 않으면 diff 기준이 영구히 어긋난다.

## 현행 드라이버 결함

| #   | 대상                       | 현행                                                                                 | 데이터시트                                                                              | 근거         |
| --- | -------------------------- | ------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------- | ------------ |
| 1   | `INT_STATUS` 비트필드      | b1 예약, b2 FIFO_FULL, b3 FIFO_THS, b4 DATA_RDY, b5 RESET_DONE, b6 예약, b7 UI_FSYNC | b1 FIFO_FULL, b2 FIFO_THS, b3 DATA_RDY, b4 RESET_DONE, b5 PLL_RDY, b6 UI_FSYNC, b7 예약 | §14.21 p.69 |
| 2   | `INT_CONFIG` 비트필드      | b0:1 예약, b2 INT1_DRIVE, b3 INT1_POLARITY, b4 INT1_MODE, b5~b7 INT2_*               | b0 INT1_POLARITY, b1 INT1_DRIVE_CIRCUIT, b2 INT1_MODE, b3~b5 INT2_*, b7:6 예약          | §14.3 p.65  |
| 3   | `ICM42688V_WHO_AM_I_VALUE` | `0x6C`을 ICM-42688 변종 ID로 기재                                                    | DS-000347에 "ICM-42688-V" 없음 — 0x6C는 IIM-42351의 WHOAMI                             | §14.58 p.88 |
| 4   | `WriteReg`                 | 주소 1바이트 + N바이트 연속 송신                                                     | SPI는 Single Write만 지원                                                               | §9.6-7 p.53 |
| 5   | `SetConfig`                | 0x52 등 설정 레지스터를 통짜 바이트로 기록                                           | 예약 필드 리셋값 보존 의무                                                              | p.63         |

**1의 파급**: `Open` case 6이 검사하는 `.FIFO_THS`(헤더 b3)는 실제로 DATA_RDY를 읽어 우연히 동작한다. `.RESET_DONE`(헤더 b5)은 실제 PLL_RDY를 읽는다. b1 이상 전부 1비트 시프트.

정상 확인: 가속도 감도(±2g 61.035µG), 자이로 감도(±250dps 7.6294mdps), 온도식(`/132.48 + 25`), `ACCEL_CONFIG0`/`GYRO_CONFIG0`의 `ODR[3:0] + 예약[4] + FS[7:5]` 배치, `PWR_MGMT0` 배치, 14바이트 버스트 범위(0x1D~0x2A), `DEVICE_CONFIG` 배치.

## 미확인 사항

- POR `?` 표기 레지스터 — 데이터시트 §13 레지스터 맵 장과 §14의 일부 절만 발췌해 읽었으므로 리셋값 미확보. 섀도 POR 테이블 확정 전에 §14.19~14.57(p.68-88)에서 개별 확인 필요
- 예약비트 마스크 `?`/`상이` 표기 행 — 비트 배치는 §13 맵에서 읽었으나 예약 위치를 절 단위로 대조하지 않음
- Bank 3 — DS-000347 §13은 "banks 0, 1, 2, 3, 4"라고 쓰면서 Bank 3 맵 표를 싣지 않았다. 실재 여부 미확인
- 버스트 읽기가 뱅크 경계·맵 공백을 만났을 때의 동작 — 데이터시트에 규정 없음
- 소프트리셋 비트(0x11 bit0)의 하드웨어 자동 클리어 여부 — 데이터시트는 "1ms 대기" 만 명시하고 자동 클리어를 언급하지 않는다. 현행 드라이버는 명시적으로 0을 다시 쓴다
