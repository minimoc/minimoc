#pragma once
#include <math.h>   // sinf() — utilisé par lfo_waveform_value() (LFO_SINE)

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

// true = transport en lecture (générateurs actifs) ; false = en pause/stop
// (générateurs gelés). Mis à jour uniquement par _sync_forward() (_midi.h),
// seul point de passage commun à toutes les sources de Play/Stop (écran
// TRANSPORT, éditeur web opcode 0x11, horloge externe maître SYNC). Défaut
// à true : un utilisateur qui ne touche jamais au transport garde le
// comportement actuel (générateurs toujours actifs).
bool transport_running = true;

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

// Étiquettes des 5 entrées / 9 sorties — topologie fixe, réutilisées par
// routing_submenu.h ET generators_submenu.h (ce dernier est inclus avant
// routing_submenu.h dans firmware.ino, d'où leur présence ici plutôt que
// dans l'un des deux sous-menus).
static const char* INPUT_LABELS[]  = { "A","B","C","D","E" };
static const char* OUTPUT_LABELS[] = { "1","2","3","4","5","6","7","8","9" };

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

// -----------------------------------------------------------------
// GÉNÉRATEURS — LFO (CC continu) et séquenceur euclidien (Note ON/OFF)
// -----------------------------------------------------------------
// Contrairement à un Flux, un générateur n'a pas d'entrée : il émet un flux
// MIDI (CC continu pour un LFO, Note ON/OFF sur pas actifs pour un
// séquenceur euclidien) indépendamment de tout message entrant. Le calcul
// de phase/pas (ISR-safe, pas d'I/O MIDI) est séparé de l'envoi effectif
// (nécessite _midi.h, disponible seulement plus tard dans firmware.ino) —
// cf. generators_on_clock_tick() ici, generators_send_cc()/
// generators_send_note_on()/off() dans _midi.h et generators_tick() dans
// generators.h. Les deux types partagent sync_mode/rate_x10hz/division_idx
// (cadence des cycles pour le LFO, des pas pour l'euclidien) et out[].

#define GEN_MAX 4

enum GeneratorType : uint8_t { GEN_LFO, GEN_EUCLID };
#define GEN_TYPE_COUNT 2

enum LfoWaveform : uint8_t { LFO_TRIANGLE, LFO_SINE, LFO_SQUARE, LFO_SAW, LFO_RANDOM };
#define LFO_WAVEFORM_COUNT 5

enum LfoSyncMode : uint8_t { LFO_SYNC_FREE, LFO_SYNC_CLOCK };

// division = nb de pulses d'horloge MIDI (24 ppq) par cycle de LFO / par pas euclidien
struct LfoDivision { const char* label; uint16_t ticks; };
static const LfoDivision LFO_DIVISIONS[] = {
    {"1/16", 6}, {"1/8", 12}, {"1/4", 24}, {"1/2", 48},
    {"1",    96}, {"2", 192}, {"4",   384},
};
#define LFO_DIVISION_COUNT 7

#define LFO_RATE_X10HZ_MIN     1u    // 0.1 Hz
#define LFO_RATE_X10HZ_MAX     500u  // 50.0 Hz
#define LFO_RATE_X10HZ_DEFAULT 10u   // 1.0 Hz

#define EUCLID_STEPS_MAX 16
#define EUCLID_VELOCITY  100  // vélocité fixe des Note ON euclidiens — pas de champ UI dédié dans cette itération

