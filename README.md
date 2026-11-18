# ios2kbd

Turn a jailbroken iPhone or iPad into a USB keyboard for another device.

Plug your iPhone or iPad into a computer, console or other USB host over USB-C, and ios2kbd makes it act as a standard USB keyboard. It types text or a file, such as a SmileBASIC payload on a Nintendo Switch 2, into whatever has focus on that device. Then it puts the port back to normal USB. You don't need a Mac or a microcontroller during the run.

> **Status:** ios2kbd is a command-line tool for jailbroken devices for now. A TrollStore app and a jailbreak app that run the same core are planned.

## Device support

### Tested

Only one device has been tested so far:

| Device | OS | Jailbreak | Targets typed into |
| --- | --- | --- | --- |
| iPad Pro 12.9-inch (6th generation, M2, `iPad14,5`) | iPadOS 17.0 | Dopamine (rootless) | Nintendo Switch 2 (SmileBASIC), macOS, Windows |


### Should work, untested

Any iPad or iPhone that meets all of these:

- **USB-C port.** USB-C iPads are the iPad Pro (2018 and later), iPad Air (4th generation and later), iPad mini (6th generation and later) and iPad (10th generation and later). USB-C iPhones are the iPhone 15 and later.
- **iOS or iPadOS 15.0 or later.** The binary is built for iOS 15.0 and up.
- **A rootless jailbreak** (installs under `/var/jb`). Your jailbreak's own compatibility list decides which chips and OS versions qualify.
- **The OS accepts a new USB device description from a root process** that has `com.apple.private.usbdevice.setdescription` and `platform-application`, and it can publish a `Keyboard` USB function. ios2kbd has no model-specific code, but only the device above has shown this works.

If you try another device, please open an issue with the model, OS version, jailbreak and result.

### Not supported yet

| Setup | Why |
| --- | --- |
| TrollStore without a jailbreak | The TrollStore app isn't built yet. |
| Rootful jailbreaks | The install path and saved-state path are fixed to `/var/jb`. |
| Lightning iPads and iPhones | Nobody knows yet whether they can act as a USB keyboard. Support depends on that. |
| iOS 14 and earlier | Below the build target. |

### Targets

Nothing in ios2kbd is specific to one target. The target can be any USB host that accepts a standard USB keyboard: Windows, Linux, macOS, Android, game consoles and so on. Only the Switch 2, macOS and Windows have been tested. Limits:

- Text must be printable ASCII and line breaks, typed with a US English layout. Set the target to that layout.
- The target must already have set up the iPhone or iPad as a normal USB device before you run ios2kbd. If it hasn't, ios2kbd stops with "no USB host has selected a configuration". Some chargers and TVs may never do this.
- It works over USB only, not Bluetooth.
- On macOS, approve the device as a keyboard in Privacy & Security.

## Requirements

- A supported device (see above).
- A terminal app with root access on the device, such as NewTerm.
- A USB-C data cable to the target. Set the target's USB keyboard layout to US English.

## Install

There's no prebuilt package yet. Until there is, build and install it from source; see [Development](#development).

## Usage

Plug the device into the target and focus a text field there (for SmileBASIC, an empty program editor). Then run ios2kbd in a terminal on the device:

```sh
sudo ios2kbd                                # types "ios2kbd" as a test
sudo ios2kbd some text
sudo ios2kbd --file /var/mobile/bootstrap.bas
```

| Option | Effect |
| --- | --- |
| `--file PATH` | Type a file: up to 1 MiB of printable ASCII and line breaks. CRLF and CR become Enter. After each Enter it clears the editor's auto-indent, so each line keeps only its own leading spaces. |
| `--caps-lock` | Swaps upper and lower case before typing. Use it when the target types letters in reversed case. It does not read or change the target's Caps Lock state. |
| `--mac` | With `--file`, clears the editor's auto-indent with macOS keys (Command+Shift+Left, then forward Delete). Without it, ios2kbd uses Home, Shift+End, Delete, which only scroll on macOS. Tested in Sublime Text. |
| `--` | Everything after it is text, even if it starts with `--`. |
| `--help` | Prints the usage. |

Tested with SmileBASIC on the Switch 2:

```sh
sudo ios2kbd --caps-lock --file /var/mobile/bootstrap.bas
```

This only types the source. Save and run it in SmileBASIC afterward.

Each key is held down for 5 ms, with USB polling at bInterval 4. There is no extra pause between keys or after Enter. Check the typed text for dropped characters before you save or run it.

An option ios2kbd doesn't recognise stops it with an error instead of being typed as text.

## How it works

1. Saves the stock USB device description to `/var/jb/var/lib/ios2kbd/stock-description.plist`.
2. Publishes one "Standard Keyboard" USB configuration and sets up the HID function. In keyboard mode the device reports USB ID `1209:0001` instead of Apple's. Otherwise Apple's Windows driver (installed with iTunes, the Apple Devices app or Windows Update) claims the whole device, and Windows never loads a keyboard driver.
3. Answers the host's HID class requests and waits until the host polls the keyboard.
4. Types the text, then restores the saved stock description and deletes the saved copy.

Sometimes, after the restore, the controller stays off the bus without the host noticing: the USB-C SuperSpeed link stays up, so the host sees no disconnect. If no host reconnects within 10 s, ios2kbd publishes a different description for 1 s and then the stock one again, which puts it back on the bus. A run that starts with no host does the same once before giving up, so a device stuck this way recovers without replugging the cable.

Ctrl-C or closing the terminal stops the typing, but the restore still runs. If a run dies before restoring, the next run restores the saved description first.

## Development

The keyboard-mode USB ID `1209:0001` is a [pid.codes](https://pid.codes) test ID. Replace it with a permanent pid.codes ID (`IOS2KBD_VENDOR_ID` and `IOS2KBD_PRODUCT_ID` in `ios2kbd.h`) before a release.

You need a Mac with Xcode, and SSH access to a jailbroken test device.

### Build and install

```sh
./install.sh root@<device-address> [ssh options...]
```

This builds `build/ios2kbd` and copies it to `/var/jb/usr/bin/ios2kbd.incoming` over SSH, then renames it into place, so a failed transfer never leaves a broken command. To build without installing, run `./build.sh [output-path]`.

The binary needs root and the entitlements in [`ios2kbd.entitlements`](ios2kbd.entitlements) (`com.apple.private.usbdevice.setdescription`, `platform-application`). `build.sh` signs them in ad hoc.

Running it over SSH works too. Use Wi-Fi, because the USB-C port is connected to the target.

### Layout

| File | Role |
| --- | --- |
| `main.c` | Arguments, file input, signals |
| `core.c` | The run and its decisions. Has no IOKit code, so tests can drive it. |
| `iokit.c` | The only code that talks to the USB device controller and the Keyboard function |
| `ios2kbd.h` | Device interfaces and outcomes |
| `tests/test_core.c` | The whole run against fake USB and keyboard devices |

### Tests

```sh
./test.sh
```

This builds `core.c` and the tests for macOS with AddressSanitizer and UndefinedBehaviorSanitizer, then runs them.

## History

ios2kbd was called ipad2jb and started inside a fork of [mac2jb](https://github.com/getBoolean/mac2jb), which is now archived. This repository keeps that history.

## License

[MIT](LICENSE)
