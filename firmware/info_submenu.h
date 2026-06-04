#pragma once
// info_submenu.h — Info / statuts  (page 0) + devices USB physiques (page 1)
// Encodeur up/down pour changer de page, BACK pour revenir au carousel.

static uint8_t info_page = 0;   // 0 = stats  1 = USB HOST (slots physiques D1..D6)

// ----------------------------------------------------------------
// Helper : convertit le descripteur de nom USB (UTF-16LE) en ASCII.
// Retourne "---" si non connecté, "VID:PID" si pas de nom.
// ----------------------------------------------------------------
static void usb_get_name(MIDIDevice_BigBuffer& dev, char* buf, uint8_t maxlen) {
    if (!(bool)dev) {
        strncpy(buf, "---", maxlen);
        buf[maxlen - 1] = '\0';
        return;
    }
    // product() retourne directement une chaîne ASCII null-terminated
    // (USBHost_t36 convertit UTF-16LE → ASCII pendant l'énumération)
    const uint8_t* p = dev.product();
    if (p != nullptr && p[0] != 0) {
        strncpy(buf, (const char*)p, maxlen - 1);
        buf[maxlen - 1] = '\0';
    } else {
        // Pas de nom : VID:PID
        snprintf(buf, maxlen, "%04X:%04X", dev.idVendor(), dev.idProduct());
    }
}

// ----------------------------------------------------------------
// Page 0 — Statistiques SD et preset
// ----------------------------------------------------------------
void info_draw_stats() {
    u8g2.clearBuffer();
    u8g2.setFont(UI_FONT_TITLE);
    u8g2.drawStr(0, 14, "miniMoc");
    u8g2.drawHLine(0, 18, SCREEN_W);
    u8g2.setFont(UI_FONT_BODY);

    char buf[26];
    snprintf(buf, sizeof(buf), "Preset: %u", current_preset);
    u8g2.drawStr(0, 31, buf);

    if (sd_ok) {
        snprintf(buf, sizeof(buf), "REC%03u %lukB",
                 (unsigned)rec_session,
                 (unsigned long)(sd_bytes_written / 1024));
        u8g2.drawStr(0, 44, buf);
        snprintf(buf, sizeof(buf), "max:%luus ov:%lu",
                 (unsigned long)sd_max_us,
                 (unsigned long)sd_overflow_count);
        u8g2.drawStr(0, 57, buf);
    } else {
        u8g2.drawStr(0, 44, "SD: non dispo");
        u8g2.drawStr(0, 57, "Version 1.2");
    }

    // Indicateur de page
    u8g2.setFont(UI_FONT_SMALL);
    u8g2.drawStr(110, 63, "1/2");

    u8g2.sendBuffer();
}

// ----------------------------------------------------------------
// Page 1 — Devices USB physiques (slots D1…D6)
// ----------------------------------------------------------------
void info_draw_usb_host() {
    u8g2.clearBuffer();
    u8g2.setFont(UI_FONT_TITLE);
    u8g2.drawStr(0, 10, "USB HOST");
    u8g2.drawHLine(0, 12, SCREEN_W);
    u8g2.setFont(UI_FONT_SMALL);

    // Tableau des 6 objects MIDIDevice (déclarés dans _midi.h)
    MIDIDevice_BigBuffer* devs[6] = {
        &MIDIUSB1, &MIDIUSB2, &MIDIUSB3,
        &MIDIUSB4, &MIDIUSB5, &MIDIUSB6
    };

    char name[19];
    for (uint8_t i = 0; i < 6; i++) {
        uint8_t y = 20 + i * 7;
        usb_get_name(*devs[i], name, sizeof(name));
        char line[24];
        snprintf(line, sizeof(line), "D%u %s", i + 1, name);
        u8g2.drawStr(0, y, line);
    }

    u8g2.drawStr(104, 63, "2/2");
    u8g2.sendBuffer();
}

// ----------------------------------------------------------------
// Dispatch
// ----------------------------------------------------------------
void info_draw() {
    if (info_page == 0) info_draw_stats();
    else                info_draw_usb_host();
}

bool info_handle_input(bool enc_up, bool enc_down, bool /*btn_valid*/, bool btn_back) {
    if (enc_down && info_page < 1) info_page++;
    if (enc_up   && info_page > 0) info_page--;
    info_draw();
    return btn_back;
}
