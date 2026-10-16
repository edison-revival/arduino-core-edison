package main

import (
	"net"
	"os/exec"
	"runtime"
	"strings"
)

// dialAddr returns the address to connect to for addr (host:port).
//
// edison-upload is built without cgo, so on Linux Go resolves names itself and asks only
// the DNS server; it doesn't do mDNS. A .local name that the system resolves through
// Avahi (nss-mdns), as ping and ssh do, then fails. For those, ask the system resolver
// through getent. Windows and macOS already go through the system's resolver.
func dialAddr(addr string) string {
	host, port, err := net.SplitHostPort(addr)
	if err != nil || runtime.GOOS != "linux" ||
		!strings.HasSuffix(strings.ToLower(strings.TrimSuffix(host, ".")), ".local") {
		return addr
	}
	if _, err := net.LookupHost(host); err == nil {
		return addr
	}
	out, err := exec.Command("getent", "ahostsv4", host).Output()
	if err != nil {
		return addr
	}
	if ip := firstAddress(string(out)); ip != "" {
		debugf("Resolved %s to %s through the system resolver", host, ip)
		return net.JoinHostPort(ip, port)
	}
	return addr
}

// firstAddress returns the first IP address in getent's output ("ADDRESS TYPE [NAME]"
// lines), or "".
func firstAddress(out string) string {
	for _, line := range strings.Split(out, "\n") {
		if f := strings.Fields(line); len(f) > 0 && net.ParseIP(f[0]) != nil {
			return f[0]
		}
	}
	return ""
}
