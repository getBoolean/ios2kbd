# ipad2jb

Type payloads into a console from a jailbroken iPad or iPhone.

ipad2jb turns the device's USB-C port into a USB keyboard for one run. It types a payload file, such as a SmileBASIC bootstrap, into whatever the device is plugged into, then puts the port back to normal USB. You don't need a Mac or a microcontroller during the run.

Tested on an M2 iPad Pro (iPadOS 17.0, Dopamine rootless), typing into a Nintendo Switch 2 and into macOS.

> **Status:** ipad2jb is a command-line tool for jailbroken devices for now. A TrollStore app and a jailbreak app that run the same core are planned.

## Requirements

- A jailbroken iPad or iPhone with USB-C (rootless `/var/jb`), with SSH access.
- A Mac with Xcode to build it.
- A USB-C data cable to the target. Set the target's USB keyboard layout to US English.

The binary needs root and the entitlements in [`ipad2jb.entitlements`](ipad2jb.entitlements) (`com.apple.private.usbdevice.setdescription`, `platform-application`). `build.sh` signs them in ad hoc.

## Install

From the Mac:

```sh
./install.sh root@<device-address>
```

This builds `build/ipad2jb`, copies it to `/var/jb/usr/bin/ipad2jb.incoming` over SSH and renames it into place, so a failed transfer never leaves a broken command. To build without installing, run `./build.sh [output-path]`.

## Usage

Plug the device into the target and focus a text field there (for SmileBASIC, an empty program editor). Then, on the device:

```sh
sudo ipad2jb                                # types "ipad2jb" as a test
sudo ipad2jb some text
sudo ipad2jb --file /var/mobile/bootstrap.bas
```

| Option | Effect |
| --- | --- |
| `--file PATH` | Type a file: up to 1 MiB of printable ASCII and line breaks. CRLF and CR become Enter. After each Enter it clears the editor's auto-indent, so each line keeps only its own leading spaces. |
| `--fast` | USB bInterval 6, 10 ms key hold, 5 ms gap between characters, 100 ms after Enter. Check that the editor keeps up. |
| `--turbo` | Experimental. bInterval 4, 5 ms key hold, no gap between characters, 100 ms after Enter. Check for dropped characters before you save or run anything. |
| `--caps-lock` | Swaps upper and lower case before typing. Use it when the target types letters in reversed case. It does not read or change the target's Caps Lock state. |

The tested SmileBASIC run was:

```sh
sudo ipad2jb --turbo --caps-lock --file /var/mobile/bootstrap.bas
```

This only types the source. Save and run it in SmileBASIC afterward.

On macOS, approve the device as a keyboard in Privacy & Security, or macOS holds the keystrokes back.

## How it works

1. Saves the stock USB device description to `/var/jb/var/lib/ipad2jb/stock-description.plist`.
2. Publishes one "Standard Keyboard" USB configuration and sets up the HID function.
3. Answers the host's HID class requests and waits until the host polls the keyboard.
4. Types the text, then restores the saved stock description and deletes the saved copy.

Ctrl-C, SIGTERM, SIGHUP and a closed SSH session stop the typing but still run the restore. If a run dies before restoring, the next run restores the saved description first.

## Layout

| File | Role |
| --- | --- |
| `main.c` | Arguments, file input, signals |
| `core.c` | The run and its decisions. Has no IOKit code, so tests can drive it. |
| `iokit.c` | The only code that talks to the USB device controller and the Keyboard function |
| `ipad2jb.h` | Device interfaces and outcomes |
| `tests/test_core.c` | The whole run against fake USB and keyboard devices |

## Tests

```sh
./test.sh
```

This builds `core.c` and the tests for macOS with AddressSanitizer and UndefinedBehaviorSanitizer, then runs them.

## History

ipad2jb started inside a fork of [mac2jb](https://github.com/getBoolean/mac2jb), which is now archived. This repository keeps that history.

## License

[MIT](LICENSE)
