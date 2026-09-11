package main

import (
	"bufio"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
)

type VoicePreset struct {
	Pad        int
	Name       string
	Desc       string
	Display    string
	Velocities [16]uint8
}

type Kit struct {
	Pad    int
	Name   string
	Genre  string
	Desc   string
	Voices [6][16]uint8
}

type PatternBank struct {
	Voices [6][]VoicePreset
	Kits   []Kit
}

func generatePatterns(rootDir string) error {
	patternsPath := filepath.Join(rootDir, "patterns.txt")
	presetsHPath := filepath.Join(rootDir, "src", "PatternPresets.h")

	fmt.Printf("Reading pattern specs from %s...\n", patternsPath)
	bank, err := parsePatternsFile(patternsPath)
	if err != nil {
		return fmt.Errorf("parsing %s: %w", patternsPath, err)
	}

	if err := bank.validate(); err != nil {
		return fmt.Errorf("validating patterns: %w", err)
	}

	fmt.Printf("Updating %s...\n", presetsHPath)
	if err := updatePatternPresetsH(presetsHPath, bank); err != nil {
		return fmt.Errorf("updating %s: %w", presetsHPath, err)
	}

	return nil
}

func parsePatternsFile(filename string) (*PatternBank, error) {
	file, err := os.Open(filename)
	if err != nil {
		return nil, err
	}
	defer file.Close()

	bank := &PatternBank{}
	for i := 0; i < 6; i++ {
		bank.Voices[i] = make([]VoicePreset, 0, 16)
	}
	bank.Kits = make([]Kit, 0, 16)

	scanner := bufio.NewScanner(file)
	lineNum := 0

	var (
		currentSectionType string // "voice" or "kit"
		currentVoiceIdx    int
		currentKit         *Kit
		currentPreset      *VoicePreset
	)

	voiceHeaderRegex := regexp.MustCompile(`^\[voice\.([0-5]):?\s*(.*)\]$`)
	kitHeaderRegex := regexp.MustCompile(`^\[kit\.([0-9]+)\]$`)
	presetHeaderRegex := regexp.MustCompile(`^([0-9]+):\s*([^|]+)(?:\|\s*(.*))?$`)

	flushCurrentKit := func() {
		if currentKit != nil {
			bank.Kits = append(bank.Kits, *currentKit)
			currentKit = nil
		}
	}

	flushCurrentPreset := func() {
		if currentPreset != nil {
			bank.Voices[currentVoiceIdx] = append(bank.Voices[currentVoiceIdx], *currentPreset)
			currentPreset = nil
		}
	}

	for scanner.Scan() {
		lineNum++
		rawLine := scanner.Text()
		line := strings.TrimSpace(rawLine)

		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}

		// Section header: [voice.X: Name]
		if m := voiceHeaderRegex.FindStringSubmatch(line); m != nil {
			flushCurrentKit()
			flushCurrentPreset()
			currentSectionType = "voice"
			idx, _ := strconv.Atoi(m[1])
			currentVoiceIdx = idx
			continue
		}

		// Section header: [kit.X]
		if m := kitHeaderRegex.FindStringSubmatch(line); m != nil {
			flushCurrentKit()
			flushCurrentPreset()
			currentSectionType = "kit"
			pad, _ := strconv.Atoi(m[1])
			currentKit = &Kit{
				Pad: pad,
			}
			continue
		}

		if currentSectionType == "voice" {
			// Check for display: override
			if strings.HasPrefix(strings.ToLower(line), "display:") {
				if currentPreset == nil {
					return nil, fmt.Errorf("line %d: 'display:' before preset header", lineNum)
				}
				currentPreset.Display = strings.TrimSpace(line[len("display:"):])
				continue
			}

			// Check for preset header: <pad>: <name> [| <desc>]
			if m := presetHeaderRegex.FindStringSubmatch(line); m != nil {
				flushCurrentPreset()
				pad, _ := strconv.Atoi(m[1])
				name := strings.TrimSpace(m[2])
				desc := ""
				if len(m) > 3 {
					desc = strings.TrimSpace(m[3])
				}
				currentPreset = &VoicePreset{
					Pad:  pad,
					Name: name,
					Desc: desc,
				}
				continue
			}

			// Otherwise, this line should contain the 16 steps
			if currentPreset == nil {
				return nil, fmt.Errorf("line %d: unexpected line outside of preset: %s", lineNum, line)
			}

			steps, err := parseSteps(line)
			if err != nil {
				return nil, fmt.Errorf("line %d (preset %s): %w", lineNum, currentPreset.Name, err)
			}
			currentPreset.Velocities = steps
			flushCurrentPreset()
			continue
		}

		if currentSectionType == "kit" {
			if currentKit == nil {
				return nil, fmt.Errorf("line %d: kit attribute outside [kit.X] block: %s", lineNum, line)
			}

			parts := strings.SplitN(line, ":", 2)
			if len(parts) != 2 {
				return nil, fmt.Errorf("line %d: expected 'key: value', got %q", lineNum, line)
			}
			key := strings.ToLower(strings.TrimSpace(parts[0]))
			val := strings.TrimSpace(parts[1])

			switch key {
			case "name":
				currentKit.Name = val
			case "genre":
				currentKit.Genre = val
			case "desc", "description":
				currentKit.Desc = val
			case "kick", "voice0", "v0", "0":
				steps, err := parseSteps(val)
				if err != nil {
					return nil, fmt.Errorf("line %d (kit %d kick): %w", lineNum, currentKit.Pad, err)
				}
				currentKit.Voices[0] = steps
			case "snare", "voice1", "v1", "1":
				steps, err := parseSteps(val)
				if err != nil {
					return nil, fmt.Errorf("line %d (kit %d snare): %w", lineNum, currentKit.Pad, err)
				}
				currentKit.Voices[1] = steps
			case "hihat", "hat", "voice2", "v2", "2":
				steps, err := parseSteps(val)
				if err != nil {
					return nil, fmt.Errorf("line %d (kit %d hihat): %w", lineNum, currentKit.Pad, err)
				}
				currentKit.Voices[2] = steps
			case "perc1", "perc_1", "voice3", "v3", "3":
				steps, err := parseSteps(val)
				if err != nil {
					return nil, fmt.Errorf("line %d (kit %d perc1): %w", lineNum, currentKit.Pad, err)
				}
				currentKit.Voices[3] = steps
			case "perc2", "perc_2", "voice4", "v4", "4":
				steps, err := parseSteps(val)
				if err != nil {
					return nil, fmt.Errorf("line %d (kit %d perc2): %w", lineNum, currentKit.Pad, err)
				}
				currentKit.Voices[4] = steps
			case "perc3", "perc_3", "voice5", "v5", "5":
				steps, err := parseSteps(val)
				if err != nil {
					return nil, fmt.Errorf("line %d (kit %d perc3): %w", lineNum, currentKit.Pad, err)
				}
				currentKit.Voices[5] = steps
			default:
				return nil, fmt.Errorf("line %d: unknown kit key %q", lineNum, key)
			}
			continue
		}

		return nil, fmt.Errorf("line %d: content found before any section header: %s", lineNum, line)
	}

	flushCurrentKit()
	flushCurrentPreset()

	if err := scanner.Err(); err != nil {
		return nil, err
	}

	return bank, nil
}

