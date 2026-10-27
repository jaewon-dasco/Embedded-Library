/*
 * NAND_MT29F2G01ABAGDWB_IT.h
 *
 *  Created on: Feb 25, 2025
 *      Author: JONE
 */

#ifndef NAND_MT29F2G01ABAGDWB_IT_H_
#define NAND_MT29F2G01ABAGDWB_IT_H_

#define NAND_MT29F2G01ABAGDWB_IT_VERSION    0.6

#include "main.h"
#include "ONE_Common.h"

#ifdef QSPI_DUALFLASH_ENABLE

#ifdef __cplusplus
extern "C"
{
#endif

#define MT29F2G_ENABLED

/* =============================================================================
 * MT29F2G01ABAGD (2Gb 3.3V SPI NAND, Micron) — 통신/제어 레퍼런스
 * 출처: m79a_2gb_3v_nand_spi.pdf Rev.F (통신·제어 필수 내용 요약)
 * =============================================================================
 *
 * [지오메트리]  2048 블록 × 64 페이지 × (2048 main + 128 spare)B. 소거 단위 = 블록(136KB).
 *              듀얼 플레인: plane = 블록주소 LSB (짝수 블록=plane0, 홀수=plane1).
 *
 * [명령셋]
 *   RESET            0xFF   인자 없음. CFG[2:0]만 000 복귀, 나머지 feature·block lock 유지
 *   READ ID          0x9F   1 dummy byte 후 2B: 0x2C(Micron) 0x24(2Gb 3.3V)
 *   GET FEATURES     0x0F   +1B 주소(A0/B0/C0/D0) → 1B 데이터
 *   SET FEATURES     0x1F   +1B 주소 +1B 데이터 (WREN 불필요)
 *   WRITE ENABLE     0x06   WEL set — PROGRAM/ERASE/PROTECT 전 필수
 *   WRITE DISABLE    0x04   WEL clear
 *   PAGE READ        0x13   +24bit row → array→cache 전송(tRD), OIP=1 유지
 *   READ FROM CACHE  0x03/0x0B(x1) 0x3B(x2) 0x6B(x4) 0xBB(dual I/O) 0xEB(quad I/O)
 *                           +16bit (plane|column) +1 dummy byte(8클럭)
 *   READ PAGE CACHE RANDOM 0x30 / LAST 0x3F   연속 읽기 파이프라인 (CRBSY 사용)
 *   PROGRAM LOAD     0x02(x1)/0x32(x4)  +16bit (plane|column) +데이터. 캐시를 all-FF로 리셋 후 적재
 *   PROGRAM LOAD RANDOM DATA 0x84(x1)/0x34(x4)  캐시 리셋 없이 지정 구간만 갱신 (부분 수정·internal data move용)
 *   PROGRAM EXECUTE  0x10   +24bit row → cache→array 굽기(tPROG), OIP=1 유지
 *   BLOCK ERASE      0xD8   +24bit row(블록) → tERS, OIP=1 유지
 *   PROTECT          0x2C   블록 0~47 영구 잠금 (4블록 그룹 단위, 비가역)
 *
 * [주소 포맷]
 *   row (24bit 필드)          : [23:17] dummy | [16:6] block | [5:0] page   (0x13/0x10/0xD8)
 *   cache 연산 (16bit 필드)   : [15:13] dummy | [12] plane(=block LSB, 2Gb 필수·1Gb만 dummy)
 *                               | [11:0] column (0~2175, spare 시작 0x800)
 *   ※ plane 비트 누락 시: 홀수 블록 program이 조용히 유실(phantom, PFAIL=0)되고
 *     read는 반대 plane 캐시(직전 값)를 반환 — 본 드라이버 v0.5에서 실증·수정.
 *
 * [Feature 레지스터]
 *   A0 (block lock, R/W)  : [7]BRWD [6]BP3 [5]BP2 [4]BP1 [3]BP0 [2]TB [1]WP#/HOLD# disable
 *                           POR = 전블록 잠금(BP=1111,TB=1). 0xFF reset으로 안 풀림 — SET FEATURE로 언락 필수
 *   B0 (config, R/W)      : [7]CFG2 [6]CFG1 [5]LOT_EN [4]ECC_EN [1]CFG0
 *                           CFG[2:0]=000 normal / 010 파라미터·OTP 접근. ECC_EN POR=1
 *   C0 (status, R)        : [7]CRBSY [6:4]ECCS[2:0] [3]P_Fail [2]E_Fail [1]WEL [0]OIP
 *                           OIP: PAGE READ·PROGRAM EXECUTE·ERASE·RESET 중 1
 *                           WEL: program/erase 성공 시 자동 클리어
 *   D0 (die select, R/W)  : [6]DS0
 *   ECCS 코드: 000 무오류 · 001 1~3bit 정정됨 · 010 정정불가(>8bit) · 011 4~6bit 정정(refresh 권장)
 *              · 101 7~8bit 정정(refresh 필수) → 오류 판정은 (SR & 0x70) == 0x20 만
 *
 * [타이밍 (max)]  tRD 70µs · tPROG 600µs · tERS 10ms · tRCBSY(cache random)
 *
 * [표준 시퀀스]
 *   읽기   : 0x13(row) → OIP=0 폴링(tRD 경과 보장 후) → 0x6B(plane|col)
 *   쓰기   : 0x06 → 0x32(plane|col)+데이터 → 0x10(row) → OIP=0 폴링 → P_Fail 확인
 *   소거   : 0x06 → 0xD8(row) → OIP=0 폴링 → E_Fail 확인
 *   파라미터 페이지: B0 CFG=010b → 0x13(row 0x01) → 0x03(col 0) → B0 원복
 *
 * [제약/주의]
 *   · 파셜 프로그램: 한 페이지당 최대 4회 (초과 금지)
 *   · spare의 ECC 영역 쓰기 금지 (사용자 메타 영역만 허용, 내부 ECC가 spare에 자동 기록)
 *   · 배드블록 마커: 블록 first/last 페이지 spare 선두 non-FF (소거하면 마커도 지워짐 — 소거 전 확인)
 *   · 공장 출하 시 최초 8블록 유효 보장, 배드블록 최대 40개/유닛
 *   · 파라미터 페이지·unique ID는 ECC 비보호 (파라미터는 다중 사본, ID는 XOR 검증)
 * ============================================================================= */

#define MT29F2G_ADDRESS(block, page, column)  ( \
    ((uint32_t)(column)  & 0xFFF)         | \
    (((uint32_t)(page)   & 0x3F)  << 12)  | \
    (((uint32_t)(block)  & 0x7FF) << 18)    \
)

