#pragma once
// usb_host_config.h — Assignation dynamique USB Host, LOCK VID:PID, EEPROM, UI
// Doit être inclus APRÈS _midi.h (utilise MIDIUSB1/2/3)

// ── EEPROM ────────────────────────────────────────────────────────
// Adresse 2     : octet magique 0xA7
// Adresse 3     : hc_locked (1 octet)
// Adresse 4..9  : port_slot[0..5] (6 octets)
// Adresse 10..33: VID+PID PAR PORT logique C/D/E/7/8/9 (6×4 octets)
//                 (clé = port index, pas slot — permet le scan à la reconnexion)
#define HC_EEPROM_BASE  2
#define HC_EEPROM_MAGIC 0xA7

struct HcLockEntry { uint16_t vid; uint16_t pid; };
// hc_port_vp[p] = VID:PID attendu sur le port logique p (C=0..9=5)
// Sur reconnexion, on scan tous les slots pour trouver ce device où qu'il soit.
static HcLockEntry hc_port_vp[HC_PORT_COUNT];
static bool        hc_slot_prev[HC_SLOT_COUNT];

static void _hc_defaults() {
    for (uint8_t i = 0; i < HC_PORT_COUNT; i++)
        port_slot[i] = (i < 3) ? (uint8_t)i : (uint8_t)(i - 3);
    hc_locked = false;
    memset(hc_port_vp, 0, sizeof(hc_port_vp));
}

// ── EEPROM save / load ────────────────────────────────────────────
void hc_save() {
    EEPROM.put(HC_EEPROM_BASE,     (uint8_t)HC_EEPROM_MAGIC);
    EEPROM.put(HC_EEPROM_BASE + 1, (uint8_t)hc_locked);
    for (uint8_t i = 0; i < HC_PORT_COUNT; i++)
        EEPROM.put(HC_EEPROM_BASE + 2 + i, port_slot[i]);
    for (uint8_t i = 0; i < HC_PORT_COUNT; i++) {
        EEPROM.put(HC_EEPROM_BASE + 8 + i * 4,     hc_port_vp[i].vid);
        EEPROM.put(HC_EEPROM_BASE + 8 + i * 4 + 2, hc_port_vp[i].pid);
    }
}

void hc_load() {
    uint8_t magic = 0;
    EEPROM.get(HC_EEPROM_BASE, magic);
    if (magic != HC_EEPROM_MAGIC) {
        // Premier boot ou EEPROM vierge — valeurs par défaut
        _hc_defaults();
        for (uint8_t i = 0; i < HC_SLOT_COUNT; i++) hc_slot_prev[i] = false;
        return;
    }
    uint8_t lk = 0;
    EEPROM.get(HC_EEPROM_BASE + 1, lk);
    hc_locked = (lk == 1);
    for (uint8_t i = 0; i < HC_PORT_COUNT; i++) {
        uint8_t def = (i < 3) ? (uint8_t)i : (uint8_t)(i - 3);
        uint8_t val = def;
        EEPROM.get(HC_EEPROM_BASE + 2 + i, val);
        port_slot[i] = (val < HC_SLOT_COUNT || val == HC_NO_SLOT) ? val : def;
    }
    for (uint8_t i = 0; i < HC_PORT_COUNT; i++) {
        EEPROM.get(HC_EEPROM_BASE + 8 + i * 4,     hc_port_vp[i].vid);
        EEPROM.get(HC_EEPROM_BASE + 8 + i * 4 + 2, hc_port_vp[i].pid);
    }
    for (uint8_t i = 0; i < HC_SLOT_COUNT; i++) hc_slot_prev[i] = false;
}

// ── Helpers device ────────────────────────────────────────────────
static inline MIDIDevice_BigBuffer* hc_device(uint8_t s) {
    switch (s) {
        case 0: return &MIDIUSB1;
        case 1: return &MIDIUSB2;
        case 2: return &MIDIUSB3;
        case 3: return &MIDIUSB4;
        case 4: return &MIDIUSB5;
        case 5: return &MIDIUSB6;
    }
    return nullptr;
}

static inline bool hc_is_conn(uint8_t s) {
    MIDIDevice_BigBuffer* d = hc_device(s);
    return d && (bool)(*d);
}

static void hc_dev_name(uint8_t s, char* buf, uint8_t maxlen) {
    if (s == HC_NO_SLOT) { strncpy(buf, "Vide", maxlen); buf[maxlen-1]='\0'; return; }
    MIDIDevice_BigBuffer* d = hc_device(s);
    if (!d || !(bool)(*d)) {
        // En MANUEL, un slot vide = device attendu mais non connecté
        if (hc_locked) snprintf(buf, maxlen, "Deconn.");
        else           strncpy(buf, "Vide", maxlen);
        buf[maxlen-1] = '\0';
        return;
    }
    const uint8_t* p = d->product();
    if (p && p[0]) { strncpy(buf, (const char*)p, maxlen-1); buf[maxlen-1]='\0'; }
    else            snprintf(buf, maxlen, "%04X:%04X", d->idVendor(), d->idProduct());
}