func parseSteps(str string) ([16]uint8, error) {
	var result [16]uint8
	tokens := strings.Fields(str)
	if len(tokens) != 16 {
		return result, fmt.Errorf("expected 16 steps, got %d (tokens: %v)", len(tokens), tokens)
	}

	for i, tok := range tokens {
		switch tok {
		case "_", ".", "0", "·":
			result[i] = 0
		case "g", "○", "▫", "◦":
			result[i] = 50
		case "N", "●", "▪":
			result[i] = 99
		case "A", "■", "▲", "◆":
			result[i] = 127
		default:
			v, err := strconv.Atoi(tok)
			if err != nil || v < 0 || v > 127 {
				return result, fmt.Errorf("step %d invalid token %q (must be _, ., g, N, A, or 0..127)", i+1, tok)
			}
			result[i] = uint8(v)
		}
	}
	return result, nil
}

func (b *PatternBank) validate() error {
	for v := 0; v < 6; v++ {
		if len(b.Voices[v]) != 16 {
			return fmt.Errorf("voice %d has %d presets, expected exactly 16", v, len(b.Voices[v]))
		}
		for i, p := range b.Voices[v] {
			expectedPad := i + 1
			if p.Pad != expectedPad {
				return fmt.Errorf("voice %d preset %d has pad %d, expected %d", v, i, p.Pad, expectedPad)
			}
		}
	}

	if len(b.Kits) != 16 {
		return fmt.Errorf("expected 16 kits, found %d", len(b.Kits))
	}
	for i, k := range b.Kits {
		expectedPad := i + 1
		if k.Pad != expectedPad {
			return fmt.Errorf("kit %d has pad %d, expected %d", i, k.Pad, expectedPad)
		}
	}

	return nil
}

func formatVelocityToken(v uint8) string {
	switch v {
	case 0:
		return "_"
	case 50:
		return "g"
	case 99:
		return "N"
	case 127:
		return "A"
	default:
		return strconv.Itoa(int(v))
	}
}

func formatRhythmString(velocities [16]uint8) string {
	var parts [4]string
	for bar := 0; bar < 4; bar++ {
		var beat [4]string
		for step := 0; step < 4; step++ {
			beat[step] = formatVelocityToken(velocities[bar*4+step])
		}
		parts[bar] = strings.Join(beat[:], " ")
	}
	return strings.Join(parts[:], "  ")
}

