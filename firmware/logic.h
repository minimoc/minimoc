#pragma once

// -----------------------------------------------------------------
// VARIABLES D'ÉTAT DE L'APPLICATION (MATRICES)
// -----------------------------------------------------------------

// -----------------------------------------------------------------
// PRESETS D'USINE — templates hardcodés, non modifiables
// -----------------------------------------------------------------
struct FactoryPreset { const char* name; uint16_t matrix[5]; };

static const FactoryPreset FACTORY_PRESETS[] = {
    { "VIDE",      {0x000, 0x000, 0x000, 0x000, 0x000} },  // aucun routage
    { "DIAGONALE", {0x001, 0x002, 0x004, 0x008, 0x010} },  // A→1 B→2 C→3 D→4 E→5
    { "MERGE",     {0x001, 0x001, 0x001, 0x001, 0x001} },  // ABCDE → sortie 1
    { "DISPATCH",  {0x1FF, 0x000, 0x000, 0x000, 0x000} },  // A → toutes sorties
    { "TOTAL",     {0x1FF, 0x1FF, 0x1FF, 0x1FF, 0x1FF} },  // ABCDE → toutes sorties
    { "SPLIT",     {0x007, 0x038, 0x000, 0x000, 0x000} },  // A→1,2,3 / B→4,5,6
    { "USB>TRS",   {0x000, 0x000, 0x001, 0x002, 0x004} },  // C→1 D→2 E→3
    { "TRS>USB",   {0x040, 0x080, 0x000, 0x000, 0x000} },  // A→7 B→8
};
#define FACTORY_COUNT 8

// -----------------------------------------------------------------
// SYNC — maître horloge/transport
// -----------------------------------------------------------------
// 0=A 1=B 2=C 3=D 4=E  0xFF=OFF (tout filtré)
uint8_t sync_master = 0xFF;

#define SYNC_EEPROM_ADDR     34   // 1 octet, après le bloc HC_EEPROM (2..33)
#define CONTRAST_EEPROM_ADDR 35   // 1 octet, après SYNC

// EEPROM : 0x7F = OFF (safe pour SysEx), 0-4 = A-E
// RAM    : 0xFF = OFF (valeur interne), 0-4 = A-E
static inline void sync_save() {
    uint8_t v = (sync_master == 0xFF) ? 0x7F : sync_master;
    EEPROM.put(SYNC_EEPROM_ADDR, v);
}
static inline void sync_load() {
    uint8_t v = 0x7F;
    EEPROM.get(SYNC_EEPROM_ADDR, v);
    sync_master = (v <= 4) ? v : 0xFF;  // tout ce qui n'est pas 0-4 → OFF
}

uint8_t screen_contrast = 200;   // valeur par défaut (~78%)

static inline void contrast_save() { EEPROM.put(CONTRAST_EEPROM_ADDR, screen_contrast); }
static inline void contrast_load() {
    uint8_t v = 200;
    EEPROM.get(CONTRAST_EEPROM_ADDR, v);
    screen_contrast = (v > 0) ? v : 200;  // 0x00 = jamais initialisé → défaut
}
static inline void contrast_apply(){ u8g2.setContrast(screen_contrast); }

// USB Host config — partagé entre logic.h, _midi.h et usb_host_config.h
#define HC_NO_SLOT    0xFF   // slot non assigné
#define HC_PORT_COUNT 6      // C, D, E, 7, 8, 9
#define HC_SLOT_COUNT 6      // MIDIUSB1 … MIDIUSB6
// port_slot[0..2] = slot physique pour IN C/D/E
// port_slot[3..5] = slot physique pour OUT 7/8/9
uint8_t port_slot[HC_PORT_COUNT] = {0, 1, 2, 0, 1, 2};
bool    hc_locked = false;

