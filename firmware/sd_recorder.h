#pragma once
#include <SdFat.h>
#include <EEPROM.h>

// ----------------------------------------------------------------
// SD RECORDER - Étape 2 : enregistrement MIDI
//
// Architecture : ring buffer RAM → écritures SD différées
//
// rec_push()  : appelé depuis _midi.h à chaque événement routé
//               → copie 9 octets dans le ring buffer (~50 ns)
//               → ne bloque JAMAIS le flux MIDI
//
// sd_tick()   : appelé dans loop()
//               → écrit sur SD quand >= REC_WRITE_MIN octets dispos
//               → sync tous les REC_SYNC_BYTES octets (pas à chaque write)
//
// Format fichier : événements de 10 octets
//   [0-3] timestamp micros() (uint32_t little-endian)
//   [4]   sortie (1-7)
//   [5]   source (0=PC/DAW, 1=Entrée A, 2=Entrée B, 3=USB Host)
//   [6]   type MIDI (ex: 0x90=NoteOn, 0xB0=CC…)
//   [7]   canal destination (1-16)
//   [8]   data1 (note / numéro CC / …)
//   [9]   data2 (vélocité / valeur CC / … / 0 si absent)
//
// Session counter : EEPROM adresse 1 (uint8_t, 1-99, wrap)
// Fichiers        : REC001.DAT … REC099.DAT
// ----------------------------------------------------------------

#define REC_BUF_SIZE     4096   // ring buffer (doit être une puissance de 2)
#define REC_EVENT_SIZE   10     // octets par événement
#define REC_WRITE_MIN    280    // écrire quand >= 28 événements dispo (280 octets)
#define REC_SYNC_BYTES   4000   // sync tous les ~4 Ko (400 événements)
#define REC_PREALLOCATE  (20UL * 1024 * 1024)  // préallocation : 20 Mo (~30h de MIDI dense)

#define REC_SESSION_EEPROM_ADDR  1   // adresse EEPROM du compteur de session

static SdFs   sd;
static FsFile sdRecFile;

// Ring buffer
static uint8_t  rec_buf[REC_BUF_SIZE];
static uint16_t rec_head = 0;
static uint16_t rec_tail = 0;

// Stats exposées au menu Info
bool     sd_ok             = false;
bool     rec_active        = false;
uint32_t sd_write_count    = 0;
uint32_t sd_max_us         = 0;
uint32_t sd_spike_count    = 0;
uint32_t sd_bytes_written  = 0;
uint32_t sd_overflow_count = 0;
uint8_t  rec_session       = 0;    // numéro de session en cours

// ----------------------------------------------------------------
// rec_push — à appeler dans _midi.h à chaque événement routé
// Temps d'exécution : ~50 ns, ne bloque jamais
// source : 0=PC/DAW  1=Entrée A  2=Entrée B  3=USB Host
// ----------------------------------------------------------------
inline void rec_push(uint8_t source, uint8_t output, uint8_t type, uint8_t ch, uint8_t d1, uint8_t d2) {
    mon_push(source, output, d2);   // toujours actif, indépendant de l'enregistrement SD

    if (!rec_active) return;

    uint16_t used = (rec_head - rec_tail) & (REC_BUF_SIZE - 1);
    if ((REC_BUF_SIZE - 1 - used) < REC_EVENT_SIZE) {
        sd_overflow_count++;
        return;
    }

    uint32_t t = micros();
    uint16_t h = rec_head;

    rec_buf[(h    ) & (REC_BUF_SIZE - 1)] = (uint8_t)(t);
    rec_buf[(h + 1) & (REC_BUF_SIZE - 1)] = (uint8_t)(t >> 8);
    rec_buf[(h + 2) & (REC_BUF_SIZE - 1)] = (uint8_t)(t >> 16);
    rec_buf[(h + 3) & (REC_BUF_SIZE - 1)] = (uint8_t)(t >> 24);
    rec_buf[(h + 4) & (REC_BUF_SIZE - 1)] = output;
    rec_buf[(h + 5) & (REC_BUF_SIZE - 1)] = source;
    rec_buf[(h + 6) & (REC_BUF_SIZE - 1)] = type;
    rec_buf[(h + 7) & (REC_BUF_SIZE - 1)] = ch;
    rec_buf[(h + 8) & (REC_BUF_SIZE - 1)] = d1;
    rec_buf[(h + 9) & (REC_BUF_SIZE - 1)] = d2;

    rec_head = (h + REC_EVENT_SIZE) & (REC_BUF_SIZE - 1);

}

