package main

/*
#cgo LDFLAGS: -ldl
#include <stdlib.h>
#include "alsa_bridge.h"
*/
import "C"

import (
	"fmt"
	"os"
	"os/exec"
	"time"
	"unsafe"
)

// =============================================================================
// Low-Latency Audio Engine (Direct ALSA PCM + Pipe Fallback)
// =============================================================================
type AudioPlayer struct {
	synth   *DrumSynth
	pcm     unsafe.Pointer
	backend string
	cmd     *exec.Cmd
	stdin   *os.File
	running bool
	stopCh  chan struct{}
}

func (p *AudioPlayer) Backend() string {
	return p.backend
}

func StartAudioPlayer(synth *DrumSynth) (*AudioPlayer, error) {
	p := &AudioPlayer{
		synth:   synth,
		running: true,
		stopCh:  make(chan struct{}),
	}

	// 1. Try direct ALSA PCM with 15ms buffer (lowest latency, hardware paced)
	var pcm unsafe.Pointer
	for _, devName := range []string{"default", "pulse", "pipewire"} {
		cDev := C.CString(devName)
		res := C.c_pcm_open(&pcm, cDev, C.uint(SampleRate), 15000)
		C.free(unsafe.Pointer(cDev))
		if res == 0 {
			p.pcm = pcm
			p.backend = fmt.Sprintf("Direct ALSA PCM (%s, 15ms buffer)", devName)
			go p.pcmLoop()
			return p, nil
		}
	}

	// 2. Fallback to paplay / aplay pipe with low latency flags
	var cmd *exec.Cmd
	var backend string
	if _, err := exec.LookPath("paplay"); err == nil {
		cmd = exec.Command("paplay", "--raw", fmt.Sprintf("--rate=%d", SampleRate), "--channels=2", "--format=s16le", "--latency-msec=20")
		backend = "paplay pipe (20ms target)"
	} else if _, err := exec.LookPath("aplay"); err == nil {
		cmd = exec.Command("aplay", "-f", "S16_LE", "-c", "2", "-r", fmt.Sprintf("%d", SampleRate), "--buffer-time=20000", "-q")
		backend = "aplay pipe (20ms buffer)"
	} else {
		return nil, fmt.Errorf("no audio device or player found")
	}

	r, w, err := os.Pipe()
	if err != nil {
		return nil, err
	}
	cmd.Stdin = r
	if err := cmd.Start(); err != nil {
		r.Close()
		w.Close()
		return nil, err
	}
	p.cmd = cmd
	p.stdin = w
	p.backend = backend

	go p.pipeLoop()
	return p, nil
}

func (p *AudioPlayer) pcmLoop() {
	buf := make([]byte, BlockSize*4)
	for p.running {
		p.synth.RenderBlock(buf, BlockSize)
		C.c_pcm_write(p.pcm, unsafe.Pointer(&buf[0]), C.ulong(BlockSize))
	}
}

func (p *AudioPlayer) pipeLoop() {
	buf := make([]byte, BlockSize*4)
	interval := time.Duration(int64(BlockSize) * int64(time.Second) / SampleRate)
	ticker := time.NewTicker(interval)
	defer ticker.Stop()

	for p.running {
		select {
		case <-p.stopCh:
			return
		case <-ticker.C:
			p.synth.RenderBlock(buf, BlockSize)
			if p.stdin != nil {
				p.stdin.Write(buf)
			}
		}
	}
}

func (p *AudioPlayer) Stop() {
	p.running = false
	close(p.stopCh)
	if p.pcm != nil {
		C.c_pcm_close(p.pcm)
		p.pcm = nil
	}
	if p.stdin != nil {
		p.stdin.Close()
	}
	if p.cmd != nil && p.cmd.Process != nil {
		p.cmd.Process.Kill()
	}
}
