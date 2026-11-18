// ios2kbd: make a jailbroken iPhone or iPad type text as a USB keyboard on the host it is
// plugged into (tested: Nintendo Switch 2 and macOS), then restore normal USB.
//
// The run (core.c) only talks to the device through the two interfaces below,
// so it can be exercised with fakes (tests/test_core.c). iokit.c implements
// them with the device's USB device controller.
#ifndef IOS2KBD_H
#define IOS2KBD_H

#include <CoreFoundation/CoreFoundation.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define IOS2KBD_REPORT_LENGTH 8
#define IOS2KBD_MAX_TEXT (1024 * 1024)

// USB IDs the keyboard description reports. The stock Apple IDs make Apple's
// Windows driver (usbaapl64) claim the whole device, so Windows never loads a
// keyboard driver. 1209:0001 is a pid.codes test ID, used until ios2kbd has its
// own pid.codes product ID.
#define IOS2KBD_VENDOR_ID 0x1209
#define IOS2KBD_PRODUCT_ID 0x0001

// --- pure helpers -----------------------------------------------------------

// US-layout boot-keyboard usage for printable ASCII and newline.
bool ios2kbd_keycode(char c, uint8_t *modifier, uint8_t *usage);
bool ios2kbd_text_is_typable(const char *text);

// Bytes to return for a class request on the keyboard interface. Host-to-device
// requests (SET_PROTOCOL, SET_IDLE) get a zero-length success; device-to-host
// requests get zeros, except GET_PROTOCOL which answers report protocol (1).
// Returns the reply length (at most `capacity` and 64). The LED report is
// received separately (ios2kbd_is_led_report).
size_t ios2kbd_class_reply(uint8_t request_type, uint8_t request, uint16_t length,
                           uint8_t *reply, size_t capacity);

// True if the request is the host setting the keyboard LEDs (SET_REPORT of an
// output report with data). Its first data byte holds the LED bits.
bool ios2kbd_is_led_report(uint8_t request_type, uint8_t request, uint16_t value, uint16_t length);
#define IOS2KBD_LED_CAPS_LOCK 0x02

// The stock description with ConfigurationDescriptors replaced by the
// host-selected configuration turned into Apple's single-configuration
// "Standard Keyboard" profile [AppleUSBMux, Keyboard]. NULL if `selected` is
// not a configuration of `stock`. Caller releases.
CFMutableDictionaryRef ios2kbd_keyboard_description(CFDictionaryRef stock, int selected);

// True for a description produced by ios2kbd_keyboard_description.
bool ios2kbd_is_keyboard_description(CFDictionaryRef description);

// Every function name used by any configuration of `description`. Caller releases.
CFSetRef ios2kbd_function_names(CFDictionaryRef description);

// --- device interfaces ------------------------------------------------------

typedef struct {
    void *context;
    // A copy of the live description without the AllowMultipleCreates flag.
    CFMutableDictionaryRef (*live_description)(void *context);
    // Publish `description` (the implementation adds AllowMultipleCreates).
    bool (*publish)(void *context, CFDictionaryRef description);
    // The configuration a host has selected, 0 if none.
    int (*selected_configuration)(void *context);
    // Every name in `functions` has a registered function interface.
    bool (*functions_registered)(void *context, CFSetRef functions);
} ios2kbd_usb;

typedef enum {
    IOS2KBD_SEND_OK,        // the host read the report
    IOS2KBD_SEND_TIMEOUT,   // the host did not poll within the write timeout
    IOS2KBD_SEND_FAILED,    // anything else: the host or the function went away
} ios2kbd_send_result;

typedef struct {
    void *context;
    // Open the Keyboard function, make it a boot keyboard and start answering
    // class requests. Returns false with `failed_step` naming the step.
    bool (*open)(void *context, const char **failed_step);
    // A host has selected a configuration containing the keyboard endpoint.
    bool (*host_selected)(void *context);
    ios2kbd_send_result (*send)(void *context, const uint8_t report[IOS2KBD_REPORT_LENGTH]);
    // The host's Caps Lock state from its last LED report; false before one.
    bool (*caps_lock)(void *context);
    void (*close)(void *context);
} ios2kbd_keyboard;

typedef struct {
    bool clear_autoindent;                     // clear inherited spaces after Enter
    bool mac_line_keys;                        // clear them with macOS keys (Cmd+Shift+Left)
    ios2kbd_usb usb;
    ios2kbd_keyboard keyboard;
    const char *stock_path;                      // saved stock description
    void (*sleep_ms)(unsigned milliseconds);
    void (*say)(const char *message);
    volatile sig_atomic_t *interrupted;          // set by the signal handler
} ios2kbd_env;

// --- the run ----------------------------------------------------------------

typedef enum {
    IOS2KBD_TYPED,
    IOS2KBD_NO_HOST,             // no host had selected a configuration
    IOS2KBD_NOT_STOCK,           // live description is a leftover keyboard one
    IOS2KBD_PREPARE_FAILED,      // could not build or save the descriptions
    IOS2KBD_PUBLISH_FAILED,
    IOS2KBD_SETUP_FAILED,        // the keyboard function could not be set up
    IOS2KBD_NEVER_POLLED,        // the host never read the keyboard
    IOS2KBD_HOST_LOST,           // a report failed mid-typing
    IOS2KBD_INTERRUPTED,
    IOS2KBD_LEFTOVER_FAILED,     // an earlier run's stock description would not restore
} ios2kbd_typing;

typedef enum {
    IOS2KBD_RESTORED,            // stock functions registered and a host reselected
    IOS2KBD_RESTORED_NO_HOST,    // stock functions registered, no host reconnected
    IOS2KBD_RESTORE_FAILED,      // stock functions did not all register
    IOS2KBD_NOTHING_TO_RESTORE,  // nothing was published
} ios2kbd_restore;

typedef struct {
    ios2kbd_typing typing;
    ios2kbd_restore restore;
} ios2kbd_outcome;

ios2kbd_outcome ios2kbd_run(const ios2kbd_env *env, const char *text);

#endif
