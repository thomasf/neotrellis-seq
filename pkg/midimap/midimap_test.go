package midimap

import (
	"testing"
)

func TestMidiNoteName(t *testing.T) {
	cases := []struct {
		note int
		want string
	}{
		{36, "C1"},
		{38, "D1"},
		{42, "F#1"},
		{48, "C2"},
		{51, "D#2"},
	}
	for _, c := range cases {
		if got := MidiNoteName(c.note); got != c.want {
			t.Errorf("MidiNoteName(%d) = %q; want %q", c.note, got, c.want)
		}
	}
}

func TestVoiceMapAndDisplay(t *testing.T) {
	if len(VoiceMap) != 16 {
		t.Fatalf("expected 16 entries in VoiceMap, got %d", len(VoiceMap))
	}

	// Verify all voices are present in VoiceMap
	for _, v := range Voices {
		info, ok := VoiceMap[v.PrimaryNote]
		if !ok {
			t.Errorf("primary note %d for voice %d not found in VoiceMap", v.PrimaryNote, v.VoiceIndex)
		}
		if info.Name != v.PrimaryTag {
			t.Errorf("expected VoiceMap[%d].Name to be %q, got %q", v.PrimaryNote, v.PrimaryTag, info.Name)
		}
	}

	// Verify VoiceDisplay has 6 elements corresponding to Voices
	if len(VoiceDisplay) != 6 {
		t.Fatalf("expected 6 entries in VoiceDisplay, got %d", len(VoiceDisplay))
	}
	for i, vd := range VoiceDisplay {
		if vd.Note != Voices[i].PrimaryNote {
			t.Errorf("VoiceDisplay[%d].Note = %d; want %d", i, vd.Note, Voices[i].PrimaryNote)
		}
		if vd.Name != Voices[i].PrimaryDisplay {
			t.Errorf("VoiceDisplay[%d].Name = %q; want %q", i, vd.Name, Voices[i].PrimaryDisplay)
		}
	}
}
