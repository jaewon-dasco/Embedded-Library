# ADXL355 (ADXL355BEZ-RL7)

| 항목        | 값                                                                               |
| ----------- | -------------------------------------------------------------------------------- |
| 제조사      | Analog Devices                                                                   |
| 인터페이스  | SPI / I2C (현행 드라이버는 I2C)                                                  |
| 주소 폭     | 8bit (0x00~0x2F, 별도 0x50~0x54)                                                 |
| 레지스터 폭 | 전 레지스터 1바이트                                                              |
| 데이터시트  | ADXL354/ADXL355 Rev. D (Table 15 p.35 / Table 16~51 p.36~43)                     |
| PDF         | `D:\0_Document\0_데이터시트\1_IC\MEMS\Datasheet_AnalogDevice_ADXL355BEZ-RL7.pdf` |

## 레지스터 맵

| 주소      | 이름             | 폭  | 접근     | 예약비트     | 자기클리어 | 휘발 | 동기화 | AutoSync | POR    | 비고                                                                          |
| --------- | ---------------- | --- | -------- | ------------ | ---------- | ---- | ------ | -------- | ------ | ----------------------------------------------------------------------------- |
| 0x00      | DEVID_AD         | 1   | RO       | —           | —         | —   | 3      | 금지     | 0xAD   | RO                                                                            |
| 0x01      | DEVID_MST        | 1   | RO       | —           | —         | —   | 3      | 금지     | 0x1D   | RO                                                                            |
| 0x02      | PARTID           | 1   | RO       | —           | —         | —   | 3      | 금지     | 0xED   | RO (355 octal)                                                                |
| 0x03      | REVID            | 1   | RO       | —           | —         | —   | 3      | 금지     | 0x01   | RO                                                                            |
| 0x04      | Status           | 1   | R/C      | 0xE0         | 0x1F       | ✓    | 1      | 금지     | 0x00   | 읽으면 전 비트 clear — 일괄 read 가 인터럽트를 대신 ACK 한다                 |
| 0x05      | FIFO_ENTRIES     | 1   | RO       | 0x80         | —         | ✓    | 1      | 금지     | 0x00   | 0~96                                                                          |
| 0x06      | TEMP2            | 1   | RO       | 0xF0         | —         | ✓    | 1      | 금지     | 0x00   | 12bit 온도 상위 4bit                                                          |
| 0x07      | TEMP1            | 1   | RO       | —           | —         | ✓    | 1      | 금지     | 0x00   | 이중버퍼 아님 — 두 바이트 사이에 갱신될 수 있다                              |
| 0x08~0x0A | XDATA3~XDATA1    | 3   | RO       | 0x0A 의 0x0F | —         | ✓    | 1      | 금지     | 0x00   | 20bit 좌측정렬 2's comp, MSB-first                                            |
| 0x0B~0x0D | YDATA3~YDATA1    | 3   | RO       | 0x0D 의 0x0F | —         | ✓    | 1      | 금지     | 0x00   | 동일                                                                          |
| 0x0E~0x10 | ZDATA3~ZDATA1    | 3   | RO       | 0x10 의 0x0F | —         | ✓    | 1      | 금지     | 0x00   | 동일                                                                          |
| 0x11      | FIFO_DATA        | 1   | RO       | —           | —         | ✓    | 1      | 금지     | 0x00   | 읽으면 FIFO pop (파괴적) — 이미지 일괄 read 범위 제외                        |
| 0x12~0x1D | (미구현)         | 12  | RESERVED | 전체         | —         | —   | —     | 금지     | —     | 주소 공백 — 이미지 낭비 12B                                                  |
| 0x1E~0x1F | OFFSET_X_H/L     | 2   | RW       | —           | —         | —   | 2      | 금지     | 0x0000 | standby 에서만 쓰기 가능                                                      |
| 0x20~0x21 | OFFSET_Y_H/L     | 2   | RW       | —           | —         | —   | 2      | 금지     | 0x0000 | standby 에서만 쓰기 가능                                                      |
| 0x22~0x23 | OFFSET_Z_H/L     | 2   | RW       | —           | —         | —   | 2      | 금지     | 0x0000 | standby 에서만 쓰기 가능                                                      |
| 0x24      | ACT_EN           | 1   | RW       | 0xF8         | —         | —   | 2      | 금지     | 0x00   | standby 에서만 쓰기 가능                                                      |
| 0x25~0x26 | ACT_THRESH_H/L   | 2   | RW       | —           | —         | —   | 2      | 금지     | 0x0000 | standby 에서만 쓰기 가능                                                      |
| 0x27      | ACT_COUNT        | 1   | RW       | —           | —         | —   | 2      | 금지     | 0x01   | standby 에서만 쓰기 가능                                                      |
| 0x28      | Filter           | 1   | RW       | 0x80         | —         | —   | 2      | 부분허용 | 0x00   | HPF_CORNER(0x70) 만 measurement 중 변경 가능, ODR_LPF(0x0F) 는 standby 에서만 |
| 0x29      | FIFO_SAMPLES     | 1   | RW       | 0x80         | —         | —   | 2      | 금지     | 0x60   | standby 에서만 쓰기 가능                                                      |
| 0x2A      | INT_MAP          | 1   | RW       | —           | —         | —   | 2      | 허용     | 0x00   | measurement 중 변경 가능                                                      |
| 0x2B      | Sync             | 1   | RW       | 0xF8         | —         | —   | 2      | 금지     | 0x00   | standby 에서만 쓰기 가능                                                      |
| 0x2C      | Range            | 1   | RW       | 0x3C         | —         | —   | 2      | 금지     | 0x81   | standby 에서만 쓰기 가능                                                      |
| 0x2D      | POWER_CTL        | 1   | RW       | 0xF8         | —         | —   | 2      | 금지     | 0x01   | Standby 비트 자체 — 모드 전이는 명시 시퀀스로만                              |
| 0x2E      | SELF_TEST        | 1   | RW       | 0xFC         | —         | —   | 3      | 금지     | 0x00   | 쓰면 자가진단 구동. ST1/ST2 는 measurement 중 변경 가능                       |
| 0x2F      | Reset            | 1   | WO       | —           | —         | —   | 3      | 금지     | 0x00   | 0x52 쓰면 리셋 — 섀도가 기억하면 매 Sync 마다 재발행                         |
| 0x50~0x54 | shadow registers | 5   | RO       | 미확인       | —         | —   | 3      | 금지     | 미확인 | SW reset 후 NVM 오적재 검증용 (REVID ≤ 0x01), p.43                           |

