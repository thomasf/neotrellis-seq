package main

import (
	"encoding/binary"
	"math"
	"math/rand"
	"sync"
)

// =============================================================================
// Analog Drum Synthesizer Engine
// =============================================================================
type ActiveVoice struct {
	Note  int
	Data  []int16
	Idx   int
	Gain  float64
	Drive float64
	PanL  float64
	PanR  float64
}

type dcBlocker struct {
	xPrev float64
	yPrev float64
}

func (b *dcBlocker) Process(x float64) float64 {
	// 1-pole high-pass DC blocking filter at ~10 Hz (SampleRate=44100)
	y := x - b.xPrev + 0.9985*b.yPrev
	b.xPrev = x
	b.yPrev = y
	return y
}

var DefaultVoiceDrives = map[int]float64{
	36: 0.1, // Kick: warm fat low-end punch
	37: 0.6, // Snare: snappy mid crunch
	38: 0.3, // Closed Hi-Hat: gentle tape sizzle
	39: 0.5, // Low Tom / Conga: round resonant warmth
	40: 0.6, // Mid Perc / Rim: punchy wood transient
	41: 1.5, // High Perc / Shaker: smooth high-frequency saturation
	42: 0.1, // 808 Sub Kick: heavy analog sub saturation
	43: 0.6, // Hand Clap: vintage compressor crunch
	44: 0.3, // Pedal Hi-Hat: crisp metallic edge
	45: 0.5, // High Tom: dynamic pitch sweep warmth
	46: 0.6, // 808 Cowbell: transistor diode bite
	47: 0.7, // Analog Zap: screaming resonant saturation
}

type DrumSynth struct {
	samples    map[int][]int16
	voices     []*ActiveVoice
	voiceDrive map[int]float64
	outL       []int32
	outR       []int32
	dcL        dcBlocker
	dcR        dcBlocker
	mu         sync.Mutex
}

func NewDrumSynth() *DrumSynth {
	s := &DrumSynth{
		samples:    make(map[int][]int16),
		voices:     make([]*ActiveVoice, 0, 48),
		voiceDrive: make(map[int]float64),
		outL:       make([]int32, BlockSize),
		outR:       make([]int32, BlockSize),
	}
	for k, v := range DefaultVoiceDrives {
		s.voiceDrive[k] = v
	}
	s.preRender()
	return s
}

