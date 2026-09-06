package main

// this is a partial port of uf2conv.py

import (
	_ "embed"
	"encoding/binary"
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"sort"
	"strconv"
	"strings"
)

//go:embed uf2families.json
var embeddedFamiliesJSON []byte

const (
	UF2MagicStart0 = 0x0A324655 // "UF2\n"
	UF2MagicStart1 = 0x9E5D5157 // Randomly selected
	UF2MagicEnd    = 0x0AB16F30
)

type familyEntry struct {
	ID          string `json:"id"`
	ShortName   string `json:"short_name"`
	Description string `json:"description"`
}

func loadFamilies() (map[string]uint32, map[uint32]string, []string, error) {
	var entries []familyEntry
	if err := json.Unmarshal(embeddedFamiliesJSON, &entries); err != nil {
		return nil, nil, nil, err
	}

	byName := make(map[string]uint32, len(entries))
	byID := make(map[uint32]string, len(entries))
	var names []string

	for _, e := range entries {
		id64, err := strconv.ParseUint(e.ID, 0, 32)
		if err != nil {
			continue
		}
		id := uint32(id64)
		byName[strings.ToUpper(e.ShortName)] = id
		byID[id] = e.ShortName
		names = append(names, e.ShortName)
	}

	sort.Strings(names)
	return byName, byID, names, nil
}

func isUF2(buf []byte) bool {
	if len(buf) < 8 {
		return false
	}
	m0 := binary.LittleEndian.Uint32(buf[0:4])
	m1 := binary.LittleEndian.Uint32(buf[4:8])
	return m0 == UF2MagicStart0 && m1 == UF2MagicStart1
}

func isHex(buf []byte) bool {
	if len(buf) == 0 || buf[0] != ':' {
		return false
	}
	for _, b := range buf {
		if b == ':' || b == '\r' || b == '\n' ||
			(b >= '0' && b <= '9') ||
			(b >= 'a' && b <= 'f') ||
			(b >= 'A' && b <= 'F') {
			continue
		}
		return false
	}
	return true
}

func convertToUF2(fileContent []byte, appStartAddr uint32, familyID uint32) []byte {
	numBlocks := (len(fileContent) + 255) / 256
	out := make([]byte, 0, numBlocks*512)

	var flags uint32 = 0
	if familyID != 0 {
		flags |= 0x2000
	}

	for blockNo := 0; blockNo < numBlocks; blockNo++ {
		ptr := blockNo * 256
		end := ptr + 256
		if end > len(fileContent) {
			end = len(fileContent)
		}
		chunk := fileContent[ptr:end]

		var block [512]byte
		binary.LittleEndian.PutUint32(block[0:4], UF2MagicStart0)
		binary.LittleEndian.PutUint32(block[4:8], UF2MagicStart1)
		binary.LittleEndian.PutUint32(block[8:12], flags)
		binary.LittleEndian.PutUint32(block[12:16], uint32(ptr)+appStartAddr)
		binary.LittleEndian.PutUint32(block[16:20], 256)
		binary.LittleEndian.PutUint32(block[20:24], uint32(blockNo))
		binary.LittleEndian.PutUint32(block[24:28], uint32(numBlocks))
		binary.LittleEndian.PutUint32(block[28:32], familyID)

		copy(block[32:32+len(chunk)], chunk)
		binary.LittleEndian.PutUint32(block[508:512], UF2MagicEnd)

		out = append(out, block[:]...)
	}

	return out
}

