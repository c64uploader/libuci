package libuci_test

import (
	"context"
	"fmt"
	"net/http"
	"os"
	"os/exec"
	"strings"
	"testing"
	"time"

	"github.com/c64uploader/go-ultimate"
)

const (
	addrSignal = 0xD7FF // test writes 1 on completion
	addrInput  = 0xC008 // host->C64 input (8 bytes for echo server info)
)

func newClient() (*ultimate.Client, error) {
	addr := getDeviceAddress()
	var opts []ultimate.Option
	if pw := os.Getenv("C64U_PASSWORD"); pw != "" {
		opts = append(opts, ultimate.WithPassword(pw))
	}
	return ultimate.New(addr, opts...)
}

func setupCtx(t *testing.T) context.Context {
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	t.Cleanup(cancel)
	return ctx
}

func getDeviceAddress() string {
	if addr := os.Getenv("C64U_ADDRESS"); addr != "" {
		return addr
	}
	return "c64u"
}

// BuildAll runs 'make' in the repo root to compile all C tests into .prg files.
func BuildAll() error {
	args := []string{"-j", "-C", "../..", "build-tests"}
	if cc := os.Getenv("CC"); cc != "" {
		args = append(args, "CC="+cc)
	}
	cmd := exec.Command("make", args...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	return cmd.Run()
}

// RunTest uploads a PRG, sends optional input, waits for completion, and returns the result.
func RunTest(ctx context.Context, t *testing.T, client *ultimate.Client, prgPath string, inputData []byte) (passed bool, screen string, err error) {
	t.Helper()

	// Reboot C64 and wait for READY
	t.Log("rebooting C64...")
	if err := client.Machine.Reset(ctx); err != nil {
		return false, "", fmt.Errorf("reset machine: %w", err)
	}

	t.Log("waiting for READY...")
	time.Sleep(2 * time.Second)
	t.Log("C64 ready")

	prg, err := os.ReadFile(prgPath)
	if err != nil {
		return false, "", fmt.Errorf("read PRG: %w", err)
	}

	// Clear signal and write host input
	if err := client.Machine.Poke(ctx, addrSignal, 0x00); err != nil {
		return false, "", fmt.Errorf("clear signal: %w", err)
	}
	if len(inputData) > 0 {
		if err := client.Machine.WriteMemory(ctx, addrInput, inputData); err != nil {
			return false, "", fmt.Errorf("write input: %w", err)
		}
	}

	t.Logf("running %s", prgPath)
	if err := client.Runners.RunPRGBytes(ctx, prg); err != nil {
		return false, "", fmt.Errorf("run PRG: %w", err)
	}

	// Wait for the test to signal completion at $D7FF.
	if _, err := waitForSignal(ctx, t, client); err != nil {
		return false, "", err
	}

	// Read screen and check for PASS
	scr, err := client.Debug.Screen(ctx)
	if err != nil {
		return false, "", fmt.Errorf("read screen: %w", err)
	}
	screen = strings.Join(scr.Rows, "\n")
	passed = strings.Contains(screen, "PASS")
	return passed, screen, nil
}

func waitForSignal(ctx context.Context, t *testing.T, client *ultimate.Client) (byte, error) {
	t.Helper()
	for {
		data, err := client.Machine.ReadMemory(ctx, addrSignal, 1)
		if err != nil {
			return 0, fmt.Errorf("read signal: %w", err)
		}
		if len(data) > 0 && data[0] != 0 {
			t.Logf("signal: 0x%02X", data[0])
			return data[0], nil
		}
		select {
		case <-ctx.Done():
			return 0, fmt.Errorf("signal timeout: %w", ctx.Err())
		case <-time.After(500 * time.Millisecond):
		}
	}
}

// helpers

func deleteDeviceFile(path string) error {
	path = strings.TrimPrefix(path, "/")
	u := fmt.Sprintf("http://%s/v1/files/%s", getDeviceAddress(), path)
	req, _ := http.NewRequest(http.MethodDelete, u, nil)
	if pw := os.Getenv("C64U_PASSWORD"); pw != "" {
		req.Header.Set("X-Password", pw)
	}
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return err
	}
	resp.Body.Close()
	return nil
}

func parseIP(s string) []byte {
	var p [4]int
	if n, _ := fmt.Sscanf(s, "%d.%d.%d.%d", &p[0], &p[1], &p[2], &p[3]); n != 4 {
		return nil
	}
	for i := 0; i < 4; i++ {
		if p[i] < 0 || p[i] > 255 {
			return nil
		}
	}
	return []byte{byte(p[0]), byte(p[1]), byte(p[2]), byte(p[3])}
}
