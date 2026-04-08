# Plan: oCANopen Stack Refactoring

> Feature: canopen-stack-refactoring
> Created: 2026-04-02
> Status: Draft

---

## 1. Background & Motivation

oCANopen is a CANopenNode-based CANopen stack library for STM32 embedded systems. Currently used as a shared library across multiple projects, but the current architecture has several issues that hinder reusability and maintainability.

**User Goal:** CO_Init -> OD_Init -> PDO_Init sequencing with easy multi-project reuse.

---

## 2. Current State Analysis (Issues Found)

### 2.1 Architecture Issues

| # | Issue | Severity | Location |
|---|-------|----------|----------|
| A1 | **Monolithic CO_init()** - OD setup, CAN init, SDO/NMT/SYNC/EM init all in one function (158~326 lines). No way to customize OD entries per project without modifying library code | High | CANopen.c:158 |
| A2 | **No OD_Init separation** - Communication OD entries (0x1000~0x1A00) are hardcoded inside CO_init(). Manufacturer-specific OD (0x2000~0x5FFF) must be added externally but there's no dedicated init function | High | CANopen.c:166-195 |
| A3 | **No PDO_Init separation** - PDO init (CO_RPDO_init/CO_TPDO_init) exists separately but is disconnected from the main init flow. No unified entry point | Medium | CO_PDO.c:701,816 |
| A4 | **Tight coupling to ONE_CAN/ONE_Time** - CO_common.h directly includes `ONE_CAN.h` and `ONE_Time.h`, making it impossible to use with different HAL layers | High | CO_common.h:19-20 |
| A5 | **Circular include impossible** - CO_PDO.h cannot include CANopen.h (CO_t), and CANopen.h includes CO_PDO.h (CO_RPDO_t). All structs are anonymous `typedef struct{...}` with no tag, blocking forward declaration. CO_PDO.h uses `void*` as workaround | High | CO_PDO.h:230-231 |

### 2.2 Code Quality Issues

| # | Issue | Severity | Location |
|---|-------|----------|----------|
| Q1 | **Typo: `CommunicationObejct`** - Should be `CommunicationObject`. Used throughout the codebase (~30+ occurrences) | Low | CANopen.h:69,99,124 |
| Q2 | **Static variables in process functions** - `CO_process_RPDO()` and `CO_process_TPDO()` use `static int16_t i`, making them non-reentrant and incompatible with multi-instance usage | Medium | CANopen.c:374,404 |
| Q3 | **Commented-out HBconsumer** - `CO_HBconsumer_init()` and `CO_HBconsumer_process()` are commented out but code remains | Low | CANopen.c:292-303,488-493 |
| Q4 | **Missing CO_delete() cleanup** - `CO_delete()` references freed pointers that don't match the CO_USE_GLOBALS pattern. Dead code path | Low | CANopen.c:331-362 |
| Q5 | **RPDO mapping attribute check bug** - Duplicate `CO_ODA_RPDO_MAPABLE` in OR expression, missing TPDO check | Medium | CO_PDO.c:241 |
| Q6 | **ManufacturerSoftwareVersion/HardwareVersion size mismatch** - Declared as `VISIBLE_STRING[4]` but registered in OD with length 10 | Medium | CANopen.h:79-80, CANopen.c:175-176 |

### 2.3 Reusability Issues

| # | Issue | Severity | Location |
|---|-------|----------|----------|
| R1 | **No project-level configuration callback** - CO_init() takes only `ParameterSaveCallback`, no hook for project-specific OD setup | High | CANopen.c:158 |
| R2 | **Fixed PDO count at compile time** - `CO_NO_RPDO=8`, `CO_NO_TPDO=8` wastes RAM on projects needing fewer PDOs | Medium | CO_common.h:28-29 |
| R3 | **No configuration struct** - NodeID, BitRate, HeartbeatTime etc. are scattered in `CO_CommunicationObejct_t` with no clean config API | Medium | CANopen.h:72-99 |
| R4 | **CANOPEN_INITIALIZER macro uses GCC extension** - `[0 ... CO_NO_RPDO-1]` range initializer is GCC-specific, not portable | Low | CANopen.h:63-64 |

---

## 3. Proposed Solution

### 3.1 New Initialization Flow

