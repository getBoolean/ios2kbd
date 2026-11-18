// Host tests for ios2kbd's core: pure helpers, and the full run driven through
// fake USB controller and keyboard implementations. Run with ../test.sh.

#include "../ios2kbd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures = 0;
#define CHECK(condition, ...) do { if (!(condition)) { failures++; \
    fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

// --- fixtures -----------------------------------------------------------------

static CFArrayRef strings(int count, const char **values) {
    CFStringRef items[8];
    for (int i = 0; i < count; i++) items[i] = CFStringCreateWithCString(NULL, values[i], kCFStringEncodingUTF8);
    CFArrayRef array = CFArrayCreate(NULL, (const void **)items, count, &kCFTypeArrayCallBacks);
    for (int i = 0; i < count; i++) CFRelease(items[i]);
    return array;
}

static CFMutableDictionaryRef configuration(const char *description, int count, const char **interfaces) {
    CFMutableDictionaryRef config = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                                              &kCFTypeDictionaryValueCallBacks);
    CFStringRef name = CFStringCreateWithCString(NULL, description, kCFStringEncodingUTF8);
    CFArrayRef names = strings(count, interfaces);
    int attributes = 0xC0, power = 500;
    CFNumberRef attributes_number = CFNumberCreate(NULL, kCFNumberIntType, &attributes);
    CFNumberRef power_number = CFNumberCreate(NULL, kCFNumberIntType, &power);
    CFDictionarySetValue(config, CFSTR("Description"), name);
    CFDictionarySetValue(config, CFSTR("Interfaces"), names);
    CFDictionarySetValue(config, CFSTR("Attributes"), attributes_number);
    CFDictionarySetValue(config, CFSTR("MaxPower"), power_number);
    CFRelease(name); CFRelease(names); CFRelease(attributes_number); CFRelease(power_number);
    return config;
}

// Three stock-like configurations; the host picks 1 (PTP) like the Switch 2.
static CFMutableDictionaryRef stock_description(void) {
    const char *ptp[] = { "PTP" };
    const char *ipod[] = { "USBAudioControl", "IapOverUsbHid" };
    const char *mux[] = { "PTP", "AppleUSBMux", "AppleUSBNCMData" };
    CFMutableDictionaryRef c1 = configuration("PTP", 1, ptp);
    CFMutableDictionaryRef c2 = configuration("iPod USB Interface", 2, ipod);
    CFMutableDictionaryRef c3 = configuration("PTP + Apple Mobile Device + NCM", 3, mux);
    CFDictionarySetValue(c2, CFSTR("AccessoryResistorSwap"), kCFBooleanTrue);
    CFDictionarySetValue(c3, CFSTR("ExtendedConfiguration"), kCFBooleanTrue);
    const void *configs[] = { c1, c2, c3 };
    CFArrayRef array = CFArrayCreate(NULL, configs, 3, &kCFTypeArrayCallBacks);
    CFMutableDictionaryRef description = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                                                   &kCFTypeDictionaryValueCallBacks);
    int vendor = 0x05ac;
    CFNumberRef vendor_number = CFNumberCreate(NULL, kCFNumberIntType, &vendor);
    CFDictionarySetValue(description, CFSTR("vendorID"), vendor_number);
    CFDictionarySetValue(description, CFSTR("ConfigurationDescriptors"), array);
    CFRelease(vendor_number); CFRelease(array); CFRelease(c1); CFRelease(c2); CFRelease(c3);
    return description;
}

// --- fakes --------------------------------------------------------------------

typedef struct {
    CFMutableDictionaryRef live;
    int selected;                 // what the host has selected right now
    int selected_after_restore;   // what the host selects after a stock publish
    int stuck_stock_publishes;    // the first stock publishes leave the controller off the bus
    int register_after_polls;     // polls before freshly published functions register
    int polls;
    int publishes, stock_publishes;
    bool refuse_publish;
} fake_usb;

static CFMutableDictionaryRef fake_live(void *context) {
    fake_usb *usb = context;
    return CFDictionaryCreateMutableCopy(NULL, 0, usb->live);
}

static bool fake_publish(void *context, CFDictionaryRef description) {
    fake_usb *usb = context;
    if (usb->refuse_publish) return false;
    usb->publishes++;
    CFRelease(usb->live);
    usb->live = CFDictionaryCreateMutableCopy(NULL, 0, description);
    usb->polls = 0;
    if (!ios2kbd_is_keyboard_description(description)) {
        usb->stock_publishes++;
        usb->selected = usb->stock_publishes > usb->stuck_stock_publishes ? usb->selected_after_restore : 0;
    }
    return true;
}

