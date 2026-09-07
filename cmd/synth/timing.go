package main

import (
	"math"
	"sync"
	"time"
)

// =============================================================================
// MIDI & Sequencer Timing Tracker
// =============================================================================
type StepTimingInfo struct {
	Note         int
	Vel          int
	IsNewStep    bool
	StepInterval time.Duration
	StepDev      time.Duration
	StepCount    int
	HasLatency   bool
	Latency      time.Duration
	ChordSkew    time.Duration
	IsKick       bool
	KickInterval time.Duration
	KickDev      time.Duration
	KickSteps    int
}

type TimingSnapshot struct {
	HasLatency       bool
	LastLatency      time.Duration
	MinLatency       time.Duration
	MaxLatency       time.Duration
	AvgLatency       time.Duration
	LatencyCount     int64

	IdealStep        time.Duration
	LastStepInterval time.Duration
	LastStepDev      time.Duration
	LastStepCount    int
	MinStepDev       time.Duration
	MaxStepDev       time.Duration
	StepJitterPP     time.Duration
	StepCount        int64

	IdealKick        time.Duration
	LastKickInterval time.Duration
	LastKickTarget   time.Duration
	LastKickDev      time.Duration
	LastKickSteps    int
	KickCount        int64

	CurrentStepVoices int
	LastChordSpread   time.Duration
	MaxChordSpread    time.Duration
}

type TimingTracker struct {
	mu sync.Mutex

	bpmFunc func() float64

	lastStepClockTime time.Time
	stepClockActive   bool

	lastStepTime     time.Time
	lastStepInterval time.Duration
	lastStepDev      time.Duration
	lastStepSteps    int
	minStepDev       time.Duration
	maxStepDev       time.Duration
	stepJitterPP     time.Duration
	stepCount        int64

	lastLatency  time.Duration
	minLatency   time.Duration
	maxLatency   time.Duration
	avgLatency   time.Duration
	latencySum   time.Duration
	latencyCount int64

	currentStepFirstNote time.Time
	currentStepVoices    int
	lastChordSpread      time.Duration
	maxChordSpread       time.Duration

	lastKickTime     time.Time
	lastKickInterval time.Duration
	lastKickTarget   time.Duration
	lastKickDev      time.Duration
	lastKickSteps    int
	kickCount        int64
}

func NewTimingTracker(bpmFunc func() float64) *TimingTracker {
	return &TimingTracker{
		bpmFunc: bpmFunc,
	}
}

func (t *TimingTracker) RecordStepClock(tick uint64, ts time.Time) {
	t.mu.Lock()
	defer t.mu.Unlock()
	t.lastStepClockTime = ts
	t.stepClockActive = true
}