## 동기화 설계 주의

**주소 공백 0x12~0x1D**: 0x11 다음 유효 주소는 0x1E 다. 이미지를 `uint8_t[0x30]` 로 잡아 주소=오프셋 항등을 유지하면 12바이트가 낭비되고, 낭비를 없애려 공백을 접으면 항등이 깨진다 — 항등 유지 쪽이 맞다. 12바이트는 이 IC 에서 지불할 만한 비용이다.

**measurement mode 쓰기 제한** (Table 15 상단 주의, p.35): measurement mode(POWER_CTL.Standby=0) 에서 변경 가능한 것은 Filter 의 HPF_CORNER, INT_MAP, SELF_TEST 의 ST1/ST2, Reset 뿐이다. 나머지 설정은 반드시 measurement 진입 **전**에 프로그램해야 한다. AutoSync 가 이 구분 없이 2 를 밀어넣으면 무효 쓰기가 되므로, 서술자에 `standbyOnly` 플래그가 필요하다.

**FIFO_DATA(0x11) 파괴적 읽기**: 읽는 순간 FIFO 에서 샘플이 빠진다. 이미지를 채우는 일괄 read 가 이 주소를 스치면 FIFO 가 통째로 소진된다. 명시 접근자 전용으로 격리한다.

**Status(0x04) clear-on-read**: DATA_RDY·FIFO_FULL·FIFO_OVR·Activity·NVM_BUSY 가 읽기로 클리어된다(p.29 "the status register clears when read"). 주기적 전체 이미지 read 는 상위가 플래그를 보기 전에 소진시킨다 — diff 기준이 흔들리는 정도가 아니라 기능 손실이다.

**Reset(0x2F) 은 WO**: 읽으면 0x00 이므로 read-back 으로 섀도를 seed 할 수 없다. diff 대상에서 영구 제외하지 않으면 0x52 가 남아 매 Sync 마다 리셋이 재발행된다.

## 현행 드라이버 결함

| 항목                      | 내용                                                                                                                                                                                    | 근거                        |
| ------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------- |
| 주소 공백 누락            | `MEMS_ADXL355BEZRL7.h` 의 `Register` 구조체가 `FIFO_DATA`(0x11) 다음 곧바로 `Parameter.XOFFSET`(0x1E) 로 이어진다 — 0x12~0x1D 12바이트가 없어 0x1E 이후 전 오프셋이 주소와 12 어긋난다 | Table 15 p.35               |
| 비트필드로 다바이트 표현  | `uint16_t TEMP:16; uint32_t XDATA:24;` — packed GCC 에서 11바이트로 붙지만 구현정의이며 바이트 주소지정이 불가능하다. 섀도 이미지에서는 `uint8_t[]` 여야 한다                          | —                          |
| 온도 절편 상수            | `ADXL355_TEMP_TO_Degree` 의 intercept = **1852**, 데이터시트는 nominal **1885 LSB @25℃** (slope −9.05 는 일치). 개체 보정 의도가 아니면 오차 ≈3.6℃                                   | p.37 TEMP 레지스터 절       |
| 자기클리어·부작용 미표기 | Status clear-on-read, FIFO_DATA pop, Reset WO 에 대한 표기가 헤더에 없다                                                                                                                | p.29 / p.39 / Table 51 p.43 |

Status·Filter·Range·POWER_CTL 의 비트 배치는 데이터시트와 일치한다(LSB-first 할당 기준).

## 미확인 사항

- 0x50~0x54 shadow register 의 개별 비트 정의·POR 값은 데이터시트 레지스터 맵에 없다(복구 절차만 서술).
- 0x2E SELF_TEST 를 AutoSync 2 로 볼지 3 으로 볼지는 상위 사용 방식에 달렸다 — 본 문서는 3 으로 분류했다.
