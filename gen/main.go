package main

import (
	"bytes"
	"fmt"
	"os"
	"sort"
	"strings"

	"github.com/lucasb-eyer/go-colorful"
)

func main() {
	generateColors()
}

var (
	selectedTheme = defaultTheme
	// selectedTheme = gruvboxTheme

	defaultActiveVoicesFunc = func(t Theme) Theme {
		return t.ChangeLightness(0.15)
	}
	defaultInactiveVoicesFunc = func(t Theme) Theme {
		return t.ChangeLightness(-0.35)
	}

	defaultTheme = Theme{
		Off:          "#000000",
		PlayPosition: "#d0d0d0",
		Voice0:       "#2050e0",
		Voice1:       "#ad8920",
		Voice2:       "#20bcaf",
		Voice3:       "#489120",
		Voice4:       "#b030b0",
		Voice5:       "#d0d040",
		Tool:         "#902020",
		ActiveVoices: func(t Theme) Theme {
			return t.ChangeLightness(0.15)
		},
		InactiveVoices: func(t Theme) Theme {
			return t.ChangeLightness(-0.35)
		},
	}

	gruvboxTheme = Theme{
		Off:          GruvboxDark0Hard,
		PlayPosition: GruvboxLight0,
		Voice0:       GruvboxBlue,
		Voice1:       GruvboxYellow,
		Voice2:       GruvboxAqua,
		Voice3:       GruvboxGreen,
		Voice4:       GruvboxPurple,
		Voice5:       GruvboxRed,
		Tool:         GruvboxBrightPurple,
		ActiveVoices: func(t Theme) Theme {
			return t.ChangeLightness(0.2)
		},
		InactiveVoices: func(t Theme) Theme {
			return t.ChangeLightness(-0.4)
		},
	}
)

type Theme struct {
	Off            HexColor
	PlayPosition   HexColor
	Voice0         HexColor
	Voice1         HexColor
	Voice2         HexColor
	Voice3         HexColor
	Voice4         HexColor
	Voice5         HexColor
	ActiveVoices   func(Theme) Theme
	InactiveVoices func(Theme) Theme
	Tool           HexColor
}

func (b *Theme) modifyColors(fn func(c *HexColor)) {
	fn(&b.Off)
	fn(&b.PlayPosition)
	fn(&b.Voice0)
	fn(&b.Voice1)
	fn(&b.Voice2)
	fn(&b.Voice3)
	fn(&b.Voice4)
	fn(&b.Voice5)
	fn(&b.Tool)
}

func (b Theme) ChangeLightness(amount float64) Theme {
	res := b
	res.modifyColors(func(c *HexColor) {
		l, a, b := c.Color().Lab()
		l = l + amount
		*c = NewHexColor(colorful.Lab(l, a, b).Clamped())
	})
	return res
}

func (b Theme) ChangeSaturation(amount float64) Theme {
	res := b
	b.modifyColors(func(c *HexColor) {
		h, s, l := c.Color().Hsl()
		// s = math.Min(math.Max(s+v, 0), 1)
		s = s + amount
		*c = NewHexColor(colorful.Hsl(h, s, l).Clamped())
	})
	return res
}

func (p Theme) NamedColors() NamedColors {
	return NamedColors{
		"OFF":  HexColor(p.Off),
		"PPOS": HexColor(p.PlayPosition),
		"VOC0": HexColor(p.Voice0),
		"VOC1": HexColor(p.Voice1),
		"VOC2": HexColor(p.Voice2),
		"VOC3": HexColor(p.Voice3),
		"VOC4": HexColor(p.Voice4),
		"VOC5": HexColor(p.Voice5),
		"TOOL": HexColor(p.Tool),
	}
}

func colorDef(c HexColor, name string, variant string) string {
	var sb strings.Builder
	sb.WriteString("#define COLOR_")
	sb.WriteString(strings.ToUpper(name))
	if variant != "" {
		sb.WriteString("_")
		sb.WriteString(strings.ToUpper(variant))
	}
	sb.WriteString(" 0x")
	sb.WriteString(strings.ToUpper(strings.TrimPrefix(c.Color().Clamped().Hex(), "#")))
	return sb.String()
}

