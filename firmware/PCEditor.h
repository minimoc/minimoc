#pragma once

// Déclarations anticipées — fonctions définies dans des headers inclus après PCEditor.h
void hc_save();    // usb_host_config.h

// Envoie F0 7D 0F slotIdx name... F7 pour chaque device USB connecté
static void _send_device_names() {
    MIDIDevice_BigBuffer* devs[HC_SLOT_COUNT] = {
        &MIDIUSB1, &MIDIUSB2, &MIDIUSB3, &MIDIUSB4, &MIDIUSB5, &MIDIUSB6
    };
    for (uint8_t s = 0; s < HC_SLOT_COUNT; s++) {
        if (!(bool)(*devs[s])) continue;
        const uint8_t* pname = devs[s]->product();
        if (!pname || !pname[0]) {
            // Pas de nom : envoyer VID:PID à la place
            uint8_t buf[16];
            uint8_t pos = 0;
            buf[pos++] = 0x7D; buf[pos++] = 0x0F; buf[pos++] = s;
            // Encoder VID:PID en ASCII 7-bit
            uint16_t vid = devs[s]->idVendor(), pid = devs[s]->idProduct();
            const char hex[] = "0123456789ABCDEF";
            buf[pos++] = hex[(vid>>12)&0xF]; buf[pos++] = hex[(vid>>8)&0xF];
            buf[pos++] = hex[(vid>>4)&0xF];  buf[pos++] = hex[vid&0xF];
            buf[pos++] = ':';
            buf[pos++] = hex[(pid>>12)&0xF]; buf[pos++] = hex[(pid>>8)&0xF];
            buf[pos++] = hex[(pid>>4)&0xF];  buf[pos++] = hex[pid&0xF];
            usbMIDI.sendSysEx(pos, buf);
        } else {
            uint8_t buf[36];
            uint8_t pos = 0;
            buf[pos++] = 0x7D; buf[pos++] = 0x0F; buf[pos++] = s;
            for (uint8_t i = 0; pname[i] && pos < 34; i++)
                buf[pos++] = pname[i] & 0x7F;
            usbMIDI.sendSysEx(pos, buf);
        }
        usbMIDI.send_now();
    }
}

// HC_NO_SLOT (0xFF) est illégal en SysEx → mapper vers 0x7F sur le fil
#define PS_SAFE(x) ((uint8_t)((x) == HC_NO_SLOT ? 0x7F : (x)))

// Encodage uint16_t en 3 octets SysEx 7 bits :
//   m0=bits 0-6, m1=bits 7-13, m2=bits 14-15
// Décodage : mask = m0 | (m1<<7) | (m2<<14)

void sendFullDumpToPC() {
  // 1. Matrice de routage (0x01) — maintenue pour compatibilité éditeur web
  for (uint8_t p = 0; p < 5; p++) {
    for (uint8_t c = 0; c < 16; c++) {
      for (uint8_t o = 0; o < 9; o++) {
        uint16_t mask = route_matrix[p][c][o];
        uint8_t data[] = { 0x7D, 0x01, p, c, o,
          (uint8_t)(mask & 0x7F),
          (uint8_t)((mask >> 7) & 0x7F),
          (uint8_t)((mask >> 14) & 0x03) };
        usbMIDI.sendSysEx(8, data); delay(2);
      }
    }
  }

  // 2. Preset actif (0x07)
  { uint8_t d[] = {0x7D, 0x07, (uint8_t)current_preset};
    usbMIDI.sendSysEx(3, d); delay(2); }

  // 4. Matrice BASIC (0x09) — F0 7D 09 IN OUT VAL F7
  for (uint8_t i = 0; i < 5; i++) {
    for (uint8_t j = 0; j < 9; j++) {
      uint8_t val = (basic_matrix[i] >> j) & 1;
      uint8_t d[] = {0x7D, 0x09, i, j, val};
      usbMIDI.sendSysEx(5, d); delay(2);
    }
  }

  // 5. Flux count (0x0B)
  { uint8_t d[] = {0x7D, 0x0B, flux_count};
    usbMIDI.sendSysEx(3, d); delay(2); }

  // 6. Dump chaque flux (0x0A) — encodage 7 bits des champs binaires
  // F0 7D 0A idx n_in n_out active [in: port m0 m1 m2]×n_in [out: port m0 m1 m2]×n_out F7
  // chan_mask encodé sur 3 octets 7-bit : bits 0-6 | bits 7-13 | bits 14-15
  for (uint8_t f = 0; f < flux_count; f++) {
    uint8_t buf[3 + FLUX_MAX_IN*4 + FLUX_MAX_OUT*4 + 2];
    uint8_t pos = 0;
    buf[pos++] = 0x7D;
    buf[pos++] = 0x0A;
    buf[pos++] = f;
    buf[pos++] = flux_list[f].n_in;
    buf[pos++] = flux_list[f].n_out;
    buf[pos++] = flux_list[f].active ? 1 : 0;
    for (uint8_t ii = 0; ii < flux_list[f].n_in; ii++) {
      buf[pos++] = flux_list[f].in[ii].port;
      buf[pos++] = flux_list[f].in[ii].chan_mask & 0x7F;
      buf[pos++] = (flux_list[f].in[ii].chan_mask >> 7)  & 0x7F;
      buf[pos++] = (flux_list[f].in[ii].chan_mask >> 14) & 0x03;
    }
    for (uint8_t oi = 0; oi < flux_list[f].n_out; oi++) {
      buf[pos++] = flux_list[f].out[oi].port;
      buf[pos++] = flux_list[f].out[oi].chan_mask & 0x7F;
      buf[pos++] = (flux_list[f].out[oi].chan_mask >> 7)  & 0x7F;
      buf[pos++] = (flux_list[f].out[oi].chan_mask >> 14) & 0x03;
    }
    usbMIDI.sendSysEx(pos, buf); delay(3);
  }

  // 7. SYNC master (0x0C) — F0 7D 0C VALUE F7 (VALUE: 0-4=A-E, 0x7F=OFF)
  // 0xFF est illégal en SysEx (>7 bits) → mapper 0xFF→0x7F sur le fil
  { uint8_t v = (sync_master == 0xFF) ? 0x7F : sync_master;
    uint8_t d[] = {0x7D, 0x0C, v}; usbMIDI.sendSysEx(3, d); delay(2); }

  // 8. HOST CONFIG (0x0D) — F0 7D 0D mode p0 p1 p2 p3 p4 p5 F7
  { uint8_t d[] = {0x7D, 0x0D, (uint8_t)hc_locked,
      PS_SAFE(port_slot[0]), PS_SAFE(port_slot[1]), PS_SAFE(port_slot[2]),
      PS_SAFE(port_slot[3]), PS_SAFE(port_slot[4]), PS_SAFE(port_slot[5])};
    usbMIDI.sendSysEx(9, d); delay(2); }

  // 9. Noms des devices USB connectés (0x0F)
  _send_device_names();

  usbMIDI.send_now();
}