struct Generator {
    uint8_t     type;           // GeneratorType
    bool        active;
    uint8_t     waveform;       // LfoWaveform — LFO uniquement
    uint8_t     sync_mode;      // LfoSyncMode — partagé
    uint16_t    rate_x10hz;     // partagé : Hz (LFO) ou pas/seconde (EUCLID), mode libre, 0.1 par unité
    uint8_t     division_idx;   // partagé : index dans LFO_DIVISIONS, mode synchro
    uint8_t     depth;          // LFO uniquement : 0-63, amplitude autour du centre
    uint8_t     center;         // LFO uniquement : 0-127, valeur centrale
    uint8_t     cc_number;      // LFO uniquement : 0-127
    uint8_t     note;           // EUCLID uniquement : note MIDI fixe, 0-127
    uint8_t     euclid_steps;   // EUCLID uniquement : nb de pas, 1-EUCLID_STEPS_MAX
    uint8_t     euclid_pulses;  // EUCLID uniquement : nb de hits, 0-euclid_steps
    uint8_t     euclid_rotation;// EUCLID uniquement : rotation du motif, 0-(euclid_steps-1)
    uint8_t     gate_percent;   // EUCLID uniquement : durée du gate en % du pas, 1-100
    uint8_t     n_out;
    FluxOutSlot out[FLUX_MAX_OUT];   // réutilise FluxOutSlot — chan_mask ne doit
                                      // JAMAIS être 0 ici (pas de canal source à
                                      // "passthrough" : 0 canaux = rien envoyé).
};
Generator gen_list[GEN_MAX];
uint8_t   gen_count = 0;

// État d'exécution — jamais persisté (cf. sd_presets.h), reconstruit à chaud.
struct GeneratorRuntime {
    uint32_t phase_acc;        // mode libre : accumulateur de phase 0..65535
    uint16_t clock_ticks;      // mode synchro : compteur de pulses dans le cycle/pas courant
    uint8_t  last_sent_value;  // LFO : dernière valeur CC 0-127 réellement envoyée
    bool     pending;          // LFO : valeur prête à être envoyée par generators_tick()
    uint8_t  pending_value;
    uint16_t random_held;      // LFO_RANDOM : valeur échantillonnée-bloquée courante
    bool     random_was_high;  // LFO_RANDOM : détection de flanc de cycle (par générateur)
    // EUCLID
    uint8_t  euclid_step;      // pas courant, 0..euclid_steps-1
    bool     gate_active;      // une note est actuellement "on", en attente de son off
    uint8_t  active_note;      // note envoyée par le Note ON en cours (pour un Note OFF cohérent)
    uint8_t  off_note;         // note à éteindre lors du prochain flush (pending_note_off)
    int32_t  gate_off_ticks;   // mode synchro : pulses d'horloge restants avant Note OFF
    uint32_t gate_off_ms;      // mode libre : timestamp millis() du Note OFF
    bool     pending_note_on;  // Note ON prête à être envoyée par generators_tick() (note = active_note)
    bool     pending_note_off; // Note OFF prête à être envoyée par generators_tick() (note = off_note)
};
GeneratorRuntime gen_rt[GEN_MAX];

// Calcule la valeur 0..65535 d'une forme d'onde pour une phase 0..65535.
// `rt` porte l'état propre à CE générateur pour LFO_RANDOM (échantillonné-
// bloqué) — sans lui, plusieurs générateurs en LFO_RANDOM partageraient le
// même flanc/valeur. Pas d'I/O MIDI ici, safe à appeler depuis un contexte ISR.
inline uint16_t lfo_waveform_value(uint8_t waveform, uint16_t phase, GeneratorRuntime &rt) {
    switch (waveform) {
        case LFO_TRIANGLE:
            return (phase < 32768u) ? (uint16_t)(phase * 2u)
                                     : (uint16_t)((65535u - phase) * 2u);
        case LFO_SINE: {
            float rad = (float)phase * (2.0f * PI / 65536.0f);
            return (uint16_t)((sinf(rad) * 0.5f + 0.5f) * 65535.0f);
        }
        case LFO_SQUARE:
            return (phase < 32768u) ? 65535u : 0u;
        case LFO_SAW:
            return phase;
        case LFO_RANDOM: {
            // Échantillonné-bloqué : une nouvelle valeur aléatoire à chaque
            // flanc de cycle (phase revenue proche de 0), tenue le reste du cycle.
            bool wrapped = rt.random_was_high && phase < 4096u;
            rt.random_was_high = phase > 61440u;
            if (wrapped) rt.random_held = (uint16_t)random(0, 65536);
            return rt.random_held;
        }
        default:
            return 0;
    }
}