// Matrice de routage : [Input 0-4][Canal In 0-15][Sortie 0-8]
// Inputs  : 0=Port A  1=Port B  2=Hub In 1  3=Hub In 2  4=Hub In 3
// Sorties : 0-5=Physiques 1-6  6=Hub Out 1  7=Hub Out 2  8=Hub Out 3
// Valeur  : bitmask uint16_t — bit i actif = canal (i+1) sur cette sortie
// NE PAS MODIFIER DIRECTEMENT : utiliser basic_matrix/flux_list + recompute_route_matrix()
uint16_t route_matrix[5][16][9] = {0};

// -----------------------------------------------------------------
// ROUTAGE HYBRIDE : BASIC (matrice 5×9) + FLUX (règles canal→canal)
// -----------------------------------------------------------------

// --- BASIC : bit j = sortie j active pour entrée i (passthrough canal) ---
uint16_t basic_matrix[5] = {0};   // 5 entrées × 9 bits de sortie

// --- FLUX avancés ---
struct FluxInSlot  { uint8_t port; uint16_t chan_mask; };
// chan_mask : bitmask canaux (bit 0 = ch1). 0 = tous les canaux.

struct FluxOutSlot { uint8_t port; uint16_t chan_mask; };
// chan_mask : bitmask canaux de sortie (bit 0 = ch1). 0 = passthrough (même canal que l'entrée).

#define FLUX_MAX_IN  5
#define FLUX_MAX_OUT 9
#define FLUX_MAX     16

struct Flux {
    uint8_t     n_in;
    uint8_t     n_out;
    FluxInSlot  in[FLUX_MAX_IN];
    FluxOutSlot out[FLUX_MAX_OUT];
    bool        active;
};

Flux    flux_list[FLUX_MAX];
uint8_t flux_count = 0;

// -----------------------------------------------------------------
// recompute_route_matrix() — reconstruit route_matrix depuis BASIC + FLUX
// À appeler après toute modification de basic_matrix ou flux_list.
// -----------------------------------------------------------------
inline void recompute_route_matrix() {
    memset(route_matrix, 0, sizeof(route_matrix));

    // Couche BASIC : passthrough canal pour chaque nœud actif
    for (uint8_t i = 0; i < 5; i++) {
        for (uint8_t j = 0; j < 9; j++) {
            if (basic_matrix[i] & (1u << j)) {
                for (uint8_t ch = 0; ch < 16; ch++)
                    route_matrix[i][ch][j] |= (uint16_t)(1u << ch);
            }
        }
    }

    // Couche FLUX : règles canal→canal cumulatives
    for (uint8_t f = 0; f < flux_count; f++) {
        if (!flux_list[f].active) continue;
        for (uint8_t ii = 0; ii < flux_list[f].n_in; ii++) {
            uint8_t  ip   = flux_list[f].in[ii].port;
            if (ip >= 5) continue;   // borne : évite accès hors tableau
            uint16_t mask = flux_list[f].in[ii].chan_mask;
            if (mask == 0) mask = 0xFFFF;   // 0 = tous les canaux
            for (uint8_t ch = 0; ch < 16; ch++) {
                if (!(mask & (1u << ch))) continue;
                for (uint8_t oi = 0; oi < flux_list[f].n_out; oi++) {
                    uint8_t  op    = flux_list[f].out[oi].port;
                    uint16_t omask = flux_list[f].out[oi].chan_mask;
                    if (op >= 9) continue;   // borne : évite accès hors tableau
                    if (omask == 0)
                        route_matrix[ip][ch][op] |= (uint16_t)(1u << ch); // passthrough
                    else
                        route_matrix[ip][ch][op] |= omask; // dispatch multi-canal
                }
            }
        }
    }
}

// Appliquer un preset d'usine en RAM (basic_matrix + flux effacés + recompute)
inline void apply_factory(uint8_t idx) {
    if (idx >= FACTORY_COUNT) return;
    memcpy(basic_matrix, FACTORY_PRESETS[idx].matrix, sizeof(basic_matrix));
    flux_count = 0;
    memset(flux_list, 0, sizeof(flux_list));
    recompute_route_matrix();
}
