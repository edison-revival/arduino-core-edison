// edison-upload uploads an Arduino sketch to an Intel Edison running edison-image, over SSH.
//
// The Arduino IDE runs it with the port the user picked: the board's USB serial port
// (the upload then goes over the USB network link on the same cable) or its network
// address (found by mDNS). The first upload logs in with the board's password and installs
// this computer's key; later uploads use the key.
package main

import (
	"errors"
	"flag"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"
	"time"
)

// The board's address on the USB network link (edison-image's gadget DHCP server).
const usbAddress = "192.168.2.15"

const sketchDir = "/var/lib/edison-sketch"

var verbose bool

func logf(format string, args ...any) { fmt.Printf(format+"\n", args...) }

func debugf(format string, args ...any) {
	if verbose {
		logf(format, args...)
	}
}

func main() {
	port := flag.String("port", "", "the port picked in the IDE: a serial port or an address")
	protocol := flag.String("protocol", "serial", "the port's protocol: serial or network")
	user := flag.String("user", "edison", "user to log in as")
	password := flag.String("password", "", "the user's password (only needed the first time)")
	forget := flag.String("forget", "", "forget the remembered host key for this address")
	flag.BoolVar(&verbose, "v", false, "verbose output")
	flag.Usage = func() {
		fmt.Fprintf(flag.CommandLine.Output(),
			"usage: edison-upload [options] SKETCH.elf\n       edison-upload -forget ADDRESS\n\n")
		flag.PrintDefaults()
	}
	flag.Parse()

	dir, err := configDir()
	if err == nil && *forget != "" {
		err = forgetHost(filepath.Join(dir, "known_hosts"), *forget)
		if err == nil {
			logf("Forgot the host key for %s.", *forget)
		}
	} else if err == nil {
		if flag.NArg() != 1 {
			flag.Usage()
			os.Exit(2)
		}
		err = upload(dir, *port, *protocol, *user, *password, flag.Arg(0))
	}
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error: %v\n", err)
		os.Exit(1)
	}
}

// configDir is where the uploader keeps its key and the boards' host keys.
func configDir() (string, error) {
	base, err := os.UserConfigDir()
	if err != nil {
		return "", err
	}
	dir := filepath.Join(base, "edison-upload")
	return dir, os.MkdirAll(dir, 0o700)
}

// target turns the IDE's port into an SSH address, and says whether it's the USB link.
func target(port, protocol string) (addr string, usb bool, err error) {
	switch protocol {
	case "serial", "":
		return usbAddress + ":22", true, nil
	case "network":
		if port == "" {
			return "", false, errors.New("no network address given")
		}
		if strings.Contains(port, ":") && !strings.HasPrefix(port, "[") &&
			strings.Count(port, ":") > 1 {
			return "[" + port + "]:22", false, nil // bare IPv6 address
		}
		if !strings.Contains(port, ":") {
			port += ":22"
		}
		return port, false, nil
	default:
		return "", false, fmt.Errorf("unsupported port protocol %q", protocol)
	}
}

func upload(dir, port, protocol, user, password, path string) error {
	f, err := os.Open(path)
	if err != nil {
		return err
	}
	defer f.Close()
	info, err := f.Stat()
	if err != nil {
		return err
	}

	addr, usb, err := target(port, protocol)
	if err != nil {
		return err
	}
	via := "over the network"
	if usb {
		via = "over the USB cable"
	}
	logf("Uploading %s (%d KiB) to %s %s", filepath.Base(path), (info.Size()+1023)/1024, addr, via)

	c, err := connect(dir, addr, usb, user, password)
	if err != nil {
		return err
	}
	defer c.Close()

	before, _ := c.output("systemctl show -P ExecMainStartTimestampMonotonic edison-sketch")
	start := time.Now()
	tmp := sketchDir + "/.sketch.elf.tmp"
	// The rename makes edison-image restart the sketch (edison-sketch.path).
	cmd := fmt.Sprintf("cat > %[1]s && chmod 755 %[1]s && mv %[1]s %[2]s/sketch.elf", tmp, sketchDir)
	if err := c.run(cmd, io.Reader(f)); err != nil {
		return fmt.Errorf("copying the sketch: %w", err)
	}
	logf("Copied in %.1f s; starting the sketch...", time.Since(start).Seconds())
	return waitForSketch(c, strings.TrimSpace(before))
}

// waitForSketch waits for the new sketch to be running, and shows its log if it fails.
func waitForSketch(c *client, before string) error {
	deadline := time.Now().Add(10 * time.Second)
	for time.Now().Before(deadline) {
		time.Sleep(300 * time.Millisecond)
		out, err := c.output("systemctl show -p ExecMainStartTimestampMonotonic,ActiveState,Result edison-sketch")
		if err != nil {
			return err
		}
		p := parseProperties(out)
		if started := p["ExecMainStartTimestampMonotonic"]; started == before || started == "0" {
			continue // not restarted yet
		}
		switch {
		case p["ActiveState"] == "active":
			logf("Sketch running.")
			return nil
		case p["Result"] != "success" || p["ActiveState"] == "failed":
			log, _ := c.output("journalctl -u edison-sketch -n 15 -o cat --no-pager")
			return fmt.Errorf("the sketch failed to start (%s):\n%s", p["Result"], log)
		}
	}
	return errors.New("the sketch didn't start within 10 s; see journalctl -u edison-sketch")
}

// parseProperties parses systemctl show's KEY=value lines.
func parseProperties(out string) map[string]string {
	p := map[string]string{}
	for _, line := range strings.Split(out, "\n") {
		if k, v, ok := strings.Cut(strings.TrimSpace(line), "="); ok {
			p[k] = v
		}
	}
	return p
}