// Mappe une valeur brute de forme d'onde (0..65535, cf. lfo_waveform_value)
// sur un CC 0-127 bipolaire autour de `center`, d'amplitude `depth` (0-63).
// Partagée par le mode synchro (ci-dessous) et le mode libre (generators.h).
inline uint8_t lfo_output_value(uint16_t raw, uint8_t center, uint8_t depth) {
    int32_t cc = (int32_t)center + ((int32_t)raw - 32768) * (int32_t)depth / 32768;
    if (cc < 0)   cc = 0;
    if (cc > 127) cc = 127;
    return (uint8_t)cc;
}

// Calcule le motif euclidien (bit i = pas i actif) : répartit `pulses` hits
// le plus uniformément possible sur `steps` pas (accumulation type
// Bresenham, approximation usuelle de Bjorklund), puis applique une
// rotation. steps 1-EUCLID_STEPS_MAX, pulses 0-steps. Pure — aucune
// dépendance MIDI, appelée depuis un contexte ISR (generators_on_clock_tick).
inline uint16_t euclid_pattern(uint8_t steps, uint8_t pulses, uint8_t rotation) {
    if (steps == 0 || pulses == 0) return 0;
    if (pulses >= steps) return (uint16_t)((1u << steps) - 1);   // tous les pas actifs
    uint16_t pattern = 0;
    uint32_t acc = 0;
    for (uint8_t i = 0; i < steps; i++) {
        acc += pulses;
        if (acc >= steps) {
            acc -= steps;
            pattern |= (uint16_t)(1u << i);
        }
    }
    rotation %= steps;
    if (rotation == 0) return pattern;
    uint16_t mask = (uint16_t)((1u << steps) - 1);
    return (uint16_t)(((pattern >> rotation) | (pattern << (steps - rotation))) & mask);
}

// Avance le cycle/pas des générateurs synchronisés d'un pulse d'horloge MIDI
// (24 ppq). Appelée depuis bpm_push_clock() (monitor.h) — ISR-safe, aucune
// écriture MIDI ici (cf. generators_send_cc()/generators_send_note_on()/off()
// dans _midi.h pour l'envoi réel, fait par generators_tick() dans generators.h).
inline void generators_on_clock_tick() {
    // L'horloge interne continue d'appeler cette fonction même à l'arrêt
    // (elle ne connaît pas Start/Stop, cf. internal_clock_isr() dans
    // _midi.h) — sans cette garde un générateur en mode Synchro continuerait
    // d'avancer pendant une pause.
    if (!transport_running) return;
    for (uint8_t i = 0; i < gen_count; i++) {
        Generator &g = gen_list[i];
        if (!g.active || g.sync_mode != LFO_SYNC_CLOCK) continue;
        GeneratorRuntime &rt = gen_rt[i];
        uint16_t total = LFO_DIVISIONS[g.division_idx].ticks;

        if (g.type == GEN_LFO) {
            rt.clock_ticks++;
            if (rt.clock_ticks >= total) rt.clock_ticks = 0;
            uint16_t phase = (uint32_t)rt.clock_ticks * 65536u / total;
            uint16_t raw   = lfo_waveform_value(g.waveform, phase, rt);
            uint8_t  cc    = lfo_output_value(raw, g.center, g.depth);
            if (cc != rt.last_sent_value) {
                rt.pending       = true;
                rt.pending_value = cc;
            }
            continue;
        }

        // GEN_EUCLID
        // 1) Fermeture du gate en cours si son décompte est écoulé.
        if (rt.gate_active && rt.gate_off_ticks > 0) {
            rt.gate_off_ticks--;
            if (rt.gate_off_ticks == 0) {
                rt.off_note         = rt.active_note;
                rt.pending_note_off = true;
                rt.gate_active       = false;
            }
        }
        // 2) Avance du pas ; au wrap, évalue le motif et arme un Note ON si actif.
        rt.clock_ticks++;
        if (rt.clock_ticks >= total) {
            rt.clock_ticks = 0;
            uint16_t pattern = euclid_pattern(g.euclid_steps, g.euclid_pulses, g.euclid_rotation);
            bool hit = g.euclid_steps > 0 && ((pattern >> rt.euclid_step) & 1u);
            if (hit) {
                if (rt.gate_active) {   // gate encore ouvert (gate_percent proche de 100%) : couper avant le retrigger
                    rt.off_note         = rt.active_note;
                    rt.pending_note_off = true;
                }
                rt.active_note     = g.note;
                rt.pending_note_on = true;
                rt.gate_active      = true;
                uint16_t gate_ticks = (uint16_t)((uint32_t)total * g.gate_percent / 100u);
                rt.gate_off_ticks   = (gate_ticks == 0) ? 1 : gate_ticks;
            }
            if (g.euclid_steps > 0) {
                rt.euclid_step++;
                if (rt.euclid_step >= g.euclid_steps) rt.euclid_step = 0;
            }
        }
    }
}