static int fake_selected(void *context) { return ((fake_usb *)context)->selected; }

static bool fake_registered(void *context, CFSetRef functions) {
    fake_usb *usb = context;
    (void)functions;
    return usb->polls++ >= usb->register_after_polls;
}

typedef struct {
    bool refuse_open;
    int host_selected_after;      // host_selected() calls before the host selects the keyboard
    int host_selected_calls;
    int timeouts_first;           // the first sends time out (host not polling yet)
    int fail_send_at;             // this send fails (host gone); -1 for never
    int interrupt_at_send;        // the signal arrives during this send; -1 for never
    volatile sig_atomic_t *interrupted;
    uint8_t sent[1024][IOS2KBD_REPORT_LENGTH];
    int sent_count;
    bool opened, closed;
} fake_keyboard;

static bool fake_open(void *context, const char **failed_step) {
    fake_keyboard *keyboard = context;
    *failed_step = "commit";
    keyboard->opened = !keyboard->refuse_open;
    return keyboard->opened;
}

static bool fake_host_selected(void *context) {
    fake_keyboard *keyboard = context;
    return keyboard->host_selected_calls++ >= keyboard->host_selected_after;
}

static ios2kbd_send_result fake_send(void *context, const uint8_t report[IOS2KBD_REPORT_LENGTH]) {
    fake_keyboard *keyboard = context;
    int index = keyboard->sent_count;
    if (index < 1024) memcpy(keyboard->sent[index], report, IOS2KBD_REPORT_LENGTH);
    keyboard->sent_count++;
    if (index == keyboard->interrupt_at_send) *keyboard->interrupted = 1;
    if (index < keyboard->timeouts_first) return IOS2KBD_SEND_TIMEOUT;
    if (index == keyboard->fail_send_at) return IOS2KBD_SEND_FAILED;
    return IOS2KBD_SEND_OK;
}

static void fake_close(void *context) { ((fake_keyboard *)context)->closed = true; }

static unsigned long slept_ms;
static void fake_sleep(unsigned milliseconds) { slept_ms += milliseconds; }
static void quiet(const char *message) { (void)message; }

typedef struct {
    fake_usb usb;
    fake_keyboard keyboard;
    volatile sig_atomic_t interrupted;
    char stock_path[256];
    ios2kbd_env env;
} harness;

static void harness_init(harness *h) {
    memset(h, 0, sizeof(*h));
    h->usb.live = stock_description();
    h->usb.selected = 1;
    h->usb.selected_after_restore = 1;
    h->keyboard.fail_send_at = -1;
    h->keyboard.interrupt_at_send = -1;
    h->keyboard.interrupted = &h->interrupted;
    char directory[] = "/tmp/ios2kbd-test-XXXXXX";
    snprintf(h->stock_path, sizeof(h->stock_path), "%s/state/stock.plist", mkdtemp(directory));
    h->env = (ios2kbd_env){
        .usb = { &h->usb, fake_live, fake_publish, fake_selected, fake_registered },
        .keyboard = { &h->keyboard, fake_open, fake_host_selected, fake_send, fake_close },
        .stock_path = h->stock_path, .sleep_ms = fake_sleep, .say = quiet, .interrupted = &h->interrupted,
    };
    slept_ms = 0;
}

static void harness_free(harness *h) { CFRelease(h->usb.live); unlink(h->stock_path); }

// The non-neutral reports, decoded back to text.
static void typed_text(const fake_keyboard *keyboard, char *out, size_t capacity) {
    size_t length = 0;
    for (int i = 0; i < keyboard->sent_count && i < 1024 && length + 1 < capacity; i++) {
        const uint8_t *r = keyboard->sent[i];
        if (r[2] == 0) continue;
        char c = '?';
        if (r[2] >= 0x04 && r[2] <= 0x1D) c = (char)((r[0] & 0x02 ? 'A' : 'a') + r[2] - 0x04);
        else if (r[2] >= 0x1E && r[2] <= 0x26) c = (char)('1' + r[2] - 0x1E);
        else if (r[2] == 0x27) c = '0';
        else if (r[2] == 0x2C) c = ' ';
        out[length++] = c;
    }
    out[length] = '\0';
}

