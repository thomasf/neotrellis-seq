package main

import (
	"fmt"
	"strings"
	"sync"
	"time"

	tea "charm.land/bubbletea/v2"
	"charm.land/lipgloss/v2"
	"charm.land/lipgloss/v2/table"
)

// =============================================================================
// Bubble Tea v2 TUI & Live Activity Monitor
// =============================================================================

type tickMsg time.Time

type midiNoteMsg struct {
	ch     int
	note   int
	vel    int
	t      time.Time
	timing StepTimingInfo
}

type midiStatusMsg struct {
	state string
	msg   string
	t     time.Time
}

type eventDispatcher struct {
	mu      sync.Mutex
	program *tea.Program
	pending []tea.Msg
}

func newEventDispatcher() *eventDispatcher {
	return &eventDispatcher{}
}

func (d *eventDispatcher) Send(msg tea.Msg) {
	d.mu.Lock()
	defer d.mu.Unlock()
	if d.program != nil {
		d.program.Send(msg)
	} else {
		d.pending = append(d.pending, msg)
	}
}

func (d *eventDispatcher) SetProgram(p *tea.Program) {
	d.mu.Lock()
	d.program = p
	pending := d.pending
	d.pending = nil
	d.mu.Unlock()
	for _, msg := range pending {
		p.Send(msg)
	}
}

type LogRow struct {
	Time       string
	Ch         int
	Tag        string
	NoteStr    string
	Vel        int
	Latency    string
	StepDev    string
	ChordSkew  string
	VoiceName  string
	IsStatus   bool
	StatusText string
}

type tuiModel struct {
	clock         *MidiClock
	synth         *DrumSynth
	alsa          *AlsaManager
	player        *AudioPlayer
	timing        *TimingTracker
	statusState   string
	statusMsg     string
	selectedVoice int
	voiceFlash    [6]time.Time
	voiceVel      [6]int
	logs          []LogRow
	width         int
	height        int
}

func newTUIModel(clock *MidiClock, synth *DrumSynth, alsa *AlsaManager, player *AudioPlayer, timing *TimingTracker) tuiModel {
	return tuiModel{
		clock:         clock,
		synth:         synth,
		alsa:          alsa,
		player:        player,
		timing:        timing,
		statusState:   "WAITING",
		statusMsg:     "Scanning for NeoTrellis M4...",
		selectedVoice: 0,
		logs:          make([]LogRow, 0, 100),
		width:         80,
		height:        24,
	}
}

func (m tuiModel) Init() tea.Cmd {
	return tickCmd()
}

func tickCmd() tea.Cmd {
	return tea.Tick(40*time.Millisecond, func(t time.Time) tea.Msg {
		return tickMsg(t)
	})
}