#define MT29F2G_ADDRESS_BLOCK(address)      (((uint32_t)(address) >> 18) & 0x7FF)
#define MT29F2G_ADDRESS_PAGE(address)       (((uint32_t)(address) >> 12) & 0x3F)
#define MT29F2G_ADDRESS_COLUMN(address)     ((uint32_t)(address) & 0xFFF)

#define MT29F2G_SPARE_COLUMN                0x800                 // OOB 시작 컬럼

#define MT29F2G_ADDRESS_MAX                 0x0FFFFFFF
#define MT29F2G_MAINDATA_LENGTH             2048
#define MT29F2G_PAGE_LENGTH                 64
#define MT29F2G_BLOCK_LENGTH                2048

/* ---- 명령 옵코드 (READ Operations / Program / Erase / Features) ---- */
#define MT29F2G_CMD_RESET                   ((uint8_t)0xFF)
#define MT29F2G_CMD_READ_ID                 ((uint8_t)0x9F)
#define MT29F2G_CMD_GET_FEATURES            ((uint8_t)0x0F)
#define MT29F2G_CMD_SET_FEATURES            ((uint8_t)0x1F)
#define MT29F2G_CMD_WRITE_ENABLE            ((uint8_t)0x06)
#define MT29F2G_CMD_WRITE_DISABLE           ((uint8_t)0x04)
#define MT29F2G_CMD_PAGE_READ               ((uint8_t)0x13)       /* array→cache (tRD) */
#define MT29F2G_CMD_READ_CACHE_X1           ((uint8_t)0x03)
#define MT29F2G_CMD_READ_CACHE_X1_FAST      ((uint8_t)0x0B)
#define MT29F2G_CMD_READ_CACHE_X2           ((uint8_t)0x3B)
#define MT29F2G_CMD_READ_CACHE_X4           ((uint8_t)0x6B)
#define MT29F2G_CMD_READ_CACHE_DUAL_IO      ((uint8_t)0xBB)
#define MT29F2G_CMD_READ_CACHE_QUAD_IO      ((uint8_t)0xEB)
#define MT29F2G_CMD_READ_PAGE_CACHE_RANDOM  ((uint8_t)0x30)
#define MT29F2G_CMD_READ_PAGE_CACHE_LAST    ((uint8_t)0x3F)
#define MT29F2G_CMD_PROGRAM_LOAD_X1         ((uint8_t)0x02)       /* 캐시 all-FF 리셋 후 적재 */
#define MT29F2G_CMD_PROGRAM_LOAD_X4         ((uint8_t)0x32)
#define MT29F2G_CMD_PROGRAM_LOAD_RANDOM_X1  ((uint8_t)0x84)       /* 캐시 리셋 없이 부분 갱신 */
#define MT29F2G_CMD_PROGRAM_LOAD_RANDOM_X4  ((uint8_t)0x34)
#define MT29F2G_CMD_PROGRAM_EXECUTE         ((uint8_t)0x10)       /* cache→array (tPROG) */
#define MT29F2G_CMD_BLOCK_ERASE             ((uint8_t)0xD8)       /* tERS */
#define MT29F2G_CMD_PROTECT                 ((uint8_t)0x2C)       /* 블록 0~47 영구 잠금 (비가역) */