static bool file_exists(const char *path) { return access(path, F_OK) == 0; }

// --- pure helpers -------------------------------------------------------------

static void test_keycodes(void) {
    uint8_t modifier, usage;
    CHECK(ios2kbd_keycode('a', &modifier, &usage) && usage == 0x04 && modifier == 0, "a");
    CHECK(ios2kbd_keycode('z', &modifier, &usage) && usage == 0x1D, "z");
    CHECK(ios2kbd_keycode('J', &modifier, &usage) && usage == 0x0D && modifier == 0x02, "J with shift");
    CHECK(ios2kbd_keycode('1', &modifier, &usage) && usage == 0x1E, "1");
    CHECK(ios2kbd_keycode('0', &modifier, &usage) && usage == 0x27, "0");
    CHECK(ios2kbd_keycode(' ', &modifier, &usage) && usage == 0x2C, "space");
    CHECK(ios2kbd_keycode('-', &modifier, &usage) && usage == 0x2D && modifier == 0, "minus");
    CHECK(ios2kbd_keycode('"', &modifier, &usage) && usage == 0x34 && modifier == 2, "quote");
    CHECK(ios2kbd_keycode('\n', &modifier, &usage) && usage == 0x28, "newline");
    for (int c = 32; c <= 126; c++)
        CHECK(ios2kbd_keycode((char)c, &modifier, &usage), "printable ASCII %d", c);
    CHECK(ios2kbd_text_is_typable("ios2kbd") && ios2kbd_text_is_typable("Hello World 42"), "typable");
    CHECK(!ios2kbd_text_is_typable("") && !ios2kbd_text_is_typable("a\b"), "untypable");
}

static void test_class_replies(void) {
    uint8_t reply[128];
    memset(reply, 0xAA, sizeof(reply));
    CHECK(ios2kbd_class_reply(0x21, 0x0B, 0, reply, sizeof(reply)) == 0, "SET_PROTOCOL is a zero-length success");
    CHECK(ios2kbd_class_reply(0x21, 0x09, 1, reply, sizeof(reply)) == 0, "SET_REPORT (LEDs) is a zero-length success");
    CHECK(ios2kbd_class_reply(0xA1, 0x03, 1, reply, sizeof(reply)) == 1 && reply[0] == 1, "GET_PROTOCOL answers report protocol");
    memset(reply, 0xAA, sizeof(reply));
    CHECK(ios2kbd_class_reply(0xA1, 0x01, 8, reply, sizeof(reply)) == 8, "GET_REPORT returns 8 bytes");
    bool zeros = true;
    for (int i = 0; i < 8; i++) zeros = zeros && reply[i] == 0;
    CHECK(zeros && reply[8] == 0xAA, "GET_REPORT is all keys up and writes nothing past its length");
    CHECK(ios2kbd_class_reply(0xA1, 0x01, 500, reply, sizeof(reply)) == 64, "replies are capped at 64 bytes");
    CHECK(ios2kbd_class_reply(0xA1, 0x01, 500, reply, 16) == 16, "replies are capped at the buffer size");
}

