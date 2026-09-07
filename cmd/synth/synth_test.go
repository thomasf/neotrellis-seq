package main

import (
	"encoding/binary"
	"math"
	"testing"
)

func TestDefaultVoiceDrives(t *testing.T) {
	s := NewDrumSynth()
	for note, expected := range DefaultVoiceDrives {
		actual := s.VoiceDrive(note)
		if math.Abs(actual-expected) > 1e-6 {
			t.Fatalf("expected voice %d drive %f, got %f", note, expected, actual)
		}
	}
}

func TestSetAndAdjustVoiceDrive(t *testing.T) {
	s := NewDrumSynth()

	// Set Kick drive
	s.SetVoiceDrive(36, 1.5)
	if s.VoiceDrive(36) != 1.5 {
		t.Fatalf("expected kick drive 1.5, got %f", s.VoiceDrive(36))
	}

	// Hi-Hat remains untouched at its default
	if s.VoiceDrive(38) != DefaultVoiceDrives[38] {
		t.Fatalf("expected hi-hat drive untouched at %f, got %f", DefaultVoiceDrives[38], s.VoiceDrive(38))
	}

	// Clamp above max
	s.SetVoiceDrive(36, 3.5)
	if s.VoiceDrive(36) != 2.0 {
		t.Fatalf("expected drive clamped to 2.0, got %f", s.VoiceDrive(36))
	}

	// Clamp below min
	s.SetVoiceDrive(36, -1.0)
	if s.VoiceDrive(36) != 0.0 {
		t.Fatalf("expected drive clamped to 0.0, got %f", s.VoiceDrive(36))
	}

	// Adjustment
	s.SetVoiceDrive(36, 0.5)
	newVal := s.AdjustVoiceDrive(36, 0.1)
	if math.Abs(newVal-0.6) > 1e-6 {
		t.Fatalf("expected drive 0.6 after +0.1 adjustment, got %f", newVal)
	}
	// Paired alt note 42 should also match
	if math.Abs(s.VoiceDrive(42)-0.6) > 1e-6 {
		t.Fatalf("expected paired alt note 42 drive 0.6, got %f", s.VoiceDrive(42))
	}
}

func TestRenderBlockWithVoiceSaturation(t *testing.T) {
	s := NewDrumSynth()

	// High drive on kick, moderate on snare
	s.SetVoiceDrive(36, 1.2)
	s.SetVoiceDrive(37, 0.9)

	// Trigger multiple loud drum voices simultaneously (full drum kit hit)
	notes := []int{36, 37, 38, 39, 40, 42, 43}
	for _, n := range notes {
		s.NoteOn(n, 127)
	}

	buf := make([]byte, BlockSize*4)
	hasNonZero := false

	// Render several blocks
	for b := 0; b < 10; b++ {
		s.RenderBlock(buf, BlockSize)
		for i := 0; i < BlockSize; i++ {
			offset := i * 4
			sl := int16(binary.LittleEndian.Uint16(buf[offset:]))
			sr := int16(binary.LittleEndian.Uint16(buf[offset+2:]))

			if sl != 0 || sr != 0 {
				hasNonZero = true
			}

			// Verify values stay within valid int16 range (no wrap or overflow)
			if sl > 32767 || sl < -32768 {
				t.Fatalf("sample out of range: %d", sl)
			}
			if sr > 32767 || sr < -32768 {
				t.Fatalf("sample out of range: %d", sr)
			}
		}
	}

	if !hasNonZero {
		t.Fatal("expected non-zero audio samples from triggered drum voices")
	}
}

func TestSilenceOutput(t *testing.T) {
	s := NewDrumSynth()
	buf := make([]byte, BlockSize*4)

	s.RenderBlock(buf, BlockSize)
	for i := 0; i < len(buf); i++ {
		if buf[i] != 0 {
			t.Fatalf("expected absolute silence in buffer when no voices active, got byte %d at %d", buf[i], i)
		}
	}
}

func TestDCBlocker(t *testing.T) {
	dc := &dcBlocker{}

	// Step input with DC offset 0.5
	var out float64
	for i := 0; i < 44100; i++ {
		out = dc.Process(0.5)
	}

	// After 1 second (44100 samples), DC offset should be virtually 0
	if math.Abs(out) > 1e-4 {
		t.Fatalf("expected DC offset to decay to ~0, got %e", out)
	}
}
