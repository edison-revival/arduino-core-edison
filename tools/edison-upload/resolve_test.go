package main

import "testing"

func TestFirstAddress(t *testing.T) {
	for _, tc := range []struct{ out, want string }{
		{"192.168.2.15    STREAM edison.local\n192.168.2.15    DGRAM  \n", "192.168.2.15"},
		{"", ""},
		{"garbage line\n10.0.0.7 RAW\n", "10.0.0.7"},
	} {
		if got := firstAddress(tc.out); got != tc.want {
			t.Errorf("firstAddress(%q) = %q, want %q", tc.out, got, tc.want)
		}
	}
}

func TestDialAddrLeavesOtherAddressesAlone(t *testing.T) {
	for _, addr := range []string{"192.168.2.15:22", "[fe80::1%eth0]:22", "example.com:22"} {
		if got := dialAddr(addr); got != addr {
			t.Errorf("dialAddr(%q) = %q, want it unchanged", addr, got)
		}
	}
}