static void test_keyboard_description(void) {
    CFMutableDictionaryRef stock = stock_description();
    CFMutableDictionaryRef keyboard = ios2kbd_keyboard_description(stock, 3);
    CHECK(keyboard != NULL, "configuration 3 builds");
    CFArrayRef configs = CFDictionaryGetValue(keyboard, CFSTR("ConfigurationDescriptors"));
    CHECK(CFArrayGetCount(configs) == 1, "exactly one configuration");
    CFDictionaryRef only = CFArrayGetValueAtIndex(configs, 0);
    CFArrayRef names = CFDictionaryGetValue(only, CFSTR("Interfaces"));
    CHECK(CFArrayGetCount(names) == 2 && CFEqual(CFArrayGetValueAtIndex(names, 0), CFSTR("AppleUSBMux")) &&
          CFEqual(CFArrayGetValueAtIndex(names, 1), CFSTR("Keyboard")), "interfaces are [AppleUSBMux, Keyboard]");
    CHECK(CFEqual(CFDictionaryGetValue(only, CFSTR("Description")), CFSTR("Standard Keyboard")), "description");
    CHECK(CFDictionaryGetValue(only, CFSTR("DefaultConfiguration")) == kCFBooleanTrue, "default configuration");
    CHECK(!CFDictionaryContainsKey(only, CFSTR("ExtendedConfiguration")), "stock-only keys dropped");
    CHECK(CFDictionaryContainsKey(only, CFSTR("MaxPower")) && CFDictionaryContainsKey(only, CFSTR("Attributes")),
          "power and attributes kept from the selected configuration");
    int vendor = 0, product = 0;
    CFNumberGetValue(CFDictionaryGetValue(keyboard, CFSTR("vendorID")), kCFNumberIntType, &vendor);
    CFNumberGetValue(CFDictionaryGetValue(keyboard, CFSTR("productID")), kCFNumberIntType, &product);
    CHECK(vendor == IOS2KBD_VENDOR_ID && product == IOS2KBD_PRODUCT_ID,
          "keyboard reports its own USB IDs so Apple's Windows driver does not claim it");
    CHECK(CFDictionaryContainsKey(keyboard, CFSTR("ConfigurationDescriptors")), "device fields kept");
    CHECK(ios2kbd_is_keyboard_description(keyboard) && !ios2kbd_is_keyboard_description(stock), "detected");
    CHECK(ios2kbd_keyboard_description(stock, 0) == NULL && ios2kbd_keyboard_description(stock, 4) == NULL,
          "out-of-range configuration refused");
    CFSetRef functions = ios2kbd_function_names(stock);
    CHECK(CFSetGetCount(functions) == 5 && CFSetContainsValue(functions, CFSTR("IapOverUsbHid")),
          "function names across every configuration, deduplicated");
    CFRelease(functions); CFRelease(keyboard); CFRelease(stock);
}

// --- the run ------------------------------------------------------------------

static void test_types_and_restores(void) {
    harness h; harness_init(&h);
    h.keyboard.timeouts_first = 3;   // the host takes a moment to start polling
    ios2kbd_outcome outcome = ios2kbd_run(&h.env, "Hi 2");
    char typed[64]; typed_text(&h.keyboard, typed, sizeof(typed));
    CHECK(outcome.typing == IOS2KBD_TYPED, "typed (got %d)", outcome.typing);
    CHECK(outcome.restore == IOS2KBD_RESTORED, "restored (got %d)", outcome.restore);
    CHECK(strcmp(typed, "Hi 2") == 0, "typed text was '%s'", typed);
    CHECK(h.usb.publishes == 2 && h.usb.stock_publishes == 1, "keyboard then stock published once each");
    CHECK(!ios2kbd_is_keyboard_description(h.usb.live), "stock is live again");
    CHECK(!file_exists(h.stock_path), "saved stock removed after restoring");
    CHECK(h.keyboard.closed, "keyboard closed");
    harness_free(&h);
}

static void test_clears_autoindent(void) {
    harness h; harness_init(&h);
    h.env.clear_autoindent = true;
    ios2kbd_outcome outcome = ios2kbd_run(&h.env, "A\n B");
    CHECK(outcome.typing == IOS2KBD_TYPED && outcome.restore == IOS2KBD_RESTORED,
          "multiline file types and restores");
    uint8_t keys[16], mods[16];
    int count = 0;
    for (int i = 0; i < h.keyboard.sent_count && count < 16; i++) {
        if (h.keyboard.sent[i][2]) {
            keys[count] = h.keyboard.sent[i][2];
            mods[count++] = h.keyboard.sent[i][0];
        }
    }
    const uint8_t expected[] = {0x04, 0x28, 0x4A, 0x4D, 0x4C, 0x2C, 0x05};
    CHECK(count == 7 && memcmp(keys, expected, sizeof(expected)) == 0,
          "Enter clears inherited indentation before original leading space");
    CHECK(count == 7 && mods[3] == 2, "End selects indentation with Shift");
    harness_free(&h);
}

static void test_clears_autoindent_with_mac_keys(void) {
    harness h; harness_init(&h);
    h.env.clear_autoindent = true;
    h.env.mac_line_keys = true;
    ios2kbd_outcome outcome = ios2kbd_run(&h.env, "A\n B");
    CHECK(outcome.typing == IOS2KBD_TYPED, "typed");
    uint8_t keys[16], mods[16];
    int count = 0;
    for (int i = 0; i < h.keyboard.sent_count && count < 16; i++) {
        if (h.keyboard.sent[i][2]) {
            keys[count] = h.keyboard.sent[i][2];
            mods[count++] = h.keyboard.sent[i][0];
        }
    }
    const uint8_t expected[] = {0x04, 0x28, 0x50, 0x4C, 0x2C, 0x05};
    CHECK(count == 6 && memcmp(keys, expected, sizeof(expected)) == 0,
          "Enter, then Cmd+Shift+Left and forward Delete, then the line's own space");
    CHECK(count == 6 && mods[2] == 0x0A && mods[3] == 0, "Left arrow held with Command and Shift");
    harness_free(&h);
}

