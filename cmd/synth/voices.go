package main

import "fmt"

// =============================================================================
// Constants & Voice Data
// =============================================================================
const (
	SampleRate = 44100
	BlockSize  = 512

	// ALSA Event types
	EventNoteOn   = 6
	EventNoteOff  = 7
	EventStart    = 30
	EventContinue = 31
	EventStop     = 32
	EventClock    = 36
	EventReset    = 41
)

var NoteNames = []string{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}

func midiNoteToStr(note int) string {
	octave := (note / 12) - 1
	name := NoteNames[note%12]
	return fmt.Sprintf("%s%d", name, octave)
}

type VoiceInfo struct {
	Tag      string
	Name     string
	FullName string
}

var VoiceMap = map[int]VoiceInfo{
	36: {"V0", "Kick", "Kick / Bass Drum"},
	37: {"V1", "Snare", "Snare / Rimshot"},
	38: {"V2", "HiHat", "Closed Hi-Hat"},
	39: {"V3", "Perc1", "Low Perc / Tom"},
	40: {"V4", "Perc2", "Mid Perc / Woodblock"},
	41: {"V5", "Perc3", "High Perc / Shaker"},
	42: {"V0-Alt", "SubKick", "808 Sub Kick"},
	43: {"V1-Alt", "Clap", "Hand Clap"},
	44: {"V2-Alt", "PedalHat", "Pedal Hi-Hat"},
	45: {"V3-Alt", "HighTom", "High Tom"},
	46: {"V4-Alt", "Cowbell", "808 Cowbell"},
	47: {"V5-Alt", "Zap", "Analog Synth Zap"},
}

var VoiceDisplay = []struct {
	Tag  string
	Name string
	Note int
}{
	{"V0", "Kick ", 36},
	{"V1", "Snare", 37},
	{"V2", "HiHat", 38},
	{"V3", "Perc1", 39},
	{"V4", "Perc2", 40},
	{"V5", "Perc3", 41},
}
