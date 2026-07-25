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
// Chaque lettre A-E a deux sources physiques distinctes (TRS/USB-Host d'un
// côté, câble USB miroir venant du PC de l'autre — ex. le smartmirror). On
// les distingue pour éviter qu'un maître "A" écoute les deux à la fois :
//   0=A(TRS)  1=B(TRS)  2=C(USB Host)  3=D(USB Host)  4=E(USB Host)
//   5=A(USB/PC)  6=B(USB/PC)  7=C(USB/PC)  8=D(USB/PC)  9=E(USB/PC)
//   10=MINIMOC (horloge interne, tempo réglable — menu TRANSPORT)
//   0xFF=OFF (tout filtré)
uint8_t sync_master = 0xFF;

static const char* SYNC_LABELS[] = {
    "A (TRS)", "B (TRS)", "C (USB Host)", "D (USB Host)", "E (USB Host)",
    "A (USB/PC)", "B (USB/PC)", "C (USB/PC)", "D (USB/PC)", "E (USB/PC)",
    "MINIMOC"
};
#define SYNC_SOURCE_COUNT 11
#define SYNC_MASTER_INTERNAL 10   // MiniMoc = horloge interne (pas un port physique)

#define SYNC_EEPROM_ADDR     34   // 1 octet, après le bloc HC_EEPROM (2..33)
#define CONTRAST_EEPROM_ADDR 35   // 1 octet, après SYNC
#define BPM_REFRESH_EEPROM_ADDR 36   // 1 octet, après CONTRAST
#define INTERNAL_BPM_EEPROM_ADDR 37  // 2 octets (uint16_t), après BPM_REFRESH

// EEPROM : 0x7F = OFF (safe pour SysEx), 0-10 = cf. SYNC_LABELS
// RAM    : 0xFF = OFF (valeur interne), 0-10 = cf. SYNC_LABELS
static inline void sync_save() {
    uint8_t v = (sync_master == 0xFF) ? 0x7F : sync_master;
    EEPROM.put(SYNC_EEPROM_ADDR, v);
}
static inline void sync_load() {
    uint8_t v = 0x7F;
    EEPROM.get(SYNC_EEPROM_ADDR, v);
    sync_master = (v < SYNC_SOURCE_COUNT) ? v : 0xFF;  // hors plage → OFF
}

// Tempo de l'horloge interne (source MINIMOC) — réglable depuis le menu
// TRANSPORT (voir transport_submenu.h), consommé par internal_clock_apply()/
// internal_clock_isr() dans _midi.h (génération par IntervalTimer).
#define INTERNAL_CLOCK_BPM_MIN     20u
#define INTERNAL_CLOCK_BPM_MAX     300u
#define INTERNAL_CLOCK_BPM_DEFAULT 100u

uint16_t internal_clock_bpm = INTERNAL_CLOCK_BPM_DEFAULT;

static inline void internal_clock_bpm_save() {
    EEPROM.put(INTERNAL_BPM_EEPROM_ADDR, internal_clock_bpm);
}
static inline void internal_clock_bpm_load() {
    uint16_t v = 0;
    EEPROM.get(INTERNAL_BPM_EEPROM_ADDR, v);
    internal_clock_bpm = (v >= INTERNAL_CLOCK_BPM_MIN && v <= INTERNAL_CLOCK_BPM_MAX)
                          ? v : INTERNAL_CLOCK_BPM_DEFAULT;
}

uint8_t screen_contrast = 200;   // valeur par défaut (~78%)

static inline void contrast_save() { EEPROM.put(CONTRAST_EEPROM_ADDR, screen_contrast); }
static inline void contrast_load() {
    uint8_t v = 200;
    EEPROM.get(CONTRAST_EEPROM_ADDR, v);
    screen_contrast = (v > 0) ? v : 200;  // 0x00 = jamais initialisé → défaut
}
static inline void contrast_apply(){ u8g2.setContrast(screen_contrast); }

// Intervalles disponibles pour le rafraîchissement auto de la page BPM (ms).
// Index 0 = MANUEL (aucun rafraîchissement auto ; clic requis).
static const uint16_t BPM_REFRESH_OPTIONS[] = { 0, 250, 500, 1000, 2000, 5000 };
#define BPM_REFRESH_OPTION_COUNT 6
#define BPM_REFRESH_DEFAULT_IDX  3   // 1000 ms = comportement actuel

uint8_t bpm_refresh_idx = BPM_REFRESH_DEFAULT_IDX;

static inline void bpm_refresh_save() { EEPROM.put(BPM_REFRESH_EEPROM_ADDR, bpm_refresh_idx); }
static inline void bpm_refresh_load() {
    uint8_t v = 0xFF;
    EEPROM.get(BPM_REFRESH_EEPROM_ADDR, v);
    bpm_refresh_idx = (v < BPM_REFRESH_OPTION_COUNT) ? v : BPM_REFRESH_DEFAULT_IDX;
}

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

