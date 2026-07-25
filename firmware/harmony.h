#pragma once
// harmony.h — Harmonisation d'accords en temps réel (TRANS_CHORD_HARMONIZE)
//
// Principe : au lieu d'un décalage fixe (TRANS_NOTE_TRANSPOSE) ou d'un
// empilement d'intervalles fixes (TRANS_HARMONIZE), ce transform recalcule à
// chaque note un décalage qui dépend de l'accord actuellement actif dans une
// progression (I-V-vi-IV, ii7-V7-Imaj7, ...) et de sa nature (majeur, mineur,
// 7e) — la note reçue est réinterprétée comme un degré de la gamme fondamentale
// puis reprojetée sur la gamme de l'accord courant (Ionien/Éolien/Dorien/
// Mixolydien selon la nature), pas juste transposée dans la gamme d'origine.
//
// Exemple (fondamentale Do, accord Ré majeur) : motif Do-Ré-Fa-Mi-Sol donne
// Ré-Mi-Sol-Fa#-La — le Fa devient Fa# car c'est la tierce de l'accord de Ré
// majeur (Ionien sur Ré), pas un décalage uniforme de +2 demi-tons.
//
// Inclus dans firmware.ino juste après logic.h (a besoin de FluxTransform) et
// avant monitor.h/_midi.h/routing_submenu.h (qui appellent les fonctions ici).

// -----------------------------------------------------------------
// Gammes — demi-tons depuis la tonique/racine, degrés I à VII
// -----------------------------------------------------------------
static const int8_t MAJOR_SCALE_STEPS[7]  = {0,2,4,5,7,9,11};   // gamme de la FONDAMENTALE (Ionien)
static const int8_t HARMONY_IONIAN[7]     = {0,2,4,5,7,9,11};   // accord majeur, maj7
static const int8_t HARMONY_AEOLIAN[7]    = {0,2,3,5,7,8,10};   // accord mineur (triade)
static const int8_t HARMONY_DORIAN[7]     = {0,2,3,5,7,9,10};   // accord mineur 7e
static const int8_t HARMONY_MIXOLYDIAN[7] = {0,2,4,5,7,9,10};   // accord dominante 7e

enum ChordQuality : uint8_t {
    CHORD_MAJOR = 0,
    CHORD_MINOR,
    CHORD_MIN7,
    CHORD_DOM7,
    CHORD_MAJ7,
};

static inline const int8_t* _harmony_quality_scale(uint8_t quality) {
    switch (quality) {
        case CHORD_MINOR: return HARMONY_AEOLIAN;
        case CHORD_MIN7:  return HARMONY_DORIAN;
        case CHORD_DOM7:  return HARMONY_MIXOLYDIAN;
        default:          return HARMONY_IONIAN;   // CHORD_MAJOR, CHORD_MAJ7
    }
}

// -----------------------------------------------------------------
// Progressions prédéfinies — degré (1-7) + nature de chaque accord
// -----------------------------------------------------------------
struct ChordDef { uint8_t degree; uint8_t quality; };   // degree : 1-7 (I..VII)

#define PROGRESSION_MAX_CHORDS 8   // le plus long preset (Canon de Pachelbel) en a 8
#define PROGRESSION_COUNT 10

struct ProgressionPreset {
    const char* name;         // libellé complet (affichage weblink)
    const char* short_label;  // libellé compact, tronqué avec "..." si besoin (écran OLED 128px)
    uint8_t     length;       // nb d'accords actifs
    ChordDef    chords[PROGRESSION_MAX_CHORDS];
};

