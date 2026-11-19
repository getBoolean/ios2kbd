// Exercise the real CLI with fake root and device entry points; never touch USB.
#include "../iokit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static uid_t test_getuid(void) { return 0; }
static ios2kbd_env captured;
static char captured_text[128];
static int runs;
static ios2kbd_outcome test_run(const ios2kbd_env *env, const char *text) {
    captured = *env;
    snprintf(captured_text, sizeof(captured_text), "%s", text);
    runs++;
    return (ios2kbd_outcome){IOS2KBD_TYPED, IOS2KBD_RESTORED};
}
bool ios2kbd_iokit_usb(ios2kbd_usb *out) { (void)out; return true; }
void ios2kbd_iokit_keyboard(ios2kbd_keyboard *out) { (void)out; }

#define main cli_main
#define getuid test_getuid
#define ios2kbd_run test_run
#include "../main.c"
#undef main
#undef getuid
#undef ios2kbd_run

static int failures;
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); failures++; \
} } while (0)

static void check_cli(char **args, int result, bool ran, bool clear, bool mac, const char *text) {
    int argc = 0;
    while (args[argc]) argc++;
    runs = 0;
    CHECK(cli_main(argc, args) == result);
    CHECK(runs == (ran ? 1 : 0));
    if (ran) {
        CHECK(captured.clear_autoindent == clear);
        CHECK(captured.mac_line_keys == mac);
        CHECK(strcmp(captured_text, text) == 0);
    }
}

int main(void) {
    char path[] = "/tmp/ios2kbd-cli-XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) { perror("mkstemp"); return 1; }
    const char *text = "A\n B\n";
    CHECK(write(fd, text, strlen(text)) == (ssize_t)strlen(text));
    close(fd);
    char *defaults[] = {"ios2kbd", "--file", path, NULL};
    char *mac[] = {"ios2kbd", "--mac", "--file", path, NULL};
    char *disabled[] = {"ios2kbd", "--no-clear-autoindent", "--file", path, NULL};
    char *both[] = {"ios2kbd", "--mac", "--no-clear-autoindent", "--file", path, NULL};
    char *reverse[] = {"ios2kbd", "--no-clear-autoindent", "--mac", "--file", path, NULL};
    char *repeated[] = {"ios2kbd", "--no-clear-autoindent", "--mac", "--no-clear-autoindent", "--mac", "--file", path, NULL};
    check_cli(defaults, 0, true, true, false, text);
    check_cli(mac, 0, true, true, true, text);
    check_cli(disabled, 0, true, false, false, text);
    check_cli(both, 0, true, false, true, text);
    check_cli(reverse, 0, true, false, true, text);
    check_cli(repeated, 0, true, false, true, text);
    char *positional[] = {"ios2kbd", "--no-clear-autoindent", "hello", "world", NULL};
    char *literal[] = {"ios2kbd", "--", "--no-clear-autoindent", NULL};
    char *unknown[] = {"ios2kbd", "--no-clear-autoinden", NULL};
    char *missing[] = {"ios2kbd", "--no-clear-autoindent", "--file", NULL};
    char *help[] = {"ios2kbd", "--no-clear-autoindent", "--help", NULL};
    check_cli(positional, 0, true, false, false, "hello world");
    check_cli(literal, 0, true, false, false, "--no-clear-autoindent");
    check_cli(unknown, 1, false, false, false, "");
    check_cli(missing, 1, false, false, false, "");
    check_cli(help, 0, false, false, false, "");
    unlink(path);
    if (failures) return 1;
    puts("ios2kbd CLI tests passed");
    return 0;
}