func (s *DrumSynth) preRender() {
	sr := float64(SampleRate)

	// 36: Kick
	{
		n := int(sr * 0.35)
		data := make([]int16, n)
		p := 0.0
		for i := 0; i < n; i++ {
			t := float64(i) / sr
			f := 46.0 + (165.0-46.0)*math.Exp(-t*28.0)
			p += 2.0 * math.Pi * f / sr
			env := math.Exp(-t * 9.0)
			click := math.Exp(-t*300.0) * 0.45
			v := math.Tanh((math.Sin(p)*env + click) * 1.5)
			data[i] = int16(v * 30000)
		}
		s.samples[36] = data
	}

	// 37: Snare
	{
		n := int(sr * 0.22)
		data := make([]int16, n)
		p := 0.0
		lp := 0.0
		r := rand.New(rand.NewSource(12345))
		for i := 0; i < n; i++ {
			t := float64(i) / sr
			f := 185.0 + 50.0*math.Exp(-t*35.0)
			p += 2.0 * math.Pi * f / sr
			tone := math.Sin(p) * math.Exp(-t*16.0) * 0.35
			noise := (r.Float64()*2.0 - 1.0) * math.Exp(-t*14.0)
			lp += 0.55 * (noise - lp)
			hp := noise - lp
			v := math.Tanh((tone + hp*0.65) * 1.3)
			data[i] = int16(v * 28000)
		}
		s.samples[37] = data
	}

	// 38: Closed Hi-Hat
	{
		freqs := []float64{245, 306, 368, 415, 523, 638}
		n := int(sr * 0.055)
		data := make([]int16, n)
		lp := 0.0
		r := rand.New(rand.NewSource(23456))
		for i := 0; i < n; i++ {
			t := float64(i) / sr
			met := 0.0
			for _, f := range freqs {
				if math.Sin(2.0*math.Pi*f*t) > 0 {
					met += 1.0
				} else {
					met -= 1.0
				}
			}
			met /= float64(len(freqs))
			sig := 0.6*met + 0.4*(r.Float64()*2.0-1.0)
			lp += 0.25 * (sig - lp)
			hp := sig - lp
			v := math.Tanh(hp * math.Exp(-t*45.0) * 1.6)
			data[i] = int16(v * 26000)
		}
		s.samples[38] = data
	}

	// 39: Perc 1 (Low Tom / Conga)
	{
		n := int(sr * 0.25)
		data := make([]int16, n)
		p := 0.0
		for i := 0; i < n; i++ {
			t := float64(i) / sr
			f := 85.0 + 65.0*math.Exp(-t*22.0)
			p += 2.0 * math.Pi * f / sr
			v := math.Sin(p) * math.Exp(-t*12.0)
			data[i] = int16(math.Tanh(v*1.3) * 28000)
		}
		s.samples[39] = data
	}

	// 40: Perc 2 (Mid Perc / Woodblock / Rim)
	{
		n := int(sr * 0.1)
		data := make([]int16, n)
		p1, p2 := 0.0, 0.0
		for i := 0; i < n; i++ {
			t := float64(i) / sr
			p1 += 1.0 * math.Pi * 480.0 / sr
			p2 += 1.0 * math.Pi * 960.0 / sr
			v := (0.7*math.Sin(p1) + 0.3*math.Sin(p2)) * math.Exp(-t*30.0)
			data[i] = int16(math.Tanh(v*1.4) * 28000)
		}
		s.samples[40] = data
	}

	// 41: Perc 3 (High Perc / Shaker)
	{
		n := int(sr * 0.08)
		data := make([]int16, n)
		lp := 0.0
		r := rand.New(rand.NewSource(34567))
		for i := 0; i < n; i++ {
			t := float64(i) / sr
			noise := r.Float64()*2.0 - 1.0
			lp += 0.35 * (noise - lp)
			hp := noise - lp
			v := math.Tanh(hp * math.Exp(-t*28.0) * 1.5)
			data[i] = int16(v * 25000)
		}
		s.samples[41] = data
	}

	// 42: V0-Alt (808 Sub Kick)
	{
		n := int(sr * 0.45)
		data := make([]int16, n)
		p := 0.0
		for i := 0; i < n; i++ {
			t := float64(i) / sr
			f := 38.0 + (140.0-38.0)*math.Exp(-t*22.0)
			p += 2.0 * math.Pi * f / sr
			env := math.Exp(-t * 6.5)
			click := math.Exp(-t*350.0) * 0.35
			v := math.Tanh((math.Sin(p)*env + click) * 1.6)
			data[i] = int16(v * 30000)
		}
		s.samples[42] = data
	}

	// 43: Hand Clap
	{
		n := int(sr * 0.28)
		data := make([]int16, n)
		lp := 0.0
		r := rand.New(rand.NewSource(56789))
		for i := 0; i < n; i++ {
			t := float64(i) / sr
			burst := math.Exp(-t * 12.0)
			if t < 0.011 {
				burst += math.Exp(-t * 200.0)
			} else if t < 0.022 {
				burst += math.Exp(-(t - 0.011) * 200.0)
			} else if t < 0.033 {
				burst += math.Exp(-(t - 0.022) * 200.0)
			}
			noise := r.Float64()*2.0 - 1.0
			lp += 0.45 * (noise - lp)
			hp := noise - lp
			v := hp * burst
			data[i] = int16(math.Tanh(v*1.2) * 27000)
		}
		s.samples[43] = data
	}

	// 44: V2-Alt (Pedal Hi-Hat)
	{
		freqs := []float64{245, 306, 368, 415, 523, 638}
		n := int(sr * 0.04)
		data := make([]int16, n)
		lp := 0.0
		r := rand.New(rand.NewSource(34125))
		for i := 0; i < n; i++ {
			t := float64(i) / sr
			met := 0.0
			for _, f := range freqs {
				if math.Sin(2.0*math.Pi*f*t) > 0 {
					met += 1.0
				} else {
					met -= 1.0
				}
			}
			met /= float64(len(freqs))
			sig := 0.7*met + 0.3*(r.Float64()*2.0-1.0)
			lp += 0.3 * (sig - lp)
			hp := sig - lp
			v := math.Tanh(hp * math.Exp(-t*55.0) * 1.8)
			data[i] = int16(v * 26000)
		}
		s.samples[44] = data
	}

	// 45: V3-Alt (High Tom)
	{
		n := int(sr * 0.2)
		data := make([]int16, n)
		p := 0.0
		for i := 0; i < n; i++ {
			t := float64(i) / sr
			f := 150.0 + 80.0*math.Exp(-t*24.0)
			p += 2.0 * math.Pi * f / sr
			v := math.Sin(p) * math.Exp(-t*15.0)
			data[i] = int16(math.Tanh(v*1.3) * 28000)
		}
		s.samples[45] = data
	}

	// 46: Cowbell (808 style)
	{
		n := int(sr * 0.2)
		data := make([]int16, n)
		p1, p2 := 0.0, 0.0
		lp := 0.0
		for i := 0; i < n; i++ {
			t := float64(i) / sr
			p1 += 2.0 * math.Pi * 540.0 / sr
			p2 += 2.0 * math.Pi * 800.0 / sr
			sq1 := -1.0
			if math.Sin(p1) > 0 {
				sq1 = 1.0
			}
			sq2 := -1.0
			if math.Sin(p2) > 0 {
				sq2 = 1.0
			}
			sig := (sq1 + sq2) * 0.5
			lp += 0.5 * (sig - lp)
			hp := sig - lp
			v := hp * math.Exp(-t*15.0)
			data[i] = int16(math.Tanh(v*1.4) * 27000)
		}
		s.samples[46] = data
	}

	// 47: V5-Alt (Analog Synth Zap / Syndrum)
	{
		n := int(sr * 0.04)
		data := make([]int16, n)
		p := 0.0
		for i := 0; i < n; i++ {
			t := float64(i) / sr
			f := 160.0 + 2640.0*math.Exp(-t*36.0)
			p += 1.3 * math.Pi * f / sr
			env := math.Exp(-t * 16.0)
			wave := math.Sin(p) + 0.35*math.Sin(2.0*p)
			v := math.Tanh(wave * env * 1.7)
			data[i] = int16(v * 28000)
		}
		s.samples[47] = data
	}
}

