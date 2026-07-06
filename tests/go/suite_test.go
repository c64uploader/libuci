package libuci_test

import (
	"context"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"

	"github.com/c64uploader/go-ultimate"
)

// runC64Test runs one compiled C test PRG on the hardware and reports the
// result. It is the single entry point used by every test function below.
//
// Protocol: the C test prints "PASS" or "FAIL <reason>" to the screen and
// writes a completion signal to $D7FF. The Go side reboots, runs the PRG,
// waits for the signal, then reads the screen. PASS is detected by the
// presence of "PASS" on screen.
//
// On failure the full screen is always logged (so the C test's FAIL reason
// is visible), and any line containing "FAIL" is highlighted separately.
func runC64Test(t *testing.T, prgName string, setup func(t *testing.T) (input []byte, cleanup func())) {
	t.Helper()
	ctx := setupCtx(t)

	var input []byte
	var cleanup func()
	if setup != nil {
		input, cleanup = setup(t)
	}
	if cleanup != nil {
		defer cleanup()
	}

	prgPath := filepath.Join(prgDir, prgName)
	if _, err := os.Stat(prgPath); os.IsNotExist(err) {
		t.Fatalf("PRG not found: %s (run 'make' in tests/)", prgPath)
	}

	passed, screen, err := RunTest(ctx, t, testClient, prgPath, input)
	if err != nil {
		t.Fatalf("execution error: %v", err)
	}
	if !passed {
		// Surface the C test's own FAIL line(s) first, then dump the screen.
		for _, line := range strings.Split(screen, "\n") {
			if l := strings.TrimSpace(line); strings.Contains(l, "FAIL") {
				t.Logf("C test reported: %s", l)
			}
		}
		t.Logf("screen:\n%s", screen)
		t.Fatalf("C test %s did not print PASS", prgName)
	}
}

// Core

func TestCore(t *testing.T) {
	t.Run("Init", func(t *testing.T) { runC64Test(t, "test_core_init.prg", nil) })
	t.Run("GetBase", func(t *testing.T) { runC64Test(t, "test_core_get_base.prg", nil) })
	t.Run("Reset", func(t *testing.T) { runC64Test(t, "test_core_reset.prg", nil) })
	t.Run("ErrorState", func(t *testing.T) { runC64Test(t, "test_core_error_state.prg", nil) })
	t.Run("Status", func(t *testing.T) { runC64Test(t, "test_core_status.prg", nil) })
}

// DOS

func TestDOS(t *testing.T) {
	t.Run("Identify", func(t *testing.T) { runC64Test(t, "test_dos_identify.prg", nil) })
	t.Run("OpenReadClose", func(t *testing.T) { runC64Test(t, "test_dos_open_read_close.prg", nil) })
	t.Run("Write", func(t *testing.T) { runC64Test(t, "test_dos_write.prg", nil) })
	t.Run("Seek", func(t *testing.T) { runC64Test(t, "test_dos_seek.prg", nil) })
	t.Run("Delete", func(t *testing.T) { runC64Test(t, "test_dos_delete.prg", nil) })
	t.Run("Rename", func(t *testing.T) { runC64Test(t, "test_dos_rename.prg", nil) })
	t.Run("Copy", func(t *testing.T) { runC64Test(t, "test_dos_copy.prg", nil) })
	t.Run("ChangeDir", func(t *testing.T) { runC64Test(t, "test_dos_change_dir.prg", nil) })
	t.Run("GetPath", func(t *testing.T) { runC64Test(t, "test_dos_get_path.prg", nil) })
	t.Run("ListDir", func(t *testing.T) { runC64Test(t, "test_dos_list_dir.prg", nil) })
	t.Run("CreateDir", func(t *testing.T) { runC64Test(t, "test_dos_create_dir.prg", nil) })
	t.Run("GetTime", func(t *testing.T) { runC64Test(t, "test_dos_get_time.prg", nil) })
	t.Run("SetTime", func(t *testing.T) { runC64Test(t, "test_dos_set_time.prg", nil) })
	t.Run("MountUnmount", func(t *testing.T) { runC64Test(t, "test_dos_mount_unmount.prg", setupSiecDisk) })
	t.Run("FileInfo", func(t *testing.T) { runC64Test(t, "test_dos_file_info.prg", nil) })
	t.Run("FileStat", func(t *testing.T) { runC64Test(t, "test_dos_file_stat.prg", nil) })
	t.Run("SwapDisk", func(t *testing.T) { runC64Test(t, "test_dos_swap_disk.prg", nil) })
	t.Run("CopyHomePath", func(t *testing.T) { runC64Test(t, "test_dos_copy_home_path.prg", nil) })
}

// Network

