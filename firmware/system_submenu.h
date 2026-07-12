#pragma once
// system_submenu.h — Sous-menu SYSTEM : INFO / HOST CONFIG / USB LINK / USB RESET / CONTRASTE / BPM REFRESH
// Doit être inclus après info_submenu.h et usb_host_config.h

enum SysSubState { SYS_LIST, SYS_INFO, SYS_HOST_CONFIG, SYS_USB_RESET, SYS_CONTRAST, SYS_BPM_REFRESH };
static SysSubState sys_state  = SYS_LIST;
static uint8_t     sys_cursor = 0;
static uint8_t     sys_scroll = 0;   // index du premier item visible dans la liste

static const char* SYS_ITEMS[] = { "INFO", "HOST CONFIG", "USB LINK", "USB RESET", "CONTRASTE", "BPM REFRESH" };
#define SYS_ITEM_COUNT 6

// Police agrandie pour la liste (7x13, identique au titre) + défilement
// pour garder le curseur visible quand tous les items ne tiennent pas.
#define SYS_ITEM0_Y  31
#define SYS_ROW_H    14
#define SYS_VISIBLE  3   // lignes visibles simultanément dans la liste

void sys_draw_list() {
    u8g2.clearBuffer();
    u8g2.setFont(UI_FONT_TITLE);
    u8g2.drawStr(0, 14, "SYSTEM");
    u8g2.drawHLine(0, 18, SCREEN_W);

    // Ajuste le défilement pour garder le curseur visible
    if (sys_cursor < sys_scroll)                    sys_scroll = sys_cursor;
    if (sys_cursor >= sys_scroll + SYS_VISIBLE)      sys_scroll = sys_cursor - SYS_VISIBLE + 1;

    u8g2.setFont(UI_FONT_TITLE);
    for (uint8_t vi = 0; vi < SYS_VISIBLE; vi++) {
        uint8_t i = sys_scroll + vi;
        if (i >= SYS_ITEM_COUNT) break;
        int y = SYS_ITEM0_Y + vi * SYS_ROW_H;
        if (i == sys_cursor) {
            u8g2.drawBox(0, y - 11, SCREEN_W, SYS_ROW_H);
            u8g2.setDrawColor(0);
        }
        u8g2.drawStr(8, y, SYS_ITEMS[i]);
        u8g2.setDrawColor(1);
    }

    // Flèches de défilement
    u8g2.setFont(UI_FONT_SMALL);
    if (sys_scroll > 0)                              u8g2.drawStr(122, 25, "^");
    if (sys_scroll + SYS_VISIBLE < SYS_ITEM_COUNT)    u8g2.drawStr(122, 63, "v");

    u8g2.sendBuffer();
}

static uint8_t contrast_saved = 200;  // sauvegarde pour annulation

void sys_draw_contrast() {
    u8g2.clearBuffer();
    u8g2.setFont(UI_FONT_TITLE);
    u8g2.drawStr(0, 14, "CONTRASTE");
    u8g2.drawHLine(0, 18, SCREEN_W);

    // Barre de progression
    uint8_t bar_w = (uint8_t)((uint16_t)screen_contrast * 108u / 255u);
    u8g2.drawFrame(10, 27, 108, 12);
    if (bar_w > 0) u8g2.drawBox(10, 27, bar_w, 12);

    // Valeur en pourcentage centrée sous la barre
    char buf[6];
    snprintf(buf, sizeof(buf), "%u%%", (uint16_t)screen_contrast * 100u / 255u);
    u8g2.setFont(UI_FONT_BODY);
    uint8_t tw = u8g2.getStrWidth(buf);
    u8g2.drawStr((SCREEN_W - tw) / 2, 50, buf);

    // Aide en bas
    u8g2.setFont(UI_FONT_SMALL);
    u8g2.drawStr(0, 63, "OK=sauver   Back=annuler");
    u8g2.sendBuffer();
}

static uint8_t bpm_refresh_saved_idx = 0;  // sauvegarde pour annulation

void sys_draw_bpm_refresh() {
    u8g2.clearBuffer();
    u8g2.setFont(UI_FONT_TITLE);
    u8g2.drawStr(0, 14, "BPM REFRESH");
    u8g2.drawHLine(0, 18, SCREEN_W);

    // Valeur courante, centrée
    uint16_t ms = BPM_REFRESH_OPTIONS[bpm_refresh_idx];
    char buf[10];
    if (ms == 0) strncpy(buf, "MANUEL", sizeof(buf));
    else         snprintf(buf, sizeof(buf), "%u ms", ms);
    u8g2.setFont(u8g2_font_8x13_tf);
    uint8_t tw = u8g2.getStrWidth(buf);
    u8g2.drawStr((SCREEN_W - tw) / 2, 38, buf);

    // Indicateur de position dans la liste d'options
    uint8_t dots_w = BPM_REFRESH_OPTION_COUNT * 8;
    uint8_t dx = (SCREEN_W - dots_w) / 2;
    for (uint8_t i = 0; i < BPM_REFRESH_OPTION_COUNT; i++) {
        uint8_t cx = dx + i * 8 + 3;
        if (i == bpm_refresh_idx) u8g2.drawDisc(cx, 48, 2);
        else                      u8g2.drawCircle(cx, 48, 2);
    }

    // Aide en bas
    u8g2.setFont(UI_FONT_SMALL);
    u8g2.drawStr(0, 63, "OK=sauver   Back=annuler");
    u8g2.sendBuffer();
}