// -----------------------------------------------------------------
// TRANSFORM — traitement optionnel attaché à un Flux
// -----------------------------------------------------------------
// Un Flux porte AU PLUS un type de transform (pas de cumul) : TRANS_NONE,
// TRANS_NOTE_TRANSPOSE (décalage de note), TRANS_HARMONIZE (empile jusqu'à
// 4 notes supplémentaires par rapport à la note reçue) ou TRANS_CHORD_HARMONIZE
// (retranspose la note reçue pour coller à l'accord courant d'une progression
// — cf. harmony.h pour l'algorithme).
enum TransformType : uint8_t {
    TRANS_NONE = 0,
    TRANS_NOTE_TRANSPOSE,
    TRANS_HARMONIZE,
    TRANS_CHORD_HARMONIZE,
};

#define FLUX_TRANSPOSE_MIN -24
#define FLUX_TRANSPOSE_MAX  24
#define HARMONIZE_MAX_INTERVALS 4

struct FluxTransform {
    uint8_t type;                                  // TransformType
    int8_t  transpose;                              // TRANS_NOTE_TRANSPOSE : demi-tons, -24..+24
    uint8_t n_intervals;                            // TRANS_HARMONIZE : nb d'intervalles actifs (0-4)
    int8_t  intervals[HARMONIZE_MAX_INTERVALS];     // TRANS_HARMONIZE : demi-tons relatifs à la note reçue
    // TRANS_CHORD_HARMONIZE (champs ajoutés en fin de struct — append-only,
    // cf. migration v5→v6 dans sd_presets.h) :
    uint8_t root_key;         // tonalité fondamentale, 0-11 (0=Do, cf. ROOT_KEY_LABELS dans harmony.h)
    uint8_t progression_id;   // index dans PROGRESSION_PRESETS (harmony.h), 0-3
    uint8_t bars_per_chord;   // nb de mesures 4/4 par accord de la progression, 1-8
};

struct Flux {
    uint8_t        n_in;
    uint8_t        n_out;
    FluxInSlot     in[FLUX_MAX_IN];
    FluxOutSlot    out[FLUX_MAX_OUT];
    bool           active;
    FluxTransform  transform;   // {TRANS_NONE,0,0,{0,0,0,0}} = pas de transform
};

Flux    flux_list[FLUX_MAX];
uint8_t flux_count = 0;

// Transform à appliquer aux notes routées via [in][chan][out][chan_out].
// Granularité au canal de sortie (pas juste [in][chan][out]) : indispensable
// pour qu'un même Flux multi-canal (ex. A.1 → 3.1 T+0 et A.1 → 3.2 T+3, via
// un chan_mask de sortie à 2 bits) applique un transform différent par canal
// de sortie.
// Alimenté uniquement par la couche FLUX (la matrice BASIC reste un pur
// passthrough). Deux Flux actifs qui ciblent exactement le même quadruplet
// (in, chan_in, out, chan_out) restent en collision — route_matrix ne peut
// représenter qu'un seul message par canal de sortie, donc le dernier Flux
// écrit gagne.
FluxTransform route_transform[5][16][9][16] = {};

// -----------------------------------------------------------------
// recompute_route_matrix() — reconstruit route_matrix depuis BASIC + FLUX
// À appeler après toute modification de basic_matrix ou flux_list.
// -----------------------------------------------------------------
inline void recompute_route_matrix() {
    memset(route_matrix, 0, sizeof(route_matrix));
    memset(route_transform, 0, sizeof(route_transform));

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
                    if (omask == 0) {
                        route_matrix[ip][ch][op] |= (uint16_t)(1u << ch); // passthrough
                        route_transform[ip][ch][op][ch] = flux_list[f].transform;
                    } else {
                        route_matrix[ip][ch][op] |= omask; // dispatch multi-canal
                        for (uint16_t om = omask; om; om &= om-1) {
                            uint8_t outch = __builtin_ctz(om);
                            route_transform[ip][ch][op][outch] = flux_list[f].transform;
                        }
                    }
                }
            }
        }
    }
}

// Appliquer un preset d'usine en RAM (basic_matrix + flux effacés + recompute)
// NB : harmony_reset_held_notes() (harmony.h) n'est PAS appelé ici — logic.h
// est inclus avant harmony.h dans firmware.ino (harmony.h a besoin de
// FluxTransform). Les appelants doivent appeler harmony_reset_held_notes()
// après apply_factory() — cf. preset_submenu.h.
inline void apply_factory(uint8_t idx) {
    if (idx >= FACTORY_COUNT) return;
    memcpy(basic_matrix, FACTORY_PRESETS[idx].matrix, sizeof(basic_matrix));
    flux_count = 0;
    memset(flux_list, 0, sizeof(flux_list));
    recompute_route_matrix();
}
