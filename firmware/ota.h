#pragma once
// OTA via WebSerial (USB CDC) — écriture directe QSPI flash
// Phase 1  : eepromemu_flash_erase_sector / eepromemu_flash_write  (eeprom.c PJRC)
//            → écriture en zone de staging (flash non-exécutée, sûr)
// Phase 2  : _flash_apply() en FASTRUN (ITCM) avec accès FLEXSPI directs
//            → copie staging→actif sans dépendre du code en flash
//
// USB Type IDE : Tools → USB Type → "Serial + MIDI"
//
// LAYOUT QSPI FLASH (Teensy 4.1, 8 MiB W25Q64) :
//   0x60000000 – 0x603FFFFF : firmware actif   (max 4 MiB)
//   0x60400000 – 0x607FFFFF : zone de staging  (4 MiB)
//
// PROTOCOLE :
//   PC → Teensy : "START_UPDATE:{size}\n"
//   Teensy → PC : "ACK\n"
//   PC → Teensy : [256 octets] × N     (1 chunk = 1 page flash)
//   Teensy → PC : "ACK\n"              après chaque page écrite en staging
//   PC → Teensy : "END_UPDATE:{crc32hex}\n"
//   Teensy → PC : "OK\n"  puis copie staging→actif et reset
//              ou "ERR:{raison}\n"

// ── Fonctions PJRC (eeprom.c) ─────────────────────────────────────────────────
extern "C" {
    void eepromemu_flash_write(void *addr, const void *data, uint32_t len);
    void eepromemu_flash_erase_sector(void *addr);
}

// ── Layout flash ──────────────────────────────────────────────────────────────
static const uint32_t OTA_FLASH_BASE   = 0x60000000UL;
static const uint32_t OTA_SECTOR_SIZE  = 4096U;
static const uint32_t OTA_PAGE_SIZE    = 256U;
static const uint32_t OTA_STAGING_BASE = 0x60400000UL;  // offset 4 MiB
static const uint32_t OTA_MAX_SIZE     = 4UL * 1024 * 1024;

// ── Protocole ─────────────────────────────────────────────────────────────────
static const uint32_t OTA_TIMEOUT = 4000; // ms sans octet → abandon

// ── CRC32 IEEE 802.3 (identique à zlib / JavaScript) ─────────────────────────
static uint32_t _crc_lut[256];
static bool     _crc_ready = false;

static void _crc_init() {
    if (_crc_ready) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        _crc_lut[i] = c;
    }
    _crc_ready = true;
}

