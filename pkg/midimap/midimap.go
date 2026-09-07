package midimap

import "fmt"

// NoteNames contains the standard 12 chromatic note names.
var NoteNames = []string{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}

// MidiNoteName returns the Ableton/GM note name formatted as "C1", "F#1", "C2", etc.
// In Ableton Live and General MIDI drum rack conventions, note 36 is C1 and note 51 is D#2.
func MidiNoteName(note int) string {
	octave := (note / 12) - 2
	name := NoteNames[note%12]
	return fmt.Sprintf("%s%d", name, octave)
}

// VoiceInfo contains tag and naming information for a MIDI note, used by synth / TUI.
type VoiceInfo struct {
	Tag      string
	Name     string
	FullName string
}

// VoiceDisplayInfo contains fixed-width display info for a voice channel in the synth TUI.
type VoiceDisplayInfo struct {
	Tag  string
	Name string
	Note int
}

// VoiceMapDef holds the definition for a single drum machine voice (0..5).
type VoiceMapDef struct {
	VoiceIndex        int    // 0..5
	HardwarePad       int    // Pad number on Trellis M4 (14, 15, 22, 23, 30, 31)
	PrimaryNote       int    // MIDI note number (e.g. 36)
	PrimaryTag        string // Short tag for synth (e.g. "Kick")
	PrimaryDisplay    string // Fixed-width display name for synth TUI (e.g. "Kick ")
	PrimaryFullName   string // Ableton / GM full instrument name (e.g. "Bass Drum 1 (Kick)")
	ConfigPrimaryName string // Name used in config.h comments (e.g. "Kick", "Closed Hi-Hat")
	RoleSummary       string // Title for table column (e.g. "Kick Drum")
	RoleDesc          string // Description text for manual
}

// DrumRackPadDef holds information for one of the 16 pads (notes 36..51) in the Ableton Drum Rack.
type DrumRackPadDef struct {
	PadNumber int    // 1..16
	Note      int    // 36..51
	Name      string // GM instrument name
	Tag       string // Tag for synth VoiceMap (e.g. "GM-40")
	ShortName string // Short name for synth VoiceMap (e.g. "Snare2")
}

// Setting represents a complete MIDI voice mapping configuration.
type Setting struct {
	Name         string
	Description  string
	Voices       []VoiceMapDef
	DrumRackPads []DrumRackPadDef
}

// VoiceMap builds a lookup map from MIDI note to VoiceInfo for the synth engine and TUI.
func (s Setting) VoiceMap() map[int]VoiceInfo {
	vm := make(map[int]VoiceInfo, len(s.DrumRackPads))
	for _, p := range s.DrumRackPads {
		foundPrimary := -1
		for _, v := range s.Voices {
			if v.PrimaryNote == p.Note {
				foundPrimary = v.VoiceIndex
				break
			}
		}

		if foundPrimary >= 0 {
			v := s.Voices[foundPrimary]
			vm[p.Note] = VoiceInfo{Tag: fmt.Sprintf("V%d", v.VoiceIndex), Name: v.PrimaryTag, FullName: v.PrimaryFullName}
		} else {
			vm[p.Note] = VoiceInfo{Tag: p.Tag, Name: p.ShortName, FullName: p.Name}
		}
	}
	return vm
}

// VoiceDisplay builds the ordered display entries for the 6 voices in the synth TUI.
func (s Setting) VoiceDisplay() []VoiceDisplayInfo {
	vd := make([]VoiceDisplayInfo, len(s.Voices))
	for i, v := range s.Voices {
		vd[i] = VoiceDisplayInfo{
			Tag:  fmt.Sprintf("V%d", v.VoiceIndex),
			Name: v.PrimaryDisplay,
			Note: v.PrimaryNote,
		}
	}
	return vd
}

