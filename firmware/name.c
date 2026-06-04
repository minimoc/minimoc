#include <usb_names.h>

// Le nom qui apparaîtra dans Ableton et Windows
// "MINIMOC" fait 7 caractères
#define MIDI_NAME   {'M','I','N','I','M','O','C'}
#define MIDI_LENGTH 7

struct usb_string_descriptor_struct usb_string_product_name = {
  2 + MIDI_LENGTH * 2,
  3,
  MIDI_NAME
};