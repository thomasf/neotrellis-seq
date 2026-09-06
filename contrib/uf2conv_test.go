package main

import (
	"bytes"
	"strings"
	"testing"
)

func TestRoundtripBinUF2(t *testing.T) {
	byName, byID, _, err := loadFamilies()
	if err != nil {
		t.Fatalf("loadFamilies failed: %v", err)
	}

	familyID, ok := byName["SAMD51"]
	if !ok {
		t.Fatalf("SAMD51 not found in families")
	}

	// Create arbitrary 1000-byte test binary
	origBin := make([]byte, 1000)
	for i := range origBin {
		origBin[i] = byte(i * 3)
	}

	appStartAddr := uint32(0x4000)
	uf2Data := convertToUF2(origBin, appStartAddr, familyID)

	if len(uf2Data)%512 != 0 {
		t.Fatalf("UF2 size %d not multiple of 512", len(uf2Data))
	}
	expectedBlocks := (len(origBin) + 255) / 256
	if len(uf2Data) != expectedBlocks*512 {
		t.Fatalf("expected %d bytes, got %d", expectedBlocks*512, len(uf2Data))
	}

	if !isUF2(uf2Data) {
		t.Fatalf("isUF2 returned false on generated UF2")
	}

	recoveredBin, startAddr, err := convertFromUF2(uf2Data, familyID, byID)
	if err != nil {
		t.Fatalf("convertFromUF2 failed: %v", err)
	}

	if startAddr != appStartAddr {
		t.Errorf("expected start addr 0x%x, got 0x%x", appStartAddr, startAddr)
	}

	// UF2 chunks are 256 bytes, so recoveredBin length is padded to 256-byte boundary
	if len(recoveredBin) < len(origBin) {
		t.Fatalf("recovered bin length %d shorter than orig %d", len(recoveredBin), len(origBin))
	}

	if !bytes.Equal(origBin, recoveredBin[:len(origBin)]) {
		t.Errorf("recovered binary content does not match original binary")
	}
}

func TestHexToUF2(t *testing.T) {
	byName, byID, _, err := loadFamilies()
	if err != nil {
		t.Fatalf("loadFamilies failed: %v", err)
	}

	familyID := byName["SAMD51"]

	hexContent := ":100000000102030405060708090A0B0C0D0E0F1068\n:00000001FF\n"
	if !isHex([]byte(hexContent)) {
		t.Fatalf("isHex returned false on valid Intel HEX")
	}

	uf2Data, startAddr, err := convertFromHexToUF2([]byte(hexContent), familyID)
	if err != nil {
		t.Fatalf("convertFromHexToUF2 failed: %v", err)
	}

	if startAddr != 0 {
		t.Errorf("expected startAddr 0, got %d", startAddr)
	}

	recoveredBin, _, err := convertFromUF2(uf2Data, familyID, byID)
	if err != nil {
		t.Fatalf("convertFromUF2 failed: %v", err)
	}

	expectedPrefix := []byte{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}
	if !bytes.Equal(recoveredBin[:16], expectedPrefix) {
		t.Errorf("recovered data does not match expected prefix")
	}
}

func TestConvertToCArray(t *testing.T) {
	data := []byte{0x01, 0x02, 0x03, 0x04}
	carray := convertToCArray(data)
	carrayStr := string(carray)

	if !strings.Contains(carrayStr, "const unsigned long bindata_len = 4;") {
		t.Errorf("missing or incorrect bindata_len")
	}
	if !strings.Contains(carrayStr, "0x01, 0x02, 0x03, 0x04,") {
		t.Errorf("missing byte array contents: %s", carrayStr)
	}
}