// ── Changement de mode ────────────────────────────────────────────
static void hc_enter_manual() {
    hc_locked = true;
    // Capturer VID:PID par PORT logique (pas par slot physique).
    // Sur reconnexion, hc_poll() scannera tous les slots pour retrouver
    // chaque device par son empreinte, quel que soit son slot d'arrivée.
    for (uint8_t p = 0; p < HC_PORT_COUNT; p++) {
        uint8_t s = port_slot[p];
        MIDIDevice_BigBuffer* d = (s != HC_NO_SLOT) ? hc_device(s) : nullptr;
        hc_port_vp[p] = (d && (bool)(*d))
            ? HcLockEntry{d->idVendor(), d->idProduct()}
            : HcLockEntry{0, 0};
    }
}

static void hc_enter_auto() {
    hc_locked = false;
    memset(hc_port_vp, 0, sizeof(hc_port_vp));
    _hc_defaults();
}

// ── Poll reconnexion (appeler depuis loop) ────────────────────────
// Algorithme : scan direct par VID:PID par port logique.
// Robuste aux connexions simultanées (pas d'échange de slots conditionnels).
void hc_poll() {
    bool any_new = false;
    for (uint8_t s = 0; s < HC_SLOT_COUNT; s++) {
        bool now = hc_is_conn(s);
        if (!hc_slot_prev[s] && now) any_new = true;
        hc_slot_prev[s] = now;
    }

    if (!any_new || !hc_locked) return;

    // Pour chaque port logique, trouver quel slot physique a le bon device
    for (uint8_t p = 0; p < HC_PORT_COUNT; p++) {
        uint16_t tvid = hc_port_vp[p].vid, tpid = hc_port_vp[p].pid;
        if (!tvid && !tpid) continue;  // port sans empreinte → ne pas toucher

        bool found = false;
        for (uint8_t s = 0; s < HC_SLOT_COUNT; s++) {
            MIDIDevice_BigBuffer* d = hc_device(s);
            if (!d || !(bool)(*d)) continue;
            if (d->idVendor() == tvid && d->idProduct() == tpid) {
                port_slot[p] = s;
                found = true;
                break;
            }
        }
        if (!found) port_slot[p] = HC_NO_SLOT;  // device absent → port désactivé
    }
}

// ── UI HOST CONFIG ────────────────────────────────────────────────
static uint8_t hc_cursor      = 0;   // 0=MODE, 1-6=ports (navigable seulement en MANUEL)
static bool    hc_editing     = false;
static uint8_t hc_edit_opt    = 0;   // 0=Vide, 1..HC_SLOT_COUNT = slot 0..5
static uint8_t hc_edit_scroll = 0;   // première option visible dans le popup

static const char* hc_port_name(uint8_t p) {
    static const char* names[] = {"IN C ","IN D ","IN E ","OUT 7","OUT 8","OUT 9"};
    return (p < 6) ? names[p] : "?";
}

