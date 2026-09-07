package main

import (
	"os"
	"strings"
	"testing"

	"github.com/thomasf/neotrellis-seq/pkg/midimap"
)

func TestMidiNoteName(t *testing.T) {
	tests := []struct {
		note     int
		expected string
	}{
		{36, "C1"},
		{37, "C#1"},
		{38, "D1"},
		{39, "D#1"},
		{40, "E1"},
		{41, "F1"},
		{42, "F#1"},
		{43, "G1"},
		{44, "G#1"},
		{45, "A1"},
		{46, "A#1"},
		{47, "B1"},
		{48, "C2"},
		{49, "C#2"},
		{50, "D2"},
		{51, "D#2"},
	}

	for _, tt := range tests {
		got := midimap.MidiNoteName(tt.note)
		if got != tt.expected {
			t.Errorf("MidiNoteName(%d) = %q, want %q", tt.note, got, tt.expected)
		}
	}
}

func TestVoiceDefinitions(t *testing.T) {
	if len(midimap.Voices) != 6 {
		t.Fatalf("expected 6 voices, got %d", len(midimap.Voices))
	}
	if len(midimap.DrumRackPads) != 16 {
		t.Fatalf("expected 16 drum rack pads, got %d", len(midimap.DrumRackPads))
	}

	seenNotes := make(map[int]bool)
	for _, v := range midimap.Voices {
		if v.PrimaryNote < 36 || v.PrimaryNote > 51 {
			t.Errorf("voice %d primary note %d out of GM drum range [36, 51]", v.VoiceIndex, v.PrimaryNote)
		}
		if seenNotes[v.PrimaryNote] {
			t.Errorf("primary note %d mapped multiple times", v.PrimaryNote)
		}
		seenNotes[v.PrimaryNote] = true
	}
}

func TestGeneratedOutputsContainKeyStrings(t *testing.T) {
	midimapH := generateMidiMapH()
	if !strings.Contains(midimapH, "DO NOT EDIT") {
		t.Errorf("midimap.h code missing DO NOT EDIT header")
	}
	if !strings.Contains(midimapH, "struct VoiceNoteMap") {
		t.Errorf("midimap.h code missing struct VoiceNoteMap")
	}
	if !strings.Contains(midimapH, "ACTIVE_VOICE_MAP") {
		t.Errorf("midimap.h code missing ACTIVE_VOICE_MAP")
	}
	if strings.Contains(midimapH, "GM_ABLETON_VOICE_MAP") {
		t.Errorf("midimap.h code should not contain GM_ABLETON_VOICE_MAP")
	}
	if strings.Contains(midimapH, "CONSECUTIVE_VOICE_MAP") {
		t.Errorf("midimap.h code should not contain CONSECUTIVE_VOICE_MAP")
	}
}

func TestGeneratedCHeadersHaveDoNotEdit(t *testing.T) {
	rootDir, err := findRootDir()
	if err != nil {
		t.Fatalf("findRootDir: %v", err)
	}

	for _, g := range subGenerators {
		if err := g.Run(rootDir); err != nil {
			t.Fatalf("subgenerator %q failed: %v", g.Name, err)
		}
	}

	headers := []string{
		"src/midimap.h",
		"src/colors.h",
		"src/PatternPresets.h",
	}

	for _, relPath := range headers {
		fullPath := rootDir + "/" + relPath
		content, err := os.ReadFile(fullPath)
		if err != nil {
			t.Errorf("reading %s: %v", relPath, err)
			continue
		}
		if !strings.Contains(string(content), "DO NOT EDIT") {
			t.Errorf("%s missing DO NOT EDIT header", relPath)
		}
	}
}

func TestGeneratedCHeadersAreFormattedWithClangFormat(t *testing.T) {
	rootDir, err := findRootDir()
	if err != nil {
		t.Fatalf("findRootDir: %v", err)
	}

	headers := []string{
		"src/midimap.h",
		"src/colors.h",
		"src/PatternPresets.h",
	}

	for _, relPath := range headers {
		fullPath := rootDir + "/" + relPath
		contentBefore, err := os.ReadFile(fullPath)
		if err != nil {
			t.Fatalf("reading %s: %v", relPath, err)
		}

		if err := runClangFormat(fullPath); err != nil {
			t.Fatalf("running clang-format on %s: %v", relPath, err)
		}

		contentAfter, err := os.ReadFile(fullPath)
		if err != nil {
			t.Fatalf("reading %s after format: %v", relPath, err)
		}

		if string(contentBefore) != string(contentAfter) {
			t.Errorf("%s was not properly formatted with clang-format during generation", relPath)
		}
	}
}

func TestAllSubGenerators(t *testing.T) {
	rootDir, err := findRootDir()
	if err != nil {
		t.Fatalf("findRootDir: %v", err)
	}

	for _, g := range subGenerators {
		if err := g.Run(rootDir); err != nil {
			t.Errorf("subgenerator %q failed: %v", g.Name, err)
		}
	}
}

func TestManualGeneratedHeaderComment(t *testing.T) {
	// 1. Check template does NOT start with GENERATED FILE DO NOT EDIT
	tmplBytes, err := manualTemplatesFS.ReadFile("templates/manual.html.tmpl")
	if err != nil {
		t.Fatalf("reading embedded manual.html.tmpl: %v", err)
	}
	if strings.HasPrefix(strings.TrimSpace(string(tmplBytes)), "<!-- GENERATED FILE DO NOT EDIT") {
		t.Errorf("manual.html.tmpl must not start with GENERATED FILE DO NOT EDIT comment")
	}

	// 2. Check generated MANUAL.html DOES start with GENERATED FILE DO NOT EDIT
	rootDir, err := findRootDir()
	if err != nil {
		t.Fatalf("findRootDir: %v", err)
	}
	manualBytes, err := os.ReadFile(rootDir + "/MANUAL.html")
	if err != nil {
		t.Fatalf("reading MANUAL.html: %v", err)
	}
	if !strings.HasPrefix(string(manualBytes), "<!-- GENERATED FILE DO NOT EDIT") {
		t.Errorf("rendered MANUAL.html must have GENERATED FILE DO NOT EDIT header comment")
	}
}
