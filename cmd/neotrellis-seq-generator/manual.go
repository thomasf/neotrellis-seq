package main

import (
	"bytes"
	"embed"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"text/template"

	"github.com/thomasf/neotrellis-seq/pkg/midimap"
)

//go:embed templates/*
var manualTemplatesFS embed.FS

type ManualData struct {
	MidiMap  MidiMapManualData
	Patterns PatternsManualData
}

type MidiMapManualData struct {
	Rows   []DrumRackRowView
	Voices []midimap.VoiceMapDef
}

type DrumRackRowView struct {
	RowName  string
	PadsDesc string
	Pads     []DrumRackPadView
}

type DrumRackPadView struct {
	PadNumber int
	Note      int
	NoteStr   string
	Name      string
	VoiceTag  string
	ModeBadge string
	CssClass  string
}

type PatternsManualData struct {
	PresetVoices []PresetVoiceView
	Kits         []KitView
}

type PresetVoiceView struct {
	Title   string
	Presets []PresetItemView
}

type PresetItemView struct {
	Pad    int
	Name   string
	Length int
	Pages  []PresetPageView
}

type PresetPageView struct {
	PageNum    int
	StartStep  int
	EndStep    int
	RhythmHTML string
}

type KitView struct {
	Pad      int
	Name     string
	Genre    string
	IsPoly   bool
	MaxPages int
	Pages    []KitPageView
}

type KitPageView struct {
	PageNum   int
	StartStep int
	EndStep   int
	Voices    []KitVoicePageView
}

type KitVoicePageView struct {
	VoiceIndex int
	VoiceLabel string
	Length     int
	IsPoly     bool
	Rhythm     string
}

func formatHTMLStepSlice(velocities []uint8) string {
	if len(velocities) == 0 {
		return ""
	}
	var beats []string
	for i := 0; i < len(velocities); i += 4 {
		end := i + 4
		if end > len(velocities) {
			end = len(velocities)
		}
		var beatToks []string
		for _, v := range velocities[i:end] {
			beatToks = append(beatToks, formatHTMLVelocityToken(v))
		}
		beats = append(beats, strings.Join(beatToks, " "))
	}
	return strings.Join(beats, "  ")
}

