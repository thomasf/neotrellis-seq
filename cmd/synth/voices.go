package main

import (
	"fmt"
	"github.com/thomasf/neotrellis-seq/pkg/midimap"
)

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

var NoteNames = midimap.NoteNames

func midiNoteToStr(note int) string {
	octave := (note / 12) - 1
	name := NoteNames[note%12]
	return fmt.Sprintf("%s%d", name, octave)
}

type VoiceInfo = midimap.VoiceInfo

var VoiceMap = midimap.VoiceMap
var VoiceDisplay = midimap.VoiceDisplay