func generateColors() {

	theme := selectedTheme

	voiceActiveStep := theme
	if theme.ActiveVoices != nil {
		voiceActiveStep = voiceActiveStep.ActiveVoices(voiceActiveStep)
	} else {
		voiceActiveStep = voiceActiveStep.ChangeLightness(0.3)
	}

	voiceInactiveStep := theme
	if theme.InactiveVoices != nil {
		voiceInactiveStep = voiceInactiveStep.InactiveVoices(voiceInactiveStep)
	} else {
		voiceInactiveStep = voiceInactiveStep.ChangeLightness(-0.3)
	}

	var lines []string

	for k, v := range theme.NamedColors() {
		lines = append(lines, colorDef(v, k, ""))
	}
	for k, v := range voiceActiveStep.NamedColors().WithPrefix("VOC").AddSuffix("_SET") {
		lines = append(lines, colorDef(v, k, ""))
	}
	for k, v := range voiceInactiveStep.NamedColors().WithPrefix("VOC").AddSuffix("_UNSET") {
		lines = append(lines, colorDef(v, k, ""))
	}

	sort.Strings(lines)

	var sb strings.Builder
	sb.WriteString(`
#ifndef COLORS_H
#define COLORS_H

`)
	sb.WriteString(strings.Join(lines, "\n"))
	sb.WriteString(`

#endif
`)
	data := []byte(sb.String())
	fileData, err := os.ReadFile("src/colors.h")
	if err != nil {
		panic(err)
	}

	if !bytes.Equal(data, fileData) {
		os.WriteFile("src/colors.h", data, 0o600)
	}
}

type HexColor string

func (h HexColor) Color() colorful.Color {
	col, err := colorful.Hex(string(h))
	if err != nil {
		panic(err)
	}
	return col
}

func (h HexColor) Blend(hc HexColor, t float64) HexColor {
	c1 := h.Color()
	c2 := hc.Color()
	return HexColor(c1.BlendLab(c2, t).Clamped().Hex())
}

func NewHexColor(c colorful.Color) HexColor {
	return HexColor(c.Clamped().Hex())
}

type NamedColors map[string]HexColor

// AddPrefix returns a copy of named colors where all keys are prefixed.
func (n NamedColors) AddPrefix(prefix string) NamedColors {
	nc := make(NamedColors, len(n))
	for k, v := range n {
		nc[fmt.Sprintf("%s%s", prefix, k)] = v
	}
	return nc
}

// AddSuffix returns a copy of named colors where all keys are suffixed.
func (n NamedColors) AddSuffix(suffix string) NamedColors {
	nc := make(NamedColors, len(n))
	for k, v := range n {
		nc[fmt.Sprintf("%s%s", k, suffix)] = v
	}
	return nc
}

func (n NamedColors) WithSuffix(suffix ...string) NamedColors {
	nc := make(NamedColors, len(n))
	for k, v := range n {
	loop:
		for _, s := range suffix {
			if strings.HasSuffix(k, s) {
				nc[k] = v
				break loop
			}
		}
	}
	return nc
}

func (n NamedColors) WithPrefix(prefix ...string) NamedColors {
	nc := make(NamedColors, len(n))
	for k, v := range n {
	loop:
		for _, p := range prefix {
			if strings.HasPrefix(k, p) {
				nc[k] = v
				break loop
			}
		}
	}
	return nc
}

// Merge merges multiple NamedColors
func Merge(nc ...NamedColors) NamedColors {
	res := make(NamedColors)
	for _, nc := range nc {
		for k, v := range nc {
			res[k] = v
		}
	}
	return res
}

