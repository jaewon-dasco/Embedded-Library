# oCANopen Library API Reference

## Core Types

### CO_t - CANopen Stack Object
```c
typedef struct{
    CO_CANmodule_t     CANmodule;
    CO_SDO_t           SDO;
    CO_EM_t            em;
    CO_EMpr_t          emPr;
    CO_NMT_t           NMT;
    CO_SYNC_t          SYNC;
    CO_RPDO_t          RPDO[CO_NO_RPDO];   // max 8
    CO_TPDO_t          TPDO[CO_NO_TPDO];   // max 8
    CO_OD_t            OD;
    CO_SDOclient_t     SDOclient;
    OCTET_STRING       errorStatusBits[10];
    uint32_t           Timestamp_us;
    uint32_t           Timestamp_ms;
    CO_CommunicationObejct_t CommunicationObejct;
}CO_t;
```

### CO_OD_EntryItem_t - OD Entry
```c
typedef struct {
    uint16_t           index;
    uint8_t            maxSubIndex;
    uint16_t           attribute;      // CO_ODA_* flags
    uint16_t           length;
    void              *pData;
    void              *pArgument;      // CO_OD_Record_t* for records
    CO_OD_extension_t  extention;
}CO_OD_EntryItem_t;
```

### CO_OD_Record_t - Record Sub-entry
```c
typedef struct{
    uint16_t attribute;   // CO_ODA_* flags
    uint16_t length;      // bytes
}CO_OD_Record_t;
```

### CO_ODF_arg_t - SDO Callback Argument
```c
typedef struct{
    void     *object;
    uint8_t  *data;
    const void *ODdataStorage;
    uint16_t dataLength;
    uint16_t attribute;
    uint8_t  *pFlags;
    uint16_t index;
    uint8_t  subIndex;
    uint8_t  reading;
    uint8_t  firstSegment;
    uint8_t  lastSegment;
    uint32_t dataLengthTotal;
}CO_ODF_arg_t;
```

---

## Attribute Flags (CO_ODA_*)

```c
CO_ODA_MEM_ROM              = 0x0001   // ROM (constant)
CO_ODA_MEM_RAM              = 0x0002   // RAM (volatile)
CO_ODA_MEM_EEPROM           = 0x0003   // EEPROM (non-volatile)
CO_ODA_READABLE             = 0x0004   // SDO read
CO_ODA_WRITEABLE            = 0x0008   // SDO write
CO_ODA_READ_AND_WRITEABLE   = 0x000C   // SDO read/write
CO_ODA_RPDO_MAPABLE         = 0x0010   // RPDO mappable
CO_ODA_TPDO_MAPABLE         = 0x0020   // TPDO mappable
CO_ODA_TPDO_DETECT_COS      = 0x0040   // Auto-send on change
```

**Combination examples:**
- `0x0F` = EEPROM + R/W (for Node ID, Bitrate)
- `0x8F` = EEPROM + R/W + ??? (legacy compatibility)
- `CO_ODA_MEM_RAM|CO_ODA_READABLE|CO_ODA_TPDO_MAPABLE` = RAM + Read + TPDO map

---

## Initializer Macros

### CANOPEN_INITIALIZER
```c
CO_t CO = CANOPEN_INITIALIZER(ODentry);
```

### CO_OD_VARIABLE_INITIALIZER - Variable Entry
```c
CO_OD_VARIABLE_INITIALIZER(Index, MaxSubIndex, Attribute, Length, DataPtr)
```
**Example:**
```c
CO_OD_VARIABLE_INITIALIZER(0x2101, 0, 0x0F, 1, &EEPROM.NodeId)
```

### CO_OD_RECODE_INITIALIZER - Record Entry
```c
CO_OD_RECODE_INITIALIZER(Index, MaxSubIndex, RecordPtr, DataPtr)
```
**Example:**
```c
static CO_OD_Record_t Record[] = {
    {CO_ODA_MEM_EEPROM|CO_ODA_READ_AND_WRITEABLE, 2},  // sub1: int16
    {CO_ODA_MEM_EEPROM|CO_ODA_READ_AND_WRITEABLE, 1},  // sub2: uint8
};
CO_OD_RECODE_INITIALIZER(0x3000, ArrayLen(Record), &Record, &EEPROM.Data)
```

### CO_OBJECTMAP_INITIALIZER - PDO Mapping
```c
CO_OBJECTMAP_INITIALIZER(Index, SubIndex, BitLength)
// = (Index << 16) | (SubIndex << 8) | BitLength
```
**Example:**
```c
Map[0] = CO_OBJECTMAP_INITIALIZER(0x9000, 1, 16);  // 0x90000110
```

---

## Core Functions

### CO_init - Initialize Stack
```c
CO_ReturnError_t CO_init(
    CO_t *CO,
    CAN_HandleTypeDef *CANHandle,
    CO_OD_Callback_t ParameterSaveCallback
);
// Returns: CO_ERROR_NO on success
```

