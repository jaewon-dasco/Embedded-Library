# NAU7802

| 항목        | 값                                                                  |
| ----------- | ------------------------------------------------------------------- |
| 제조사      | Nuvoton                                                             |
| 인터페이스  | I2C (slave 7bit 0x2A, HAL 8bit 0x54)                                |
| 주소 폭     | 8bit (0x00~0x1C, 0x1F)                                              |
| 레지스터 폭 | 전 레지스터 1바이트                                                 |
| 데이터시트  | NAU7802 24-bit ADC Rev. 1.7 — §10 요약맵 p.28, §11 상세 p.29~38  |
| PDF         | `D:\0_Document\0_데이터시트\1_IC\ADC\Datasheet_NUVOTON_NAU7802.pdf` |

## 레지스터 맵

| 주소      | 이름                 | 폭  | 접근           | 예약비트                 | 자기클리어      | 휘발   | 동기화   | AutoSync | POR        | 비고                                                                                        |
| --------- | -------------------- | --- | -------------- | ------------------------ | --------------- | ------ | -------- | -------- | ---------- | ------------------------------------------------------------------------------------------- |
| 0x00      | PU_CTRL              | 1   | RW             | 0x28 RO (CR b5, PUR b3)  | —              | ✓      | 123 혼재 | 부분허용 | 0x00       | **바이트 diff 불가** — CS(0x10) 쓰면 변환 재트리거, RR(0x01) 레벨 리셋, CR·PUR 은 RO      |
| 0x01      | CTRL1                | 1   | RW             | —                       | —              | —     | 2        | 허용     | 0x00       | CRP b7 · DRDY_SEL b6 · VLDO[5:3] · GAINS[2:0]                                            |
| 0x02      | CTRL2                | 1   | RW             | 0x08 RO (CAL_ERR b3)     | **0x04 (CALS)** | ✓      | 2        | 부분허용 | 0x00       | CALS 는 diff 금지 — 쓰면 교정 재트리거, 완료 시 자동 0                                     |
| 0x03~0x05 | OCAL1_B2~B0          | 3   | RW             | —                       | —              | —     | 2        | 허용     | 0x000000   | CH1 offset, 24bit MSB-first                                                                 |
| 0x06~0x09 | GCAL1_B3~B0          | 4   | RW             | —                       | —              | —     | 2        | 허용     | 0x00800000 | CH1 gain, 32bit MSB-first                                                                   |
| 0x0A~0x0C | OCAL2_B2~B0          | 3   | RW             | —                       | —              | —     | 2        | 허용     | 0x000000   | CH2 offset (NAU7802 전용)                                                                   |
| 0x0D~0x10 | GCAL2_B3~B0          | 4   | RW             | —                       | —              | —     | 2        | 허용     | 0x00800000 | CH2 gain (NAU7802 전용)                                                                     |
| 0x11      | I2C Control          | 1   | RW             | —                       | —              | —     | 2        | 허용     | 0x00       | CRSD b7 · FDR/FRD b6 · SPE b5 · WPD b4 · SI b3 · BOPGA b2 · TS b1 · BGPCP b0         |
| 0x12~0x14 | ADCO_B2~B0           | 3   | RO             | —                       | —              | ✓      | 1        | 금지     | —         | **0x12 읽기가 24bit 전체를 latch** — 반드시 0x12 부터 3바이트 burst                        |
| 0x15      | ADC registers        | 1   | RW (읽기 별칭) | b7·b6 미문서            | —              | ✓      | 3        | 금지     | 미확인     | 읽기 값이 0x1B[7] RD_OTP_SEL 에 따라 ADC regs 또는 OTP[31:24] — read-back diff 불가        |
| 0x16      | OTP[23:16]           | 1   | RO             | —                       | —              | —     | 3        | 금지     | —         | 0x17 은 OTP[15:8] (§10 표에 미기재)                                                        |
| 0x17      | OTP[15:8]            | 1   | RO             | —                       | —              | —     | 3        | 금지     | —         | §11.10 표에만 등장                                                                         |
| 0x18~0x1A | (미문서)             | 3   | RO             | 미확인                   | —              | 미확인 | —       | 금지     | 미확인     | §11.11~11.13 "Read Only" 만 기재                                                           |
| 0x1B      | PGA                  | 1   | RW             | 0x06 (b2·b1 Reserved=0) | —              | —     | 2        | 허용     | 0x00       | RD_OTP_SEL b7 · LDOMODE b6 · PGA out buf b5 · PGA bypass b4 · PGAINV b3 · PGACHPDIS b0 |
| 0x1C      | POWER                | 1   | RW             | —                       | —              | —     | 2        | 허용     | 0x00       | PGA_CAP_EN b7 · MASTER_BIAS[6:4] · ADC_CURR[3:2] · PGA_CURR[1:0]                         |
| 0x1D~0x1E | (없음)               | 2   | RESERVED       | 전체                     | —              | —     | —       | 금지     | —         | 주소 공백                                                                                   |
| 0x1F      | Device Revision Code | 1   | RO             | —                       | —              | —     | 3        | 금지     | 미확인     | 헤더는 `CHIP_ID` 로 명명                                                                    |