static void test_key_hold(void) {
    harness one, two;
    harness_init(&one);
    ios2kbd_run(&one.env, "A");
    unsigned long one_ms = slept_ms;
    harness_init(&two);
    ios2kbd_run(&two.env, "AB");
    CHECK(slept_ms == one_ms + 5, "each key adds only its 5 ms hold (got %lu vs %lu)", slept_ms, one_ms);
    harness_free(&one); harness_free(&two);
}

static void test_interrupt_during_typing(void) {
    harness h; harness_init(&h);
    h.keyboard.interrupt_at_send = 110;  // after the settle period, mid-text
    ios2kbd_outcome outcome = ios2kbd_run(&h.env, "abcdefghij");
    CHECK(outcome.typing == IOS2KBD_INTERRUPTED, "interrupted is not reported as typed (got %d)", outcome.typing);
    CHECK(outcome.restore == IOS2KBD_RESTORED, "still restored (got %d)", outcome.restore);
    harness_free(&h);
}

static void test_no_host(void) {
    harness h; harness_init(&h);
    h.usb.selected = 0;
    h.usb.selected_after_restore = 0;   // no cable
    ios2kbd_outcome outcome = ios2kbd_run(&h.env, "ios2kbd");
    CHECK(outcome.typing == IOS2KBD_NO_HOST && outcome.restore == IOS2KBD_NOTHING_TO_RESTORE, "no host");
    CHECK(h.usb.publishes == 2 && h.usb.stock_publishes == 1, "one reattach attempt (got %d publishes)",
          h.usb.publishes);
    CHECK(!ios2kbd_is_keyboard_description(h.usb.live) && !file_exists(h.stock_path), "stock live, nothing saved");
    CHECK(h.keyboard.sent_count == 0, "nothing typed");
    harness_free(&h);
}

static void test_stuck_off_bus_at_start(void) {
    harness h; harness_init(&h);
    h.usb.selected = 0;                 // a host is attached but the controller is off the bus
    ios2kbd_outcome outcome = ios2kbd_run(&h.env, "ab");
    char typed[16]; typed_text(&h.keyboard, typed, sizeof(typed));
    CHECK(outcome.typing == IOS2KBD_TYPED && strcmp(typed, "ab") == 0, "reattached, then typed (got %d)",
          outcome.typing);
    CHECK(outcome.restore == IOS2KBD_RESTORED, "restored (got %d)", outcome.restore);
    harness_free(&h);
}

static void test_stuck_off_bus_after_restore(void) {
    harness h; harness_init(&h);
    h.usb.stuck_stock_publishes = 1;    // the restore leaves the controller off the bus
    ios2kbd_outcome outcome = ios2kbd_run(&h.env, "ab");
    CHECK(outcome.typing == IOS2KBD_TYPED, "typed");
    CHECK(outcome.restore == IOS2KBD_RESTORED, "reattached after the restore (got %d)", outcome.restore);
    CHECK(h.usb.stock_publishes == 2 && h.usb.selected == 1, "stock published again after another description");
    CHECK(!ios2kbd_is_keyboard_description(h.usb.live) && !file_exists(h.stock_path), "stock live, saved copy removed");
    harness_free(&h);
}

static void test_leftover_keyboard_description(void) {
    harness h; harness_init(&h);
    CFMutableDictionaryRef keyboard = ios2kbd_keyboard_description(h.usb.live, 1);
    CFRelease(h.usb.live); h.usb.live = keyboard;
    ios2kbd_outcome outcome = ios2kbd_run(&h.env, "ios2kbd");
    CHECK(outcome.typing == IOS2KBD_NOT_STOCK && h.usb.publishes == 0, "refuses to save a keyboard description as stock");
    harness_free(&h);
}