func convertFromUF2(buf []byte, familyID uint32, familiesByID map[uint32]string) ([]byte, uint32, error) {
	numBlocks := len(buf) / 512
	var currAddr *uint32
	var currFamilyID *uint32
	var familiesOrder []uint32
	familiesFound := make(map[uint32]uint32)
	var prevFlag *uint32
	allFlagsSame := true
	var lastFlag uint32
	var outp []byte
	var appStartAddr uint32 = 0x2000

	for blockNo := 0; blockNo < numBlocks; blockNo++ {
		ptr := blockNo * 512
		block := buf[ptr : ptr+512]

		m0 := binary.LittleEndian.Uint32(block[0:4])
		m1 := binary.LittleEndian.Uint32(block[4:8])
		if m0 != UF2MagicStart0 || m1 != UF2MagicStart1 {
			fmt.Printf("Skipping block at %d; bad magic\n", ptr)
			continue
		}

		flags := binary.LittleEndian.Uint32(block[8:12])
		if flags&1 != 0 {
			// NO-flash flag set; skip block
			continue
		}

		dataLen := binary.LittleEndian.Uint32(block[16:20])
		if dataLen > 476 {
			return nil, 0, fmt.Errorf("invalid UF2 data size at %d", ptr)
		}

		newAddr := binary.LittleEndian.Uint32(block[12:16])
		hdFamily := binary.LittleEndian.Uint32(block[28:32])

		if (flags&0x2000 != 0) && currFamilyID == nil {
			f := hdFamily
			currFamilyID = &f
		}

		if currAddr == nil || ((flags&0x2000 != 0) && *currFamilyID != hdFamily) {
			f := hdFamily
			currFamilyID = &f
			a := newAddr
			currAddr = &a
			if familyID == 0 || familyID == hdFamily {
				appStartAddr = newAddr
			}
		}

		padding := int64(newAddr) - int64(*currAddr)
		if padding < 0 {
			return nil, 0, fmt.Errorf("block out of order at %d", ptr)
		}
		if padding > 10*1024*1024 {
			return nil, 0, fmt.Errorf("more than 10M of padding needed at %d", ptr)
		}
		if padding%4 != 0 {
			return nil, 0, fmt.Errorf("non-word padding size at %d", ptr)
		}

		for padding > 0 {
			padding -= 4
			outp = append(outp, 0, 0, 0, 0)
		}

		if familyID == 0 || ((flags&0x2000 != 0) && familyID == hdFamily) {
			outp = append(outp, block[32:32+dataLen]...)
		}

		*currAddr = newAddr + dataLen

		if flags&0x2000 != 0 {
			if minAddr, exists := familiesFound[hdFamily]; exists {
				if newAddr < minAddr {
					familiesFound[hdFamily] = newAddr
				}
			} else {
				familiesOrder = append(familiesOrder, hdFamily)
				familiesFound[hdFamily] = newAddr
			}
		}

		if prevFlag == nil {
			f := flags
			prevFlag = &f
		}
		if *prevFlag != flags {
			allFlagsSame = false
		}
		lastFlag = flags

		if blockNo == numBlocks-1 {
			fmt.Println("--- UF2 File Header Info ---")
			for _, famHex := range familiesOrder {
				name := familiesByID[famHex]
				fmt.Printf("Family ID is %s, hex value is 0x%08x\n", name, famHex)
				fmt.Printf("Target Address is 0x%08x\n", familiesFound[famHex])
			}
			if allFlagsSame {
				fmt.Printf("All block flag values consistent, 0x%04x\n", lastFlag)
			} else {
				fmt.Println("Flags were not all the same")
			}
			fmt.Println("----------------------------")
			if len(familiesFound) > 1 && familyID == 0 {
				outp = nil
				appStartAddr = 0
			}
		}
	}

	return outp, appStartAddr, nil
}

type hexBlock struct {
	addr uint32
	data [256]byte
}

func convertFromHexToUF2(buf []byte, familyID uint32) ([]byte, uint32, error) {
	var appStartAddr *uint32
	var upper uint32 = 0
	var currBlock *hexBlock
	var blocks []*hexBlock

	lines := strings.Split(string(buf), "\n")
	for _, rawLine := range lines {
		line := strings.TrimRight(rawLine, "\r\n ")
		if len(line) == 0 || line[0] != ':' {
			continue
		}

		rec := make([]byte, 0, (len(line)-1)/2)
		for i := 1; i+1 < len(line); i += 2 {
			val, err := strconv.ParseUint(line[i:i+2], 16, 8)
			if err != nil {
				return nil, 0, fmt.Errorf("invalid hex in line: %s", line)
			}
			rec = append(rec, byte(val))
		}

		if len(rec) < 5 {
			continue
		}

		tp := rec[3]
		switch tp {
		case 4: // Extended Linear Address
			upper = ((uint32(rec[4]) << 8) | uint32(rec[5])) << 16
		case 2: // Extended Segment Address
			upper = ((uint32(rec[4]) << 8) | uint32(rec[5])) << 4
		case 1: // EOF
			goto doneLines
		case 0: // Data
			addr := upper + ((uint32(rec[1]) << 8) | uint32(rec[2]))
			if appStartAddr == nil {
				a := addr
				appStartAddr = &a
			}
			dataBytes := rec[4 : len(rec)-1]
			for _, b := range dataBytes {
				baseAddr := addr & ^uint32(0xff)
				if currBlock == nil || currBlock.addr != baseAddr {
					currBlock = &hexBlock{addr: baseAddr}
					blocks = append(blocks, currBlock)
				}
				currBlock.data[addr&0xff] = b
				addr++
			}
		}
	}

doneLines:
	numBlocks := len(blocks)
	var flags uint32 = 0
	if familyID != 0 {
		flags |= 0x2000
	}

	out := make([]byte, 0, numBlocks*512)
	for i, blk := range blocks {
		var block [512]byte
		binary.LittleEndian.PutUint32(block[0:4], UF2MagicStart0)
		binary.LittleEndian.PutUint32(block[4:8], UF2MagicStart1)
		binary.LittleEndian.PutUint32(block[8:12], flags)
		binary.LittleEndian.PutUint32(block[12:16], blk.addr)
		binary.LittleEndian.PutUint32(block[16:20], 256)
		binary.LittleEndian.PutUint32(block[20:24], uint32(i))
		binary.LittleEndian.PutUint32(block[24:28], uint32(numBlocks))
		binary.LittleEndian.PutUint32(block[28:32], familyID)
		copy(block[32:288], blk.data[:])
		binary.LittleEndian.PutUint32(block[508:512], UF2MagicEnd)
		out = append(out, block[:]...)
	}

	var startAddr uint32 = 0
	if appStartAddr != nil {
		startAddr = *appStartAddr
	}
	return out, startAddr, nil
}