```
[Application Code]
       |
       v
  CO_Init(&co, &config)          // Phase 1: Core init (CAN, SDO, NMT, EM, SYNC)
       |                         //          Communication OD auto-registered
       v
  CO_OD_addVariable(...)         // Phase 2: App-specific OD entries (0x2000+)
  CO_OD_addVariable(...)         //          Project adds its own variables
       |
       v
  CO_TPDO_init(&co, ...)         // Phase 3: TPDO configuration
  CO_RPDO_init(&co, ...)         //           RPDO configuration
       |                         //           Maps OD vars to PDOs
       v
  CO_Start(&co)                  // Phase 4: Enter Pre-Operational / Operational
       |
       v
  while(1) { CO_process(&co); }  // Main loop
```

### 3.2 Configuration Structure

```c
typedef struct {
    uint8_t         nodeId;           // 1-127
    uint16_t        bitRate;          // 10,20,50,125,250,500,800,1000
    uint16_t        heartbeatTime_ms; // Producer heartbeat (0=disabled)
    uint8_t         nmtStartup;       // 0=PreOp, 1=Operational
    CAN_HandleTypeDef *canHandle;     // HAL CAN handle
    CO_OD_Callback_t  paramSaveCb;    // Parameter save callback
} CO_Config_t;
```

### 3.3 Refactoring Scope

| Phase | Function | Responsibility |
|-------|----------|---------------|
| 1 | `CO_Init()` | CAN module + communication OD + SDO + NMT + EM + SYNC |
| 2 | `CO_OD_addVariable()` / `CO_OD_addRecord()` | Application uses existing API for manufacturer OD entries |
| 3 | `CO_TPDO_init()` / `CO_RPDO_init()` | Already exist, fix void* -> CO_t* |
| 4 | `CO_Start()` | **NEW** - Validate config, set NMT state, start heartbeat |

---

## 4. Detailed Tasks

### Task 0: Enable cross-header references via tagged structs + forward declaration

**Problem:** 현재 모든 구조체가 anonymous `typedef struct { ... } CO_SDO_t;` 형태라서
다른 헤더에서 forward declaration이 불가능. CO_PDO.h에서 CO_t*를 쓸 수 없어 void*로 우회 중.

**Solution:** 모든 모듈 구조체에 tag 부여 + 필요한 곳에서 forward declaration 사용.

```c
// === Before (anonymous struct - forward declaration 불가) ===
typedef struct {
    CO_EM_t *em;
    ...
} CO_SDO_t;

// === After (tagged struct - forward declaration 가능) ===
typedef struct CO_SDO_s {
    struct CO_EM_s *em;     // forward reference to CO_EM_t
    ...
} CO_SDO_t;
```

**변경 규칙:**
1. 각 CO_xxxx.h 헤더의 주요 구조체에 tag 이름 부여 (`_s` suffix)
2. 다른 모듈 타입을 포인터로 참조할 때 `struct CO_xxxx_s *` 사용
3. CANopen.h의 CO_t도 `typedef struct CO_s { ... } CO_t;`로 변경
4. CO_PDO.h 등에서 `struct CO_s;` forward declaration으로 CO_t* 사용 가능

**적용 대상:**

| Header | struct tag | Forward declarations needed |
|--------|-----------|---------------------------|
| `CO_driver.h` | `CO_CANmodule_s`, `CO_CANtx_s`, `CO_CANrx_s` | - |
| `CO_OD.h` | `CO_OD_s`, `CO_OD_EntryItem_s`, `CO_OD_Entry_s` | - |
| `CO_SDO.h` | `CO_SDO_s` | `struct CO_EM_s`, `struct CO_OD_s` |
| `CO_Emergency.h` | `CO_EM_s`, `CO_EMpr_s` | `struct CO_SDO_s` |
| `CO_NMT_Heartbeat.h` | `CO_NMT_s` | `struct CO_EMpr_s` |
| `CO_SYNC.h` | `CO_SYNC_s` | `struct CO_EM_s`, `struct CO_SDO_s` |
| `CO_PDO.h` | `CO_RPDO_s`, `CO_TPDO_s` | `struct CO_s` (CO_t), `struct CO_SDO_s`, `struct CO_EM_s`, `struct CO_SYNC_s` |
| `CO_HBconsumer.h` | `CO_HBconsumer_s` | `struct CO_EM_s` |
| `CANopen.h` | `CO_s` | - (includes all headers) |

**Include 관계 (변경 후):**

