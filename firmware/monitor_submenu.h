#pragma once
// monitor_submenu.h — Glue entre le carousel et monitor.h
// Page 0 : VU-mètre (mon_draw)
// Page 1 : BPM maître en grand

static uint8_t mon_page = 0;   // 0=VU  1=BPM

// ── Page BPM ─────────────────────────────────────────────────────────
void mon_draw_bpm() {
    bpm_compute();
    u8g2.clearBuffer();

    // En-tête : source maître + indicateur de page
    u8g2.setFont(UI_FONT_SMALL);
    static const char* _mon_sync_lbl[] = {"A","B","C","D","E"};
    const char* master_lbl = (sync_master <= 4) ? _mon_sync_lbl[sync_master] : "OFF";
    char hdr[20];
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
bool mon_handle_input(bool enc_up, bool enc_down, bool btn_valid, bool btn_back) {
    if (btn_back) return true;   // retour au carousel

    bool page_changed = false;
    if (enc_down && mon_page < 1) { mon_page++; page_changed = true; }
    if (enc_up   && mon_page > 0) { mon_page--; page_changed = true; }

    // Throttle : redessiner au max toutes les 300ms pour réduire le blocage I2C (~8ms/sendBuffer à 1MHz).
    // Un changement de page force un redraw immédiat.
    static uint32_t mon_last_draw_ms = 0;
    uint32_t now = millis();
    if (page_changed || (now - mon_last_draw_ms >= 1000)) {
        mon_last_draw_ms = now;
        if (mon_page == 1) mon_draw_bpm();
        else               mon_draw();
    }

    return false;
}

void mon_enter() {
    mon_page = 0;
}