const (
	SolarizedBase03  = "#002b36"
	SolarizedBase02  = "#073642"
	SolarizedBase01  = "#586e75"
	SolarizedBase00  = "#657b83"
	SolarizedBase0   = "#839496"
	SolarizedBase1   = "#93a1a1"
	SolarizedBase2   = "#eee8d5"
	SolarizedBase3   = "#fdf6e3"
	SolarizedBlue    = "#268bd2"
	SolarizedCyan    = "#2aa198"
	SolarizedGreen   = "#859900"
	SolarizedMagenta = "#d33682"
	SolarizedOrange  = "#cb4b16"
	SolarizedRed     = "#dc322f"
	SolarizedViolet  = "#6c71c4"
	SolarizedYellow  = "#b58900"

	GruvboxDark0Hard    = "#1d2021"
	GruvboxDark0        = "#282828"
	GruvboxDark0Soft    = "#32302f"
	GruvboxDark1        = "#3c3836"
	GruvboxDark2        = "#504945"
	GruvboxDark3        = "#665c54"
	GruvboxDark4        = "#7c6f64"
	GruvboxGray         = "#928374"
	GruvboxLight0Hard   = "#f9f5d7"
	GruvboxLight0       = "#fbf1c7"
	GruvboxLight0Soft   = "#f2e5bc"
	GruvboxLight1       = "#ebdbb2"
	GruvboxLight2       = "#d5c4a1"
	GruvboxLight3       = "#bdae93"
	GruvboxLight4       = "#a89984"
	GruvboxRed          = "#cc241d"
	GruvboxGreen        = "#98971a"
	GruvboxYellow       = "#d79921"
	GruvboxBlue         = "#458588"
	GruvboxPurple       = "#b16286"
	GruvboxAqua         = "#689d6a"
	GruvboxOrange       = "#d65d0e"
	GruvboxBrightRed    = "#fb4933"
	GruvboxBrightGreen  = "#b8bb26"
	GruvboxBrightYellow = "#fabd2f"
	GruvboxBrightBlue   = "#83a598"
	GruvboxBrightPurple = "#d3869b"
	GruvboxBrightAqua   = "#8ec07c"
	GruvboxBrightOrange = "#fe8019"
	GruvboxDarkRed      = "#9d0006"
	GruvboxDarkGreen    = "#79740e"
	GruvboxDarkYellow   = "#b57614"
	GruvboxDarkBlue     = "#076678"
	GruvboxDarkPurple   = "#8f3f71"
	GruvboxDarkAqua     = "#427b58"
	GruvboxDarkOrange   = "#af3a03"

	ZenburnFgM1     = "#656555"
	ZenburnFgM05    = "#989890"
	ZenburnFg       = "#DCDCCC"
	ZenburnFgP1     = "#FFFFEF"
	ZenburnFgP2     = "#FFFFFD"
	ZenburnBgM2     = "#000000"
	ZenburnBgM1     = "#2B2B2B"
	ZenburnBgM08    = "#303030"
	ZenburnBgM05    = "#383838"
	ZenburnBg       = "#3F3F3F"
	ZenburnBgP05    = "#494949"
	ZenburnBgP1     = "#4F4F4F"
	ZenburnBgP2     = "#5F5F5F"
	ZenburnBgP3     = "#6F6F6F"
	ZenburnRedM6    = "#6C3333"
	ZenburnRedM5    = "#7C4343"
	ZenburnRedM4    = "#8C5353"
	ZenburnRedM3    = "#9C6363"
	ZenburnRedM2    = "#AC7373"
	ZenburnRedM1    = "#BC8383"
	ZenburnRed      = "#CC9393"
	ZenburnRedP1    = "#DCA3A3"
	ZenburnRedP2    = "#ECB3B3"
	ZenburnOrange   = "#DFAF8F"
	ZenburnYellowM2 = "#D0BF8F"
	ZenburnYellowM1 = "#E0CF9F"
	ZenburnYellow   = "#F0DFAF"
	ZenburnGreenM5  = "#2F4F2F"
	ZenburnGreenM4  = "#3F5F3F"
	ZenburnGreenM3  = "#4F6F4F"
	ZenburnGreenM2  = "#5F7F5F"
	ZenburnGreenM1  = "#6F8F6F"
	ZenburnGreen    = "#7F9F7F"
	ZenburnGreenP1  = "#8FB28F"
	ZenburnGreenP2  = "#9FC59F"
	ZenburnGreenP3  = "#AFD8AF"
	ZenburnGreenP4  = "#BFEBBF"
	ZenburnCyan     = "#93E0E3"
	ZenburnBlueP3   = "#BDE0F3"
	ZenburnBlueP2   = "#ACE0E3"
	ZenburnBlueP1   = "#94BFF3"
	ZenburnBlue     = "#8CD0D3"
	ZenburnBlueM1   = "#7CB8BB"
	ZenburnBlueM2   = "#6CA0A3"
	ZenburnBlueM3   = "#5C888B"
	ZenburnBlueM4   = "#4C7073"
	ZenburnBlueM5   = "#366060"
	ZenburnMagenta  = "#DC8CC3"

	// ehm, monokaiXX are probably not "correct" in any way
	Monokai03      = "#272822"
	Monokai02      = "#3E3D31"
	Monokai01      = "#75715E"
	Monokai00      = "#49483E"
	Monokai0       = "#F8F8F2"
	MonokaiYellow  = "#E6DB74"
	MonokaiOrange  = "#FD971F"
	MonokaiRed     = "#F92672"
	MonokaiMagenta = "#FD5FF0"
	MonokaiBlue    = "#66D9EF"
	MonokaiGreen   = "#A6E22E"
	MonokaiCyan    = "#A1EFE4"
	MonokaiViolet  = "#AE81FF"
)
