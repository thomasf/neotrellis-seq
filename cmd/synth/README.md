This program is only meant as a convinience during development, no attention
has been given to anythying beyond that use. Most of it is generated code which
will not be reviewed in detail as long as the observed behaviour is what I need
for development.

- **Generates precision MIDI clock** (24 PPQN, Start, Stop, Continue, Reset; default 120 BPM)
- **Provides a built-in 6-voice analog drum synth** (Kick 36, Snare 37, Hi-Hat 38, Perc1 39, Perc2 40, Perc3 41 + 6 Alt voices 42–47) with zero setup or soundfont required
- **Auto-reconnects over ALSA MIDI**: when `dev.sh` uploads new firmware, the board reboots and drops USB MIDI; `synth.sh` detects reconnection automatically and resumes immediately
- **Monitors incoming MIDI data in real time**: displays received notes, velocities, ghost/normal/accent badges, and live pad trigger meters
- **Interactive keyboard controls**: `[Space]` toggles Start/Stop, `[+/-]` adjusts tempo, `[R]` sends MIDI Reset / rewinds, `[1-6]` triggers voice sound checks, and `[Q]` quits