func TestNet(t *testing.T) {
	t.Run("Identify", func(t *testing.T) { runC64Test(t, "test_net_identify.prg", nil) })
	t.Run("GetMac", func(t *testing.T) { runC64Test(t, "test_net_get_mac.prg", nil) })
	t.Run("GetIp", func(t *testing.T) { runC64Test(t, "test_net_get_ip.prg", nil) })
	t.Run("IfCount", func(t *testing.T) { runC64Test(t, "test_net_if_count.prg", nil) })
	t.Run("TcpEcho", func(t *testing.T) {
		runC64Test(t, "test_net_tcp_echo.prg", func(t *testing.T) ([]byte, func()) {
			server, err := NewTCPServer(ModeEcho, 0)
			if err != nil {
				t.Fatal(err)
			}
			localIP, err := GetLocalIPForDevice()
			if err != nil {
				server.Stop()
				t.Skipf("cannot determine local IP: %v", err)
			}
			t.Logf("TCP echo server at %s:%d", localIP, server.Port())
			return prepareEchoServerInfo(localIP, server.Port(), 0), server.Stop
		})
	})
	t.Run("TcpEchoLarge", func(t *testing.T) {
		runC64Test(t, "test_net_tcp_echo_large.prg", func(t *testing.T) ([]byte, func()) {
			// Server writes 600 bytes (byte i = i&0xFF) then closes.
			server, err := NewTCPServer(ModeSendPayload, 600)
			if err != nil {
				t.Fatal(err)
			}
			localIP, err := GetLocalIPForDevice()
			if err != nil {
				server.Stop()
				t.Skipf("cannot determine local IP: %v", err)
			}
			t.Logf("TCP payload server at %s:%d (600 bytes)", localIP, server.Port())
			return prepareEchoServerInfo(localIP, server.Port(), 0), server.Stop
		})
	})
	t.Run("HttpLarge", func(t *testing.T) {
		runC64Test(t, "test_net_http_large.prg", func(t *testing.T) ([]byte, func()) {
			// Server reads the HTTP request, sends a 3000-byte response, closes.
			server, err := NewTCPServer(ModeHTTPResponder, 3000)
			if err != nil {
				t.Fatal(err)
			}
			localIP, err := GetLocalIPForDevice()
			if err != nil {
				server.Stop()
				t.Skipf("cannot determine local IP: %v", err)
			}
			t.Logf("HTTP responder at %s:%d (3000-byte body)", localIP, server.Port())
			return prepareEchoServerInfo(localIP, server.Port(), 0), server.Stop
		})
	})
	t.Run("UdpEcho", func(t *testing.T) {
		runC64Test(t, "test_net_udp_echo.prg", func(t *testing.T) ([]byte, func()) {
			server, err := NewUDPEchoServer()
			if err != nil {
				t.Fatal(err)
			}
			localIP, err := GetLocalIPForDevice()
			if err != nil {
				server.Stop()
				t.Skipf("cannot determine local IP: %v", err)
			}
			t.Logf("UDP echo server at %s:%d", localIP, server.Port())
			return prepareEchoServerInfo(localIP, 0, server.Port()), server.Stop
		})
	})
}

// Control

func TestCtrl(t *testing.T) {
	t.Run("Identify", func(t *testing.T) { runC64Test(t, "test_ctrl_identify.prg", nil) })
	t.Run("DiskPower", func(t *testing.T) { runC64Test(t, "test_ctrl_disk_power.prg", nil) })
	t.Run("EnableDisable", func(t *testing.T) { runC64Test(t, "test_ctrl_enable_disable.prg", nil) })
	t.Run("HwInfo", func(t *testing.T) { runC64Test(t, "test_ctrl_hwinfo.prg", nil) })
	t.Run("DrvInfo", func(t *testing.T) { runC64Test(t, "test_ctrl_drvinfo.prg", nil) })
}

// SoftIEC

func TestSoftIEC(t *testing.T) {
	t.Run("Identify", func(t *testing.T) { runC64Test(t, "test_siec_identify.prg", nil) })
	t.Run("OpenClose", func(t *testing.T) { runC64Test(t, "test_siec_open_close.prg", setupSiecDisk) })
	t.Run("Load", func(t *testing.T) { runC64Test(t, "test_siec_load.prg", setupSiecDisk) })
	t.Run("Save", func(t *testing.T) { runC64Test(t, "test_siec_save.prg", setupSiecDisk) })
	t.Run("ChkinChrin", func(t *testing.T) { runC64Test(t, "test_siec_chkin_chrin.prg", setupSiecDisk) })
	t.Run("ChkoutChrout", func(t *testing.T) { runC64Test(t, "test_siec_chkout_chrout.prg", setupSiecDisk) })
}

// setupSiecDisk recreates the ucitest.d64 image used by the SoftIEC tests.
func setupSiecDisk(t *testing.T) ([]byte, func()) {
	ctx := context.Background()
	_ = testClient.Drives.Unmount(ctx, ultimate.DriveA)
	_ = deleteDeviceFile("/temp/ucitest.d64")
	if err := testClient.Files.CreateD64(ctx, "/temp/ucitest.d64",
		ultimate.CreateOptions{DiskName: "UCITEST", Tracks: 35}); err != nil {
		t.Logf("warning: recreate ucitest.d64: %v", err)
	}
	time.Sleep(100 * time.Millisecond)
	return nil, nil
}