func (s *DrumSynth) NoteOn(note, velocity int) {
	if velocity <= 0 {
		return
	}

	data, ok := s.samples[note]
	if !ok {
		// Map GM / Ableton notes above 47 to appropriate drum synth samples
		switch note {
		case 49: // Crash Cymbal
			data = s.samples[47] // Zap / metallic transient
		case 50: // High Tom
			data = s.samples[45] // High Tom
		case 51: // Ride Cymbal
			data = s.samples[44] // Pedal / Open Hat
		default:
			baseNote := 36 + (note % 12)
			data = s.samples[baseNote]
			if data == nil {
				data = s.samples[36]
			}
		}
	}

	gain := math.Pow(float64(velocity)/127.0, 1.25)
	panL, panR := 0.7, 0.7

	switch note {
	case 36, 42:
		panL, panR = 0.7, 0.7 // Kicks (centered)
	case 37, 38, 40, 43:
		panL, panR = 0.75, 0.65 // Snare / Clap / Rim
	case 44, 46:
		panL, panR = 0.5, 0.8 // Hi-Hats
	case 39, 41, 45, 47, 48, 50:
		panL, panR = 0.8, 0.5 // Toms
	case 49, 51:
		panL, panR = 0.6, 0.85 // Cymbals / Ride / Crash
	}

	s.mu.Lock()
	defer s.mu.Unlock()

	// Choke pedal hat on closed hat
	if note == 38 {
		for _, v := range s.voices {
			if v.Note == 44 {
				v.Gain *= 0.1
			}
		}
	}

	drv, ok := s.voiceDrive[note]
	if !ok {
		baseNote := 36 + (note % 12)
		drv = s.voiceDrive[baseNote]
		if drv == 0 {
			drv = 0.5
		}
	}

	s.voices = append(s.voices, &ActiveVoice{
		Note:  note,
		Data:  data,
		Idx:   0,
		Gain:  gain,
		Drive: drv,
		PanL:  panL,
		PanR:  panR,
	})

	if len(s.voices) > 48 {
		s.voices = s.voices[1:]
	}
}

// VoiceDrive returns the individual saturation drive for the given voice note.
func (s *DrumSynth) VoiceDrive(note int) float64 {
	s.mu.Lock()
	defer s.mu.Unlock()
	if d, ok := s.voiceDrive[note]; ok {
		return d
	}
	base := 36 + (note % 12)
	if d, ok := s.voiceDrive[base]; ok {
		return d
	}
	return 0.5
}

// SetVoiceDrive sets the individual saturation drive for a voice note.
func (s *DrumSynth) SetVoiceDrive(note int, drive float64) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if drive < 0.0 {
		drive = 0.0
	} else if drive > 2.0 {
		drive = 2.0
	}
	drive = math.Round(drive*100) / 100
	s.voiceDrive[note] = drive
}

