#pragma once
// monitor_submenu.h — Glue entre le carousel et monitor.h

bool mon_handle_input(bool /*enc_up*/, bool /*enc_down*/, bool /*btn_valid*/, bool btn_back) {
    if (btn_back) return true;   // retour au carousel
    mon_draw();
    return false;
}

void mon_enter() {
    // Reset optionnel : on laisse les données existantes pour une transition fluide
}
