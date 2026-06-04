#pragma once
#include "graphics.h"
#include "logic.h"

// EEPROM — uniquement l'index du preset actif et les métadonnées système
// Les données de preset sont sur la carte SD (sd_presets.h)
//
// Adresses EEPROM réservées :
//   0 : index du preset actif (uint8_t, 1-indexed)
//   1 : compteur de session SD (sd_recorder.h)
//   2 : réservé

#define MAX_PRESETS SD_PRESET_MAX   // 32, défini dans sd_presets.h

const int ACTIVE_PRESET_ADDR = 0;

uint8_t current_preset = 1;

// ----------------------------------------------------------------
// Sauvegarde le preset courant sur SD + mémorise l'index en EEPROM
// ----------------------------------------------------------------
bool silent_save_preset() {
    bool ok = sd_preset_save(current_preset);
    if (ok) EEPROM.put(ACTIVE_PRESET_ADDR, current_preset);
    return ok;
}

// ----------------------------------------------------------------
// Charge le preset courant depuis SD + mémorise l'index en EEPROM
// ----------------------------------------------------------------
bool silent_load_preset() {
    bool ok = sd_preset_load(current_preset);
    if (ok) EEPROM.put(ACTIVE_PRESET_ADDR, current_preset);
    return ok;
}
