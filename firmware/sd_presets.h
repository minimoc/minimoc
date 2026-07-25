#pragma once
// sd_presets.h — Presets sur carte SD
//
// Format v9 /PRESETS/PRESTnn.DAT :
//   [0-7]   SdPresetHeader (magic, version=9, n_inputs, n_outputs, reserved)
//   [8+]    route_matrix  (n_in × 16 × n_out × 2 octets) — DÉRIVÉ, reconstruit au chargement
//   [...]   basic_matrix[5]   (10 octets)
//   [1 oct] flux_count
//   [...]   flux_list[0..flux_count-1] (flux_count × sizeof(Flux), inclut transform)
//   [1 oct] gen_count
//   [...]   gen_list[0..gen_count-1] (gen_count × sizeof(Generator), générateurs LFO/euclidien/motif)
//
// Format v8 (legacy) : gen_list au layout d'avant l'ajout du type PATTERN
// (pas de champ `pattern_id`) → migration automatique, `pattern_id=0` non
// utilisé puisque `type` reste LFO/EUCLID pour les presets migrés (voir
// GeneratorV8Legacy plus bas).
// Format v7 (legacy) : gen_list au layout d'avant l'ajout du type EUCLID
// (pas de champ `type`, pas de note/euclid_steps/euclid_pulses/
// euclid_rotation/gate_percent) → migration automatique vers `type=GEN_LFO`
// (voir GeneratorV7Legacy plus bas).
// Format v6 (legacy) : identique à v7 mais sans gen_count/gen_list (absents
// du fichier) → gen_count mis à 0 au chargement, pas de struct "legacy" à
// prévoir (section neuve, pas un champ modifié).
// Format v5 (legacy) : flux_list avec FluxTransform sans les champs accord
// (root_key/progression_id/bars_per_chord) → migration automatique (voir
// FluxV5Legacy plus bas).
// Format v4 (legacy) : flux_list avec `transpose` brut au lieu de `transform`
// (union TransformType) → migration automatique (voir FluxV4Legacy plus bas).
// Format v3 (legacy) : flux_list sans transform du tout → migration automatique
// (voir FluxV3Legacy plus bas).
// Format v1 (legacy) : route_matrix seul → migration automatique vers basic_matrix

#define SD_PRESET_MAX   32
#define SD_PRESET_DIR   "/PRESETS"
#define SD_PRESET_MAGIC 0x4D4E4D43UL   // 'MNMC' little-endian
#define SD_PRESET_VER   9              // v1=route_matrix, v2=+basic+flux(chan mono), v3=flux(chan_mask multi), v4=+flux.transpose, v5=+flux.transform (TransformType), v6=+flux.transform accords (TRANS_CHORD_HARMONIZE), v7=+gen_count/gen_list (générateurs LFO), v8=+Generator.type (séquenceur euclidien), v9=+Generator.pattern_id (motifs de batterie)

// Layout figé de `struct Flux` tel qu'écrit par le firmware v3, avant l'ajout
// d'un transform. Sert uniquement à relire les presets SD existants —
// ne pas modifier même si `Flux` évolue encore par la suite.
struct FluxV3Legacy {
    uint8_t     n_in;
    uint8_t     n_out;
    FluxInSlot  in[FLUX_MAX_IN];
    FluxOutSlot out[FLUX_MAX_OUT];
    bool        active;
};

// Layout figé de `struct Flux` tel qu'écrit par le firmware v4, avant la
// généralisation du champ `transpose` en `transform` (TransformType + union
// de paramètres). Sert uniquement à relire les presets SD existants —
// ne pas modifier même si `Flux` évolue encore par la suite.
struct FluxV4Legacy {
    uint8_t     n_in;
    uint8_t     n_out;
    FluxInSlot  in[FLUX_MAX_IN];
    FluxOutSlot out[FLUX_MAX_OUT];
    bool        active;
    int8_t      transpose;
};

// Layout figé de FluxTransform tel qu'écrit par le firmware v5, avant l'ajout
// de TRANS_CHORD_HARMONIZE (root_key/progression_id/bars_per_chord). Sert
// uniquement à relire les presets SD existants — ne pas modifier même si
// FluxTransform évolue encore par la suite.
struct FluxTransformV5Legacy {
    uint8_t type;
    int8_t  transpose;
    uint8_t n_intervals;
    int8_t  intervals[HARMONIZE_MAX_INTERVALS];
};