/* ---- Feature 레지스터 주소 (GET/SET FEATURES 1B 주소) ---- */
#define MT29F2G_FEATURE_BLOCKLOCK           ((uint8_t)0xA0)       /* R/W */
#define MT29F2G_FEATURE_CONFIG              ((uint8_t)0xB0)       /* R/W */
#define MT29F2G_FEATURE_STATUS              ((uint8_t)0xC0)       /* R   */
#define MT29F2G_FEATURE_DIESELECT           ((uint8_t)0xD0)       /* R/W */

/* ---- 0xA0 Block Lock 비트 ---- */
#define MT29F2G_LOCK_BRWD                   ((uint8_t)0x80)
#define MT29F2G_LOCK_BP3                    ((uint8_t)0x40)
#define MT29F2G_LOCK_BP2                    ((uint8_t)0x20)
#define MT29F2G_LOCK_BP1                    ((uint8_t)0x10)
#define MT29F2G_LOCK_BP0                    ((uint8_t)0x08)
#define MT29F2G_LOCK_TB                     ((uint8_t)0x04)
#define MT29F2G_LOCK_WP_HOLD_DISABLE        ((uint8_t)0x02)
#define MT29F2G_LOCK_BP_MASK                ((uint8_t)0x7C)       /* BP3..BP0|TB — 0이면 전블록 언락 */
#define MT29F2G_LOCK_UNLOCK_ALL             ((uint8_t)0x02)       /* 전블록 언락 + WP#/HOLD# disable */
#define MT29F2G_LOCK_POR_DEFAULT            ((uint8_t)0x7C)       /* POR 기본 = 전블록 잠금 (0xFF reset으로 안 풀림) */

