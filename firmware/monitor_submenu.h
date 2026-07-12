#pragma once
// monitor_submenu.h — Glue entre le carousel et monitor.h
// Page 0 : VU-mètre (mon_draw)
// Page 1 : BPM maître en grand

static uint8_t mon_page = 0;   // 0=VU  1=BPM

// ── Page BPM ─────────────────────────────────────────────────────────
void mon_draw_bpm() {
    u8g2.clearBuffer();

    // En-tête : source maître + indicateur de page
    u8g2.setFont(UI_FONT_SMALL);
    const char* master_lbl = (sync_master < SYNC_SOURCE_COUNT) ? SYNC_LABELS[sync_master] : "OFF";
    char hdr[24];   // "MASTER: " (8) + plus long libellé, ex. "C (USB Host)" (12) + '\0'
    snprintf(hdr, sizeof(hdr), "MASTER: %s", master_lbl);
    u8g2.drawStr(0, 8, hdr);
    u8g2.drawStr(110, 8, "2/2");

    bool has_bpm = (bpm_value > 1.0f) &&
                   ((millis() - bpm_last_ms) < BPM_TIMEOUT_MS);

    if (has_bpm) {
        uint16_t bpm_int = (uint16_t)(bpm_value + 0.5f);
        char bpm_str[5];
        snprintf(bpm_str, sizeof(bpm_str), "%u", bpm_int);
        u8g2.setFont(u8g2_font_logisoso42_tn);
        int16_t tw = (int16_t)u8g2.getStrWidth(bpm_str);
        u8g2.drawStr((128 - tw) / 2, 62, bpm_str);
    } else {
        u8g2.setFont(u8g2_font_8x13_tf);
        const char* msg = "NO CLOCK";
        int16_t tw = (int16_t)u8g2.getStrWidth(msg);
        u8g2.drawStr((128 - tw) / 2, 40, msg);
    }

    u8g2.sendBuffer();
}

// ── Handler ───────────────────────────────────────────────────────────
bool mon_handle_input(bool enc_up, bool enc_down, bool btn_valid, bool btn_back, bool btn_held) {
    if (btn_back) return true;   // retour au carousel

    bool page_changed = false;
    if (enc_down && mon_page < 1) { mon_page++; page_changed = true; }
    if (enc_up   && mon_page > 0) { mon_page--; page_changed = true; }

    // Throttle séparé par page pour réduire le blocage I2C (~8ms/sendBuffer à 1MHz).
    // Un changement de page force un redraw immédiat.
    static uint32_t mon_last_draw_ms = 0;   // page VU-mètre (fixe, 1000ms)
    static uint32_t bpm_last_draw_ms = 0;   // page BPM (configurable, SYSTEM > BPM REFRESH)
    uint32_t now = millis();

    if (mon_page == 1) {
        // Un clic sur l'encodeur force toujours un rafraîchissement immédiat, y compris
        // en mode MANUEL (interval == 0, jamais de rafraîchissement auto). Si le bouton
        // reste appuyé en continu, répétition fixe toutes les 1000ms tant qu'il est tenu,
        // indépendamment de l'intervalle configuré.
        uint16_t interval = BPM_REFRESH_OPTIONS[bpm_refresh_idx];
        uint32_t elapsed  = now - bpm_last_draw_ms;
        bool auto_due = (interval > 0) && (elapsed >= interval);
        bool held_due = btn_held && (elapsed >= 1000);
        if (page_changed || btn_valid || held_due || auto_due) {
            bpm_last_draw_ms = now;
            mon_draw_bpm();
        }
    } else {
        if (page_changed || (now - mon_last_draw_ms >= 1000)) {
            mon_last_draw_ms = now;
            mon_draw();
        }
    }

    return false;
}

void mon_enter() {
    mon_page = 0;
}