struct FluxV5Legacy {
    uint8_t                n_in;
    uint8_t                n_out;
    FluxInSlot              in[FLUX_MAX_IN];
    FluxOutSlot             out[FLUX_MAX_OUT];
    bool                    active;
    FluxTransformV5Legacy   transform;
};

// Layout figé de `struct Generator` tel qu'écrit par le firmware v7, avant
// l'ajout du type EUCLID (`type`, `note`, `euclid_steps`, `euclid_pulses`,
// `euclid_rotation`, `gate_percent`) — à l'époque, un seul type de
// générateur (LFO) existait donc pas de champ `type`. Sert uniquement à
// relire les presets SD existants — ne pas modifier même si `Generator`
// évolue encore par la suite.
struct GeneratorV7Legacy {
    bool        active;
    uint8_t     waveform;
    uint8_t     sync_mode;
    uint16_t    rate_x10hz;
    uint8_t     division_idx;
    uint8_t     depth;
    uint8_t     center;
    uint8_t     cc_number;
    uint8_t     n_out;
    FluxOutSlot out[FLUX_MAX_OUT];
};

// Layout figé de `struct Generator` tel qu'écrit par le firmware v8, avant
// l'ajout du type PATTERN (`pattern_id`). Sert uniquement à relire les
// presets SD existants — ne pas modifier même si `Generator` évolue encore.
struct GeneratorV8Legacy {
    uint8_t     type;
    bool        active;
    uint8_t     waveform;
    uint8_t     sync_mode;
    uint16_t    rate_x10hz;
    uint8_t     division_idx;
    uint8_t     depth;
    uint8_t     center;
    uint8_t     cc_number;
    uint8_t     note;
    uint8_t     euclid_steps;
    uint8_t     euclid_pulses;
    uint8_t     euclid_rotation;
    uint8_t     gate_percent;
    uint8_t     n_out;
    FluxOutSlot out[FLUX_MAX_OUT];
};

struct SdPresetHeader {
    uint32_t magic;
    uint8_t  version;
    uint8_t  n_inputs;
    uint8_t  n_outputs;
    uint8_t  reserved;
};

// ----------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------
static void _preset_path(uint8_t num, char* buf, size_t len) {
    snprintf(buf, len, SD_PRESET_DIR "/PRST%02u.DAT", num);
}
static uint8_t _n_inputs()  { return (uint8_t)(sizeof(route_matrix) / sizeof(route_matrix[0])); }
static uint8_t _n_outputs() { return (uint8_t)(sizeof(route_matrix[0][0]) / sizeof(route_matrix[0][0][0])); }

// ----------------------------------------------------------------
// Migration v1 : tente de déduire basic_matrix depuis route_matrix
// Une connexion port i→sortie j est "basique" si TOUS les canaux
// ont un passthrough identitaire (ch→ch).
// ----------------------------------------------------------------
static void _migrate_route_to_basic() {
    memset(basic_matrix, 0, sizeof(basic_matrix));
    flux_count = 0;
    for (uint8_t i = 0; i < 5; i++) {
        for (uint8_t j = 0; j < 9; j++) {
            bool passthrough = false;
            for (uint8_t ch = 0; ch < 16; ch++) {
                if (route_matrix[i][ch][j] & (1u << ch)) { passthrough = true; break; }
            }
            if (passthrough) basic_matrix[i] |= (uint16_t)(1u << j);
        }
    }
    // Ne pas appeler recompute_route_matrix() ici : route_matrix déjà chargée
}

// ----------------------------------------------------------------
// sd_preset_init
// ----------------------------------------------------------------
void sd_preset_init() {
    if (!sd_ok) return;
    if (!sd.exists(SD_PRESET_DIR)) sd.mkdir(SD_PRESET_DIR);
}

// ----------------------------------------------------------------
// sd_preset_exists
// ----------------------------------------------------------------
bool sd_preset_exists(uint8_t num) {
    if (!sd_ok || num < 1 || num > SD_PRESET_MAX) return false;
    char path[28];
    _preset_path(num, path, sizeof(path));
    return sd.exists(path);
}

