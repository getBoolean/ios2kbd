# ipad2jb

Type payloads into a console from a jailbroken iPad or iPhone with USB-C.

ipad2jb turns the device's USB-C port into a USB keyboard for one run. It types a payload file, such as a SmileBASIC bootstrap, into whatever the device is plugged into, then puts the port back to normal USB. You don't need a Mac or a microcontroller during the run.

> **Status:** ipad2jb is a command-line tool for jailbroken devices for now. A TrollStore app and a jailbreak app that run the same core are planned.

## Device support

### Tested

Only one device has been tested so far:

| Device | OS | Jailbreak | Targets typed into |
| --- | --- | --- | --- |
| iPad Pro 12.9-inch (6th generation, M2, `iPad14,5`) | iPadOS 17.0 | Dopamine (rootless) | Nintendo Switch 2 (SmileBASIC), macOS |

### Should work, untested

Any iPad or iPhone that meets all of these:

- **USB-C port.** USB-C iPads are the iPad Pro (2018 and later), iPad Air (4th generation and later), iPad mini (6th generation and later) and iPad (10th generation and later). USB-C iPhones are the iPhone 15 and later.
- **iOS or iPadOS 15.0 or later.** The binary is built for iOS 15.0 and up.
- **A rootless jailbreak** (installs under `/var/jb`) with root over SSH. Your jailbreak's own compatibility list decides which chips and OS versions qualify.
- **The OS accepts a new USB device description from a root process** that has `com.apple.private.usbdevice.setdescription` and `platform-application`, and it can publish a `Keyboard` USB function. ipad2jb has no model-specific code, but only the device above has shown this works.

If you try another device, please open an issue with the model, OS version, jailbreak and result.

### Not supported yet

| Setup | Why |
| --- | --- |
| TrollStore without a jailbreak | The TrollStore app isn't built yet. |
| Rootful jailbreaks | The install path and saved-state path are fixed to `/var/jb`. |
| Lightning iPads and iPhones | Nobody knows yet whether they can act as a USB keyboard. Support depends on that. |
| iOS 14 and earlier | Below the build target. |

### Targets

The target can be anything that accepts a standard USB keyboard with a US English layout. The Switch 2 and macOS have been tested. On macOS, approve the device as a keyboard in Privacy & Security.

## Requirements

- A supported device (see above), with SSH access.
- A Mac with Xcode to build it.
- A USB-C data cable to the target. Set the target's USB keyboard layout to US English.

The binary needs root and the entitlements in [`ipad2jb.entitlements`](ipad2jb.entitlements). `build.sh` signs them in ad hoc.

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