```
CO_Common.h  ─── 기본 타입, 에러코드, 유틸리티
    ↑
CO_OD.h      ─── OD 구조체 (CO_OD_s)
    ↑
CO_Driver.h  ─── CAN 드라이버 (CO_CANmodule_s)
    ↑
CO_SDO.h     ─── SDO (CO_SDO_s) + forward: CO_EM_s
CO_Emergency.h── Emergency (CO_EM_s) + forward: CO_SDO_s
CO_NMT.h     ─── NMT (CO_NMT_s) + forward: CO_EMpr_s
CO_SYNC.h    ─── SYNC (CO_SYNC_s)
CO_PDO.h     ─── PDO (CO_RPDO_s, CO_TPDO_s) + forward: CO_s
    ↑
CANopen.h    ─── CO_t (CO_s) = 모든 모듈 통합, includes all above
```

**핵심:** 각 헤더는 자신의 구조체를 정의하고, 다른 모듈 타입이 포인터로만 필요할 때
forward declaration (`struct CO_xxxx_s *`)을 사용. 실제 멤버 접근은 .c 파일에서
해당 헤더를 include하여 수행.

**Files:** ALL header files in `Inc/`

---

### Task 1: Introduce CO_Config_t and refactor CO_Init()

**Changes:**
- Create `CO_Config_t` struct in `CANopen.h`
- Refactor `CO_Init()` to accept `CO_Config_t *config` instead of scattered params
- Keep internal OD setup for communication objects (0x1000~0x1FFF) inside CO_Init()
- Config values populate `CommunicationObject` struct internally

**Files:** `CANopen.h`, `CANopen.c`

### Task 2: Add CO_Start() function

**Changes:**
- New function that finalizes setup and transitions NMT state
- Validates that CAN module is initialized
- Sets `operatingState` based on `nmtStartup` config
- Sends boot-up message

**Files:** `CANopen.h`, `CANopen.c`

### Task 3: Fix ManufacturerSoftwareVersion/HardwareVersion size

**Changes:**
- Align `VISIBLE_STRING[4]` declaration with OD registration length
- Expand array to `VISIBLE_STRING[10]` to match OD registration

**Files:** `CANopen.h`

### Task 4: Fix static variable in CO_process_RPDO/TPDO

**Changes:**
- Move `static int16_t i` into `CO_t` struct as instance member (`rpdoProcessIdx`, `tpdoProcessIdx`)
- Enables multi-instance support

**Files:** `CANopen.h`, `CANopen.c`

### Task 5: Fix RPDO mapping attribute check

**Changes:**
- Fix duplicate `CO_ODA_RPDO_MAPABLE` in condition at CO_PDO.c:241
- Correct to check `CO_ODA_RPDO_MAPABLE` for R_T==0 and `CO_ODA_TPDO_MAPABLE` for R_T!=0

**Files:** `CO_PDO.c`

### Task 6: Fix void* -> CO_t* in PDO init functions

**Changes:**
- Change `CO_RPDO_init(void *pCANopen, ...)` to `CO_RPDO_init(CO_t *CO, ...)`
- Change `CO_TPDO_init(void *pCANopen, ...)` to `CO_TPDO_init(CO_t *CO, ...)`
- Remove internal `CO_t *pCO = (CO_t *)pCANopen` cast

**Files:** `CO_PDO.h`, `CO_PDO.c`

### Task 7: Clean up dead code

**Changes:**
- Remove commented-out HBconsumer code or properly #ifdef guard it
- Fix CO_delete() to match CO_USE_GLOBALS pattern (simplify to memset/reset)
- Remove unused malloc path since CO_USE_GLOBALS is always defined

**Files:** `CANopen.c`

### Task 8: Fix CommunicationObejct typo (Optional)

**Changes:**
- Rename `CommunicationObejct` -> `CommunicationObject` throughout
- This is a breaking change for existing projects
- **Decision needed:** Fix now or maintain backward compatibility

**Files:** `CANopen.h`, `CANopen.c` (all references)

### Task 9: Add usage example

**Changes:**
- Create reference example showing the new 4-phase init flow:
  ```c
  CO_t co = CANOPEN_INITIALIZER(myODItems);
  CO_Config_t cfg = { .nodeId=1, .bitRate=250, .heartbeatTime_ms=300 };

  CO_Init(&co, &cfg);

  // App OD entries
  CO_OD_addVariable(&co.OD, 0x3000, 0, 0x2F, 2, &myData.temperature);
  CO_OD_addVariable(&co.OD, 0x3001, 0, 0x2F, 2, &myData.humidity);

  // PDO mapping
  uint32_t tpdo1Map[] = { CO_OBJECTMAP_INITIALIZER(0x3000, 0, 16),
                           CO_OBJECTMAP_INITIALIZER(0x3001, 0, 16) };
  CO_TPDO_init(&co, 0, 255, 100, 300, tpdo1Map, 2);

  CO_Start(&co);
  while(1) { CO_process(&co); }
  ```

