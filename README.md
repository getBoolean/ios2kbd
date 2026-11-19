# ios2kbd

Turn a jailbroken iPhone or iPad into a USB keyboard for another device.

Plug it into a computer or console over USB-C, and ios2kbd types text or a file into whatever has focus there, then puts USB back to normal.

> **Status:** command-line tool for rootless jailbreaks. A TrollStore app and a jailbreak app are planned.

## Requirements

- A USB-C iPhone or iPad on iOS 15 or later with a rootless jailbreak (see [Device support](#device-support))
- A terminal with root on the device, such as NewTerm
- A USB-C data cable, with the target's keyboard layout set to US English

## Install

Download `ios2kbd_<version>_iphoneos-arm64.deb` from the [latest release](https://github.com/getBoolean/ios2kbd/releases/latest) and install it with Sileo, Zebra or Filza, or in a root terminal on the device:

```sh
dpkg -i ios2kbd_<version>_iphoneos-arm64.deb
```

## Usage

Plug into the target, focus a text field there, then run on the device:

```sh
sudo ios2kbd hello world
sudo ios2kbd --file /var/mobile/bootstrap.bas
sudo ios2kbd --no-clear-autoindent --file /var/mobile/sample.txt
```

| Option | Effect |
| --- | --- |
| `--file PATH` | Type a file (up to 1 MiB of printable ASCII and line breaks). Clears the editor's inherited auto-indent after each non-final Enter by default. |
| `--no-clear-autoindent` | Skip clearing inherited auto-indent when typing a file. Leaves the editor's own auto-indent setting unchanged. |
| `--mac` | Clear auto-indent with macOS keys. Use with `--file` when typing into a Mac. |
| `--` | Treat everything after it as text. |
| `--help` | Print the usage. |

Place `--mac` and `--no-clear-autoindent` before `--file PATH`, in either order. With both flags, auto-indent clearing is skipped.

For SmileBASIC on the Switch 2, focus an empty program editor and run `sudo ios2kbd --file bootstrap.bas`. Then save and run it in SmileBASIC.

Notes:

- Check the typed text before you run it.
- On macOS, approve the device as a keyboard in Privacy & Security the first time.
- Ctrl-C stops typing; USB is still restored.
- If the host's Caps Lock is on, letters still come out in the right case.

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

## How it works

ios2kbd saves the device's USB description, publishes a single keyboard configuration (USB ID `1209:0001`, so Apple's Windows driver doesn't claim it), types, then restores the saved description. If the host doesn't reconnect afterwards, it re-publishes to get back on the bus. An interrupted run is restored by the next one.

## Development

Needs a Mac with Xcode and SSH access to a test device.

```sh
./install.sh root@<device-address>   # build and install over SSH
./build.sh                           # build only, into build/ios2kbd
./test.sh                            # host tests with sanitizers
./package.sh 1.2.3                   # binary, .deb and SHA256SUMS in build/release
```

CI builds and tests every push and pull request. Pushing a tag like `v1.2.3` publishes a GitHub release with the binary, the `.deb` and `SHA256SUMS`.

`1209:0001` is a [pid.codes](https://pid.codes) test ID. Replace it with a permanent ID in `ios2kbd.h` before a release.

ios2kbd was called ipad2jb and started in a fork of [mac2jb](https://github.com/getBoolean/mac2jb).

## License

[MIT](LICENSE)
