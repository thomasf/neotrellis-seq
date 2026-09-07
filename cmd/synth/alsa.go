package main

/*
#cgo LDFLAGS: -ldl
#include <stdlib.h>
#include "alsa_bridge.h"
*/
import "C"

import (
	"fmt"
	"sync"
	"sync/atomic"
	"time"
	"unsafe"
)

// InitAlsa loads libasound.so.2 dynamically
func InitAlsa() error {
	if C.init_alsa() != 0 {
		return fmt.Errorf("cannot load libasound.so.2")
	}
	return nil
}

// =============================================================================
// ALSA MIDI Sequencer Manager (Lock-Free Fast-Path)
// =============================================================================
type AlsaManager struct {
	seq            unsafe.Pointer
	clientID       int
	inPort         int
	outPort        int
	targetPattern  string
	targetClient   atomic.Int32
	targetPort     atomic.Int32
	targetName     string
	isConnected    atomic.Bool
	mu             sync.Mutex
	running        bool
	timing         *TimingTracker
	onNoteOn       func(ch, note, vel int, t time.Time, info StepTimingInfo)
	onNoteOff      func(ch, note, vel int)
	onStatusChange func(state, msg string)
}

func NewAlsaManager(pattern string, timing *TimingTracker, onNoteOn func(ch, note, vel int, t time.Time, info StepTimingInfo), onNoteOff func(ch, note, vel int), onStatus func(state, msg string)) (*AlsaManager, error) {
	if err := InitAlsa(); err != nil {
		return nil, err
	}

	var seq unsafe.Pointer
	cName := C.CString("NeoTrellis Companion")
	defer C.free(unsafe.Pointer(cName))

	cid := int(C.c_open_seq(&seq, cName))
	if cid < 0 {
		return nil, fmt.Errorf("cannot open ALSA sequencer duplex client")
	}

	cIn := C.CString("in")
	cOut := C.CString("out")
	defer C.free(unsafe.Pointer(cIn))
	defer C.free(unsafe.Pointer(cOut))

	// inPort: CAP_WRITE | CAP_SUBS_WRITE (allow external devices to connect and write into us)
	inPort := int(C.c_create_port(seq, cIn, (1<<1)|(1<<6)))
	// outPort: CAP_READ | CAP_SUBS_READ (allow external devices to connect and read from us)
	outPort := int(C.c_create_port(seq, cOut, (1<<0)|(1<<5)))

	m := &AlsaManager{
		seq:            seq,
		clientID:       cid,
		inPort:         inPort,
		outPort:        outPort,
		targetPattern:  pattern,
		running:        true,
		timing:         timing,
		onNoteOn:       onNoteOn,
		onNoteOff:      onNoteOff,
		onStatusChange: onStatus,
	}
	m.targetClient.Store(-1)
	m.targetPort.Store(-1)

	go m.reconnectLoop()
	go m.pollLoop()

	return m, nil
}

func (m *AlsaManager) reconnectLoop() {
	pattern := C.CString(m.targetPattern)
	defer C.free(unsafe.Pointer(pattern))
	nameBuf := make([]byte, 256)

	for m.running {
		var targetClient, targetPort C.int
		found := C.c_find_target(m.seq, C.int(m.clientID), pattern, &targetClient, &targetPort, (*C.char)(unsafe.Pointer(&nameBuf[0])), C.int(len(nameBuf)))

		tc := int(targetClient)
		tp := int(targetPort)
		wasConnected := m.isConnected.Load()

		if found != 0 {
			tname := C.GoString((*C.char)(unsafe.Pointer(&nameBuf[0])))
			prevClient := int(m.targetClient.Load())
			prevPort := int(m.targetPort.Load())

			if !wasConnected || prevClient != tc || prevPort != tp {
				C.c_connect(m.seq, C.int(m.outPort), C.int(m.inPort), targetClient, targetPort)
				m.targetClient.Store(int32(tc))
				m.targetPort.Store(int32(tp))
				m.mu.Lock()
				m.targetName = tname
				m.mu.Unlock()
				m.isConnected.Store(true)

				// Send Start upon connection to trigger the sequencer
				C.c_send_realtime(m.seq, C.int(m.outPort), C.int(tc), C.int(tp), C.int(EventStart))

				if m.onStatusChange != nil {
					m.onStatusChange("CONNECTED", fmt.Sprintf("%s @ ALSA %d:%d", tname, tc, tp))
				}
			}
		} else {
			if wasConnected {
				m.mu.Lock()
				oldName := m.targetName
				m.targetName = ""
				m.mu.Unlock()

				m.targetClient.Store(-1)
				m.targetPort.Store(-1)
				m.isConnected.Store(false)

				if m.onStatusChange != nil {
					m.onStatusChange("DISCONNECTED", fmt.Sprintf("%s disconnected (waiting for device...)", oldName))
				}
			}
		}

		time.Sleep(300 * time.Millisecond)
	}
}

func (m *AlsaManager) pollLoop() {
	for m.running {
		// Only wait in poll if no events are already buffered in userspace
		if C.c_event_pending(m.seq) == 0 {
			C.c_poll_wait(m.seq, 10)
		}

		// Drain ALL simultaneous queued events immediately without artificial delay
		for {
			var evType, ch, note, vel C.int
			if C.c_poll_event(m.seq, &evType, &ch, &note, &vel) == 0 {
				break
			}
			switch int(evType) {
			case EventNoteOn:
				v := int(vel)
				if v > 0 {
					now := time.Now()
					var info StepTimingInfo
					if m.timing != nil {
						info = m.timing.RecordNoteOn(int(note), v, now)
					}
					if m.onNoteOn != nil {
						m.onNoteOn(int(ch), int(note), v, now, info)
					}
				} else {
					if m.onNoteOff != nil {
						m.onNoteOff(int(ch), int(note), v)
					}
				}
			case EventNoteOff:
				if m.onNoteOff != nil {
					m.onNoteOff(int(ch), int(note), int(vel))
				}
			}
		}
	}
}

// SendRealtime is 100% lock-free for zero jitter
func (m *AlsaManager) SendRealtime(evType int) {
	tc := m.targetClient.Load()
	if tc < 0 {
		return
	}
	tp := m.targetPort.Load()
	C.c_send_realtime(m.seq, C.int(m.outPort), C.int(tc), C.int(tp), C.int(evType))
}

func (m *AlsaManager) Close() {
	m.running = false
	if m.seq != nil {
		C.c_close_seq(m.seq)
		m.seq = nil
	}
}
