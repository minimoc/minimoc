#pragma once
// sd_presets.h — Presets sur carte SD
//
// Format v2 /PRESETS/PRESTnn.DAT :
//   [0-7]   SdPresetHeader (magic, version=2, n_inputs, n_outputs, reserved)
//   [8+]    route_matrix  (n_in × 16 × n_out × 2 octets) — DÉRIVÉ, reconstruit au chargement
//   [...]   basic_matrix[5]   (10 octets)
//   [1 oct] flux_count
//   [...]   flux_list[0..flux_count-1] (flux_count × sizeof(Flux))
//
// Format v1 (legacy) : route_matrix seul → migration automatique vers basic_matrix

#define SD_PRESET_MAX   32
#define SD_PRESET_DIR   "/PRESETS"
#define SD_PRESET_MAGIC 0x4D4E4D43UL   // 'MNMC' little-endian
#define SD_PRESET_VER   3              // v1=route_matrix, v2=+basic+flux(chan mono), v3=flux(chan_mask multi)

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
        (hdr.version != 1 && hdr.version != 3)) {
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
    } else {   // v3
        // v2 : charger basic_matrix + flux
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