func convertToCArray(fileContent []byte) []byte {
	var sb strings.Builder
	sb.WriteString(fmt.Sprintf("const unsigned long bindata_len = %d;\n", len(fileContent)))
	sb.WriteString("const unsigned char bindata[] __attribute__((aligned(16))) = {")
	for i, b := range fileContent {
		if i%16 == 0 {
			sb.WriteByte('\n')
		}
		sb.WriteString(fmt.Sprintf("0x%02x, ", b))
	}
	sb.WriteString("\n};\n")
	return []byte(sb.String())
}

func writeFile(name string, buf []byte) error {
	err := os.WriteFile(name, buf, 0666)
	if err != nil {
		return err
	}
	fmt.Printf("Wrote %d bytes to %s\n", len(buf), name)
	return nil
}

func fatal(msg string) {
	fmt.Println(msg)
	os.Exit(1)
}

func main() {
	var (
		base   string
		output string
		family string
		carray bool
		info   bool
	)

	flag.StringVar(&base, "b", "0x2000", "set base address of application for BIN format")
	flag.StringVar(&output, "o", "", "write output to named file")
	flag.Bool("c", false, "convert (default)")
	flag.StringVar(&family, "f", "0x0", "specify familyID - number or name")
	flag.BoolVar(&carray, "C", false, "convert binary file to a C array, not UF2")
	flag.BoolVar(&info, "i", false, "display header information from UF2, do not convert")

	flag.Parse()

	if flag.NArg() == 0 {
		flag.Usage()
		fatal("Need input file")
	}
	inputFile := flag.Arg(0)

	familiesByName, familiesByID, sortedFamilyNames, err := loadFamilies()
	if err != nil {
		fatal(fmt.Sprintf("Failed to load UF2 families: %v", err))
	}

	baseVal, err := strconv.ParseUint(base, 0, 32)
	if err != nil {
		fatal(fmt.Sprintf("Invalid base address: %s", base))
	}
	appStartAddr := uint32(baseVal)

	var familyID uint32
	if id, ok := familiesByName[strings.ToUpper(family)]; ok {
		familyID = id
	} else {
		idVal, err := strconv.ParseUint(family, 0, 32)
		if err != nil {
			fatal("Family ID needs to be a number or one of: " + strings.Join(sortedFamilyNames, ", "))
		}
		familyID = uint32(idVal)
	}

	inpBuf, err := os.ReadFile(inputFile)
	if err != nil {
		fatal(fmt.Sprintf("Failed to read input file: %v", err))
	}

	fromUF2 := isUF2(inpBuf)
	ext := "uf2"
	var outBuf []byte

	if fromUF2 && !info {
		outBuf, appStartAddr, err = convertFromUF2(inpBuf, familyID, familiesByID)
		if err != nil {
			fatal(err.Error())
		}
		ext = "bin"
	} else if fromUF2 && info {
		_, _, err = convertFromUF2(inpBuf, familyID, familiesByID)
		if err != nil {
			fatal(err.Error())
		}
		return
	} else if isHex(inpBuf) {
		outBuf, appStartAddr, err = convertFromHexToUF2(inpBuf, familyID)
		if err != nil {
			fatal(err.Error())
		}
		ext = "uf2"
	} else if carray {
		outBuf = convertToCArray(inpBuf)
		ext = "h"
	} else {
		outBuf = convertToUF2(inpBuf, appStartAddr, familyID)
		ext = "uf2"
	}

	if !info {
		fmt.Printf("Converted to %s, output size: %d, start address: 0x%x\n", ext, len(outBuf), appStartAddr)
	}

	if output == "" {
		output = "flash." + ext
	}

	if err := writeFile(output, outBuf); err != nil {
		fatal(err.Error())
	}
}