// Standard 16 pads covering C1 to D#2 (notes 36 to 51) in General MIDI / Ableton Drum Racks.
var gmDrumRackPads = []DrumRackPadDef{
	{PadNumber: 1, Note: 36, Name: "Bass Drum 1 (Kick)", Tag: "GM-36", ShortName: "Kick"},
	{PadNumber: 2, Note: 37, Name: "Side Stick / Rim", Tag: "GM-37", ShortName: "Rim"},
	{PadNumber: 3, Note: 38, Name: "Acoustic Snare", Tag: "GM-38", ShortName: "Snare"},
	{PadNumber: 4, Note: 39, Name: "Hand Clap", Tag: "GM-39", ShortName: "Clap"},
	{PadNumber: 5, Note: 40, Name: "Electric Snare", Tag: "GM-40", ShortName: "Snare2"},
	{PadNumber: 6, Note: 41, Name: "Low Floor Tom", Tag: "GM-41", ShortName: "FloorTom"},
	{PadNumber: 7, Note: 42, Name: "Closed Hi-Hat", Tag: "GM-42", ShortName: "HiHat"},
	{PadNumber: 8, Note: 43, Name: "High Floor Tom", Tag: "GM-43", ShortName: "HiFloor"},
	{PadNumber: 9, Note: 44, Name: "Pedal Hi-Hat", Tag: "GM-44", ShortName: "PedalHat"},
	{PadNumber: 10, Note: 45, Name: "Low Tom", Tag: "GM-45", ShortName: "LowTom"},
	{PadNumber: 11, Note: 46, Name: "Open Hi-Hat", Tag: "GM-46", ShortName: "OpenHat"},
	{PadNumber: 12, Note: 47, Name: "Low-Mid Tom", Tag: "GM-47", ShortName: "MidTom"},
	{PadNumber: 13, Note: 48, Name: "Hi-Mid Tom", Tag: "GM-48", ShortName: "HiMid"},
	{PadNumber: 14, Note: 49, Name: "Crash Cymbal 1", Tag: "GM-49", ShortName: "Crash"},
	{PadNumber: 15, Note: 50, Name: "High Tom", Tag: "GM-50", ShortName: "HighTom"},
	{PadNumber: 16, Note: 51, Name: "Ride Cymbal 1", Tag: "GM-51", ShortName: "Ride"},
}

// GMAbleton maps the 6 sequencer voices onto standard Ableton 4x4 drum rack pads (C1-D#2).
var GMAbleton = Setting{
	Name:         "gm_ableton",
	Description:  "Ableton Drum Rack / General MIDI voice mapping (C1–D#2, notes 36–51)",
	DrumRackPads: gmDrumRackPads,
	Voices: []VoiceMapDef{
		{
			VoiceIndex:        0,
			HardwarePad:       14,
			PrimaryNote:       36,
			PrimaryTag:        "Kick",
			PrimaryDisplay:    "Kick ",
			PrimaryFullName:   "Bass Drum 1 (Kick)",
			ConfigPrimaryName: "Kick",
			RoleSummary:       "Kick Drum",
			RoleDesc:          "4-on-the-floor anchors, syncopated drops, and primary downbeats.",
		},
		{
			VoiceIndex:        1,
			HardwarePad:       15,
			PrimaryNote:       38,
			PrimaryTag:        "Snare",
			PrimaryDisplay:    "Snare",
			PrimaryFullName:   "Acoustic Snare",
			ConfigPrimaryName: "Snare",
			RoleSummary:       "Snare",
			RoleDesc:          "Classic backbeats on steps 5 & 13, ghost flams, and syncopated rolls.",
		},
		{
			VoiceIndex:        2,
			HardwarePad:       22,
			PrimaryNote:       42,
			PrimaryTag:        "HiHat",
			PrimaryDisplay:    "HiHat",
			PrimaryFullName:   "Closed Hi-Hat",
			ConfigPrimaryName: "Closed Hi-Hat",
			RoleSummary:       "Closed Hi-Hat",
			RoleDesc:          "Driving closed sixteenths, swung shuffles, and steady timekeeping.",
		},
		{
			VoiceIndex:        3,
			HardwarePad:       23,
			PrimaryNote:       41,
			PrimaryTag:        "FloorTom",
			PrimaryDisplay:    "Perc1",
			PrimaryFullName:   "Low Floor Tom",
			ConfigPrimaryName: "Low Floor Tom",
			RoleSummary:       "Low Floor Tom",
			RoleDesc:          "Deep floor tom thuds, sub-punches, and low rhythmic syncopation.",
		},
		{
			VoiceIndex:        4,
			HardwarePad:       30,
			PrimaryNote:       45,
			PrimaryTag:        "LowTom",
			PrimaryDisplay:    "Perc2",
			PrimaryFullName:   "Low Tom",
			ConfigPrimaryName: "Low Tom",
			RoleSummary:       "Low Tom",
			RoleDesc:          "Melodic tom fills, woodblock-style accents, and syncopated stabs.",
		},
		{
			VoiceIndex:        5,
			HardwarePad:       31,
			PrimaryNote:       49,
			PrimaryTag:        "Crash",
			PrimaryDisplay:    "Perc3",
			PrimaryFullName:   "Crash Cymbal 1",
			ConfigPrimaryName: "Crash Cymbal",
			RoleSummary:       "Crash Cymbal",
			RoleDesc:          "Explosive crash accents on downbeats and turnarounds.",
		},
	},
}