func (m tuiModel) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	switch msg := msg.(type) {
	case tea.WindowSizeMsg:
		m.width = msg.Width
		m.height = msg.Height
		return m, nil

	case tea.KeyPressMsg:
		switch msg.String() {
		case "q", "Q", "ctrl+c":
			return m, tea.Quit
		case " ":
			m.clock.Toggle()
			return m, nil
		case "+", "=":
			m.clock.AdjustBPM(5.0)
			return m, nil
		case "-", "_":
			m.clock.AdjustBPM(-5.0)
			return m, nil
		case "]", "}":
			m.clock.AdjustBPM(1.0)
			return m, nil
		case "[", "{":
			m.clock.AdjustBPM(-1.0)
			return m, nil
		case "r", "R":
			m.clock.Reset()
			return m, nil
		case "tab", "right", "down":
			m.selectedVoice = (m.selectedVoice + 1) % 6
			return m, nil
		case "shift+tab", "left", "up":
			m.selectedVoice = (m.selectedVoice + 5) % 6
			return m, nil
		case "s":
			note := 36 + m.selectedVoice
			disp := VoiceDisplay[m.selectedVoice]
			newDrv := m.synth.AdjustVoiceDrive(note, -0.1)
			m.appendLog(LogRow{
				Time:       time.Now().Format("15:04:05.000"),
				IsStatus:   true,
				StatusText: fmt.Sprintf("=== %s (%s) Drive: %.1f ===", disp.Tag, strings.TrimSpace(disp.Name), newDrv),
			})
			return m, nil
		case "S":
			note := 36 + m.selectedVoice
			disp := VoiceDisplay[m.selectedVoice]
			newDrv := m.synth.AdjustVoiceDrive(note, +0.1)
			m.appendLog(LogRow{
				Time:       time.Now().Format("15:04:05.000"),
				IsStatus:   true,
				StatusText: fmt.Sprintf("=== %s (%s) Drive: %.1f ===", disp.Tag, strings.TrimSpace(disp.Name), newDrv),
			})
			return m, nil
		case "1", "2", "3", "4", "5", "6":
			vIdx := int(msg.String()[0] - '1')
			m.selectedVoice = vIdx
			note := 36 + vIdx
			m.synth.NoteOn(note, 120)
			now := time.Now()
			m.voiceFlash[vIdx] = now.Add(180 * time.Millisecond)
			m.voiceVel[vIdx] = 120
			vInfo, ok := VoiceMap[note]
			if !ok {
				vInfo = VoiceInfo{Tag: fmt.Sprintf("V%d", vIdx), Name: "Manual"}
			}
			m.appendLog(LogRow{
				Time:      now.Format("15:04:05.000"),
				Ch:        1,
				Tag:       vInfo.Tag,
				NoteStr:   fmt.Sprintf("%2d (%-3s)", note, midiNoteToStr(note)),
				Vel:       120,
				Latency:   "  --  ",
				StepDev:   "  --  ",
				ChordSkew: " 0.00 ms",
				VoiceName: vInfo.Name,
			})
			return m, nil
		}

	case tickMsg:
		return m, tickCmd()

	case midiNoteMsg:
		nowStr := msg.t.Format("15:04:05.000")
		vInfo, ok := VoiceMap[msg.note]
		if !ok {
			vInfo = VoiceInfo{Tag: "V?", Name: fmt.Sprintf("Note %d", msg.note), FullName: midiNoteToStr(msg.note)}
		}

		vIdx := -1
		if msg.note >= 36 && msg.note <= 41 {
			vIdx = msg.note - 36
		} else if msg.note >= 42 && msg.note <= 47 {
			vIdx = msg.note - 42
		}

		if vIdx >= 0 && vIdx < 6 {
			m.voiceFlash[vIdx] = msg.t.Add(180 * time.Millisecond)
			m.voiceVel[vIdx] = msg.vel
		}

		latStr := "  --  "
		if msg.timing.HasLatency {
			latStr = fmt.Sprintf("%5.2f ms", float64(msg.timing.Latency.Nanoseconds())/1e6)
		}

		devStr := "  --  "
		if msg.timing.IsNewStep && msg.timing.StepInterval > 0 {
			devMs := float64(msg.timing.StepDev.Nanoseconds()) / 1e6
			if devMs >= 0 {
				devStr = fmt.Sprintf("+%5.2f ms", devMs)
			} else {
				devStr = fmt.Sprintf("%6.2f ms", devMs)
			}
		}

		skewStr := " 0.00 ms"
		if !msg.timing.IsNewStep {
			skewStr = fmt.Sprintf("+%4.2f ms", float64(msg.timing.ChordSkew.Nanoseconds())/1e6)
		}

		m.appendLog(LogRow{
			Time:      nowStr,
			Ch:        msg.ch + 1,
			Tag:       vInfo.Tag,
			NoteStr:   fmt.Sprintf("%2d (%-3s)", msg.note, midiNoteToStr(msg.note)),
			Vel:       msg.vel,
			Latency:   latStr,
			StepDev:   devStr,
			ChordSkew: skewStr,
			VoiceName: vInfo.Name,
		})
		return m, nil

	case midiStatusMsg:
		m.statusState = msg.state
		m.statusMsg = msg.msg
		m.appendLog(LogRow{
			Time:       msg.t.Format("15:04:05.000"),
			IsStatus:   true,
			StatusText: fmt.Sprintf("=== [%s] %s ===", msg.state, msg.msg),
		})
		return m, nil
	}

	return m, nil
}