static uint32_t _crc32(uint32_t crc, const uint8_t *d, size_t n) {
    crc = ~crc;
    for (size_t i = 0; i < n; i++) crc = _crc_lut[(crc ^ d[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

// ── Macros FLEXSPI LUT (idem eeprom.c) ───────────────────────────────────────
#define _OTA_LUT0(op,pads,operand) FLEXSPI_LUT_INSTRUCTION((op),(pads),(operand))
#define _OTA_LUT1(op,pads,operand) (FLEXSPI_LUT_INSTRUCTION((op),(pads),(operand)) << 16)
#define _OTA_CMD   FLEXSPI_LUT_OPCODE_CMD_SDR
#define _OTA_ADDR  FLEXSPI_LUT_OPCODE_RADDR_SDR
#define _OTA_READ  FLEXSPI_LUT_OPCODE_READ_SDR
#define _OTA_WRITE FLEXSPI_LUT_OPCODE_WRITE_SDR
#define _OTA_P1    FLEXSPI_LUT_NUM_PADS_1
#define _OTA_P4    FLEXSPI_LUT_NUM_PADS_4

// ── Attente fin d'opération flash (WIP polling + réactivation XIP) ─────────────
// FASTRUN : s'exécute depuis l'ITCM — pas d'appel en flash externe.
// N'appelle PAS __enable_irq() : les IRQ restent coupées pendant _flash_apply.
FASTRUN static void _ota_wait_wip() {
    // LUT60 = read status register (cmd 0x05, lit 1 octet)
    FLEXSPI_LUTKEY = FLEXSPI_LUTKEY_VALUE;
    FLEXSPI_LUTCR  = FLEXSPI_LUTCR_UNLOCK;
    FLEXSPI_LUT60  = _OTA_LUT0(_OTA_CMD,_OTA_P1,0x05) | _OTA_LUT1(_OTA_READ,_OTA_P1,1);
    FLEXSPI_LUT61  = 0;
    uint8_t status;
    do {
        FLEXSPI_IPRXFCR = FLEXSPI_IPRXFCR_CLRIPRXF;
        FLEXSPI_IPCR0   = 0;
        FLEXSPI_IPCR1   = FLEXSPI_IPCR1_ISEQID(15) | FLEXSPI_IPCR1_IDATSZ(1);
        FLEXSPI_IPCMD   = FLEXSPI_IPCMD_TRG;
        while (!(FLEXSPI_INTR & FLEXSPI_INTR_IPCMDDONE));
        FLEXSPI_INTR    = FLEXSPI_INTR_IPCMDDONE;
        asm("":::"memory");
        status = *(volatile uint8_t *)&FLEXSPI_RFDR0;
    } while (status & 1); // WIP bit
    // SWRESET : purge le buffer AHB et réactive le XIP
    FLEXSPI_MCR0 |= FLEXSPI_MCR0_SWRESET;
    while (FLEXSPI_MCR0 & FLEXSPI_MCR0_SWRESET);
}

// ── Copie staging → firmware actif (FASTRUN = ITCM) ──────────────────────────
// Toutes les opérations FLEXSPI sont inline — aucun appel vers la flash externe.
FASTRUN static void _flash_apply(uint32_t size) {
    __disable_irq();

    static uint8_t _pg[256]; // buffer DTCM — hors flash

    const uint32_t sectors = (size + OTA_SECTOR_SIZE - 1) / OTA_SECTOR_SIZE;
    const uint32_t pages   = (size + OTA_PAGE_SIZE  - 1) / OTA_PAGE_SIZE;

    // ── Étape 1 : effacement de tous les secteurs de la partition active ──────
    for (uint32_t s = 0; s < sectors; s++) {
        uint32_t addr = OTA_FLASH_BASE + s * OTA_SECTOR_SIZE;

        // Write enable (cmd 0x06)
        FLEXSPI_LUTKEY = FLEXSPI_LUTKEY_VALUE;
        FLEXSPI_LUTCR  = FLEXSPI_LUTCR_UNLOCK;
        FLEXSPI_LUT60  = _OTA_LUT0(_OTA_CMD,_OTA_P1,0x06);
        FLEXSPI_LUT61  = 0; FLEXSPI_LUT62 = 0; FLEXSPI_LUT63 = 0;
        FLEXSPI_IPCR0  = 0;
        FLEXSPI_IPCR1  = FLEXSPI_IPCR1_ISEQID(15);
        FLEXSPI_IPCMD  = FLEXSPI_IPCMD_TRG;
        while (!(FLEXSPI_INTR & FLEXSPI_INTR_IPCMDDONE));
        FLEXSPI_INTR   = FLEXSPI_INTR_IPCMDDONE;

        // Sector erase 4K (cmd 0x20)
        FLEXSPI_LUT60  = _OTA_LUT0(_OTA_CMD,_OTA_P1,0x20) | _OTA_LUT1(_OTA_ADDR,_OTA_P1,24);
        FLEXSPI_LUT61  = 0;
        FLEXSPI_IPCR0  = addr & 0x00FFF000;
        FLEXSPI_IPCR1  = FLEXSPI_IPCR1_ISEQID(15);
        FLEXSPI_IPCMD  = FLEXSPI_IPCMD_TRG;
        while (!(FLEXSPI_INTR & FLEXSPI_INTR_IPCMDDONE));
        FLEXSPI_INTR   = FLEXSPI_INTR_IPCMDDONE;

        _ota_wait_wip(); // attend WIP=0, réactive XIP
    }

    // ── Étape 2 : copie page par page depuis staging vers partition active ────
    for (uint32_t p = 0; p < pages; p++) {
        // Lecture depuis staging via XIP (possible après SWRESET de l'étape 1)
        const uint8_t *src = (const uint8_t *)(OTA_STAGING_BASE + p * OTA_PAGE_SIZE);
        for (uint32_t i = 0; i < OTA_PAGE_SIZE; i++) _pg[i] = src[i];

        uint32_t dst = OTA_FLASH_BASE + p * OTA_PAGE_SIZE;

        // Write enable (cmd 0x06)
        FLEXSPI_LUTKEY = FLEXSPI_LUTKEY_VALUE;
        FLEXSPI_LUTCR  = FLEXSPI_LUTCR_UNLOCK;
        FLEXSPI_LUT60  = _OTA_LUT0(_OTA_CMD,_OTA_P1,0x06);
        FLEXSPI_LUT61  = 0; FLEXSPI_LUT62 = 0; FLEXSPI_LUT63 = 0;
        FLEXSPI_IPCR0  = 0;
        FLEXSPI_IPCR1  = FLEXSPI_IPCR1_ISEQID(15);
        FLEXSPI_IPCMD  = FLEXSPI_IPCMD_TRG;
        while (!(FLEXSPI_INTR & FLEXSPI_INTR_IPCMDDONE));
        FLEXSPI_INTR   = FLEXSPI_INTR_IPCMDDONE;

        // Page program quad (cmd 0x32, 256 octets depuis DTCM)
        FLEXSPI_LUT60  = _OTA_LUT0(_OTA_CMD,_OTA_P1,0x32) | _OTA_LUT1(_OTA_ADDR,_OTA_P1,24);
        FLEXSPI_LUT61  = _OTA_LUT0(_OTA_WRITE,_OTA_P4,1);
        FLEXSPI_LUT62  = 0; FLEXSPI_LUT63 = 0;
        FLEXSPI_IPTXFCR = FLEXSPI_IPTXFCR_CLRIPTXF;
        FLEXSPI_IPCR0  = dst & 0x00FFFFFF;
        FLEXSPI_IPCR1  = FLEXSPI_IPCR1_ISEQID(15) | FLEXSPI_IPCR1_IDATSZ(OTA_PAGE_SIZE);
        FLEXSPI_IPCMD  = FLEXSPI_IPCMD_TRG;

        // Alimentation du TX FIFO par blocs de 8 octets (sans memcpy — hors flash)
        uint32_t remaining = OTA_PAGE_SIZE, offset = 0;
        while (!(FLEXSPI_INTR & FLEXSPI_INTR_IPCMDDONE)) {
            if ((FLEXSPI_INTR & FLEXSPI_INTR_IPTXWE) && remaining > 0) {
                uint32_t n  = remaining > 8 ? 8 : remaining;
                uint32_t w0 = 0, w1 = 0;
                for (uint32_t b = 0; b < 4 && b < n; b++)
                    w0 |= (uint32_t)_pg[offset + b] << (b * 8);
                for (uint32_t b = 4; b < n; b++)
                    w1 |= (uint32_t)_pg[offset + b] << ((b - 4) * 8);
                FLEXSPI_TFDR0 = w0;
                FLEXSPI_TFDR1 = w1;
                FLEXSPI_INTR  = FLEXSPI_INTR_IPTXWE;
                offset += n; remaining -= n;
            }
        }
        FLEXSPI_INTR = FLEXSPI_INTR_IPCMDDONE | FLEXSPI_INTR_IPTXWE;

        _ota_wait_wip(); // attend WIP=0, réactive XIP pour la lecture staging suivante
    }

    // Reset — le Teensy boot sur le nouveau firmware en 0x60000000
    SCB_AIRCR = 0x05FA0004; // SYSRESETREQ Cortex-M7
    while (1);
}

// ── Buffer ligne (non-bloquant dans ota_tick) ─────────────────────────────────
static String _ota_line;

// ── Lecture brute bloquante (pendant transfert actif seulement) ───────────────
static bool _read_raw(uint8_t *out, uint32_t n) {
    uint32_t got = 0, t0 = millis();
    while (got < n) {
        if (millis() - t0 > OTA_TIMEOUT) return false;
        int b = Serial.read();
        if (b >= 0) { out[got++] = (uint8_t)b; t0 = millis(); }
    }
    return true;
}

static String _read_line_blocking(uint32_t timeout_ms = 5000) {
    String s;
    uint32_t t0 = millis();
    while (millis() - t0 < timeout_ms) {
        int c = Serial.read();
        if (c < 0) continue;
        if ((char)c == '\n') return s;
        if ((char)c != '\r') s += (char)c;
    }
    return "";
}

// ── Réception + écriture staging via eepromemu ────────────────────────────────
static void _ota_run(uint32_t total) {
    if (total == 0 || total > OTA_MAX_SIZE) { Serial.println("ERR:BAD_SIZE"); return; }

    static uint8_t buf[OTA_PAGE_SIZE];
    uint32_t received    = 0;
    uint32_t running_crc = 0;
    uint32_t last_sector = 0xFFFFFFFF;

    Serial.println("ACK");

    while (received < total) {
        uint32_t chunk = min((uint32_t)OTA_PAGE_SIZE, total - received);
        if (!_read_raw(buf, chunk)) { Serial.println("ERR:TIMEOUT"); return; }

        uint8_t *dst         = (uint8_t *)(OTA_STAGING_BASE + received);
        uint32_t sector_addr = (uint32_t)dst & ~(OTA_SECTOR_SIZE - 1);

        if (sector_addr != last_sector) {
            eepromemu_flash_erase_sector((void *)sector_addr);
            last_sector = sector_addr;
        }
        eepromemu_flash_write(dst, buf, chunk);

        running_crc  = _crc32(running_crc, buf, chunk);
        received    += chunk;
        Serial.println("ACK");
    }

    String end = _read_line_blocking(5000);
    if (!end.startsWith("END_UPDATE:")) { Serial.println("ERR:NO_END"); return; }
    uint32_t expected = (uint32_t)strtoul(end.substring(11).c_str(), nullptr, 16);
    if (running_crc != expected)        { Serial.println("ERR:CRC");    return; }

    Serial.println("OK");
    Serial.flush();
    delay(200);

    _flash_apply(total); // ne revient jamais
    while (1);
}

// ── API publique ──────────────────────────────────────────────────────────────

void ota_setup() {
    Serial.begin(0);   // USB CDC — baud ignoré ; nécessite USB Type "Serial + MIDI"
    _crc_init();
}

// Non-bloquant : appeler dans loop() ; bloque uniquement pendant un transfert actif
void ota_tick() {
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n') {
            String line = _ota_line;
            _ota_line   = "";
            line.trim();
            if (line.startsWith("START_UPDATE:")) {
                uint32_t sz = (uint32_t)strtoul(line.substring(13).c_str(), nullptr, 10);
                _ota_run(sz);
            } else if (line == "PING") {
                Serial.println("PONG:READY");
            }
            return;
        }
        if (c != '\r' && _ota_line.length() < 64) _ota_line += c;
    }
}
