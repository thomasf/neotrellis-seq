package main

import (
	"flag"
	"fmt"
	"os"
	"os/signal"
	"syscall"
	"time"

	tea "charm.land/bubbletea/v2"
)

// =============================================================================
// Main Entry Point
// =============================================================================
func main() {
	bpmFlag := flag.Float64("bpm", 120.0, "Initial clock tempo in BPM (default: 120.0)")
	noClockFlag := flag.Bool("no-clock", false, "Disable sending MIDI clock (monitor/synth only)")
	targetFlag := flag.String("target", "Trellis", "Target device search pattern (default: Trellis)")
	testFlag := flag.Bool("test", false, "Play sound check of all drum voices and exit")
	noTUIFlag := flag.Bool("no-tui", false, "Disable interactive ANSI TUI (line logging mode)")

	flag.Parse()

	timing := NewTimingTracker(nil)

	synth := NewDrumSynth()

	player, err := StartAudioPlayer(synth)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Audio warning: %v\n", err)
	} else {
		defer player.Stop()
	}

	if *testFlag {
		if player != nil {
			fmt.Printf("Audio backend: %s\n", player.Backend())
		}
		time.Sleep(250*time.Millisecond)
		fmt.Println("Running sound check on Go drum synthesizer voices (individual voice saturation)...")
		testNotes := []int{36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47}
		for _, note := range testNotes {
			info := VoiceMap[note]
			drv := synth.VoiceDrive(note)
			fmt.Printf("  -> Triggering Note %d: %s (%s) [Drive: %.1f]\n", note, info.Tag, info.FullName, drv)
			synth.NoteOn(note, 120)
			time.Sleep(250 * time.Millisecond)
		}
		time.Sleep(400 * time.Millisecond)
		fmt.Println("Go sound check completed successfully!")
		return
	}

	dispatcher := newEventDispatcher()

	alsa, err := NewAlsaManager(*targetFlag, timing,
		func(ch, note, vel int, t time.Time, info StepTimingInfo) {
			synth.NoteOn(note, vel)
			if *noTUIFlag {
				nowStr := t.Format("15:04:05.000")
				vInfo, ok := VoiceMap[note]
				if !ok {
					vInfo = VoiceInfo{Tag: "V?", Name: fmt.Sprintf("Note %d", note), FullName: midiNoteToStr(note)}
				}
				if info.IsNewStep {
					kickInfo := ""
					if info.IsKick && info.KickInterval > 0 {
						kickInfo = fmt.Sprintf(" | Kick Δ [%d st]: %6.2fms (dev: %+5.2fms)",
							info.KickSteps,
							float64(info.KickInterval.Nanoseconds())/1e6,
							float64(info.KickDev.Nanoseconds())/1e6)
					}
					latInfo := "n/a"
					if info.HasLatency {
						latInfo = fmt.Sprintf("%5.2fms", float64(info.Latency.Nanoseconds())/1e6)
					}
					stepInfo := "first step"
					if info.StepInterval > 0 {
						if info.StepCount > 1 {
							stepInfo = fmt.Sprintf("%6.2fms [%d st] (dev: %+5.2fms)",
								float64(info.StepInterval.Nanoseconds())/1e6,
								info.StepCount,
								float64(info.StepDev.Nanoseconds())/1e6)
						} else {
							stepInfo = fmt.Sprintf("%6.2fms (dev: %+5.2fms)",
								float64(info.StepInterval.Nanoseconds())/1e6,
								float64(info.StepDev.Nanoseconds())/1e6)
						}
					}
					fmt.Printf("[MIDI %s] %-8s Note %2d (v=%3d) | Latency: %s | Step Δ: %s%s\n",
						nowStr, vInfo.Name, note, vel, latInfo, stepInfo, kickInfo)
				} else {
					fmt.Printf("[MIDI %s]   └─ %-8s Note %2d (v=%3d) | Chord Skew: %+5.2fms\n",
						nowStr, vInfo.Name, note, vel,
						float64(info.ChordSkew.Nanoseconds())/1e6)
				}
			} else {
				dispatcher.Send(midiNoteMsg{
					ch:     ch,
					note:   note,
					vel:    vel,
					t:      t,
					timing: info,
				})
			}
		},
		func(ch, note, vel int) {
			// Note off handler
		},
		func(state, msg string) {
			if *noTUIFlag {
				fmt.Printf("[%s] %s\n", state, msg)
			} else {
				dispatcher.Send(midiStatusMsg{
					state: state,
					msg:   msg,
					t:     time.Now(),
				})
			}
		},
	)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error initializing ALSA sequencer: %v\n", err)
		os.Exit(1)
	}
	defer alsa.Close()

	clock := NewMidiClock(alsa, *bpmFlag, !*noClockFlag, timing)
	timing.bpmFunc = clock.BPM
	clock.Start()
	defer clock.Pause()

	if *noTUIFlag {
		fmt.Println("\n=== NeoTrellis Development Companion (Go, Simple Log Mode) ===")
		fmt.Printf("Clock: %.1f BPM\n", clock.BPM())
		if player != nil {
			fmt.Printf("Audio: %s\n", player.Backend())
		}
		fmt.Println("Voice Saturation: Enabled (individual per voice)")
		fmt.Println("Press Ctrl+C to stop.")

		sigCh := make(chan os.Signal, 1)
		signal.Notify(sigCh, os.Interrupt, syscall.SIGTERM)
		<-sigCh
	} else {
		model := newTUIModel(clock, synth, alsa, player, timing)
		p := tea.NewProgram(model)
		dispatcher.SetProgram(p)

		if _, err := p.Run(); err != nil {
			fmt.Fprintf(os.Stderr, "Error running TUI: %v\n", err)
			os.Exit(1)
		}
	}

	fmt.Println("\nNeoTrellis Companion (Go) stopped.")
}