func (m *tuiModel) appendLog(row LogRow) {
	m.logs = append(m.logs, row)
	if len(m.logs) > 200 {
		m.logs = m.logs[len(m.logs)-200:]
	}
}

func (m tuiModel) View() tea.View {
	now := time.Now()

	statusBadge := lipgloss.NewStyle().Bold(true).Foreground(lipgloss.Color("#00B894")).Render("● CONNECTED")
	if m.statusState != "CONNECTED" {
		statusBadge = lipgloss.NewStyle().Bold(true).Foreground(lipgloss.Color("#FDCB6E")).Render("○ " + m.statusState)
	}

	clockBadge := lipgloss.NewStyle().Bold(true).Foreground(lipgloss.Color("#00B894")).Render("▶ RUNNING")
	if !m.clock.running.Load() {
		clockBadge = lipgloss.NewStyle().Bold(true).Foreground(lipgloss.Color("#FDCB6E")).Render("■ STOPPED")
	}

	ticks := m.clock.tickCount.Load()
	beats := ticks / 24
	bars := (beats / 4) + 1
	beatInBar := (beats % 4) + 1
	bpm := m.clock.BPM()

	title := lipgloss.NewStyle().
		Bold(true).
		Foreground(lipgloss.Color("#FFFFFF")).
		Background(lipgloss.Color("#4834D4")).
		Padding(0, 2).
		Render("NeoTrellis M4 Sequencer Development Companion")

	audioStr := "None"
	if m.player != nil {
		audioStr = m.player.Backend()
	}

	infoLine1 := fmt.Sprintf(" %s %s - %s", lipgloss.NewStyle().Bold(true).Render("Status:"), statusBadge, m.statusMsg)

	selDisp := VoiceDisplay[m.selectedVoice]
	selDrv := m.synth.VoiceDrive(selDisp.Note)
	voiceBadge := lipgloss.NewStyle().Bold(true).Foreground(lipgloss.Color("#FD79A8")).
		Render(fmt.Sprintf("%s (%s) [Drv: %.1f]", selDisp.Tag, strings.TrimSpace(selDisp.Name), selDrv))

	infoLine2 := fmt.Sprintf(" %s  %s [%.1f BPM]  Bar: %3d  Beat: %d.%d  Ticks: %5d  │  Audio: %s  │  Voice: %s",
		lipgloss.NewStyle().Bold(true).Render("Clock:"),
		clockBadge, bpm, bars, beatInBar, (ticks%24)/6+1, ticks, audioStr, voiceBadge)

	// Timing box
	var timingLines []string
	timingLines = append(timingLines, lipgloss.NewStyle().Bold(true).Foreground(lipgloss.Color("#81ECEC")).Render("Timing & Latency Analysis:"))
	if m.timing != nil {
		snap := m.timing.Snapshot()
		if snap.HasLatency {
			timingLines = append(timingLines, fmt.Sprintf("  Clock->Note Latency : %6.2f ms  (min: %5.2f ms, max: %5.2f ms, avg: %5.2f ms)",
				float64(snap.LastLatency.Nanoseconds())/1e6,
				float64(snap.MinLatency.Nanoseconds())/1e6,
				float64(snap.MaxLatency.Nanoseconds())/1e6,
				float64(snap.AvgLatency.Nanoseconds())/1e6))
		} else {
			timingLines = append(timingLines, fmt.Sprintf("  Clock->Note Latency : %s", lipgloss.NewStyle().Foreground(lipgloss.Color("#636E72")).Render("waiting for clock/note trigger...")))
		}

		if snap.StepCount > 0 {
			if snap.LastStepCount > 1 {
				stepTarget := float64(snap.IdealStep.Nanoseconds()) * float64(snap.LastStepCount) / 1e6
				timingLines = append(timingLines, fmt.Sprintf("  Step Interval (16th): %6.2f ms  (grid [%d steps]: %5.2f ms, dev: %+5.2f ms, jitter: %5.2f ms)",
					float64(snap.LastStepInterval.Nanoseconds())/1e6,
					snap.LastStepCount,
					stepTarget,
					float64(snap.LastStepDev.Nanoseconds())/1e6,
					float64(snap.StepJitterPP.Nanoseconds())/1e6))
			} else {
				timingLines = append(timingLines, fmt.Sprintf("  Step Interval (16th): %6.2f ms  (ideal: %5.2f ms, dev: %+5.2f ms, jitter: %5.2f ms)",
					float64(snap.LastStepInterval.Nanoseconds())/1e6,
					float64(snap.IdealStep.Nanoseconds())/1e6,
					float64(snap.LastStepDev.Nanoseconds())/1e6,
					float64(snap.StepJitterPP.Nanoseconds())/1e6))
			}
		} else {
			timingLines = append(timingLines, fmt.Sprintf("  Step Interval (16th): %s", lipgloss.NewStyle().Foreground(lipgloss.Color("#636E72")).Render("waiting for steps...")))
		}

		if snap.KickCount > 0 {
			timingLines = append(timingLines, fmt.Sprintf("  Beat Interval (Kick): %6.2f ms  (grid [%d steps]: %5.2f ms, dev: %+5.2f ms)",
				float64(snap.LastKickInterval.Nanoseconds())/1e6,
				snap.LastKickSteps,
				float64(snap.LastKickTarget.Nanoseconds())/1e6,
				float64(snap.LastKickDev.Nanoseconds())/1e6))
		} else {
			timingLines = append(timingLines, fmt.Sprintf("  Beat Interval (Kick): %s", lipgloss.NewStyle().Foreground(lipgloss.Color("#636E72")).Render("waiting for kick notes...")))
		}

		if snap.MaxChordSpread > 0 || snap.CurrentStepVoices > 1 {
			timingLines = append(timingLines, fmt.Sprintf("  Chord Spread (Skew) : %6.2f ms  (max inter-voice skew: %5.2f ms across %d voices)",
				float64(snap.LastChordSpread.Nanoseconds())/1e6,
				float64(snap.MaxChordSpread.Nanoseconds())/1e6,
				snap.CurrentStepVoices))
		} else {
			timingLines = append(timingLines, fmt.Sprintf("  Chord Spread (Skew) :   <0.01 ms  (voices trigger simultaneously)"))
		}
	}
	timingBox := lipgloss.NewStyle().
		BorderStyle(lipgloss.RoundedBorder()).
		BorderForeground(lipgloss.Color("#4A4B5A")).
		Padding(0, 1).
		Render(strings.Join(timingLines, "\n"))

	// Voice activity cards
	cardCols := 3
	if m.width >= 140 {
		cardCols = 6
	}
	cardWidth := (m.width - 8) / cardCols
	if cardWidth < 20 {
		cardWidth = 20
	} else if cardWidth > 26 {
		cardWidth = 26
	}

	cards := make([]string, 6)
	for i := 0; i < 6; i++ {
		active := now.Before(m.voiceFlash[i])
		disp := VoiceDisplay[i]
		drv := m.synth.VoiceDrive(disp.Note)
		isSelected := (i == m.selectedVoice)

		borderCol := lipgloss.Color("#2D3436")
		if isSelected {
			borderCol = lipgloss.Color("#FD79A8")
		} else if active {
			borderCol = lipgloss.Color("#74B9FF")
		}

		titleStr := fmt.Sprintf("%s: %s", disp.Tag, disp.Name)
		if isSelected {
			titleStr = fmt.Sprintf("▸%s: %s◂", disp.Tag, strings.TrimSpace(disp.Name))
		}

		velStr := fmt.Sprintf("%3d", m.voiceVel[i])
		if !active {
			velStr = "---"
		}

		if active {
			cards[i] = lipgloss.NewStyle().
				Bold(true).
				Foreground(lipgloss.Color("#FFFFFF")).
				Background(lipgloss.Color("#0984E3")).
				BorderStyle(lipgloss.RoundedBorder()).
				BorderForeground(borderCol).
				Width(cardWidth).
				Align(lipgloss.Center).
				Render(fmt.Sprintf("● %s\nVel: %s  Drv: %.1f", titleStr, velStr, drv))
		} else {
			style := lipgloss.NewStyle().
				BorderStyle(lipgloss.RoundedBorder()).
				BorderForeground(borderCol).
				Width(cardWidth).
				Align(lipgloss.Center)
			if isSelected {
				style = style.Foreground(lipgloss.Color("#FFFFFF"))
			} else {
				style = style.Foreground(lipgloss.Color("#636E72"))
			}
			cards[i] = style.Render(fmt.Sprintf("○ %s\nVel: %s  Drv: %.1f", titleStr, velStr, drv))
		}
	}

	var voiceMatrix string
	if cardCols == 6 {
		voiceMatrix = lipgloss.JoinHorizontal(lipgloss.Top, cards...)
	} else {
		voiceRow1 := lipgloss.JoinHorizontal(lipgloss.Top, cards[0], cards[1], cards[2])
		voiceRow2 := lipgloss.JoinHorizontal(lipgloss.Top, cards[3], cards[4], cards[5])
		voiceMatrix = lipgloss.JoinVertical(lipgloss.Left, voiceRow1, voiceRow2)
	}

	controls := lipgloss.NewStyle().
		Foreground(lipgloss.Color("#00CEC9")).
		Render("Controls: [Space] Play/Stop  │  [Tab/1-6] Select Voice  │  [s/S] Drive ±0.1  │  [+/-] BPM ±5  │  [R] Reset  │  [Q] Quit")

	// Activity Log Table
	logHeading := lipgloss.NewStyle().Bold(true).Render("Live MIDI Activity Monitor:")

	t := table.New().
		Headers("TIME", "CH", "TAG", "NOTE", "VEL", "LATENCY", "STEP DEV", "CHORD SKEW", "VOICE").
		Border(lipgloss.NormalBorder()).
		BorderTop(true).
		BorderBottom(false).
		BorderLeft(false).
		BorderRight(false).
		BorderColumn(false).
		BorderHeader(true).
		StyleFunc(func(row, col int) lipgloss.Style {
			s := lipgloss.NewStyle().Padding(0, 1)
			if row == table.HeaderRow {
				return s.Bold(true)
			}
			return s
		})

	maxRows := 8
	if m.height > 27 {
		maxRows = m.height - 23
		if maxRows > 30 {
			maxRows = 30
		}
	}

	startIdx := len(m.logs) - maxRows
	if startIdx < 0 {
		startIdx = 0
	}

	for _, log := range m.logs[startIdx:] {
		if log.IsStatus {
			t.Row(
				log.Time,
				"--",
				"SYS",
				"--",
				"--",
				"--",
				"--",
				"--",
				log.StatusText,
			)
		} else {
			t.Row(
				log.Time,
				fmt.Sprintf("%2d", log.Ch),
				log.Tag,
				log.NoteStr,
				fmt.Sprintf("%3d", log.Vel),
				log.Latency,
				log.StepDev,
				log.ChordSkew,
				log.VoiceName,
			)
		}
	}

	content := lipgloss.JoinVertical(
		lipgloss.Left,
		title,
		infoLine1,
		infoLine2,
		timingBox,
		voiceMatrix,
		controls,
		logHeading,
		t.Render(),
	)

	v := tea.NewView(content)
	v.AltScreen = true
	return v
}
