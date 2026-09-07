package main

import (
	"bytes"
	"flag"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
)

type SubGenerator struct {
	Name string
	Desc string
	Run  func(rootDir string) error
}

var subGenerators = []SubGenerator{
	{
		Name: "midimap",
		Desc: "Ableton Drum Rack / General MIDI mappings (src/midimap.h)",
		Run:  generateMidiMaps,
	},
	{
		Name: "patterns",
		Desc: "Pattern presets & 6-voice kits from patterns.txt (src/PatternPresets.h)",
		Run:  generatePatterns,
	},
	{
		Name: "palette",
		Desc: "NeoPixel color palette & theme generation (src/colors.h)",
		Run:  generatePalette,
	},
	{
		Name: "manual",
		Desc: "User manual HTML documentation (MANUAL.html) via embedded Go templates",
		Run:  generateManual,
	},
}

// runClangFormat formats a C/C++ source or header file using clang-format.
func runClangFormat(filePath string) error {
	cmd := exec.Command("clang-format", "-i", filePath)
	if out, err := cmd.CombinedOutput(); err != nil {
		return fmt.Errorf("running clang-format on %s: %w\n%s", filePath, err, strings.TrimSpace(string(out)))
	}
	return nil
}

// writeGeneratedCHeader writes a generated C header file and always runs clang-format on it right after.
func writeGeneratedCHeader(filePath string, content []byte) error {
	oldData, err := os.ReadFile(filePath)
	hasOld := err == nil

	if err := os.WriteFile(filePath, content, 0644); err != nil {
		return fmt.Errorf("writing %s: %w", filePath, err)
	}

	if err := runClangFormat(filePath); err != nil {
		return err
	}

	newData, err := os.ReadFile(filePath)
	if err != nil {
		return fmt.Errorf("reading %s: %w", filePath, err)
	}

	if hasOld && bytes.Equal(oldData, newData) {
		fmt.Printf("%s is already up to date.\n", filePath)
	} else {
		fmt.Printf("Updated %s\n", filePath)
	}

	return nil
}

func findRootDir() (string, error) {
	dir, err := os.Getwd()
	if err != nil {
		return "", err
	}
	for {
		if _, err := os.Stat(filepath.Join(dir, "src", "config.h")); err == nil {
			return dir, nil
		}
		parent := filepath.Dir(dir)
		if parent == dir {
			break
		}
		dir = parent
	}
	return "", fmt.Errorf("could not locate repository root containing src/config.h")
}

func main() {
	rootDirFlag := flag.String("root", "", "Path to repository root (auto-detected if omitted)")
	flag.Usage = func() {
		fmt.Fprintf(flag.CommandLine.Output(), "Usage: %s [flags] [subgenerator]\n\n", os.Args[0])
		fmt.Fprintf(flag.CommandLine.Output(), "Available subgenerators:\n")
		fmt.Fprintf(flag.CommandLine.Output(), "  all (default)  Run all generators\n")
		for _, g := range subGenerators {
			fmt.Fprintf(flag.CommandLine.Output(), "  %-14s %s\n", g.Name, g.Desc)
		}
		fmt.Fprintf(flag.CommandLine.Output(), "\nFlags:\n")
		flag.PrintDefaults()
	}
	flag.Parse()

	rootDir := *rootDirFlag
	if rootDir == "" {
		detected, err := findRootDir()
		if err != nil {
			fmt.Fprintf(os.Stderr, "Error: %v\n", err)
			os.Exit(1)
		}
		rootDir = detected
	}

	target := "all"
	if flag.NArg() > 0 {
		target = strings.ToLower(flag.Arg(0))
	}

	var toRun []SubGenerator
	if target == "all" || target == "" {
		toRun = subGenerators
	} else {
		for _, g := range subGenerators {
			if strings.EqualFold(g.Name, target) {
				toRun = append(toRun, g)
				break
			}
		}
		if len(toRun) == 0 {
			fmt.Fprintf(os.Stderr, "Error: unknown subgenerator %q\n", target)
			flag.Usage()
			os.Exit(1)
		}
	}

	fmt.Printf("Neotrellis Sequence Generator (root: %s)\n", rootDir)
	fmt.Printf("Running %d generator(s)...\n\n", len(toRun))

	type result struct {
		name string
		err  error
	}
	var results []result

	for _, g := range toRun {
		fmt.Printf("=== [%s] %s ===\n", g.Name, g.Desc)
		err := g.Run(rootDir)
		results = append(results, result{name: g.Name, err: err})
		if err != nil {
			fmt.Fprintf(os.Stderr, "Error in %s: %v\n", g.Name, err)
		}
		fmt.Println()
	}

	// Report summary at the end
	fmt.Println("==================================================")
	fmt.Println("Generator Summary:")
	var failures []result
	for _, r := range results {
		if r.err == nil {
			fmt.Printf("  [PASS] %-12s (completed successfully)\n", r.name)
		} else {
			fmt.Printf("  [FAIL] %-12s: %v\n", r.name, r.err)
			failures = append(failures, r)
		}
	}
	fmt.Println("==================================================")

	if len(failures) > 0 {
		fmt.Fprintf(os.Stderr, "\nFinished with %d failure(s).\n", len(failures))
		os.Exit(1)
	}

	fmt.Println("\nAll generators completed successfully.")
}