/* ---- 0xB0 Configuration 비트 ---- */
#define MT29F2G_CFG_CFG2                    ((uint8_t)0x80)
#define MT29F2G_CFG_CFG1                    ((uint8_t)0x40)
#define MT29F2G_CFG_LOT_EN                  ((uint8_t)0x20)
#define MT29F2G_CFG_ECC_EN                  ((uint8_t)0x10)
#define MT29F2G_CFG_CFG0                    ((uint8_t)0x02)
#define MT29F2G_CFG_BITS                    ((uint8_t)0xC2)       /* CFG[2:0] 마스크 (000=normal) */
#define MT29F2G_CFG_ACCESS_PARAMETER        ((uint8_t)0x40)       /* CFG=010b — 파라미터/OTP/UniqueID 접근 */
#define MT29F2G_CFG_NORMAL                  (MT29F2G_CFG_ECC_EN)  /* 일반 동작: ECC on + CFG=000 */

/* ---- 0xC0 Status Register 비트 - Page.43 ---- */
#define MT29F2G_SR_WIP                      ((uint8_t)0x01)       /*!< OIP: PAGE READ·PROGRAM·ERASE·RESET 진행 중 */
#define MT29F2G_SR_WREN                     ((uint8_t)0x02)       /*!< WEL (program/erase 성공 시 자동 클리어) */
#define MT29F2G_SR_EFAIL                    ((uint8_t)0x04)       /*!< Erase fail */
#define MT29F2G_SR_PFAIL                    ((uint8_t)0x08)       /*!< Program fail */
#define MT29F2G_SR_CRBSY                    ((uint8_t)0x80)       /*!< Cache read busy (READ PAGE CACHE RANDOM 전용) */
#define MT29F2G_SR_ECC                      ((uint8_t)0x70)       /*!< ECCS[2:0] 마스크 */

/* ---- ECCS 코드 (SR & MT29F2G_SR_ECC) ---- */
#define MT29F2G_ECCS_NONE                   ((uint8_t)0x00)       /* 무오류 */
#define MT29F2G_ECCS_CORRECTED_1_3          ((uint8_t)0x10)       /* 1~3bit 정정됨 (데이터 정상) */
#define MT29F2G_ECCS_UNCORRECTABLE          ((uint8_t)0x20)       /* >8bit 정정불가 — 유일한 오류 */
#define MT29F2G_ECCS_CORRECTED_4_6          ((uint8_t)0x30)       /* 4~6bit 정정 (refresh 권장) */
#define MT29F2G_ECCS_CORRECTED_7_8          ((uint8_t)0x50)       /* 7~8bit 정정 (refresh 필수) */

/* ---- 타이밍 (datasheet max) ---- */
#define MT29F2G_TRD_MAX_US                  70                    /* PAGE READ array→cache */
#define MT29F2G_TPROG_MAX_US                600                   /* PROGRAM EXECUTE */
#define MT29F2G_TERS_MAX_MS                 10                    /* BLOCK ERASE */

/* ---- 주소 변환 ----
 * row(24bit 필드)        : [16:6]=block | [5:0]=page — 0x13/0x10/0xD8
 * cache 연산(16bit 필드) : [12]=plane(블록 LSB, 2Gb 필수) | [11:0]=column — 0x03/0x3B/0x6B/0x02/0x32 */
#define MT29F2G_ROW_ADDRESS(address)        (((uint32_t)(address) >> 12) & 0x1FFFF)
#define MT29F2G_CACHE_ADDRESS(address)      (((((uint32_t)(address) >> 18) & 0x1) << 12) | ((uint32_t)(address) & 0x0FFF))

#define MT29F2G_READ_MODE                   4
#define MT29F2G_WRITE_MODE                  4