---

## 5. Priority & Execution Order

```
[HIGH]   Task 0: Tagged structs + forward declaration (순환 include 해결) ★ 최우선
[HIGH]   Task 1: CO_Config_t + CO_Init() refactoring
[HIGH]   Task 2: CO_Start() function
[HIGH]   Task 6: void* -> CO_t* type safety (Task 0 완료 후 자동 해결)
[MEDIUM] Task 4: Static variable fix (multi-instance)
[MEDIUM] Task 5: RPDO mapping attribute fix
[MEDIUM] Task 3: Version string size fix
[LOW]    Task 7: Dead code cleanup
[LOW]    Task 8: Typo fix (optional, breaking)
[LOW]    Task 9: Usage example
```

**Execution Order:** Task 0 -> 6 -> 1 -> 2 -> 4 -> 5 -> 3 -> 7 -> 8 -> 9

> Task 0 완료 시 Task 6(void* -> CO_t*)이 자연스럽게 가능해짐

---

## 6. File Change Summary

| File | Action | Tasks | Description |
|------|--------|-------|-------------|
| `Inc/CO_common.h` | Modify | - | Conditional defaults for CO_NO_RPDO/TPDO |
| `Inc/CO_driver.h` | Modify | T0 | Add struct tags: CO_CANmodule_s, CO_CANtx_s, CO_CANrx_s |
| `Inc/CO_OD.h` | Modify | T0 | Add struct tags: CO_OD_s, CO_OD_EntryItem_s |
| `Inc/CO_SDO.h` | Modify | T0 | Add struct tag CO_SDO_s + forward declarations |
| `Inc/CO_Emergency.h` | Modify | T0 | Add struct tags CO_EM_s, CO_EMpr_s + forward declarations |
| `Inc/CO_NMT_Heartbeat.h` | Modify | T0 | Add struct tag CO_NMT_s + forward declarations |
| `Inc/CO_SYNC.h` | Modify | T0 | Add struct tag CO_SYNC_s + forward declarations |
| `Inc/CO_PDO.h` | Modify | T0,T6 | Add struct tags + forward declare CO_s + void*->CO_t* |
| `Inc/CO_HBconsumer.h` | Modify | T0 | Add struct tag CO_HBconsumer_s |
| `Inc/CANopen.h` | Modify | T0,T1,T2,T3,T4 | CO_s tag, CO_Config_t, CO_Start(), version fix |
| `Src/CANopen.c` | Modify | T1,T2,T4,T7 | CO_Init refactoring, CO_Start, static removal, cleanup |
| `Src/CO_PDO.c` | Modify | T5,T6 | void*->CO_t*, attribute check fix |

**Total: 12 files modified, 0 new files**

---

## 7. Impact & Migration

### Breaking Changes
- `CO_init()` signature: `(CO_t*, CAN_HandleTypeDef*, CO_OD_Callback_t)` -> `(CO_t*, CO_Config_t*)`
- PDO init: `void*` -> `CO_t*` (compile error in existing code, easy fix)
- New required call: `CO_Start()` after PDO setup

### Migration Steps for Existing Projects
1. Create `CO_Config_t cfg = { .nodeId=X, .bitRate=Y, .canHandle=&hcan, .paramSaveCb=cb };`
2. Replace `CO_init(&co, &hcan, cb)` with `CO_Init(&co, &cfg)`
3. Keep OD add and PDO init calls as-is
4. Add `CO_Start(&co)` before main loop

---

## 8. Success Criteria

- [ ] **Tagged structs**: 모든 주요 구조체에 tag 부여, forward declaration으로 순환 include 해결
- [ ] **Cross-reference**: CO_PDO.h에서 `CO_t*` 직접 사용 가능 (void* 제거)
- [ ] **4-phase init**: `CO_Init()` -> `OD setup` -> `PDO_Init()` -> `CO_Start()`
- [ ] **CO_Config_t**: 단일 설정 구조체로 프로젝트별 파라미터 관리
- [ ] **No library modification**: 프로젝트마다 라이브러리 소스 수정 불필요
- [ ] **Bug fixes**: Q5 (RPDO attribute), Q6 (version string size)
- [ ] **Multi-instance safe**: static 지역변수 제거
- [ ] **Usage example**: 새 초기화 흐름 예제 제공
