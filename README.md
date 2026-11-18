# ios2kbd

Turn a jailbroken iPhone or iPad into a USB keyboard for another device.

Plug it into a computer or console over USB-C, and ios2kbd types text or a file into whatever has focus there, then puts USB back to normal.

> **Status:** command-line tool for rootless jailbreaks. A TrollStore app and a jailbreak app are planned.

## Requirements

- A USB-C iPhone or iPad on iOS 15 or later with a rootless jailbreak (see [Compatibility](#compatibility))
- A terminal with root on the device, such as NewTerm
- A USB-C data cable, with the target's keyboard layout set to US English

## Usage

Plug into the target, focus a text field there, then run on the device:

```sh
sudo ios2kbd hello world
sudo ios2kbd --file /var/mobile/bootstrap.bas
```

| Option | Effect |
| --- | --- |
| `--file PATH` | Type a file (up to 1 MiB of printable ASCII and line breaks). Clears the editor's auto-indent after each Enter. |
| `--mac` | Clear auto-indent with macOS keys. Use with `--file` when typing into a Mac. |
| `--caps-lock` | Swap upper and lower case, for targets that type letters in reversed case. |
| `--` | Treat everything after it as text. |
| `--help` | Print the usage. |

For SmileBASIC on the Switch 2, focus an empty program editor and run `sudo ios2kbd --caps-lock --file bootstrap.bas`. Then save and run it in SmileBASIC.

Notes:

- Check the typed text before you run it.
- On macOS, approve the device as a keyboard in Privacy & Security the first time.
- Ctrl-C stops typing; USB is still restored.

## Compatibility

**Tested:** iPad Pro 12.9-inch (M2) on iPadOS 17.0 with Dopamine, typing into a Switch 2, macOS and Windows.

**Should work:** other USB-C iPhones and iPads on iOS 15 or later with a rootless jailbreak, and any USB host that accepts a standard keyboard. Please open an issue if you try one.

**Not yet:** TrollStore without a jailbreak, rootful jailbreaks, and Lightning devices.

## How it works

ios2kbd saves the device's USB description, publishes a single keyboard configuration (USB ID `1209:0001`, so Apple's Windows driver doesn't claim it), types, then restores the saved description. If the host doesn't reconnect afterwards, it re-publishes to get back on the bus. An interrupted run is restored by the next one.

## Development

Needs a Mac with Xcode and SSH access to a test device.

```sh
./install.sh root@<device-address>   # build and install over SSH
./build.sh                           # build only, into build/ios2kbd
./test.sh                            # host tests with sanitizers
```

`1209:0001` is a [pid.codes](https://pid.codes) test ID. Replace it with a permanent ID in `ios2kbd.h` before a release.

ios2kbd was called ipad2jb and started in a fork of [mac2jb](https://github.com/getBoolean/mac2jb).

## License

[MIT](LICENSE)