// ── USB LINK — polling myusb.Task() pendant 2s pour forcer l'énumération ──
// Utile au démarrage (devices non reconnus) ou après une perte à chaud.
// Pas de reboot : les routages MIDI en cours sont préservés.
static void sys_do_usb_link() {
    u8g2.clearBuffer();
    u8g2.setFont(UI_FONT_BODY);
    u8g2.drawStr(20, 24, "USB LINK");
    u8g2.drawStr(6,  40, "Enumeration...");
    u8g2.sendBuffer();

    // Polling intensif : donne le temps au hub et aux devices d'énumérer
    uint32_t t = millis();
    while (millis() - t < 2500) {
        myusb.Task();
        MIDIUSB1.read(); MIDIUSB2.read(); MIDIUSB3.read();
        MIDIUSB4.read(); MIDIUSB5.read(); MIDIUSB6.read();
        delay(2);
    }

    // Résultat
    uint8_t n = 0;
    for (uint8_t i = 0; i < HC_SLOT_COUNT; i++) if (hc_is_conn(i)) n++;

    u8g2.clearBuffer();
    u8g2.setFont(UI_FONT_BODY);
    char buf[24];
    if (n > 0) {
        snprintf(buf, sizeof(buf), "%u device(s) OK", n);
        u8g2.drawStr(14, 36, buf);
    } else {
        u8g2.drawStr(14, 30, "Aucun device");
        u8g2.setFont(UI_FONT_SMALL);
        u8g2.drawStr(4, 46, "-> USB RESET si hub");
    }
    u8g2.sendBuffer();
    delay(2000);
}

// ── USB RESET — reboot logiciel ARM Cortex-M7 ────────────────────────────
// myusb.begin() en cours d'exécution crashe (réinit DMA/IRQ).
// Reboot complet = seule approche sûre (~3s). Utiliser si LINK échoue.
static void sys_do_usb_reset() {
    u8g2.clearBuffer();
    u8g2.setFont(UI_FONT_BODY);
    u8g2.drawStr(16, 28, "Reboot USB");
    u8g2.drawStr(20, 44, "en cours...");
    u8g2.sendBuffer();
    delay(400);
    SCB_AIRCR = 0x05FA0004;   // VECTKEY | SYSRESETREQ
    while (true) {}
}

bool sys_handle_input(bool enc_up, bool enc_down, bool btn_valid, bool btn_back) {
    switch (sys_state) {

        case SYS_LIST:
            if (enc_up   && sys_cursor > 0)                sys_cursor--;
            if (enc_down && sys_cursor < SYS_ITEM_COUNT-1) sys_cursor++;
            if (btn_valid) {
                switch (sys_cursor) {
                    case 0: info_page = 0; sys_state = SYS_INFO;       info_draw(); break;
                    case 1: hc_enter();    sys_state = SYS_HOST_CONFIG; hc_draw();  break;
                    case 2: sys_do_usb_link();  sys_draw_list(); break;
                    case 3: sys_do_usb_reset(); sys_draw_list(); break;
                    case 4: contrast_saved = screen_contrast; sys_state = SYS_CONTRAST; sys_draw_contrast(); break;
                    case 5: bpm_refresh_saved_idx = bpm_refresh_idx; sys_state = SYS_BPM_REFRESH; sys_draw_bpm_refresh(); break;
                }
            }
            if (btn_back) { sys_state = SYS_LIST; return true; }
            if (!btn_valid) sys_draw_list();
            break;

        case SYS_INFO:
            if (info_handle_input(enc_up, enc_down, btn_valid, btn_back)) {
                sys_state = SYS_LIST; sys_draw_list();
            }
            break;

        case SYS_HOST_CONFIG:
            if (hc_handle_input(enc_up, enc_down, btn_valid, btn_back)) {
                sys_state = SYS_LIST; sys_draw_list();
            }
            break;

        case SYS_USB_RESET:
            break;

        case SYS_CONTRAST:
            if (enc_up)   { screen_contrast = (screen_contrast > 16)   ? screen_contrast - 16 : 16;  contrast_apply(); }
            if (enc_down) { screen_contrast = (screen_contrast <= 239) ? screen_contrast + 16 : 255; contrast_apply(); }
            if (btn_valid) { contrast_save(); sys_state = SYS_LIST; sys_draw_list(); break; }
            if (btn_back)  { screen_contrast = contrast_saved; contrast_apply(); sys_state = SYS_LIST; sys_draw_list(); break; }
            if (!btn_valid) sys_draw_contrast();
            break;

        case SYS_BPM_REFRESH:
            if (enc_up   && bpm_refresh_idx > 0)                           bpm_refresh_idx--;
            if (enc_down && bpm_refresh_idx < BPM_REFRESH_OPTION_COUNT-1)  bpm_refresh_idx++;
            if (btn_valid) { bpm_refresh_save(); sys_state = SYS_LIST; sys_draw_list(); break; }
            if (btn_back)  { bpm_refresh_idx = bpm_refresh_saved_idx; sys_state = SYS_LIST; sys_draw_list(); break; }
            if (!btn_valid) sys_draw_bpm_refresh();
            break;
    }
    return false;
}

void sys_enter() { sys_state = SYS_LIST; sys_cursor = 0; sys_scroll = 0; }
