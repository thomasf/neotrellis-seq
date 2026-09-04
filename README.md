# midi drum sequencer for adafruit neotrellis m4


**This is an work in progress, documents might currently reflect current, planned or scrapped features**


## Requirements

- [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/) — builds and uploads the firmware
- [Go](https://go.dev/dl/) — regenerates the colour palette (`src/colors.h`)
- [watchexec](https://github.com/watchexec/watchexec) — for the auto-rebuild loop in `dev.sh`
- `clang-format` — for `fmt.sh`

All toolchain and library versions are pinned exactly in `platformio.ini`;
PlatformIO downloads them on the first build.


## Development on the device

Connect the Trellis M4 over USB and run:

```sh
./run.sh
```
The upload needs no button press — the board is reset into its bootloader by a
1200 baud touch on the USB serial port.

To rebuild and re-upload automatically on every save, use:

```sh
./dev.sh
```

It watches `.cpp`, `.h`, `.ini` and `.go` files and re-runs `run.sh` on change.

### What the `dev` environment changes

| Flag | Effect |
| --- | --- |
| `DEBUG` | Enables `debug_print()` output on the serial port. **`setup()` blocks until a serial monitor is attached**, so a `DEBUG` build will not start on its own — this is why `run.sh` opens the monitor straight after uploading. |
| `INTERNAL_CLOCK` | Runs the sequencer from its own clock at `BPM` (see `src/config.h`) instead of following external MIDI clock, so you can work without a clock source attached. |

Note that `run.sh` rewrites the tracked file `src/colors.h` on every iteration.
Edit the themes in `contrib/palette.go`, never `src/colors.h` directly.


## Building and installing a release

```sh
./release.sh
```

This builds the `default` environment (no `DEBUG`, no `INTERNAL_CLOCK`) and
converts `firmware.bin` into `neotrellis-seq.uf2` — a SAMD51 UF2 image flashed at
`0x4000`, i.e. above the bootloader. The resulting `.uf2` is committed to the
repository, so a release build is normally followed by committing it.

To install it on the device:

1. Double-tap the reset button on the back of the Trellis M4. The board enters
   its bootloader and mounts as a USB drive named `TRELM4BOOT`.
2. Copy `neotrellis-seq.uf2` onto that drive.
3. The board flashes itself and reboots into the new firmware. The drive
   disappears on its own — there is nothing to eject.

This is the only step anyone needs to install a release; PlatformIO is not
required on the target machine.

Alternatively, with the toolchain available, `platformio run -t upload` uploads
the release build over USB directly.


## Other scripts

| Script | Purpose |
| --- | --- |
| `fmt.sh` | Formats Go and C++ sources (`gofmt`, `clang-format`) |
| `compile_commands.sh` | Generates `compile_commands.json` for editors and language servers |