// AdjustVoiceDrive adjusts the individual saturation drive for a voice note by delta.
func (s *DrumSynth) AdjustVoiceDrive(note int, delta float64) float64 {
	s.mu.Lock()
	defer s.mu.Unlock()
	cur, ok := s.voiceDrive[note]
	if !ok {
		base := 36 + (note % 12)
		cur = s.voiceDrive[base]
		if cur == 0 {
			cur = 0.5
		}
	}
	newVal := math.Round((cur+delta)*10) / 10
	if newVal < 0.0 {
		newVal = 0.0
	} else if newVal > 2.0 {
		newVal = 2.0
	}
	s.voiceDrive[note] = newVal
	// Also sync paired alt note if applicable (e.g. 36 Kick and 42 SubKick)
	if note >= 36 && note <= 41 {
		s.voiceDrive[note+6] = newVal
	}
	return newVal
}

// saturateVoice applies analog tape/console saturation individually per voice.
// drive: voice drive [0.0 = clean, 2.0 = heavy saturation].
// gain: velocity-derived gain factor [0.0 - 1.0], dynamically scaling saturation intensity.
func saturateVoice(x, drive, gain float64) float64 {
	if drive <= 0.001 {
		if x > 0.95 {
			return 0.95 + 0.05*math.Tanh((x-0.95)/0.05)
		} else if x < -0.95 {
			return -0.95 + 0.05*math.Tanh((x+0.95)/0.05)
		}
		return x
	}

	// Dynamic drive modulated by velocity: harder hits drive deeper into saturation
	effDrive := (1.0 + drive*1.4) * (0.85 + 0.3*gain)
	in := x * effDrive

	// Subtle asymmetric even harmonics for analog warmth and thickness
	warmth := 0.06 * drive
	if in > 0 {
		in += warmth * in * in
	} else {
		in -= warmth * 0.4 * in * in
	}

	// Non-linear hyperbolic tangent saturation
	out := math.Tanh(in)

	// Level compensation
	norm := 1.0 / math.Tanh(effDrive*0.85)
	out *= norm

	if out > 0.999 {
		out = 0.999
	} else if out < -0.999 {
		out = -0.999
	}
	return out
}

func (s *DrumSynth) RenderBlock(buf []byte, numFrames int) {
	s.mu.Lock()
	defer s.mu.Unlock()

	if len(s.outL) < numFrames {
		s.outL = make([]int32, numFrames)
		s.outR = make([]int32, numFrames)
	}
	for i := 0; i < numFrames; i++ {
		s.outL[i] = 0
		s.outR[i] = 0
	}

	alive := s.voices[:0]
	for _, v := range s.voices {
		avail := len(v.Data) - v.Idx
		if avail > numFrames {
			avail = numFrames
		}

		gl := v.Gain * v.PanL
		gr := v.Gain * v.PanR
		drv := v.Drive

		for i := 0; i < avail; i++ {
			samp := float64(v.Data[v.Idx+i]) / 32768.0
			satSamp := saturateVoice(samp, drv, v.Gain)
			s.outL[i] += int32(satSamp * gl * 32768.0)
			s.outR[i] += int32(satSamp * gr * 32768.0)
		}

		v.Idx += avail
		if v.Idx < len(v.Data) {
			alive = append(alive, v)
		}
	}
	s.voices = alive

	for i := 0; i < numFrames; i++ {
		xL := float64(s.outL[i]) / 32768.0
		xR := float64(s.outR[i]) / 32768.0

		// Master DC blocker to ensure zero DC offset across all voices
		xL = s.dcL.Process(xL)
		xR = s.dcR.Process(xR)

		// Master transparent soft-limiting so multi-voice peaks never digitally hard-clip
		if xL > 0.95 {
			xL = 0.95 + 0.05*math.Tanh((xL-0.95)/0.05)
		} else if xL < -0.95 {
			xL = -0.95 + 0.05*math.Tanh((xL+0.95)/0.05)
		}

		if xR > 0.95 {
			xR = 0.95 + 0.05*math.Tanh((xR-0.95)/0.05)
		} else if xR < -0.95 {
			xR = -0.95 + 0.05*math.Tanh((xR+0.95)/0.05)
		}

		sl := int32(xL * 32767.0)
		if sl > 32767 {
			sl = 32767
		} else if sl < -32768 {
			sl = -32768
		}

		sr := int32(xR * 32767.0)
		if sr > 32767 {
			sr = 32767
		} else if sr < -32768 {
			sr = -32768
		}

		offset := i * 4
		binary.LittleEndian.PutUint16(buf[offset:], uint16(int16(sl)))
		binary.LittleEndian.PutUint16(buf[offset+2:], uint16(int16(sr)))
	}
}
