# midi drum sequencer for adafruit neotrellis m4

📖 **[Online User Manual & Pad Map](https://thomasf.github.io/neotrellis-seq/)**

**This is an work in progress, documents might currently reflect current, planned or scrapped features**


## Requirements

- [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/) — builds and uploads the firmware
- [Go](https://go.dev/dl/) — regenerates MIDI note mappings (`src/midimap.h`), colour palette (`src/colors.h`), and pattern presets (`src/PatternPresets.h`, `MANUAL.html`)
- [watchexec](https://github.com/watchexec/watchexec) — for the auto-rebuild loop in `dev.sh`
- `clang-format` — for `fmt.sh`

All toolchain and library versions are pinned exactly in `platformio.ini`;
PlatformIO downloads them on the first build.


## MIDI

The sequencer is a USB-MIDI device that follows the host's transport. By default,
it maps its 6 voices directly onto the 16-pad Ableton Drum Rack layout in the
General MIDI C1–D#2 range (notes 36–51 on `MIDI_CHANNEL`):
Kick (36/C1), Snare (38/D1), Closed Hi-Hat (42/F#1), Low Floor Tom (41/F1),
Low Tom (45/A1), and Crash Cymbal (49/C#2). MIDI notes can be interactively
reassigned or swapped across all 16 drum rack pads in Menu Mode (`Hold VOICE + tap step`).
In Function mode (`FN`), holding `FN + VOICE` swaps pattern sequences between voices.

A legacy sequential note mode (notes 36–41) is also supported by selecting
`Default = Consecutive` in `pkg/midimap/midimap.go` and running the generator.

The note offs go out one clock before the next step and the
note ons of that step follow in a single USB transfer, so the two never
arrive together.

| Message | Effect |
| --- | --- |
| Clock | Advances a step every `CLOCK_DIVISION` clocks (6, a sixteenth note). Ignored while stopped. |
| Start | Rewinds every voice to step 0. The first clock after Start plays it, as the spec requires. |
| Stop | Silences the voices and stops advancing. Position and phase are kept. |
| Continue | Resumes from where Stop left off. |
| Song Position Pointer | Moves every voice to the given sixteenth (modulo its pattern length), to take effect on the next Continue. |
| System Reset | Stop plus rewind to step 0. |

The device boots in the running state so a master that only sends clock, with
no Start, still drives it.


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

It watches `.cpp`, `.h`, `.ini`, `.go` and `.txt` files and re-runs `run.sh` on change.

### What the `dev` environment changes

| Flag | Effect |
| --- | --- |
| `DEBUG` | Enables `debug_print()` output on the serial port. **`setup()` blocks until a serial monitor is attached**, so a `DEBUG` build will not start on its own — this is why `run.sh` opens the monitor straight after uploading. |
| `INTERNAL_CLOCK` | Runs the sequencer from its own clock at `BPM` (see `src/config.h`) instead of following external MIDI clock, so you can work without a clock source attached. |

Note that `run.sh` runs the generator tool (`cmd/neotrellis-seq-generator`).
Edit the themes in `cmd/neotrellis-seq-generator/palette.go`, never `src/colors.h` directly.
Similarly, edit rhythm presets and kits in `patterns.txt`, never `src/PatternPresets.h` directly (`go run ./cmd/neotrellis-seq-generator` updates `src/PatternPresets.h`, `src/midimap.h`, `src/colors.h`, and `MANUAL.html`).

### Development Synth & MIDI Clock Companion

To test the sequencer with real audio and visual feedback while developing, run
in a separate terminal (linux only):


```sh
go run ./cmd/synth/
```


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

## Documentation

The full user manual, interactive pad map, pattern presets, and kit reference are available online at:
**[https://thomasf.github.io/neotrellis-seq/](https://thomasf.github.io/neotrellis-seq/)**

The manual is also available locally in [`MANUAL.html`](MANUAL.html) (designed for web browsing and formatted for black-and-white printing).


## Other scripts

| Script | Purpose |
| --- | --- |
| `fmt.sh` | Formats Go and C++ sources (`gofmt`, `clang-format`) |
| `compile_commands.sh` | Generates `compile_commands.json` for editors and language servers |