typedef struct
{
    uint32_t Signature;                      /*!< Parameter page signature */
    uint16_t Revision;                       /*!< Revision number */
    uint16_t Feature;                        /*!< Feature support */
    uint16_t OptionalCommands;               /*!< Optional commands support */
    uint8_t Reserved1[22];                   /*!< Reserved */
    uint8_t DeviceManufacturer[12];          /*!< Device manufacturer */
    uint8_t DeviceModel[20];                 /*!< Device model */
    uint8_t ManufacturerID;                  /*!< Manufacturer ID */
    uint16_t DateCode;                       /*!< Date code */
    uint8_t Reserved2[12];                   /*!< Reserved */
    uint32_t PageSize;                       /*!< Number of data bytes per page */
    uint16_t SpareSize;                      /*!< Number of spare bytes per page */
    uint16_t PartialPageSize;                /*!< Number of data bytes per partial page */
    uint8_t Reserved3[2];                    /*!< Reserved */
    uint16_t PartialSpareSize;               /*!< Number of spare bytes per partial page */
    uint32_t BlockSize;                      /*!< Number of pages per block */
    uint32_t UnitSize;                       /*!< Number of blocks per unit */
    uint8_t LogicalUnit;                     /*!< Number of logical units */
    uint8_t AddressCycles;                   /*!< Number of address cycles */
    uint8_t BitsPerCell;                     /*!< Number of bits per cell */
    uint8_t BadBlocksMaximumPerUnit;         /*!< Bad blocks maximum per unit */
    uint8_t Reserved4;                       /*!< Reserved */
    uint16_t BlockEndurance;                 /*!< Block endurance */
    uint8_t GuaranteedBlock;                 /*!< Guaranteed valid blocks at beginning of target */
    uint16_t BlockEnduranceGuaranteed;       /*!< Block endurance for guaranteed valid blocks */
    uint8_t ProgramsPerPage;                 /*!< Number of programs per page */
    uint8_t PartialProgrammingAttributes;    /*!< Partial programming attributes */
    uint8_t ECCBits;                         /*!< Number of ECC bits */
    uint8_t InterleavedAddressBits;          /*!< Number of interleaved address bits */
    uint8_t InterleavedOperationAttributes;  /*!< Interleaved operation attributes */
    uint8_t Reserved5[13];                   /*!< Reserved */
    uint8_t PinCapacitance;                  /*!< I/O pin capacitance */
    uint8_t TimingModeSupport;               /*!< Timing mode support */
    uint16_t ProgramCacheTiming;             /*!< Program cache timing */
    uint8_t Reserved6;                       /*!< Reserved */
    uint8_t tPROG[2];                        /*!< tPROG maximum page program time */
    uint8_t tERS[2];                         /*!< tERS maximum block erase time */
    uint8_t tR[2];                           /*!< tR maximum page read time */
    uint8_t tCCS[2];                         /*!< tCCS minimum */
    uint8_t Reserved7[23];                   /*!< Reserved */
    uint16_t VendorSpecificRevision;         /*!< Vendor-specific revision number */
    uint8_t VendorSpecific[14];              /*!< Vendor specific */
    uint8_t Reserved8[68];                   /*!< Reserved */
    uint8_t ECCCorrectAbility;               /*!< ECC maximum correct ability */
    uint8_t DieSelectFeature;                /*!< Die select feature */
    uint8_t Reserved9[4];                    /*!< Reserved */
    uint16_t IntegrityCRC;                   /*!< Integrity CRC */
} MT29F2G_Info_t;

typedef union {
    uint32_t Address;         // 전체 주소 (1바이트 단위 논리 주소)

    struct {
        uint32_t Data  : 12;  // A[11:0]  → Data address (0~2175)
        uint32_t Page  : 6;   // A[17:12] → Page number within block (0~63)
        uint32_t Block : 11;  // A[28:18] → Block number (0~2047)
                                            // A[31:29] → 상위 비트 여유
    };
} MT29F2G_Address_t;

typedef enum {
	MT29F2G_RESULT_FATAL_FAULT        = -14,
	MT29F2G_RESULT_WREN_FAIL          = -13,  // write enable fail
	MT29F2G_RESULT_ECC_ERROR          = -12,
	MT29F2G_RESULT_ERASE_FAIL         = -11,
	MT29F2G_RESULT_PROGRAM_FAIL       = -10,
	MT29F2G_RESULT_AUTOPOLLING_ERROR  = -9,
	MT29F2G_RESULT_STATE_FAIL         = -8,
	MT29F2G_RESULT_RX_ERROR           = -7,
	MT29F2G_RESULT_TX_ERROR           = -6,
	MT29F2G_RESULT_COM_ERROR          = -5,
	MT29F2G_RESULT_TIMEOUT            = -4,
	MT29F2G_RESULT_BADBLOCK           = -3,
	MT29F2G_RESULT_BUSY               = -2,
	MT29F2G_RESULT_PARAMETER_ERROR    = -1,
	MT29F2G_RESULT_NULL               = 0,
	MT29F2G_RESULT_RUN,
	MT29F2G_RESULT_OK,
} MT29F2G_Result_t;