// NB : les libellés complets (weblink) et une description par progression
// sont dupliqués côté weblink/index.html (PROGRESSION_LABELS/PROGRESSION_DESCRIPTIONS)
// — pas de dépendance runtime entre le firmware et le JS, juste à garder synchro
// si cette liste évolue encore.
static const ProgressionPreset PROGRESSION_PRESETS[PROGRESSION_COUNT] = {
    { "I - V - vi - IV",                  "I-V-vi-IV",         4,
      { {1,CHORD_MAJOR}, {5,CHORD_MAJOR}, {6,CHORD_MINOR}, {4,CHORD_MAJOR} } },
    { "ii7 - V7 - Imaj7",                 "ii7-V7-Imaj7",      3,
      { {2,CHORD_MIN7},  {5,CHORD_DOM7},  {1,CHORD_MAJ7},  {0,CHORD_MAJOR} } },
    { "I - vi - IV - V",                  "I-vi-IV-V",         4,
      { {1,CHORD_MAJOR}, {6,CHORD_MINOR}, {4,CHORD_MAJOR}, {5,CHORD_MAJOR} } },
    { "vi - IV - I - V",                  "vi-IV-I-V",         4,
      { {6,CHORD_MINOR}, {4,CHORD_MAJOR}, {1,CHORD_MAJOR}, {5,CHORD_MAJOR} } },
    { "I - IV - V",                       "I-IV-V",            3,
      { {1,CHORD_MAJOR}, {4,CHORD_MAJOR}, {5,CHORD_MAJOR} } },
    { "I - IV - vi - V",                  "I-IV-vi-V",         4,
      { {1,CHORD_MAJOR}, {4,CHORD_MAJOR}, {6,CHORD_MINOR}, {5,CHORD_MAJOR} } },
    { "vi7 - ii7 - V7 - Imaj7",           "vi7-ii7-V7-Imaj7",  4,
      { {6,CHORD_MIN7},  {2,CHORD_MIN7},  {5,CHORD_DOM7},  {1,CHORD_MAJ7} } },
    { "Imaj7 - vi7 - ii7 - V7",           "Imaj7-vi7-ii7-V7",  4,
      { {1,CHORD_MAJ7},  {6,CHORD_MIN7},  {2,CHORD_MIN7},  {5,CHORD_DOM7} } },
    { "I7 - IV7 - V7",                    "I7-IV7-V7",         3,
      { {1,CHORD_DOM7},  {4,CHORD_DOM7},  {5,CHORD_DOM7} } },
    { "I - V - vi - iii - IV - I - IV - V", "I-V-vi-iii-IV-...", 8,
      { {1,CHORD_MAJOR}, {5,CHORD_MAJOR}, {6,CHORD_MINOR}, {3,CHORD_MINOR},
        {4,CHORD_MAJOR}, {1,CHORD_MAJOR}, {4,CHORD_MAJOR}, {5,CHORD_MAJOR} } },
};

// -----------------------------------------------------------------
// Toniques — libellés français (Do=0 .. Si=11)
// -----------------------------------------------------------------
static const char* ROOT_KEY_LABELS[12] = {
    "Do","Do#","Ré","Ré#","Mi","Fa","Fa#","Sol","Sol#","La","La#","Si"
};

#define CHORD_BARS_MIN     1
#define CHORD_BARS_MAX     8
#define CHORD_BARS_DEFAULT 1

// -----------------------------------------------------------------
// Compteur de mesure global — avance sur l'horloge MIDI (temps réel ou
// interne), remis à zéro sur Start/Continue. Un seul compteur partagé par
// tous les Flux TRANS_CHORD_HARMONIZE : chacun dérive son accord courant
// statelessement à partir de ce compteur + de sa propre config
// (progression_id, bars_per_chord) — aucun état par-Flux à maintenir.
// -----------------------------------------------------------------
static uint32_t harmony_bar_ctr         = 0;   // mesures écoulées depuis Start/Continue
static uint8_t  harmony_beat_in_bar_ctr = 0;   // 0-3, position dans la mesure 4/4 courante

// Appelé depuis bpm_push_clock() (monitor.h) à chaque temps complet (24 ticks
// MIDI Clock). Ne touche que 2 entiers → sûr à appeler depuis l'ISR de
// l'horloge interne (internal_clock_isr() dans _midi.h).
inline void harmony_on_beat() {
    if (++harmony_beat_in_bar_ctr >= 4) {
        harmony_beat_in_bar_ctr = 0;
        harmony_bar_ctr++;
    }
}

// Appelé depuis bpm_reset_beat() (monitor.h), sur Start/Continue.
inline void harmony_on_transport_reset() {
    harmony_bar_ctr         = 0;
    harmony_beat_in_bar_ctr = 0;
}

// Accord actif pour un FluxTransform donné, dérivé du compteur de mesure global.
static inline ChordDef harmony_current_chord(const FluxTransform& tr) {
    uint8_t prog = (tr.progression_id < PROGRESSION_COUNT) ? tr.progression_id : 0;
    uint8_t plen = PROGRESSION_PRESETS[prog].length;
    uint8_t bpc  = (tr.bars_per_chord >= CHORD_BARS_MIN) ? tr.bars_per_chord : CHORD_BARS_DEFAULT;   // garde-fou div/0
    uint32_t idx = (harmony_bar_ctr / bpc) % plen;
    return PROGRESSION_PRESETS[prog].chords[idx];
}