func (t *TimingTracker) RecordNoteOn(note, vel int, now time.Time) StepTimingInfo {
	t.mu.Lock()
	defer t.mu.Unlock()

	bpm := 120.0
	if t.bpmFunc != nil {
		bpm = t.bpmFunc()
	}
	idealStep := time.Duration(float64(time.Minute) / (bpm * 4.0)) // 16th note (125ms @ 120BPM)

	var info StepTimingInfo
	info.Note = note
	info.Vel = vel

	// Simultaneous chord notes within 15ms belong to the same sequencer step
	isNewStep := t.lastStepTime.IsZero() || now.Sub(t.lastStepTime) >= 15*time.Millisecond
	info.IsNewStep = isNewStep

	if isNewStep {
		t.currentStepFirstNote = now
		t.currentStepVoices = 1
		info.ChordSkew = 0

		if !t.lastStepTime.IsZero() {
			interval := now.Sub(t.lastStepTime)
			// Quantize elapsed interval to nearest number of 16th-note steps
			steps := int(math.Round(float64(interval) / float64(idealStep)))
			if steps < 1 {
				steps = 1
			}
			gridTarget := time.Duration(steps) * idealStep
			dev := interval - gridTarget

			t.lastStepInterval = interval
			t.lastStepDev = dev
			t.lastStepSteps = steps
			info.StepInterval = interval
			info.StepDev = dev
			info.StepCount = steps

			if t.stepCount == 0 || dev < t.minStepDev {
				t.minStepDev = dev
			}
			if t.stepCount == 0 || dev > t.maxStepDev {
				t.maxStepDev = dev
			}
			t.stepJitterPP = t.maxStepDev - t.minStepDev
			t.stepCount++
		}
		t.lastStepTime = now

		if t.stepClockActive && !t.lastStepClockTime.IsZero() {
			lat := now.Sub(t.lastStepClockTime)
			// Ensure latency is measured from the most recent step boundary clock
			if lat >= 0 && lat < idealStep {
				info.HasLatency = true
				info.Latency = lat
				t.lastLatency = lat
				if t.latencyCount == 0 || lat < t.minLatency {
					t.minLatency = lat
				}
				if t.latencyCount == 0 || lat > t.maxLatency {
					t.maxLatency = lat
				}
				t.latencySum += lat
				t.latencyCount++
				t.avgLatency = t.latencySum / time.Duration(t.latencyCount)
			}
		}
	} else {
		t.currentStepVoices++
		skew := now.Sub(t.currentStepFirstNote)
		info.ChordSkew = skew
		t.lastChordSpread = skew
		if skew > t.maxChordSpread {
			t.maxChordSpread = skew
		}
	}

	// Kick rhythm analysis (Notes 35, 36, 42)
	if note == 35 || note == 36 || note == 42 {
		info.IsKick = true
		if !t.lastKickTime.IsZero() {
			kInterval := now.Sub(t.lastKickTime)
			// Quantize elapsed interval between kicks to the nearest 16th step
			kSteps := int(math.Round(float64(kInterval) / float64(idealStep)))
			if kSteps < 1 {
				kSteps = 1
			}
			kTarget := time.Duration(kSteps) * idealStep
			kDev := kInterval - kTarget

			t.lastKickInterval = kInterval
			t.lastKickTarget = kTarget
			t.lastKickDev = kDev
			t.lastKickSteps = kSteps

			info.KickInterval = kInterval
			info.KickDev = kDev
			info.KickSteps = kSteps
			t.kickCount++
		}
		t.lastKickTime = now
	}

	return info
}

func (t *TimingTracker) Snapshot() TimingSnapshot {
	t.mu.Lock()
	defer t.mu.Unlock()

	bpm := 120.0
	if t.bpmFunc != nil {
		bpm = t.bpmFunc()
	}
	idealStep := time.Duration(float64(time.Minute) / (bpm * 4.0))
	idealKick := time.Duration(float64(time.Minute) / bpm)

	kTarget := t.lastKickTarget
	if kTarget == 0 {
		kTarget = idealKick
	}

	return TimingSnapshot{
		HasLatency:       t.latencyCount > 0,
		LastLatency:      t.lastLatency,
		MinLatency:       t.minLatency,
		MaxLatency:       t.maxLatency,
		AvgLatency:       t.avgLatency,
		LatencyCount:     t.latencyCount,

		IdealStep:        idealStep,
		LastStepInterval: t.lastStepInterval,
		LastStepDev:      t.lastStepDev,
		LastStepCount:    t.lastStepSteps,
		MinStepDev:       t.minStepDev,
		MaxStepDev:       t.maxStepDev,
		StepJitterPP:     t.stepJitterPP,
		StepCount:        t.stepCount,

		IdealKick:        idealKick,
		LastKickInterval: t.lastKickInterval,
		LastKickTarget:   kTarget,
		LastKickDev:      t.lastKickDev,
		LastKickSteps:    t.lastKickSteps,
		KickCount:        t.kickCount,

		CurrentStepVoices: t.currentStepVoices,
		LastChordSpread:   t.lastChordSpread,
		MaxChordSpread:    t.maxChordSpread,
	}
}