static void test_never_polled(void) {
    harness h; harness_init(&h);
    h.keyboard.host_selected_after = 1 << 30;
    ios2kbd_outcome outcome = ios2kbd_run(&h.env, "ios2kbd");
    CHECK(outcome.typing == IOS2KBD_NEVER_POLLED, "never polled (got %d)", outcome.typing);
    CHECK(outcome.restore == IOS2KBD_RESTORED, "restored anyway");
    harness_free(&h);
}

static void test_host_lost_mid_typing(void) {
    harness h; harness_init(&h);
    h.keyboard.fail_send_at = 105;
    ios2kbd_outcome outcome = ios2kbd_run(&h.env, "abcdefghij");
    CHECK(outcome.typing == IOS2KBD_HOST_LOST, "host lost (got %d)", outcome.typing);
    CHECK(outcome.restore == IOS2KBD_RESTORED, "restored anyway");
    harness_free(&h);
}

static void test_setup_failure_restores(void) {
    harness h; harness_init(&h);
    h.keyboard.refuse_open = true;
    ios2kbd_outcome outcome = ios2kbd_run(&h.env, "ios2kbd");
    CHECK(outcome.typing == IOS2KBD_SETUP_FAILED && outcome.restore == IOS2KBD_RESTORED, "setup failure restores");
    harness_free(&h);
}

static void test_restore_without_host_is_quick(void) {
    harness h; harness_init(&h);
    h.usb.selected_after_restore = 0;   // cable pulled before the restore
    ios2kbd_outcome outcome = ios2kbd_run(&h.env, "ios2kbd");
    CHECK(outcome.typing == IOS2KBD_TYPED, "typed");
    CHECK(outcome.restore == IOS2KBD_RESTORED_NO_HOST, "restored without a host (got %d)", outcome.restore);
    CHECK(h.usb.stock_publishes == 2, "one reattach attempt, no more (got %d)", h.usb.stock_publishes);
    CHECK(!ios2kbd_is_keyboard_description(h.usb.live), "stock left live");
    CHECK(!file_exists(h.stock_path), "saved stock removed once the functions are back");
    harness_free(&h);
}

static void test_restore_retries_stranded_functions(void) {
    harness h; harness_init(&h);
    h.usb.register_after_polls = 1 << 30;   // functions never register
    ios2kbd_outcome outcome = ios2kbd_run(&h.env, "ios2kbd");
    CHECK(outcome.restore == IOS2KBD_RESTORE_FAILED, "restore failure reported (got %d)", outcome.restore);
    CHECK(h.usb.stock_publishes == 4, "stock republished 4 times (got %d)", h.usb.stock_publishes);
    CHECK(file_exists(h.stock_path), "saved stock kept for the next run");
    harness_free(&h);
}

static void test_leftover_is_restored_first(void) {
    harness h; harness_init(&h);
    // A previous run saved the stock description and died with the keyboard published.
    h.usb.register_after_polls = 1 << 30;
    ios2kbd_run(&h.env, "ios2kbd");
    CHECK(file_exists(h.stock_path), "precondition: leftover saved stock");
    h.usb.register_after_polls = 0;
    h.usb.stock_publishes = 0;
    h.keyboard.sent_count = 0;
    ios2kbd_outcome outcome = ios2kbd_run(&h.env, "ab");
    char typed[16]; typed_text(&h.keyboard, typed, sizeof(typed));
    CHECK(outcome.typing == IOS2KBD_TYPED && strcmp(typed, "ab") == 0, "typed after restoring the leftover");
    CHECK(h.usb.stock_publishes == 2, "leftover restored, then restored again after typing (got %d)",
          h.usb.stock_publishes);
    harness_free(&h);
}

int main(void) {
    test_keycodes();
    test_clears_autoindent();
    test_clears_autoindent_with_mac_keys();
    test_key_hold();
    test_class_replies();
    test_keyboard_description();
    test_types_and_restores();
    test_interrupt_during_typing();
    test_no_host();
    test_leftover_keyboard_description();
    test_stuck_off_bus_at_start();
    test_stuck_off_bus_after_restore();
    test_never_polled();
    test_host_lost_mid_typing();
    test_setup_failure_restores();
    test_restore_without_host_is_quick();
    test_restore_retries_stranded_functions();
    test_leftover_is_restored_first();
    if (failures) { fprintf(stderr, "%d check(s) failed\n", failures); return 1; }
    printf("ios2kbd core tests passed\n");
    return 0;
}
