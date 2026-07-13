#pragma once
// sd_presets.h — Presets sur carte SD
//
// Format v5 /PRESETS/PRESTnn.DAT :
//   [0-7]   SdPresetHeader (magic, version=5, n_inputs, n_outputs, reserved)
//   [8+]    route_matrix  (n_in × 16 × n_out × 2 octets) — DÉRIVÉ, reconstruit au chargement
//   [...]   basic_matrix[5]   (10 octets)
//   [1 oct] flux_count
//   [...]   flux_list[0..flux_count-1] (flux_count × sizeof(Flux), inclut transform)
//
// Format v4 (legacy) : flux_list avec `transpose` brut au lieu de `transform`
// (union TransformType) → migration automatique (voir FluxV4Legacy plus bas).
// Format v3 (legacy) : flux_list sans transform du tout → migration automatique
// (voir FluxV3Legacy plus bas).
// Format v1 (legacy) : route_matrix seul → migration automatique vers basic_matrix

#define SD_PRESET_MAX   32
#define SD_PRESET_DIR   "/PRESETS"
#define SD_PRESET_MAGIC 0x4D4E4D43UL   // 'MNMC' little-endian
#define SD_PRESET_VER   5              // v1=route_matrix, v2=+basic+flux(chan mono), v3=flux(chan_mask multi), v4=+flux.transpose, v5=+flux.transform (TransformType)

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
        (hdr.version != 1 && hdr.version != 3 && hdr.version != 4 && hdr.version != 5)) {
        f.close();
        return false;
    }

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
    } else {   // v5 : basic_matrix + flux (avec transform)
        f.read(basic_matrix, sizeof(basic_matrix));
        uint8_t fc = 0;
        f.read(&fc, 1);
        flux_count = min(fc, (uint8_t)FLUX_MAX);
        if (flux_count > 0)
            f.read(flux_list, flux_count * sizeof(Flux));
        recompute_route_matrix();
    }

    f.close();
    return true;
}