// Réaligne tous les générateurs synchronisés sur le prochain temps fort
// (Start/Continue) — appelée depuis bpm_reset_beat() (monitor.h). Pour un
// séquenceur euclidien, coupe aussi proprement une note encore ouverte
// plutôt que de la laisser bloquée (pending_note_off, flushé normalement
// par generators_tick() — pas d'I/O MIDI directe ici, cf. commentaire ISR
// plus haut).
inline void generators_on_transport_reset() {
    for (uint8_t i = 0; i < gen_count; i++) {
        if (gen_list[i].sync_mode != LFO_SYNC_CLOCK) continue;
        GeneratorRuntime &rt = gen_rt[i];
        rt.clock_ticks = 0;
        if (gen_list[i].type == GEN_EUCLID) {
            rt.euclid_step = 0;
            if (rt.gate_active) {
                rt.off_note         = rt.active_note;
                rt.pending_note_off = true;
                rt.gate_active       = false;
            }
        }
    }
}

// Ferme immédiatement toute note euclidienne encore ouverte à l'arrêt du
// transport (STOP/PAUSE, cf. _sync_forward() dans _midi.h) — pas d'I/O MIDI
// ici, juste les flags pending_note_off/off_note ; l'envoi réel reste fait
// par generators_tick() (generators.h), comme partout ailleurs dans ce moteur.
inline void generators_on_transport_stop() {
    for (uint8_t i = 0; i < gen_count; i++) {
        if (gen_list[i].type != GEN_EUCLID) continue;
        GeneratorRuntime &rt = gen_rt[i];
        if (rt.gate_active) {
            rt.off_note         = rt.active_note;
            rt.pending_note_off = true;
            rt.gate_active       = false;
        }
    }
}

// Reset complet de l'état d'exécution — à appeler après chargement de preset
// ou factory reset (cf. preset_submenu.h), au même titre que
// harmony_reset_held_notes(). NB : si un séquenceur euclidien avait une note
// encore ouverte (gate_active) à cet instant précis, elle ne reçoit pas de
// Note OFF explicite ici (pas d'accès à generators_send_note_off(), défini
// plus tard dans _midi.h) — fenêtre étroite en pratique (gates courts), non
// traitée dans cette itération.
inline void generators_reset_runtime() {
    memset(gen_rt, 0, sizeof(gen_rt));
}

// Appliquer un preset d'usine en RAM (basic_matrix + flux + générateurs
// effacés + recompute).
// NB : harmony_reset_held_notes() (harmony.h) n'est PAS appelé ici — logic.h
// est inclus avant harmony.h dans firmware.ino (harmony.h a besoin de
// FluxTransform). Les appelants doivent appeler harmony_reset_held_notes()
// après apply_factory() — cf. preset_submenu.h. generators_reset_runtime()
// n'a pas cette contrainte (défini dans ce même fichier) : appelé ici directement.
inline void apply_factory(uint8_t idx) {
    if (idx >= FACTORY_COUNT) return;
    memcpy(basic_matrix, FACTORY_PRESETS[idx].matrix, sizeof(basic_matrix));
    flux_count = 0;
    memset(flux_list, 0, sizeof(flux_list));
    gen_count = 0;
    memset(gen_list, 0, sizeof(gen_list));
    generators_reset_runtime();
    recompute_route_matrix();
}