### CO_process - Main Processing Loop
```c
CO_NMT_reset_cmd_t CO_process(CO_t *CO);
// Returns: CO_RESET_NOT(0), CO_RESET_COMM(1), CO_RESET_APP(2)
```

### CO_TPDO_init - Configure Transmit PDO
```c
CO_ReturnError_t CO_TPDO_init(
    void *pCANopen,         // CO_t*
    uint32_t COBID,         // 0 = auto (0x180+nodeID, 0x280+, ...)
    uint8_t Transmission,   // 255=async, 0-240=sync
    uint16_t InhibitTime,   // x100us min interval
    uint16_t EventTime,     // ms periodic
    uint32_t *pMapping,     // CO_OBJECTMAP_INITIALIZER array
    uint8_t SizeOfMapping   // number of mapped objects
);
```

### CO_RPDO_init - Configure Receive PDO
```c
CO_ReturnError_t CO_RPDO_init(
    void *pCANopen,         // CO_t*
    uint32_t COBID,         // 0 = auto (0x200+nodeID, 0x300+, ...)
    uint16_t EventTime,     // ms
    uint32_t *pMapping,     // CO_OBJECTMAP_INITIALIZER array
    uint8_t SizeOfMapping   // number of mapped objects
);
```

---

## Standard OD Indices (Built-in)

| Index  | Description |
|--------|-------------|
| 0x1000 | Device Type |
| 0x1001 | Error Register |
| 0x1005 | COB-ID SYNC |
| 0x100A | SW Version (`CO.CommunicationObejct.ManufacturerSoftwareVersion`) |
| 0x1010 | Store Parameters (save callback trigger) |
| 0x1011 | Restore Parameters (load callback trigger) |
| 0x1014 | COB-ID Emergency |
| 0x1017 | Heartbeat Time |
| 0x1018 | Identity Object |
| 0x1200 | SDO Server |
| 0x1400+ | RPDO Comm Parameters |
| 0x1600+ | RPDO Mapping |
| 0x1800+ | TPDO Comm Parameters |
| 0x1A00+ | TPDO Mapping |

## User-defined OD Indices (Mi_CANopen.c)

| Index  | Description |
|--------|-------------|
| 0x2101 | Node ID |
| 0x2102 | Bitrate |
| 0x3000+ | Manufacturer Sensor Parameters |
| 0x6000+ | Device Profile (CiA-410 etc.) |
| 0x9000 | PDO Data Buffer |

---

## Usage Pattern

```c
// 1. Define Records
static CO_OD_Record_t Record_PDO[] = {
    {CO_ODA_MEM_RAM|CO_ODA_READABLE|CO_ODA_TPDO_MAPABLE, 2},
    {CO_ODA_MEM_RAM|CO_ODA_READABLE|CO_ODA_TPDO_MAPABLE, 2},
};

// 2. Define OD Entries
CO_OD_EntryItem_t ODentry[] = {
    CO_OD_VARIABLE_INITIALIZER(0x2101, 0, 0x0F, 1, &EEPROM.NodeId),
    CO_OD_VARIABLE_INITIALIZER(0x2102, 0, 0x8F, 2, &EEPROM.Bitrate),
    CO_OD_RECODE_INITIALIZER(0x9000, ArrayLen(Record_PDO), &Record_PDO, &PDO),
};

// 3. Create CO object
CO_t CO = CANOPEN_INITIALIZER(ODentry);

// 4. Initialize
CO_init(&CO, &hcan1, SaveCallback);

// 5. Configure TPDO
uint32_t Map[2];
Map[0] = CO_OBJECTMAP_INITIALIZER(0x9000, 1, 16);
Map[1] = CO_OBJECTMAP_INITIALIZER(0x9000, 2, 16);
CO_TPDO_init(&CO, 0, 255, 100, 200, Map, 2);

// 6. Cyclic process
while(1) {
    CO_process(&CO);  // returns CO_RESET_NOT normally
}
```

---

## Save/Load Callback

```c
int SaveCallback(void *arg)
{
    CO_ODF_arg_t *ODF_arg = (CO_ODF_arg_t *)arg;

    switch(*((uint32_t *)ODF_arg->data))
    {
        case 0x65766173: // "save" (0x1010)
            NativeFlash_Write((uint8_t *)&EEPROM, sizeof(EEPROM));
            break;
        case 0x64616F6C: // "load" (0x1011)
            LoadDefault(&EEPROM);
            NativeFlash_Write((uint8_t *)&EEPROM, sizeof(EEPROM));
            break;
        default:
            return 1; // abort
    }
    return 0;
}
```

---

## File Locations

```
oCANopen/
  Inc/  CANopen.h, CO_Common.h, CO_Driver.h, CO_OD.h, CO_PDO.h, CO_SDO.h, ...
  Src/  CANopen.c, CO_Common.c, CO_Driver.c, CO_OD.c, CO_PDO.c, CO_SDO.c, ...
  doc/  CANopen.md (this file)
```