void handleEditorSysex(byte* data, unsigned size) {
  if (size < 3 || data[1] != 0x7D) return;
  uint8_t command = data[2];

  switch (command) {

    case 0x01:   // UPDATE ROUTING (legacy direct) — F0 7D 01 PORT CHAN OUT M0 M1 M2 F7
      if (size >= 10) {
        uint8_t  ip = data[3], ch = data[4], op = data[5];
        uint16_t mask = (uint16_t)data[6] | ((uint16_t)data[7]<<7) | ((uint16_t)data[8]<<14);
        if (ip < 5 && ch < 16 && op < 9)
          route_matrix[ip][ch][op] = mask;
      }
      break;

    case 0x02:   // REQUEST FULL DUMP
      sendFullDumpToPC();
      break;

    case 0x05:   // CHARGER PRESET N°X
      if (size >= 4) {
        uint8_t t = data[3];
        if (t >= 1 && t <= MAX_PRESETS) {
          current_preset = t;
          silent_load_preset();
          sendFullDumpToPC();
        }
      }
      break;

    case 0x06:   // SAUVER DANS PRESET N°X
      if (size >= 4) {
        uint8_t t = data[3];
        if (t >= 1 && t <= MAX_PRESETS) {
          current_preset = t;
          silent_save_preset();
        }
      }
      break;

    case 0x08:   // PING / HEARTBEAT
      { uint8_t pong[] = {0xF0, 0x7D, 0x08, 0xF7};
        usbMIDI.sendSysEx(sizeof(pong), pong, true); }
      break;

    case 0x09:   // UPDATE BASIC — F0 7D 09 IN OUT VAL F7
      if (size >= 6) {
        uint8_t i = data[3], j = data[4], val = data[5];
        if (i < 5 && j < 9) {
          if (val) basic_matrix[i] |=  (uint16_t)(1u << j);
          else     basic_matrix[i] &= ~(uint16_t)(1u << j);
          recompute_route_matrix();
        }
      }
      break;

    case 0x0A:   // ADD/UPDATE FLUX — F0 7D 0A idx n_in n_out active [ins] [outs] F7
      if (size >= 7) {
        uint8_t f = data[3];
        if (f <= flux_count && f < FLUX_MAX) {
          if (f == flux_count) flux_count++;
          memset(&flux_list[f], 0, sizeof(Flux));   // zeroise avant écriture — évite valeurs résiduelles
          flux_list[f].n_in   = min(data[4], (uint8_t)FLUX_MAX_IN);
          flux_list[f].n_out  = min(data[5], (uint8_t)FLUX_MAX_OUT);
          flux_list[f].active = (data[6] != 0);
          // chan_mask encodé sur 3 octets 7-bit : bits 0-6 | bits 7-13 | bits 14-15
          uint8_t pos = 7;
          for (uint8_t ii = 0; ii < flux_list[f].n_in && pos+3 < size; ii++, pos+=4) {
            flux_list[f].in[ii].port = data[pos];
            flux_list[f].in[ii].chan_mask = (uint16_t)data[pos+1]
                                          | ((uint16_t)data[pos+2] << 7)
                                          | ((uint16_t)data[pos+3] << 14);
          }
          for (uint8_t oi = 0; oi < flux_list[f].n_out && pos+3 < size; oi++, pos+=4) {
            flux_list[f].out[oi].port      = data[pos];
            flux_list[f].out[oi].chan_mask = (uint16_t)data[pos+1]
                                           | ((uint16_t)data[pos+2] << 7)
                                           | ((uint16_t)data[pos+3] << 14);
          }
          recompute_route_matrix();
        }
      }
      break;

    case 0x0B:   // DELETE FLUX — F0 7D 0B idx F7
      if (size >= 4) {
        uint8_t f = data[3];
        if (f < flux_count) {
          for (uint8_t i = f; i < flux_count - 1; i++) flux_list[i] = flux_list[i+1];
          flux_count--;
          recompute_route_matrix();
        }
      }
      break;

    case 0x0E: {  // QUICK POLL — preset + sync + basic_matrix + flux
      { uint8_t p[] = {0x7D, 0x07, (uint8_t)current_preset};
        usbMIDI.sendSysEx(3, p); }
      { uint8_t sv = (sync_master == 0xFF) ? 0x7F : sync_master;
        uint8_t s[] = {0x7D, 0x0C, sv};
        usbMIDI.sendSysEx(3, s); }
      usbMIDI.send_now();
      for (uint8_t i = 0; i < 5; i++) {
        for (uint8_t j = 0; j < 9; j++) {
          uint8_t val = (basic_matrix[i] >> j) & 1;
          uint8_t d[] = {0x7D, 0x09, i, j, val};
          usbMIDI.sendSysEx(5, d);
        }
        usbMIDI.send_now();
      }
      // flux : count puis données de chaque flux
      { uint8_t d[] = {0x7D, 0x0B, flux_count};
        usbMIDI.sendSysEx(3, d);
        usbMIDI.send_now(); }
      for (uint8_t f = 0; f < flux_count; f++) {
        uint8_t buf[3 + FLUX_MAX_IN*4 + FLUX_MAX_OUT*4 + 2];
        uint8_t pos = 0;
        buf[pos++] = 0x7D; buf[pos++] = 0x0A; buf[pos++] = f;
        buf[pos++] = flux_list[f].n_in;
        buf[pos++] = flux_list[f].n_out;
        buf[pos++] = 1;
        for (uint8_t ii = 0; ii < flux_list[f].n_in; ii++) {
          buf[pos++] = flux_list[f].in[ii].port;
          buf[pos++] = flux_list[f].in[ii].chan_mask & 0x7F;
          buf[pos++] = (flux_list[f].in[ii].chan_mask >> 7)  & 0x7F;
          buf[pos++] = (flux_list[f].in[ii].chan_mask >> 14) & 0x03;
        }
        for (uint8_t oi = 0; oi < flux_list[f].n_out; oi++) {
          buf[pos++] = flux_list[f].out[oi].port;
          buf[pos++] = flux_list[f].out[oi].chan_mask & 0x7F;
          buf[pos++] = (flux_list[f].out[oi].chan_mask >> 7)  & 0x7F;
          buf[pos++] = (flux_list[f].out[oi].chan_mask >> 14) & 0x03;
        }
        usbMIDI.sendSysEx(pos, buf);
        usbMIDI.send_now();
      }
      // host config
      { uint8_t d[] = {0x7D, 0x0D, (uint8_t)hc_locked,
          PS_SAFE(port_slot[0]), PS_SAFE(port_slot[1]), PS_SAFE(port_slot[2]),
          PS_SAFE(port_slot[3]), PS_SAFE(port_slot[4]), PS_SAFE(port_slot[5])};
        usbMIDI.sendSysEx(9, d);
        usbMIDI.send_now(); }
      // noms des devices USB connectés
      _send_device_names();
      break;
    }

    case 0x0C:   // SET SYNC MASTER — F0 7D 0C VALUE F7 (0-4=A-E, 0x7F=OFF)
      // Sur le fil : 0x7F = OFF ; en interne on stocke 0xFF pour OFF
      if (size >= 4) {
        uint8_t v = data[3];
        if (v <= 4 || v == 0x7F) {
          sync_master = (v == 0x7F) ? 0xFF : v;
          sync_save();
        }
      }
      break;

    case 0x0D:   // SET HOST CONFIG — F0 7D 0D mode p0 p1 p2 p3 p4 p5 F7
      // 0x7F sur le fil = HC_NO_SLOT en interne
      if (size >= 10) {
        hc_locked = (data[3] != 0);
        for (uint8_t i = 0; i < 6; i++) {
          uint8_t v = data[4 + i];
          if      (v < HC_SLOT_COUNT) port_slot[i] = v;
          else if (v == 0x7F)         port_slot[i] = HC_NO_SLOT;
          // sinon : ignorer valeur invalide
        }
        hc_save();
      }
      break;

    case 0x10:   // REBOOT INTO HALFKAY BOOTLOADER
      usbMIDI.send_now();
      delay(10);
      _reboot_Teensyduino_();   // fonction Teensyduino — entre en mode HalfKay
      while(1);
      break;
  }
}
