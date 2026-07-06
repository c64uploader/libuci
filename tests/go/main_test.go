package libuci_test

import (
	"context"
	"fmt"
	"os"
	"path/filepath"
	"testing"

	"github.com/c64uploader/go-ultimate"
)

var (
	testClient *ultimate.Client
	prgDir     string
)

func TestMain(m *testing.M) {
	// Build C tests
	fmt.Println("building C tests...")
	if err := BuildAll(); err != nil {
		fmt.Fprintf(os.Stderr, "build failed: %v\n", err)
		os.Exit(1)
	}

	// Create client
	var err error
	testClient, err = newClient()
	if err != nil {
		fmt.Fprintf(os.Stderr, "client: %v\n", err)
		os.Exit(1)
	}

	// Connectivity check
	fmt.Printf("connecting to %s...\n", getDeviceAddress())
	v, err := testClient.Version(context.Background())
	if err != nil {
		fmt.Fprintf(os.Stderr, "cannot reach device: %v\n", err)
		os.Exit(1)
	}
	fmt.Printf("connected (API %s)\n", v.Version)

	// Find PRG dir
	prgDir = "../build"
	if _, err := os.Stat(prgDir); os.IsNotExist(err) {
		dir, _ := os.Getwd()
		for range 5 {
			d := filepath.Join(dir, "tests", "build")
			if _, err := os.Stat(d); err == nil {
				prgDir = d
				break
			}
			dir = filepath.Dir(dir)
		}
	}

	// Setup test data
	fmt.Println("setting up test data...")
	ctx := context.Background()
	_ = testClient.Drives.Unmount(ctx, ultimate.DriveA)
	_ = deleteDeviceFile("/usb0/ucitest.d64")
	if err := testClient.Files.CreateD64(ctx, "/usb0/ucitest.d64",
		ultimate.CreateOptions{DiskName: "UCITEST", Tracks: 35}); err != nil {
		fmt.Fprintf(os.Stderr, "warning: cannot create ucitest.d64: %v\n", err)
	}

	// Run tests
	code := m.Run()

	// Cleanup
	_ = deleteDeviceFile("/usb0/ucitest.d64")
	os.Exit(code)
}
