// The real ios2kbd device interfaces, backed by the device's USB device controller.
#ifndef IOS2KBD_IOKIT_H
#define IOS2KBD_IOKIT_H

#include "ios2kbd.h"

// Load the private controller API and fill `out`; false if unavailable.
bool ios2kbd_iokit_usb(ios2kbd_usb *out);
void ios2kbd_iokit_keyboard(ios2kbd_keyboard *out, unsigned poll_interval);

#endif
