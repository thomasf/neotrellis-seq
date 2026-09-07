package main

import (
	"math"
	"sync"
	"sync/atomic"
	"time"
)

// =============================================================================
// Precision Monotonic MIDI Clock Generator (24 PPQN)
// =============================================================================
type MidiClock struct {
	alsa      *AlsaManager
	timing    *TimingTracker
	bpmBits   atomic.Uint64
	enabled   bool
	running   atomic.Bool
	tickCount atomic.Uint64
	stopCh    chan struct{}
	mu        sync.Mutex
}

func NewMidiClock(alsa *AlsaManager, bpm float64, enabled bool, timing *TimingTracker) *MidiClock {
	c := &MidiClock{
		alsa:    alsa,
		timing:  timing,
		enabled: enabled,
	}
	c.bpmBits.Store(math.Float64bits(bpm))
	return c
}

func (c *MidiClock) BPM() float64 {
	return math.Float64frombits(c.bpmBits.Load())
}

func (c *MidiClock) SetBPM(bpm float64) {
	if bpm < 20.0 {
		bpm = 20.0
	} else if bpm > 300.0 {
		bpm = 300.0
	}
	c.bpmBits.Store(math.Float64bits(bpm))
}

func (c *MidiClock) AdjustBPM(delta float64) {
	c.SetBPM(c.BPM() + delta)
}

func (c *MidiClock) Start() {
	c.mu.Lock()
	defer c.mu.Unlock()
	if !c.enabled || c.running.Load() {
		return
	}
	c.running.Store(true)
	c.stopCh = make(chan struct{})
	c.alsa.SendRealtime(EventStart)

	go c.loop()
}

func (c *MidiClock) Pause() {
	c.mu.Lock()
	defer c.mu.Unlock()
	if !c.running.Load() {
		return
	}
	c.running.Store(false)
	close(c.stopCh)
	c.alsa.SendRealtime(EventStop)
}

func (c *MidiClock) Resume() {
	c.mu.Lock()
	defer c.mu.Unlock()
	if !c.enabled || c.running.Load() {
		return
	}
	c.running.Store(true)
	c.stopCh = make(chan struct{})
	c.alsa.SendRealtime(EventContinue)

	go c.loop()
}

func (c *MidiClock) Toggle() {
	if c.running.Load() {
		c.Pause()
	} else {
		c.Resume()
	}
}

func (c *MidiClock) Reset() {
	c.tickCount.Store(0)
	c.alsa.SendRealtime(EventReset)
	if c.running.Load() {
		c.alsa.SendRealtime(EventStart)
	}
}

func (c *MidiClock) loop() {
	bpm := c.BPM()
	interval := time.Duration(float64(time.Minute) / (bpm * 24.0))
	ticker := time.NewTicker(interval)
	defer ticker.Stop()

	lastBPM := bpm
	for {
		select {
		case <-c.stopCh:
			return
		case <-ticker.C:
			tick := c.tickCount.Add(1)
			now := time.Now()
			c.alsa.SendRealtime(EventClock)

			// Step boundary clock tick is tick 1, 7, 13, 19... ((tick - 1) % 6 == 0)
			if (tick-1)%6 == 0 && c.timing != nil {
				c.timing.RecordStepClock(tick, now)
			}

			curBPM := c.BPM()
			if curBPM != lastBPM {
				lastBPM = curBPM
				interval = time.Duration(float64(time.Minute) / (curBPM * 24.0))
				ticker.Reset(interval)
			}
		}
	}
}
