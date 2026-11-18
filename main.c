// ios2kbd command line:  sudo ios2kbd [TEXT...]
// TEXT defaults to "ios2kbd"; --file PATH reads ASCII text from a separate file.
// Needs root and the entitlements in ios2kbd.entitlements.

#include "iokit.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define USAGE "usage: ios2kbd [--mac] [--] [TEXT...]\n" \
              "       ios2kbd [--mac] --file PATH\n" \
              "TEXT or the file: 1-%d bytes of printable ASCII and newlines.\n"
#define STOCK_PATH "/var/jb/var/lib/ios2kbd/stock-description.plist"

static volatile sig_atomic_t interrupted = 0;
static void on_signal(int signal_number) { (void)signal_number; interrupted = 1; }

static void say(const char *message) { printf("ios2kbd: %s\n", message); fflush(stdout); }
static void sleep_ms(unsigned milliseconds) { usleep(milliseconds * 1000); }

static bool join_arguments(int argc, char **argv, char *text, size_t capacity) {
    if (argc < 2) { snprintf(text, capacity, "ios2kbd"); return true; }
    text[0] = '\0';
    for (int i = 1; i < argc; i++) {
        if (strlen(text) + strlen(argv[i]) + 2 > capacity) return false;
        if (i > 1) strcat(text, " ");
        strcat(text, argv[i]);
    }
    return true;
}

static bool read_file(const char *path, char *text) {
    FILE *file = fopen(path, "rb");
    if (!file) { perror(path); return false; }
    size_t count = fread(text, 1, IOS2KBD_MAX_TEXT + 1, file);
    bool ok = !ferror(file) && count > 0 && count <= IOS2KBD_MAX_TEXT &&
              memchr(text, 0, count) == NULL;
    fclose(file);
    if (!ok) return false;
    // Normalize Windows and classic Mac line endings to one Enter per line.
    size_t out = 0;
    for (size_t i = 0; i < count; i++) {
        if (text[i] == '\r') {
            text[out++] = '\n';
            if (i + 1 < count && text[i + 1] == '\n') i++;
        } else text[out++] = text[i];
    }
    text[out] = 0;
    return true;
}

int main(int argc, char **argv) {
    static char text[IOS2KBD_MAX_TEXT + 2];
    bool mac = false;
    while (argc > 1 && strcmp(argv[1], "--mac") == 0) { mac = true; argc--; argv++; }
    // A mistyped option must never be typed into the host as text. Text that
    // starts with "--" goes after a "--" separator.
    bool literal = argc > 1 && strcmp(argv[1], "--") == 0;
    if (literal) { argc--; argv++; }
    if (!literal && argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        printf(USAGE, IOS2KBD_MAX_TEXT);
        return 0;
    }
    bool file_mode = !literal && argc > 1 && strcmp(argv[1], "--file") == 0;
    if (!literal && !file_mode && argc > 1 && strncmp(argv[1], "--", 2) == 0) {
        fprintf(stderr, "ios2kbd: unknown option %s\n", argv[1]);
        fprintf(stderr, USAGE, IOS2KBD_MAX_TEXT);
        return 1;
    }
    bool loaded = file_mode ? argc == 3 && read_file(argv[2], text)
                            : join_arguments(argc, argv, text, sizeof(text));
    if (!loaded || !ios2kbd_text_is_typable(text)) {
        fprintf(stderr, USAGE, IOS2KBD_MAX_TEXT);
        return 1;
    }
    if (getuid() != 0) { fprintf(stderr, "ios2kbd: run as root: sudo ios2kbd\n"); return 1; }

    // Interrupts end the typing early but never skip the restore. A closed
    // terminal or SSH session must not kill the process with SIGPIPE either.
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGHUP, on_signal);
    signal(SIGPIPE, SIG_IGN);

    ios2kbd_env env = {
        .clear_autoindent = file_mode, .mac_line_keys = mac,
        .stock_path = STOCK_PATH, .sleep_ms = sleep_ms, .say = say,
        .interrupted = &interrupted,
    };
    if (!ios2kbd_iokit_usb(&env.usb)) { say("USB device controller unavailable"); return 1; }
    ios2kbd_iokit_keyboard(&env.keyboard);

    ios2kbd_outcome outcome = ios2kbd_run(&env, text);
    if (outcome.typing == IOS2KBD_TYPED) printf("ios2kbd: typed %zu bytes\n", strlen(text));
    bool restored = outcome.restore == IOS2KBD_RESTORED || outcome.restore == IOS2KBD_RESTORED_NO_HOST;
    return outcome.typing == IOS2KBD_TYPED && restored ? 0 : 1;
}
