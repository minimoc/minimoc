#pragma once
// debug.h — Dump diagnostique USB Host sur carte SD
// Déclenché par appui long (2s) sur le bouton encodeur (PIN_VALID).
// Fichiers : /DIAG/USB_001.TXT, USB_002.TXT, …

static uint8_t dbg_count = 0;

static const char* _dbg_port(uint8_t p) {
    static const char* n[] = {"IN C ","IN D ","IN E ","OUT 7","OUT 8","OUT 9"};
    return (p < 6) ? n[p] : "?";
}

void debug_dump_usb() {
    if (!sd_ok) return;

    if (!sd.exists("/DIAG")) sd.mkdir("/DIAG");

    dbg_count++;
    char path[24];
    snprintf(path, sizeof(path), "/DIAG/USB_%03u.TXT", dbg_count);

    FsFile f = sd.open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (!f) return;

    char buf[56];

    f.println("=== miniMoc USB Debug Dump ===");
    snprintf(buf, sizeof(buf), "Uptime : %lu ms", millis());
    f.println(buf);
    snprintf(buf, sizeof(buf), "Mode   : %s", hc_locked ? "MANUEL" : "AUTO");
    f.println(buf);
    snprintf(buf, sizeof(buf), "Preset : %u", current_preset);
    f.println(buf);
    f.println();

    // ── État de chaque slot MIDIUSB ──────────────────────────────
    f.println("[ Slots USB ]");
    MIDIDevice_BigBuffer* devs[HC_SLOT_COUNT] = {
        &MIDIUSB1, &MIDIUSB2, &MIDIUSB3,
        &MIDIUSB4, &MIDIUSB5, &MIDIUSB6
    };
    for (uint8_t s = 0; s < HC_SLOT_COUNT; s++) {
        MIDIDevice_BigBuffer* d = devs[s];
        if ((bool)(*d)) {
            char dname[24] = "";
            const uint8_t* p = d->product();
            if (p && p[0]) strncpy(dname, (const char*)p, sizeof(dname) - 1);
            else            snprintf(dname, sizeof(dname), "(no name)");
            snprintf(buf, sizeof(buf), "D%u: CONNECTED  %04X:%04X  \"%s\"",
                     s + 1, d->idVendor(), d->idProduct(), dname);
        } else {
            snprintf(buf, sizeof(buf), "D%u: empty", s + 1);
        }
        f.println(buf);
    }
    f.println();

    // ── Assignations port_slot ───────────────────────────────────
    f.println("[ Assignations port_slot ]");
    for (uint8_t p = 0; p < HC_PORT_COUNT; p++) {
        uint8_t s = port_slot[p];
        if (s == HC_NO_SLOT)
            snprintf(buf, sizeof(buf), "%s -> [Vide]", _dbg_port(p));
        else
            snprintf(buf, sizeof(buf), "%s -> slot %u (D%u)", _dbg_port(p), s, s + 1);
        f.println(buf);
    }
    f.println();

    // ── VID:PID attendus par port (mode MANUEL) ─────────────────
    f.println("[ VID:PID attendus par port (MANUEL) ]");
    for (uint8_t p = 0; p < HC_PORT_COUNT; p++) {
        snprintf(buf, sizeof(buf), "%s -> %04X:%04X",
                 _dbg_port(p), hc_port_vp[p].vid, hc_port_vp[p].pid);
        f.println(buf);
    }

    f.sync();
    f.close();
}