func buildManualData(rootDir string) (*ManualData, error) {
	// 1. MidiMap Data
	rowRanges := []struct {
		RowName  string
		PadsDesc string
		StartPad int
		EndPad   int
	}{
		{"Row 4", "Top Row of 4x4: Pads 13–16 / Notes 48–51", 13, 16},
		{"Row 3", "Pads 9–12 / Notes 44–47", 9, 12},
		{"Row 2", "Pads 5–8 / Notes 40–43", 5, 8},
		{"Row 1", "Bottom Row of 4x4: Pads 1–4 / Notes 36–39", 1, 4},
	}

	var rows []DrumRackRowView
	for _, rr := range rowRanges {
		var pads []DrumRackPadView
		for padNum := rr.StartPad; padNum <= rr.EndPad; padNum++ {
			p := midimap.DrumRackPads[padNum-1]
			vPrimary := -1
			for _, v := range midimap.Voices {
				if v.PrimaryNote == p.Note {
					vPrimary = v.VoiceIndex
					break
				}
			}

			noteStr := fmt.Sprintf("%d / %s", p.Note, midimap.MidiNoteName(p.Note))
			var pv DrumRackPadView
			pv.PadNumber = p.PadNumber
			pv.Note = p.Note
			pv.NoteStr = noteStr
			pv.Name = p.Name

			if vPrimary >= 0 {
				pv.CssClass = fmt.Sprintf("drum-rack-pad voc%d", vPrimary)
				pv.VoiceTag = fmt.Sprintf("VOICE %d", vPrimary)
				pv.ModeBadge = "Assigned"
			} else {
				pv.CssClass = "drum-rack-pad unassigned"
				pv.VoiceTag = "&mdash;"
				pv.ModeBadge = "Available"
			}
			pads = append(pads, pv)
		}
		rows = append(rows, DrumRackRowView{
			RowName:  rr.RowName,
			PadsDesc: rr.PadsDesc,
			Pads:     pads,
		})
	}

	midiMapData := MidiMapManualData{
		Rows:   rows,
		Voices: midimap.Voices,
	}

	// 2. Patterns Data
	patternsPath := filepath.Join(rootDir, "patterns.txt")
	bank, err := parsePatternsFile(patternsPath)
	if err != nil {
		return nil, fmt.Errorf("reading %s: %w", patternsPath, err)
	}
	if err := bank.validate(); err != nil {
		return nil, fmt.Errorf("validating %s: %w", patternsPath, err)
	}

	voiceTitles := [6]string{
		"Voice 0: Kick / Bass Drum Presets",
		"Voice 1: Snare / Clap Presets",
		"Voice 2: Hi-Hat Presets",
		"Voice 3: Percussion 1 Presets (Low/Mid)",
		"Voice 4: Percussion 2 Presets (Mid/High)",
		"Voice 5: Percussion 3 Presets (High/Texture)",
	}

	var presetVoices []PresetVoiceView
	for v := 0; v < 6; v++ {
		var items []PresetItemView
		for _, preset := range bank.Voices[v] {
			var pages []PresetPageView
			if preset.Display != "" {
				rhythm := preset.Display
				if !strings.HasPrefix(rhythm, "<") {
					rhythm = fmt.Sprintf("<code>%s</code>", rhythm)
				}
				pages = append(pages, PresetPageView{
					PageNum:    1,
					StartStep:  1,
					EndStep:    len(preset.Velocities),
					RhythmHTML: rhythm,
				})
			} else {
				numPages := (len(preset.Velocities) + 15) / 16
				for p := 0; p < numPages; p++ {
					start := p * 16
					end := (p + 1) * 16
					if end > len(preset.Velocities) {
						end = len(preset.Velocities)
					}
					pages = append(pages, PresetPageView{
						PageNum:    p + 1,
						StartStep:  start + 1,
						EndStep:    end,
						RhythmHTML: formatHTMLStepSlice(preset.Velocities[start:end]),
					})
				}
			}
			items = append(items, PresetItemView{
				Pad:    preset.Pad,
				Name:   escapeHTML(preset.Name),
				Length: len(preset.Velocities),
				Pages:  pages,
			})
		}
		presetVoices = append(presetVoices, PresetVoiceView{
			Title:   voiceTitles[v],
			Presets: items,
		})
	}

	voiceLabels := [6]string{"KICK", "SNARE", "HIHAT", "PERC1", "PERC2", "PERC3"}
	var kits []KitView
	for _, kit := range bank.Kits {
		maxPages := 1
		for v := 0; v < 6; v++ {
			p := (len(kit.Voices[v]) + 15) / 16
			if p > maxPages {
				maxPages = p
			}
		}

		isPoly := false
		firstLen := len(kit.Voices[0])
		for v := 1; v < 6; v++ {
			if len(kit.Voices[v]) != firstLen {
				isPoly = true
				break
			}
		}

		var kitPages []KitPageView
		for p := 0; p < maxPages; p++ {
			startStep := p*16 + 1
			endStep := (p + 1) * 16

			var vViews []KitVoicePageView
			for v := 0; v < 6; v++ {
				vLen := len(kit.Voices[v])
				if vLen == 0 {
					vLen = 16
				}
				playedSteps := make([]uint8, 16)
				for i := 0; i < 16; i++ {
					stepTick := p*16 + i
					playedSteps[i] = kit.Voices[v][stepTick%vLen]
				}
				rhythm := formatHTMLStepSlice(playedSteps)
				vViews = append(vViews, KitVoicePageView{
					VoiceIndex: v,
					VoiceLabel: voiceLabels[v],
					Length:     vLen,
					IsPoly:     vLen%16 != 0,
					Rhythm:     rhythm,
				})
			}

			kitPages = append(kitPages, KitPageView{
				PageNum:   p + 1,
				StartStep: startStep,
				EndStep:   endStep,
				Voices:    vViews,
			})
		}

		kits = append(kits, KitView{
			Pad:      kit.Pad,
			Name:     escapeHTML(kit.Name),
			Genre:    escapeHTML(kit.Genre),
			IsPoly:   isPoly,
			MaxPages: maxPages,
			Pages:    kitPages,
		})
	}

	patternsData := PatternsManualData{
		PresetVoices: presetVoices,
		Kits:         kits,
	}

	return &ManualData{
		MidiMap:  midiMapData,
		Patterns: patternsData,
	}, nil
}

func generateManual(rootDir string) error {
	manualHTMLPath := filepath.Join(rootDir, "MANUAL.html")
	fmt.Printf("Generating %s from embedded Go templates...\n", manualHTMLPath)

	data, err := buildManualData(rootDir)
	if err != nil {
		return fmt.Errorf("building manual data: %w", err)
	}

	funcMap := template.FuncMap{
		"safeHTML": func(s string) string {
			return s
		},
		"escapeHTML":   escapeHTML,
		"midiNoteName": midimap.MidiNoteName,
	}

	tmpl, err := template.New("manual.html.tmpl").Funcs(funcMap).ParseFS(manualTemplatesFS, "templates/*")
	if err != nil {
		return fmt.Errorf("parsing manual templates: %w", err)
	}

	var buf bytes.Buffer
	buf.WriteString("<!-- GENERATED FILE DO NOT EDIT. Code generated by cmd/neotrellis-seq-generator; DO NOT EDIT. -->\n")
	if err := tmpl.ExecuteTemplate(&buf, "manual.html.tmpl", data); err != nil {
		return fmt.Errorf("executing manual template: %w", err)
	}

	oldContent, err := os.ReadFile(manualHTMLPath)
	if err == nil && bytes.Equal(oldContent, buf.Bytes()) {
		fmt.Printf("%s is already up to date.\n", manualHTMLPath)
		return nil
	}

	if err := os.WriteFile(manualHTMLPath, buf.Bytes(), 0644); err != nil {
		return fmt.Errorf("writing %s: %w", manualHTMLPath, err)
	}
	fmt.Printf("Updated %s\n", manualHTMLPath)
	return nil
}