func formatHTMLVelocityToken(v uint8) string {
	switch {
	case v == 0:
		return "·"
	case v <= 60:
		return "○"
	case v <= 110:
		return "●"
	default:
		return "■"
	}
}

func formatHTMLRhythmString(velocities [16]uint8) string {
	var parts [4]string
	for bar := 0; bar < 4; bar++ {
		var beat [4]string
		for step := 0; step < 4; step++ {
			beat[step] = formatHTMLVelocityToken(velocities[bar*4+step])
		}
		parts[bar] = strings.Join(beat[:], " ")
	}
	return strings.Join(parts[:], "  ")
}

func updatePatternPresetsH(filePath string, bank *PatternBank) error {
	var sb strings.Builder

	sb.WriteString(`// Code generated by cmd/neotrellis-seq-generator (patterns) from patterns.txt; DO NOT EDIT.
// Any manual changes will be overwritten. Edit patterns.txt instead.

#ifndef PATTERN_PRESETS_H
#define PATTERN_PRESETS_H

#include "Sequencer.h"
#include "config.h"
#include <cstdint>

namespace PatternPresets {

// Velocity shorthand constants for pattern definitions
static constexpr uint8_t A = ACCENT_VELOCITY;  // 127: Accented step
static constexpr uint8_t N = DEFAULT_VELOCITY; // 99:  Standard step
static constexpr uint8_t g = GHOST_VELOCITY;   // 50:  Muted / ghost step
static constexpr uint8_t _ = 0;                // 0:   Rest / silence

struct Preset {
  const char *name;
  uint8_t velocities[16];
};

// -----------------------------------------------------------------------------
// Voice-Specific Pattern Presets (16 patterns each, mapped to STEP 1..16)
// Triggered via: FN1 + ACCENT + STEP n (for the selected voice)
//
// Voice 0: Kick / Bass Drum
// Voice 1: Snare / Clap
// Voice 2: Hi-Hat
// Voice 3, 4, 5: Percussion 1 (Low), Percussion 2 (Mid), Percussion 3 (High)
// -----------------------------------------------------------------------------
`)

	presetArrayNames := [6]string{
		"KICK_PRESETS",
		"SNARE_PRESETS",
		"HIHAT_PRESETS",
		"PERC1_PRESETS",
		"PERC2_PRESETS",
		"PERC3_PRESETS",
	}

	voiceNames := [6]string{
		"Voice 0: Kick / Bass Drum Presets",
		"Voice 1: Snare / Clap Presets",
		"Voice 2: Hi-Hat Presets",
		"Voice 3: Percussion 1 Presets (Low/Mid)",
		"Voice 4: Percussion 2 Presets (Mid/High)",
		"Voice 5: Percussion 3 Presets (High/Texture)",
	}

	for v := 0; v < 6; v++ {
		sb.WriteString(fmt.Sprintf("\n// %s\n", voiceNames[v]))
		sb.WriteString(fmt.Sprintf("static constexpr Preset %s[16] = {\n", presetArrayNames[v]))

		for row := 0; row < 4; row++ {
			switch row {
			case 0:
				sb.WriteString("    // Row 0 (Pads 1-4)\n")
			case 1:
				sb.WriteString("    // Row 1 (Pads 5-8)\n")
			case 2:
				sb.WriteString("    // Row 2 (Pads 9-12)\n")
			case 3:
				sb.WriteString("    // Row 3 (Pads 13-16)\n")
			}

			for p := 0; p < 4; p++ {
				preset := bank.Voices[v][row*4+p]
				allStandard := true
				for _, vel := range preset.Velocities {
					if vel != 0 && vel != 50 && vel != 99 && vel != 127 {
						allStandard = false
						break
					}
				}

				if allStandard {
					var toks []string
					for _, vel := range preset.Velocities {
						toks = append(toks, formatVelocityToken(vel))
					}
					line := fmt.Sprintf("    {\"%s\", {%s}},", preset.Name, strings.Join(toks, ", "))
					if len(line) > 80 {
						sb.WriteString(fmt.Sprintf("    {\"%s\",\n     {%s}},\n", preset.Name, strings.Join(toks, ", ")))
					} else {
						sb.WriteString(line + "\n")
					}
				} else {
					var toks []string
					for _, vel := range preset.Velocities {
						toks = append(toks, strconv.Itoa(int(vel)))
					}
					sb.WriteString(fmt.Sprintf("    {\"%s\",\n     {%s}},\n", preset.Name, strings.Join(toks, ", ")))
				}
			}
			if row < 3 {
				sb.WriteString("\n")
			}
		}
		sb.WriteString("};\n")
	}

	sb.WriteString(`
inline void apply_preset(uint32_t voice, Pattern *p, uint32_t preset_index) {
  if (!p || voice >= VOICES || preset_index >= 16) {
    return;
  }
  const Preset *preset_list = nullptr;
  switch (voice) {
  case 0:
    preset_list = KICK_PRESETS;
    break;
  case 1:
    preset_list = SNARE_PRESETS;
    break;
  case 2:
    preset_list = HIHAT_PRESETS;
    break;
  case 3:
    preset_list = PERC1_PRESETS;
    break;
  case 4:
    preset_list = PERC2_PRESETS;
    break;
  case 5:
    preset_list = PERC3_PRESETS;
    break;
  default:
    return;
  }
  const Preset &preset = preset_list[preset_index];
  p->length = 16;
  for (uint32_t i = 0; i < 16; ++i) {
    p->steps[i] = Step(preset.velocities[i]);
  }
}

// -----------------------------------------------------------------------------
// 16 Full 6-Voice Coordinated Kits
// Triggered via: FN1 + ALL + ACCENT + STEP n (for the entire kit)
//
// Voices:
//   Voice 0: Kick / Bass Drum
//   Voice 1: Snare / Clap
//   Voice 2: Hi-Hat
//   Voice 3: Percussion 1 (Low/Mid)
//   Voice 4: Percussion 2 (Mid/High)
//   Voice 5: Percussion 3 (High/Texture)
// -----------------------------------------------------------------------------

struct KitPreset {
  const char *name;
  const char *genre;
  uint8_t voices[VOICES][16];
};

static constexpr KitPreset KITS[16] = {
`)

	for i, kit := range bank.Kits {
		pad := i + 1
		row := (pad - 1) / 4
		posInRow := (pad - 1) % 4
		if posInRow == 0 {
			switch row {
			case 0:
				sb.WriteString("    // -------------------------------------------------------------------------\n")
				sb.WriteString("    // Row 0 (Pads 1-4)\n")
				sb.WriteString("    // -------------------------------------------------------------------------\n")
			case 1:
				sb.WriteString("    // -------------------------------------------------------------------------\n")
				sb.WriteString("    // Row 1 (Pads 5-8)\n")
				sb.WriteString("    // -------------------------------------------------------------------------\n")
			case 2:
				sb.WriteString("    // -------------------------------------------------------------------------\n")
				sb.WriteString("    // Row 2 (Pads 9-12)\n")
				sb.WriteString("    // -------------------------------------------------------------------------\n")
			case 3:
				sb.WriteString("    // -------------------------------------------------------------------------\n")
				sb.WriteString("    // Row 3 (Pads 13-16)\n")
				sb.WriteString("    // -------------------------------------------------------------------------\n")
			}
		}

		sb.WriteString(fmt.Sprintf("    // Kit %d (Step %d): %s\n", i, pad, kit.Name))
		sb.WriteString(fmt.Sprintf("    {\"%s\",\n", kit.Name))
		sb.WriteString(fmt.Sprintf("     \"%s\",\n", kit.Genre))
		sb.WriteString("     {\n")
		for v := 0; v < 6; v++ {
			var toks []string
			for _, vel := range kit.Voices[v] {
				toks = append(toks, formatVelocityToken(vel))
			}
			sb.WriteString(fmt.Sprintf("         {%s},\n", strings.Join(toks, ", ")))
		}
		sb.WriteString("     }},\n")
		if i < len(bank.Kits)-1 {
			sb.WriteString("\n")
		}
	}

	sb.WriteString(`
};

inline void apply_kit_voice(uint32_t voice, uint32_t kit_index, Pattern *p) {
  if (!p || voice >= VOICES || kit_index >= 16) {
    return;
  }
  const auto &pattern_data = KITS[kit_index].voices[voice];
  p->length = 16;
  for (uint32_t i = 0; i < 16; ++i) {
    p->steps[i] = Step(pattern_data[i]);
  }
}

} // namespace PatternPresets

namespace DancePatterns = PatternPresets;

#endif // PATTERN_PRESETS_H
`)

	return writeGeneratedCHeader(filePath, []byte(sb.String()))
}

func escapeHTML(s string) string {
	// Simple escape for XML/HTML special characters in descriptions and titles,
	// preserving existing valid entities like &hellip;, &rarr;, &amp;, &ndash;.
	if strings.Contains(s, "&") && (strings.Contains(s, "&rarr;") || strings.Contains(s, "&ndash;") || strings.Contains(s, "&amp;") || strings.Contains(s, "&hellip;") || strings.Contains(s, "&atilde;")) {
		return s
	}
	s = strings.ReplaceAll(s, "&", "&amp;")
	s = strings.ReplaceAll(s, "<", "&lt;")
	s = strings.ReplaceAll(s, ">", "&gt;")
	return s
}