void hc_draw() {
    u8g2.clearBuffer();
    u8g2.setFont(UI_FONT_TITLE);
    u8g2.drawStr(0, 10, "HOST CONFIG");
    u8g2.drawHLine(0, 12, SCREEN_W);
    u8g2.setFont(UI_FONT_SMALL);

    // 7 rows: row 0 = LOCK, rows 1-6 = ports C/D/E/7/8/9
    // Baseline y0=20, row height=7
    for (uint8_t r = 0; r < 7; r++) {
        uint8_t y = 20 + r * 7;
        bool sel = (!hc_editing && hc_cursor == r);
        if (sel) { u8g2.drawBox(0, y-6, SCREEN_W, 7); u8g2.setDrawColor(0); }

        if (r == 0) {
            // Ligne MODE — toujours interactive
            u8g2.drawStr(2, y, hc_locked ? "MODE: MANUEL" : "MODE: AUTO  ");
        } else {
            // Lignes ports — navigables et éditables seulement en MANUEL
            uint8_t p = r - 1;
            char dev[13]; hc_dev_name(port_slot[p], dev, sizeof(dev));
            char line[26];
            if (hc_locked)
                snprintf(line, sizeof(line), "%s %s", hc_port_name(p), dev);
            else
                snprintf(line, sizeof(line), "  %s %s", hc_port_name(p), dev);
            u8g2.drawStr(2, y, line);
        }
        u8g2.setDrawColor(1);
    }

    // Popup d'assignation — liste scrollable (Vide + HC_SLOT_COUNT slots)
    if (hc_editing) {
        uint8_t p         = hc_cursor - 1;
        uint8_t total     = HC_SLOT_COUNT + 1;   // 7 options : 0=Vide, 1-6=slots
        uint8_t visible   = 4;                   // lignes visibles dans le popup
        // Calcul du scroll pour garder le curseur visible
        if (hc_edit_opt < hc_edit_scroll)
            hc_edit_scroll = hc_edit_opt;
        if (hc_edit_opt >= hc_edit_scroll + visible)
            hc_edit_scroll = hc_edit_opt - visible + 1;

        // Fond blanc + cadre
        u8g2.setDrawColor(1); u8g2.drawBox(8, 8, 112, 52);
        u8g2.setDrawColor(0); u8g2.drawFrame(8, 8, 112, 52);
        // Titre
        char title[18]; snprintf(title, sizeof(title), "ASSIGN %s", hc_port_name(p));
        u8g2.drawStr(12, 19, title);
        // Flèches scroll
        if (hc_edit_scroll > 0)                       u8g2.drawStr(108, 19, "^");
        if (hc_edit_scroll + visible < total)          u8g2.drawStr(108, 59, "v");
        u8g2.setDrawColor(1);
        // Options visibles
        for (uint8_t vi = 0; vi < visible; vi++) {
            uint8_t opt = hc_edit_scroll + vi;
            if (opt >= total) break;
            uint8_t oy = 28 + vi * 9;
            if (hc_edit_opt == opt) {
                u8g2.setDrawColor(0); u8g2.drawBox(10, oy-7, 108, 8);
                u8g2.setDrawColor(1);
            } else {
                u8g2.setDrawColor(0);
            }
            char oname[18];
            if (opt == 0) {
                strncpy(oname, "[Vide]", sizeof(oname));
            } else {
                char dn[11]; hc_dev_name(opt-1, dn, sizeof(dn));
                snprintf(oname, sizeof(oname), "D%u: %s", opt, dn);
            }
            u8g2.drawStr(12, oy, oname);
            u8g2.setDrawColor(1);
        }
    }
    u8g2.sendBuffer();
}

bool hc_handle_input(bool enc_up, bool enc_down, bool btn_valid, bool btn_back) {
    if (hc_editing) {
        // ── Popup d'assignation (seulement en mode MANUEL) ──
        if (enc_up   && hc_edit_opt > 0)            hc_edit_opt--;
        if (enc_down && hc_edit_opt < HC_SLOT_COUNT) hc_edit_opt++;
        if (btn_valid) {
            uint8_t p        = hc_cursor - 1;
            uint8_t new_slot = (hc_edit_opt == 0) ? HC_NO_SLOT : (uint8_t)(hc_edit_opt - 1);
            port_slot[p]     = new_slot;
            // Mettre à jour l'empreinte VID:PID du port en même temps :
            // sans ça, le scan au prochain reboot rechercherait l'ancien device.
            if (new_slot != HC_NO_SLOT) {
                MIDIDevice_BigBuffer* d = hc_device(new_slot);
                hc_port_vp[p] = (d && (bool)(*d))
                    ? HcLockEntry{d->idVendor(), d->idProduct()}
                    : HcLockEntry{0, 0};
            } else {
                hc_port_vp[p] = {0, 0};
            }
            hc_editing = false;
        }
        if (btn_back) hc_editing = false;
    } else {
        // ── Navigation dans la liste ──
        // En AUTO : curseur bloqué sur la ligne MODE (row 0)
        uint8_t cursor_max = hc_locked ? 6 : 0;
        if (enc_up   && hc_cursor > 0)           hc_cursor--;
        if (enc_down && hc_cursor < cursor_max)  hc_cursor++;

        if (btn_valid) {
            if (hc_cursor == 0) {
                // Basculer le mode AUTO ↔ MANUEL
                if (hc_locked) hc_enter_auto();
                else           hc_enter_manual();
            } else if (hc_locked) {
                // Éditer un port (seulement en MANUEL)
                hc_editing     = true;
                uint8_t s      = port_slot[hc_cursor - 1];
                hc_edit_opt    = (s == HC_NO_SLOT) ? 0 : (uint8_t)(s + 1);
                hc_edit_scroll = (hc_edit_opt < 2) ? 0 : (uint8_t)(hc_edit_opt - 1);
            }
        }

        if (btn_back) {
            hc_cursor  = 0;
            hc_editing = false;
            hc_save();   // toujours persister l'état (AUTO ou MANUEL) à la sortie
            return true;
        }
    }
    hc_draw();
    return false;
}

void hc_enter() { hc_cursor = 0; hc_editing = false; }