// ----------------------------------------------------------------
// sd_preset_save — v2
// ----------------------------------------------------------------
bool sd_preset_save(uint8_t num) {
    if (!sd_ok || num < 1 || num > SD_PRESET_MAX) return false;

    char path[28];
    _preset_path(num, path, sizeof(path));

    FsFile f = sd.open(path, O_RDWR | O_CREAT | O_TRUNC);
    if (!f) return false;

    SdPresetHeader hdr = { SD_PRESET_MAGIC, SD_PRESET_VER, _n_inputs(), _n_outputs(), 0 };
    f.write(&hdr,              sizeof(hdr));
    f.write(route_matrix,      sizeof(route_matrix));
    f.write(basic_matrix,      sizeof(basic_matrix));
    f.write(&flux_count,       1);
    if (flux_count > 0)
        f.write(flux_list, flux_count * sizeof(Flux));
    f.write(&gen_count,        1);
    if (gen_count > 0)
        f.write(gen_list, gen_count * sizeof(Generator));
    f.sync();
    f.close();
    return true;
}

// ----------------------------------------------------------------
// sd_preset_load — v2 avec migration v1
// ----------------------------------------------------------------
bool sd_preset_load(uint8_t num) {
    if (!sd_ok || num < 1 || num > SD_PRESET_MAX) return false;

    char path[28];
    _preset_path(num, path, sizeof(path));

    FsFile f = sd.open(path, O_RDONLY);
    if (!f) return false;

    SdPresetHeader hdr;
    if ((size_t)f.read(&hdr, sizeof(hdr)) != sizeof(hdr) ||
        hdr.magic != SD_PRESET_MAGIC ||
        (hdr.version != 1 && hdr.version != 3 && hdr.version != 4 && hdr.version != 5 &&
         hdr.version != 6 && hdr.version != 7 && hdr.version != 8 && hdr.version != 9)) {
        f.close();
        return false;
    }

    // Générateurs : absents des formats < v7 → toujours réinitialisés avant
    // la branche de version, puis remplis uniquement par la branche v7.
    gen_count = 0;
    memset(gen_list, 0, sizeof(gen_list));

    uint8_t cur_in  = _n_inputs();
    uint8_t cur_out = _n_outputs();

    // Charger route_matrix (avec migration de taille si nécessaire)
    if (hdr.n_inputs == cur_in && hdr.n_outputs == cur_out) {
            f.read(route_matrix, sizeof(route_matrix));
    } else {
        memset(route_matrix, 0, sizeof(route_matrix));
        uint8_t ni = min(hdr.n_inputs,  cur_in);
        uint8_t no = min(hdr.n_outputs, cur_out);
        for (uint8_t p = 0; p < hdr.n_inputs; p++) {
            for (uint8_t c = 0; c < 16; c++) {
                for (uint8_t o = 0; o < hdr.n_outputs; o++) {
                    uint16_t val = 0; f.read(&val, 2);
                    if (p < ni && o < no) route_matrix[p][c][o] = val;
                }
            }
        }
    }
    if (hdr.version == 1) {
        // Migration v1 : déduire basic_matrix depuis route_matrix, pas de flux
        _migrate_route_to_basic();
    } else if (hdr.version == 3) {
        // v3 : basic_matrix + flux SANS transform → migration vers Flux courant
        f.read(basic_matrix, sizeof(basic_matrix));
        uint8_t fc = 0;
        f.read(&fc, 1);
        flux_count = min(fc, (uint8_t)FLUX_MAX);
        if (flux_count > 0) {
            FluxV3Legacy legacy[FLUX_MAX];
            f.read(legacy, flux_count * sizeof(FluxV3Legacy));
            for (uint8_t i = 0; i < flux_count; i++) {
                flux_list[i].n_in  = legacy[i].n_in;
                flux_list[i].n_out = legacy[i].n_out;
                memcpy(flux_list[i].in,  legacy[i].in,  sizeof(legacy[i].in));
                memcpy(flux_list[i].out, legacy[i].out, sizeof(legacy[i].out));
                flux_list[i].active = legacy[i].active;
                memset(&flux_list[i].transform, 0, sizeof(FluxTransform));   // TRANS_NONE
            }
        }
        recompute_route_matrix();
    } else if (hdr.version == 4) {
        // v4 : basic_matrix + flux avec `transpose` brut → migration vers `transform`
        f.read(basic_matrix, sizeof(basic_matrix));
        uint8_t fc = 0;
        f.read(&fc, 1);
        flux_count = min(fc, (uint8_t)FLUX_MAX);
        if (flux_count > 0) {
            FluxV4Legacy legacy[FLUX_MAX];
            f.read(legacy, flux_count * sizeof(FluxV4Legacy));
            for (uint8_t i = 0; i < flux_count; i++) {
                flux_list[i].n_in  = legacy[i].n_in;
                flux_list[i].n_out = legacy[i].n_out;
                memcpy(flux_list[i].in,  legacy[i].in,  sizeof(legacy[i].in));
                memcpy(flux_list[i].out, legacy[i].out, sizeof(legacy[i].out));
                flux_list[i].active = legacy[i].active;
                memset(&flux_list[i].transform, 0, sizeof(FluxTransform));
                flux_list[i].transform.type      = (legacy[i].transpose != 0) ? TRANS_NOTE_TRANSPOSE : TRANS_NONE;
                flux_list[i].transform.transpose = legacy[i].transpose;
            }
        }
        recompute_route_matrix();
    } else if (hdr.version == 5) {
        // v5 : basic_matrix + flux avec FluxTransform sans les champs accord
        // → migration vers `transform` avec défauts sûrs (root=Do, prog=0, 1 mesure)
        f.read(basic_matrix, sizeof(basic_matrix));
        uint8_t fc = 0;
        f.read(&fc, 1);
        flux_count = min(fc, (uint8_t)FLUX_MAX);
        if (flux_count > 0) {
            FluxV5Legacy legacy[FLUX_MAX];
            f.read(legacy, flux_count * sizeof(FluxV5Legacy));
            for (uint8_t i = 0; i < flux_count; i++) {
                flux_list[i].n_in  = legacy[i].n_in;
                flux_list[i].n_out = legacy[i].n_out;
                memcpy(flux_list[i].in,  legacy[i].in,  sizeof(legacy[i].in));
                memcpy(flux_list[i].out, legacy[i].out, sizeof(legacy[i].out));
                flux_list[i].active = legacy[i].active;
                memset(&flux_list[i].transform, 0, sizeof(FluxTransform));
                flux_list[i].transform.type        = legacy[i].transform.type;
                flux_list[i].transform.transpose   = legacy[i].transform.transpose;
                flux_list[i].transform.n_intervals = legacy[i].transform.n_intervals;
                memcpy(flux_list[i].transform.intervals, legacy[i].transform.intervals,
                       sizeof(legacy[i].transform.intervals));
                flux_list[i].transform.root_key       = 0;
                flux_list[i].transform.progression_id = 0;
                flux_list[i].transform.bars_per_chord = CHORD_BARS_DEFAULT;
            }
        }
        recompute_route_matrix();
    } else if (hdr.version == 6) {   // v6 : basic_matrix + flux (avec transform incluant les champs accord)
        f.read(basic_matrix, sizeof(basic_matrix));
        uint8_t fc = 0;
        f.read(&fc, 1);
        flux_count = min(fc, (uint8_t)FLUX_MAX);
        if (flux_count > 0)
            f.read(flux_list, flux_count * sizeof(Flux));
        recompute_route_matrix();
    } else if (hdr.version == 7) {
        // v7 : basic_matrix + flux + gen_count/gen_list, gen_list au layout
        // d'avant le type EUCLID → migration vers `type=GEN_LFO` + défauts
        // euclidiens sûrs (non utilisés puisque type reste LFO).
        f.read(basic_matrix, sizeof(basic_matrix));
        uint8_t fc = 0;
        f.read(&fc, 1);
        flux_count = min(fc, (uint8_t)FLUX_MAX);
        if (flux_count > 0)
            f.read(flux_list, flux_count * sizeof(Flux));
        uint8_t gc = 0;
        f.read(&gc, 1);
        gen_count = min(gc, (uint8_t)GEN_MAX);
        if (gen_count > 0) {
            GeneratorV7Legacy legacy[GEN_MAX];
            f.read(legacy, gen_count * sizeof(GeneratorV7Legacy));
            for (uint8_t i = 0; i < gen_count; i++) {
                memset(&gen_list[i], 0, sizeof(Generator));
                gen_list[i].type         = GEN_LFO;
                gen_list[i].active       = legacy[i].active;
                gen_list[i].waveform     = legacy[i].waveform;
                gen_list[i].sync_mode    = legacy[i].sync_mode;
                gen_list[i].rate_x10hz   = legacy[i].rate_x10hz;
                gen_list[i].division_idx = legacy[i].division_idx;
                gen_list[i].depth        = legacy[i].depth;
                gen_list[i].center       = legacy[i].center;
                gen_list[i].cc_number    = legacy[i].cc_number;
                gen_list[i].note            = 60;
                gen_list[i].euclid_steps    = 8;
                gen_list[i].euclid_pulses   = 3;
                gen_list[i].euclid_rotation = 0;
                gen_list[i].gate_percent    = 50;
                gen_list[i].n_out = legacy[i].n_out;
                memcpy(gen_list[i].out, legacy[i].out, sizeof(legacy[i].out));
            }
        }
        recompute_route_matrix();
    } else if (hdr.version == 8) {
        // v8 : basic_matrix + flux + gen_count/gen_list, gen_list au layout
        // d'avant le type PATTERN → migration avec pattern_id=0 (non utilisé
        // puisque type reste LFO/EUCLID pour les presets migrés).
        f.read(basic_matrix, sizeof(basic_matrix));
        uint8_t fc = 0;
        f.read(&fc, 1);
        flux_count = min(fc, (uint8_t)FLUX_MAX);
        if (flux_count > 0)
            f.read(flux_list, flux_count * sizeof(Flux));
        uint8_t gc = 0;
        f.read(&gc, 1);
        gen_count = min(gc, (uint8_t)GEN_MAX);
        if (gen_count > 0) {
            GeneratorV8Legacy legacy[GEN_MAX];
            f.read(legacy, gen_count * sizeof(GeneratorV8Legacy));
            for (uint8_t i = 0; i < gen_count; i++) {
                memset(&gen_list[i], 0, sizeof(Generator));
                gen_list[i].type            = legacy[i].type;
                gen_list[i].active          = legacy[i].active;
                gen_list[i].waveform        = legacy[i].waveform;
                gen_list[i].sync_mode       = legacy[i].sync_mode;
                gen_list[i].rate_x10hz      = legacy[i].rate_x10hz;
                gen_list[i].division_idx    = legacy[i].division_idx;
                gen_list[i].depth           = legacy[i].depth;
                gen_list[i].center          = legacy[i].center;
                gen_list[i].cc_number       = legacy[i].cc_number;
                gen_list[i].note            = legacy[i].note;
                gen_list[i].euclid_steps    = legacy[i].euclid_steps;
                gen_list[i].euclid_pulses   = legacy[i].euclid_pulses;
                gen_list[i].euclid_rotation = legacy[i].euclid_rotation;
                gen_list[i].gate_percent    = legacy[i].gate_percent;
                gen_list[i].pattern_id      = 0;
                gen_list[i].n_out = legacy[i].n_out;
                memcpy(gen_list[i].out, legacy[i].out, sizeof(legacy[i].out));
            }
        }
        recompute_route_matrix();
    } else {   // v9 : basic_matrix + flux + gen_count/gen_list (LFO + euclidien + motif)
        f.read(basic_matrix, sizeof(basic_matrix));
        uint8_t fc = 0;
        f.read(&fc, 1);
        flux_count = min(fc, (uint8_t)FLUX_MAX);
        if (flux_count > 0)
            f.read(flux_list, flux_count * sizeof(Flux));
        uint8_t gc = 0;
        f.read(&gc, 1);
        gen_count = min(gc, (uint8_t)GEN_MAX);
        if (gen_count > 0)
            f.read(gen_list, gen_count * sizeof(Generator));
        recompute_route_matrix();
    }

    f.close();
    return true;
}