//MT29F2G01ABAGDWB_IT
typedef struct
{
	QSPI_HandleTypeDef *pHandle;
	//MT29F2G_Info_t Info;

	uint8_t State;

	uint8_t IsOpen;
} MT29F2G_t;

extern oResult_t MT29F2G_Init(MT29F2G_t *pMT29F2G, QSPI_HandleTypeDef *pHandle);
extern oResult_t MT29F2G_DeInit(MT29F2G_t *pMT29F2G);
extern oResult_t MT29F2G_Erase(MT29F2G_t *pMT29F2G, uint32_t NumberOfBlock);
extern oResult_t MT29F2G_BadBlockCheck(MT29F2G_t *pMT29F2G, uint32_t BlockAddress);
extern oResult_t MT29F2G_Reset(MT29F2G_t *pMT29F2G);
extern oResult_t MT29F2G_ReadPage(MT29F2G_t *pMT29F2G, uint32_t Block, uint32_t Page, uint8_t *pData, uint32_t Size);
extern oResult_t MT29F2G_WritePage(MT29F2G_t *pMT29F2G, uint32_t Block, uint32_t Page, uint8_t *pData, uint32_t Size);
extern oResult_t MT29F2G_ReadByte(MT29F2G_t *pMT29F2G, uint32_t Block, uint32_t Page, uint32_t ByteIndex, uint8_t *pData, uint32_t Size);
extern oResult_t MT29F2G_WriteByte(MT29F2G_t *pMT29F2G, uint32_t Block, uint32_t Page, uint32_t ByteIndex, uint8_t *pData, uint32_t Size);
extern MT29F2G_Result_t MT29F2G_WriteEnable(MT29F2G_t *pMT29F2G);   // 0x06 — WEL set
extern MT29F2G_Result_t MT29F2G_WriteDisable(MT29F2G_t *pMT29F2G);  // 0x04 — WEL clear
// 비블로킹(RESULT_RUN) 원시 read — 재진입 시 동일 (Address,pData,Size)로 재호출해야 함(단일 소유자 전제).
// 일반 호출은 MT29F2G_ReadByte(블로킹) 사용 권장 — 비블로킹 전용 호출자(색인기 등)만 직접 사용.
extern MT29F2G_Result_t MT29F2G_ReadMemory(MT29F2G_t *pMT29F2G, uint32_t Address, uint8_t *pData, uint32_t Size);

#ifdef __cplusplus
}
#endif

#endif
#endif /* NAND_MT29F2G01ABAGDWB_IT_H_ */