// ----------------------------------------------------------------
// sd_setup — init SD + ouverture de session
// ----------------------------------------------------------------
void sd_setup() {
    if (!sd.begin(SdioConfig(FIFO_SDIO))) return;

    // Numéro de session : lire EEPROM, incrémenter, sauvegarder
    uint8_t session = 0;
    EEPROM.get(REC_SESSION_EEPROM_ADDR, session);
    session++;
    if (session > 99) session = 1;
    EEPROM.put(REC_SESSION_EEPROM_ADDR, session);
    rec_session = session;

    // Nom de fichier : REC001.DAT … REC099.DAT
    char fname[16];
    snprintf(fname, sizeof(fname), "REC%03u.DAT", session);

    if (!sdRecFile.open(fname, O_RDWR | O_CREAT | O_TRUNC)) return;
    if (!sdRecFile.preAllocate(REC_PREALLOCATE)) {
        sdRecFile.close();
        return;
    }

    sd_ok      = true;
    rec_active = true;
}

// ----------------------------------------------------------------
// sd_tick — drainer le ring buffer vers la SD
// À appeler dans loop(), sans contrainte de fréquence
// ----------------------------------------------------------------
void sd_tick() {
    if (!sd_ok || !rec_active) return;

    uint16_t avail = (rec_head - rec_tail) & (REC_BUF_SIZE - 1);
    if (avail < REC_WRITE_MIN) return;

    // Nombre d'événements complets à écrire (max 56 = 504 octets)
    uint16_t events = avail / REC_EVENT_SIZE;
    if (events > 56) events = 56;
    uint16_t nb = events * REC_EVENT_SIZE;

    // Copie ring buffer → buffer linéaire
    static uint8_t wbuf[56 * REC_EVENT_SIZE];
    for (uint16_t i = 0; i < nb; i++) {
        wbuf[i]   = rec_buf[rec_tail];
        rec_tail  = (rec_tail + 1) & (REC_BUF_SIZE - 1);
    }

    uint32_t t0 = micros();
    sdRecFile.write(wbuf, nb);
    uint32_t elapsed = micros() - t0;

    sd_write_count++;
    sd_bytes_written += nb;
    if (elapsed > sd_max_us) sd_max_us = elapsed;
    if (elapsed > 3000)      sd_spike_count++;

    // Sync tous les REC_SYNC_BYTES octets (pas à chaque write)
    static uint32_t bytes_since_sync = 0;
    bytes_since_sync += nb;
    if (bytes_since_sync >= REC_SYNC_BYTES) {
        sdRecFile.sync();
        bytes_since_sync = 0;
    }
}

// ----------------------------------------------------------------
// sd_close — vider le ring buffer et fermer proprement
// ----------------------------------------------------------------
void sd_close() {
    if (!sd_ok) return;
    rec_active = false;

    // Écrire les événements restants dans le ring buffer
    uint16_t avail = (rec_head - rec_tail) & (REC_BUF_SIZE - 1);
    while (avail >= REC_EVENT_SIZE) {
        uint8_t evt[REC_EVENT_SIZE];
        for (int i = 0; i < REC_EVENT_SIZE; i++) {
            evt[i]   = rec_buf[rec_tail];
            rec_tail = (rec_tail + 1) & (REC_BUF_SIZE - 1);
        }
        sdRecFile.write(evt, REC_EVENT_SIZE);
        avail -= REC_EVENT_SIZE;
    }

    sdRecFile.sync();
    sdRecFile.truncate();
    sdRecFile.close();
    sd_ok = false;
}
