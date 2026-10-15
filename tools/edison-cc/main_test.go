package main

import (
	"os"
	"reflect"
	"runtime"
	"testing"
)

func TestSubstituteNull(t *testing.T) {
	null := os.DevNull
	if runtime.GOOS == "windows" {
		null = "nul"
	}
	got, ok := substituteNull([]string{"c++", "-E", "a.cpp", "-o", null, "-MMD"}, "TMP")
	if want := []string{"c++", "-E", "a.cpp", "-o", "TMP", "-MMD"}; !ok || !reflect.DeepEqual(got, want) {
		t.Errorf("null output: got %v %v, want %v", got, ok, want)
	}
	in := []string{"c++", "-E", "a.cpp", "-o", "build/sketch_merged.cpp"}
	if got, ok := substituteNull(in, "TMP"); ok || !reflect.DeepEqual(got, in) {
		t.Errorf("real output changed: %v %v", got, ok)
	}
}
