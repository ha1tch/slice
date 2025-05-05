package main

import (
	"os"
	"os/exec"
	"strings"
	"testing"
)

const testFileName = "test.txt"
const binaryName = "slice"

func TestMain(m *testing.M) {
	// Build the CLI binary
	if err := exec.Command("go", "build", "-o", binaryName).Run(); err != nil {
		panic("Failed to build slice binary: " + err.Error())
	}

	code := m.Run()

	// Cleanup
	_ = os.Remove(binaryName)
	_ = os.Remove(testFileName)

	os.Exit(code)
}

func writeTestFile(t *testing.T, content string) {
	if err := os.WriteFile(testFileName, []byte(content), 0644); err != nil {
		t.Fatalf("Failed to write test file: %v", err)
	}
}

func runSlice(t *testing.T, args ...string) (string, error) {
	cmd := exec.Command("./"+binaryName, args...)
	output, err := cmd.CombinedOutput()
	return string(output), err
}

func TestBasicSlice(t *testing.T) {
	writeTestFile(t, "Line 1\nLine 2\nLine 3\n")
	output, err := runSlice(t, "--start", "7", "--size", "7", "--file", testFileName)
	if err != nil {
		t.Fatalf("Command failed: %v", err)
	}
	expected := "Line 2\n"
	if output != expected {
		t.Errorf("Expected %q, got %q", expected, output)
	}
}

func TestStartAtZero(t *testing.T) {
	writeTestFile(t, "1234567890")
	output, err := runSlice(t, "--start", "0", "--size", "5", "--file", testFileName)
	if err != nil {
		t.Fatalf("Command failed: %v", err)
	}
	expected := "12345"
	if output != expected {
		t.Errorf("Expected %q, got %q", expected, output)
	}
}

func TestSliceAtEOF(t *testing.T) {
	writeTestFile(t, "1234567890")
	output, err := runSlice(t, "--start", "8", "--size", "4", "--file", testFileName)
	if err != nil {
		t.Fatalf("Command failed: %v", err)
	}
	expected := "90"
	if strings.TrimSpace(output) != expected {
		t.Errorf("Expected %q, got %q", expected, output)
	}
}

func TestOversizedSlice(t *testing.T) {
	writeTestFile(t, "1234567890")
	output, err := runSlice(t, "--start", "0", "--size", "100", "--file", testFileName)
	if err != nil {
		t.Fatalf("Command failed: %v", err)
	}
	expected := "1234567890"
	if strings.TrimSpace(output) != expected {
		t.Errorf("Expected %q, got %q", expected, output)
	}
}

func TestInvalidFile(t *testing.T) {
	output, err := runSlice(t, "--start", "0", "--size", "10", "--file", "nonexistent.txt")
	if err == nil {
		t.Fatalf("Expected error for nonexistent file")
	}
	if !strings.Contains(output, "file does not exist") {
		t.Errorf("Unexpected error message: %q", output)
	}
}

func TestMissingArguments(t *testing.T) {
	output, err := runSlice(t, "--start", "0", "--size", "10")
	if err == nil {
		t.Fatalf("Expected error for missing arguments")
	}
	if !strings.Contains(output, "Error:") {
		t.Errorf("Unexpected error message: %q", output)
	}
}