// -----------------------------------------------------------------
// Calcul du décalage (demi-tons) à appliquer à note_in pour l'accord donné.
//
// pc_offset       : classe de hauteur de note_in relative à root_key (0-11)
// degree_idx      : degré de la gamme fondamentale le plus proche (0-6)
// chroma_residual : résidu si note_in n'est pas exactement sur un degré diatonique
//
// Le terme d'octave s'annule exactement entre note d'entrée et de sortie
// (les deux s'écrivent root_key + 12*octave + ...) : tout se résume à ce seul
// décalage, sans jamais calculer l'octave séparément.
// -----------------------------------------------------------------
static inline int8_t harmony_compute_offset(uint8_t root_key, ChordDef chord, byte note_in) {
    int16_t pc_offset = ((int16_t)note_in - (int16_t)root_key) % 12;
    if (pc_offset < 0) pc_offset += 12;

    uint8_t degree_idx = 0;
    for (int8_t d = 6; d >= 0; d--) {
        if (MAJOR_SCALE_STEPS[d] <= pc_offset) { degree_idx = d; break; }
    }
    int8_t chroma_residual = (int8_t)pc_offset - MAJOR_SCALE_STEPS[degree_idx];

    uint8_t deg = (chord.degree >= 1 && chord.degree <= 7) ? chord.degree : 1;
    int8_t chord_root_pc_offset = MAJOR_SCALE_STEPS[deg - 1];
    const int8_t* target_scale  = _harmony_quality_scale(chord.quality);

    int16_t offset = (int16_t)chord_root_pc_offset + target_scale[degree_idx] + chroma_residual - pc_offset;
    return (int8_t)offset;
}

// -----------------------------------------------------------------
// Table des notes tenues — nécessaire pour que NoteOff réutilise exactement
// le décalage appliqué au NoteOn correspondant (l'accord courant peut avoir
// changé entre-temps si la note est tenue à cheval sur un changement
// d'accord ; cf. l'invariant documenté dans _midi.h juste avant
// _send_offset_note_on/off).
// -----------------------------------------------------------------
#define HARMONY_HELD_MAX 48

struct HarmonyHeldNote {
    bool    active;
    uint8_t out;
    uint8_t dc;
    uint8_t note_in;
    int8_t  offset;
};
static HarmonyHeldNote harmony_held[HARMONY_HELD_MAX] = {};

// À appeler après un rechargement massif de config (nouveau preset, factory
// reset) pour éviter que des slots consommés par une ancienne config de Flux
// restent occupés indéfiniment. Hygiène, pas critique pour la correction.
static inline void harmony_reset_held_notes() {
    memset(harmony_held, 0, sizeof(harmony_held));
}

// NoteOn : calcule le décalage pour l'accord courant, l'enregistre dans un
// slot libre (clé out+dc+note_in) pour le NoteOff correspondant, le renvoie.
static inline int8_t harmony_note_on_offset(const FluxTransform& tr, byte out, byte dc, byte note) {
    ChordDef chord = harmony_current_chord(tr);
    int8_t offset  = harmony_compute_offset(tr.root_key, chord, note);
    for (uint8_t i = 0; i < HARMONY_HELD_MAX; i++) {
        if (!harmony_held[i].active) {
            harmony_held[i] = { true, out, dc, note, offset };
            break;   // table pleine → note quand même jouée, juste pas trackée (fallback au NoteOff)
        }
    }
    return offset;
}

// NoteOff : retrouve le décalage utilisé au NoteOn correspondant (out+dc+note_in)
// et le libère. Si introuvable (table pleine / état incohérent), recalcule
// avec l'accord ACTUEL en best-effort — imparfait mais mieux qu'aucun NoteOff.
static inline int8_t harmony_note_off_offset(const FluxTransform& tr, byte out, byte dc, byte note) {
    for (uint8_t i = 0; i < HARMONY_HELD_MAX; i++) {
        if (harmony_held[i].active && harmony_held[i].out == out &&
            harmony_held[i].dc == dc && harmony_held[i].note_in == note) {
            harmony_held[i].active = false;
            return harmony_held[i].offset;
        }
    }
    return harmony_compute_offset(tr.root_key, harmony_current_chord(tr), note);
}