PU_CTRL 비트(§11.1, p.29): AVDDS b7 · OSCS b6 · **CR b5 (RO)** · **CS b4 (액션)** · **PUR b3 (RO)** · PUA b2 · PUD b1 · **RR b0 (레벨 트리거)**.

## 동기화 설계 주의

**PU_CTRL 한 바이트에 123 이 섞이고 RO 비트가 그 안에 있다.** CR(변환 완료)·PUR(파워업 완료)은 부품이 쓰는 RO 상태이고, CS 는 쓰면 변환이 시작되는 액션, RR 은 리셋 레벨, PUA·PUD 는 순수 설정이다. **바이트 단위 diff 는 성립하지 않는다** — CR=1 을 읽어 섀도에 담은 뒤 그 바이트를 되쓰면 CS·RR 까지 함께 재기록되어 변환이 임의 시점에 재트리거된다. diff 는 반드시 **비트 마스크 단위**여야 하고, 마스크는 `writeMask & ~actionMask & ~roMask` = `0xC6 & ~0x11 & ~0x28` 로 좁혀진다.

**0x15 읽기 별칭**(§11.10, p.34): 같은 주소가 `0x1B[7] RD_OTP_SEL` 값에 따라 ADC 레지스터 또는 OTP[31:24] 를 돌려준다. 이 바이트를 read-back 으로 diff 기준 삼으면 섀도가 OTP 값으로 오염된다. 이미지 일괄 read 범위에서 제외하고 write-only 취급한다.

**0x12 latching read 순서 제약**(§11.9, p.33): 0x12 를 읽는 순간 24bit 결과 전체가 latch 되고 0x13·0x14 는 그 latch 를 흘려보낸다. 읽기를 0x13 에서 시작하거나 순서를 섞으면 서로 다른 변환의 바이트가 섞인다. 임의 범위 `ReadReg(adr, size)` 로 덮으면 안 되는 구간이다. 또한 CR=1 확인 전에 0x12 를 읽으면 직전 변환 결과가 나온다.

**CALS(0x02 b2) 자기클리어**: "Action register bit. When calibration is finished, it will reset to 0"(§11.3, p.30). 섀도가 1을 기억하면 무한 재교정에 빠진다. 교정 중 쓰기는 무시되므로 실패도 조용하다.

**이미지 크기**: N = 0x20 = 32B. 0x1D·0x1E 2바이트 공백 + 0x18~0x1A 미문서 3바이트가 낭비 구간이다. OCAL(3B)·GCAL(4B) 은 연속 주소에 MSB-first 로 놓이므로 연속 이미지 + swap 접근자로 자연스럽게 처리된다.

## 현행 드라이버 결함

| 항목                             | 내용                                                                                                                     | 근거         |
| -------------------------------- | ------------------------------------------------------------------------------------------------------------------------ | ------------ |
| CAL_ERR 를 일반 define 으로 취급 | `NAU7802_CTRL2_CAL_ERR 0b00001000` 이 다른 설정 비트와 같은 층에 있다 — **RO** 라 쓰기 대상이 아니므로 마스크 분리 필요 | §11.3 p.30  |
| 0x1B 예약비트 정의 없음          | b2·b1 Reserved(default 0) 에 대한 마스크가 없어 RMW 시 보호되지 않는다                                                  | §11.14 p.36 |
| 0x15 비트 define 부재            | `NAU7802_REG_ADC_CTRL 0x15` 만 있고 REG_CHPS[5:4]·ADC_VCM[3:2]·REG_CHP[1:0] 정의가 없다. 읽기 별칭 경고도 없다         | §11.10 p.34 |
| 0x1F 명명                        | 헤더는 `CHIP_ID`, 데이터시트는 "Device Revision Code" (RO)                                                               | §10 p.28    |
| 0x11 b6 비트명                   | 헤더에 define 없음. 데이터시트 자체가 §10 표 "FDR" 과 §11.8 "FRD" 로 불일치                                            | p.28 / p.32  |

## 미확인 사항

- 0x15 의 b7·b6 는 §11.10 에 정의가 없다(§11.8 이 "REG0x15 bit7 must be also set to 1" 로만 언급).
- 0x18~0x1A 는 "Read Only" 표기만 있고 내용 정의가 없다.
- 0x15·0x1F 의 POR 값이 데이터시트에 명시돼 있지 않다.