// Consecutive is the legacy mapping: 6 sequential notes (36-41).
var Consecutive = Setting{
	Name:         "consecutive",
	Description:  "Legacy consecutive mapping (notes 36-41)",
	DrumRackPads: gmDrumRackPads,
	Voices: []VoiceMapDef{
		{VoiceIndex: 0, HardwarePad: 14, PrimaryNote: 36, PrimaryTag: "Kick", PrimaryDisplay: "Kick ", PrimaryFullName: "Kick", ConfigPrimaryName: "Voice 0", RoleSummary: "Voice 0", RoleDesc: "Default note: 36"},
		{VoiceIndex: 1, HardwarePad: 15, PrimaryNote: 37, PrimaryTag: "Snare", PrimaryDisplay: "Snare", PrimaryFullName: "Snare", ConfigPrimaryName: "Voice 1", RoleSummary: "Voice 1", RoleDesc: "Default note: 37"},
		{VoiceIndex: 2, HardwarePad: 22, PrimaryNote: 38, PrimaryTag: "HiHat", PrimaryDisplay: "HiHat", PrimaryFullName: "Hi-Hat", ConfigPrimaryName: "Voice 2", RoleSummary: "Voice 2", RoleDesc: "Default note: 38"},
		{VoiceIndex: 3, HardwarePad: 23, PrimaryNote: 39, PrimaryTag: "Perc1", PrimaryDisplay: "Perc1", PrimaryFullName: "Percussion 1", ConfigPrimaryName: "Voice 3", RoleSummary: "Voice 3", RoleDesc: "Default note: 39"},
		{VoiceIndex: 4, HardwarePad: 30, PrimaryNote: 40, PrimaryTag: "Perc2", PrimaryDisplay: "Perc2", PrimaryFullName: "Percussion 2", ConfigPrimaryName: "Voice 4", RoleSummary: "Voice 4", RoleDesc: "Default note: 40"},
		{VoiceIndex: 5, HardwarePad: 31, PrimaryNote: 41, PrimaryTag: "Perc3", PrimaryDisplay: "Perc3", PrimaryFullName: "Percussion 3", ConfigPrimaryName: "Voice 5", RoleSummary: "Voice 5", RoleDesc: "Default note: 41"},
	},
}

// Default is the single active MIDI voice mapping setting.
// Only one setting is active at a time; change this variable in Go source to switch mappings.
var Default = GMAbleton

// Package-level shortcuts accessing the active Default setting:
var (
	Voices       = Default.Voices
	DrumRackPads = Default.DrumRackPads
	VoiceMap     = Default.VoiceMap()
	VoiceDisplay = Default.VoiceDisplay()
)