/* History

2026-06-26 | v0.1
	- baseline (NAND_MT29F2G01ABAGDWB_IT.h)
2026-07-16 | v0.2
	- Erase/WriteMemory tERS·tPROG WIP 대기를 비블로킹 상태머신으로 전환 (블로킹 AutoPolling 제거):
	  · Erase: WriteEnable+0xD8 발행(case 0) → WIP 매 사이클 폴링(case 1), NAND_ERASE_TIMEOUT(100ms) 상한
	  · WriteMemory: WREN+PROGRAM LOAD/EXECUTE 발행(case 0) → WIP 폴링(case 1), NAND_PROGRAM_TIMEOUT(50ms) 상한, MT29F2G_RESULT_RUN 반환
	  · WriteByte: MT29F2G_RESULT_RUN → RESULT_RUN 전파
	  · 근거: 블록 소거 시 최대 ~10ms main 루프 블로킹 제거 (ReadMemory/BadBlockCheck 등 µs 대기는 블로킹 유지)
	  · 상위 Mi_Storage v0.12 (MiStorage_Write case 1·program·EraseDataArea RUN 처리)와 짝
2026-07-20 | v0.3
	- MT29F2G_BadBlockCheck: 검사 페이지 6→2 (첫 page0 + 마지막 page). 부팅 색인 read 수 감소.
	  Micron 배드블록 마킹이 블록 첫·마지막 페이지 spare에 위치하는 규약 준수.
	- MT29F2G_Init: SetFeature(0xB0,0x11) 후 GetFeature 리드백 검증 단계 추가 — (cfg & 0x10)==0x10(ECC_EN)
	  확인, 실패 시 재-reset 재시도(최대 10). 전원 인가 직후 config 미반영 상태를 걸러 Open 신뢰성 확보.
	- NAND_TIMEOUT_VALUE 1000→100ms 적용: HAL QSPI 단일 트랜잭션(cmd/data/AutoPolling) 블로킹 상한.
	  QUADSPI 페리페럴 완료/상태매치 플래그를 SysTick(ms)로 감시하는 SW 타이머 — 칩 응답 여부가 아니라
	  페리페럴 행(BUSY 미해제)·AutoPolling 칩 busy 고착에서만 걸림. 정상 ~0.15ms·최악 erase-WIP ~10ms 대비
	  10배 여유. main 루프 블로킹을 1000→100ms로 축소. 전블록 read(13만건) 무결로 안전 확인 후 적용.
2026-07-22 | v0.4
	- ReadMemory: PAGE READ(0x13) 후 폴링 비트 CRBSY(0x80)→WIP(0x01, OIP) 교정.
	  0x13은 tRD(~115µs) 동안 OIP/WIP를 세우고 CRBSY는 continuous cache read 전용이라 평범한 0x13 뒤엔
	  세워지지 않음 → CRBSY 폴링은 즉시 통과, tRD 완료 전 READ CACHE(0x6B)가 직전 페이지 캐시를 읽을 위험.
	  데이터시트 정합 시퀀스로 정상 read 강건화 (기록 페이지 read에 영향 없음 확인).
2026-07-22 | v0.5
	- 데이터시트(Micron MT29F2G01AB) 전량 대조 후 근본원인 수정: ReadMemory가 tRD 완료를 보장하지 않아
	  (WIP 폴링 첫 샘플이 OIP 상승 전/통신오류 -1을 "완료"로 오판) 0x6B가 직전 캐시(마지막 프로그램
	  데이터)를 반환 — "쓰기 후 즉시 읽기 = 마지막 쓴 값 고정, 전원차단으로만 복구" 증상의 원인.
	  0x13 후 무조건 120µs(tRD 70µs 초과 여유) 대기 후 OIP=0 확인으로 교정. 폴링도 `!=0`(busy·오류
	  모두 대기)로 변경해 fall-through 차단.
	- ReadMemory를 비블로킹 2-state 상태머신으로 전환(case0: 0x13 발행, case1: tRD대기+0x6B수신).
	  재진입 계약: 완료 전 다른 (Address,pData,Size)로 재호출 시 PARAMETER_ERROR(단일 소유자 위반 검출).
	  DMA 미사용 — 0x6B 수신 자체는 사이클 내 블로킹 유지, tRD 대기만 사이클에 분산.
	  ReadByte(블로킹 스핀 래퍼)·BadBlockCheck·GetInfo는 완료까지 스핀해 기존 호출자 동작 무변경 유지.
	- Init 언락 검증 추가: 0xB0(ECC_EN) 반영만 확인하던 것을 0xA0(블록 언락, BP/TB=0)도 read-back
	  검증하도록 확장 — 무음 락(program/erase 조용히 실패) 방지. Config 0xB0 `0x11`→`0x10`(reserved
	  bit0 제거). ECC 판정 `(State&0x70)==0x20`(정정불가 010b만)으로 교정 — 기존 `&0x20`은 011b
	  (4~6비트 정정성공, 데이터 정상)까지 오판.
	- HW 검증: block1 P1/P2/P3 서로 다른 값 연속 쓰기→즉시 읽기 3/3 정확 일치(allPass=1), 부팅 2회
	  재현 동일. 빌드 0 error/0 warning.
2026-07-23 | v0.5 (계속)
	- **[치명] 2Gb 듀얼플레인 plane select 비트 누락 수정** — 이 드라이버 역사상 모든 "쓰기 후 읽기
	  =마지막 값"·"배드블록 의심(block1/3)" 증상의 진짜 근본 원인.
	  · READ FROM CACHE(0x03/0x3B/0x6B)·PROGRAM LOAD(0x02/0x32)의 16bit 주소 필드 =
	    [15:13]dummy | [12]plane select | [11:0]column. 2Gb는 plane select=블록주소 LSB 필수
	    (1Gb에서만 dummy — 데이터시트 Fig.13/15/20/23 note).
	  · 기존 `Address&0xFFF`(plane 항상 0) → 홀수 블록(plane1): PROGRAM LOAD가 plane0 캐시에
	    적재되고 EXECUTE는 빈 plane1 캐시를 구움(phantom program, WIP 정상·PFAIL=0), READ는
	    0x13이 plane1 캐시에 로드하나 0x6B가 plane0 캐시(직전 값)를 반환(stale). 짝수 블록만 정상.
	  · 수정: 캐시 연산 주소 = ((Address>>18)&1)<<12 | (Address&0xFFF).
	  · HW 검증(시리얼 로그): 홀수 블록1·3 쓰기/읽기 3/3 정확(수정 전 phantom/stale 재현 후 수정
	    확인), 400건 링 시드 홀짝 블록 전수 계상(Cnt 401/401), 4블록 소거 gap-resume·재부팅 재현.
	- 비블로킹 read 재진입 인자 변경 시 하드실패(PARAMETER_ERROR)→이전 read 폐기 후 새 read
	  재시작(self-healing)으로 변경 — 미완료 0x13은 칩 상태를 남기지 않아 폐기 안전.
	- 정리: 오류 유발 잔재 제거 — ReadBlock/WriteBlock(정의 없는 유령 선언), PFAIL/EFAIL/ECC_FAIL
	  (실제 SR 비트값과 불일치: 진짜는 SR_PFAIL=0x08), SR_ECCFAULT(0x20 단독 판정 — SR_ECC 0x70
	  코드 판정으로 대체됨), TOTLA_PAGE_LENGTH(오타명 미사용), 구버전 주소 매크로 주석 블록.
	  WriteDisable(0x04, WEL 클리어) API 복원 + WriteEnable/WriteDisable 헤더 선언 추가.
	- 데이터시트 통신/제어 레퍼런스를 헤더에 코드화: 명령 옵코드(MT29F2G_CMD_*)·Feature 주소
	  (MT29F2G_FEATURE_*)·Block Lock/Config 비트(MT29F2G_LOCK_*·MT29F2G_CFG_*)·ECCS 코드(MT29F2G_ECCS_*)·
	  타이밍 max(TRD/TPROG/TERS)·주소 변환(ROW_ADDRESS/CACHE_ADDRESS) define + 요약 주석 블록.
	  .c의 매직넘버 전부(옵코드·레지스터 주소·비트마스크·주소식 37곳)를 define으로 치환 —
	  값 동일(동작 무변경), 빌드 0err/0warn.
2026-08-07 | v0.6
	- ReadMemory WIP 폴링에 NAND_READ_TIMEOUT(10ms) 상한 — 무상한이라 칩 무응답 시 비블로킹
	  호출자는 영구 RESULT_RUN, 블로킹 래퍼는 하드 무한루프였다. tRD 대기용 RdTimer와는
	  틱 베이스가 달라(us vs ms) 별도 타이머(RdWipTimer) 사용
*/
