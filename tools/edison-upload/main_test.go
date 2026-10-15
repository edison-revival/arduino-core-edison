package main

import (
	"crypto/ed25519"
	"crypto/rand"
	"net"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"golang.org/x/crypto/ssh"
)

func TestTarget(t *testing.T) {
	cases := []struct {
		port, protocol, want string
		usb                  bool
	}{
		{"/dev/ttyACM0", "serial", "192.168.2.15:22", true},
		{"COM5", "serial", "192.168.2.15:22", true},
		{"192.168.1.40", "network", "192.168.1.40:22", false},
		{"192.168.1.40:2222", "network", "192.168.1.40:2222", false},
		{"fe80::1", "network", "[fe80::1]:22", false},
		{"[fe80::1]:22", "network", "[fe80::1]:22", false},
	}
	for _, c := range cases {
		got, usb, err := target(c.port, c.protocol)
		if err != nil || got != c.want || usb != c.usb {
			t.Errorf("target(%q, %q) = %q, %v, %v; want %q, %v", c.port, c.protocol,
				got, usb, err, c.want, c.usb)
		}
	}
	if _, _, err := target("x", "dfu"); err == nil {
		t.Error("unsupported protocol accepted")
	}
}

func TestLoadKeyIsStable(t *testing.T) {
	dir := t.TempDir()
	a, err := loadKey(dir)
	if err != nil {
		t.Fatal(err)
	}
	b, err := loadKey(dir)
	if err != nil {
		t.Fatal(err)
	}
	if string(a.PublicKey().Marshal()) != string(b.PublicKey().Marshal()) {
		t.Error("key changed between runs")
	}
}

func newHostKey(t *testing.T) ssh.PublicKey {
	pub, _, err := ed25519.GenerateKey(rand.Reader)
	if err != nil {
		t.Fatal(err)
	}
	k, err := ssh.NewPublicKey(pub)
	if err != nil {
		t.Fatal(err)
	}
	return k
}

func TestHostKeyTrustOnFirstUse(t *testing.T) {
	dir := t.TempDir()
	remote := &net.TCPAddr{IP: net.ParseIP("192.168.1.40"), Port: 22}
	k1, k2 := newHostKey(t), newHostKey(t)
	check := func(k ssh.PublicKey) error {
		cb, err := hostKeyCallback(dir, false)
		if err != nil {
			t.Fatal(err)
		}
		return cb("192.168.1.40:22", remote, k)
	}
	if err := check(k1); err != nil {
		t.Fatalf("first use: %v", err)
	}
	if err := check(k1); err != nil {
		t.Fatalf("same key: %v", err)
	}
	if err := check(k2); err == nil || !strings.Contains(err.Error(), "-forget") {
		t.Fatalf("changed key: got %v", err)
	}
	if err := forgetHost(filepath.Join(dir, "known_hosts"), "192.168.1.40"); err != nil {
		t.Fatal(err)
	}
	if err := check(k2); err != nil {
		t.Fatalf("after forget: %v", err)
	}
}

func TestHostKeyRememberedOnceAcrossDials(t *testing.T) {
	dir := t.TempDir()
	cb, err := hostKeyCallback(dir, false)
	if err != nil {
		t.Fatal(err)
	}
	remote := &net.TCPAddr{IP: net.ParseIP("192.168.1.40"), Port: 22}
	k := newHostKey(t)
	for i := 0; i < 2; i++ { // connect dials with the key, then with the password
		if err := cb("192.168.1.40:22", remote, k); err != nil {
			t.Fatal(err)
		}
	}
	data, err := os.ReadFile(filepath.Join(dir, "known_hosts"))
	if err != nil {
		t.Fatal(err)
	}
	if n := strings.Count(string(data), "\n"); n != 1 {
		t.Errorf("known_hosts has %d lines, want 1:\n%s", n, data)
	}
}

func TestUSBAcceptsAnyHostKey(t *testing.T) {
	dir := t.TempDir()
	cb, err := hostKeyCallback(dir, true)
	if err != nil {
		t.Fatal(err)
	}
	remote := &net.TCPAddr{IP: net.ParseIP(usbAddress), Port: 22}
	for i := 0; i < 2; i++ {
		if err := cb(usbAddress+":22", remote, newHostKey(t)); err != nil {
			t.Fatal(err)
		}
	}
	if _, err := os.Stat(filepath.Join(dir, "known_hosts")); err == nil {
		t.Error("USB link keys shouldn't be remembered")
	}
}
