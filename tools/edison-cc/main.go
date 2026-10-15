// edison-cc runs a compiler command, standing in for an output file that is the null
// device. arduino-cli preprocesses with "-o nul" (Windows) or "-o /dev/null" when only the
// side effects are wanted (finding a sketch's libraries), but zig writes its output via a
// temporary file that it then copies to the -o path, which on Windows creates a real,
// undeletable-from-Explorer file named "nul" in the current directory.
//
//	edison-cc COMPILER ARGS...
package main

import (
	"errors"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
)

func isNullDevice(path string) bool {
	if runtime.GOOS == "windows" {
		return strings.EqualFold(path, "nul") || strings.EqualFold(path, `\\.\nul`)
	}
	return path == os.DevNull
}

// substituteNull replaces a null-device output path with tmp. It returns the new arguments
// and whether a substitution was made.
func substituteNull(args []string, tmp string) ([]string, bool) {
	out := append([]string(nil), args...)
	for i := 0; i+1 < len(out); i++ {
		if out[i] == "-o" && isNullDevice(out[i+1]) {
			out[i+1] = tmp
			return out, true
		}
	}
	return out, false
}

func run(args []string) (int, error) {
	if len(args) < 1 {
		return 2, errors.New("usage: edison-cc COMPILER ARGS")
	}
	dir, err := os.MkdirTemp("", "edison-cc-")
	if err != nil {
		return 1, err
	}
	defer os.RemoveAll(dir)
	cmdArgs, _ := substituteNull(args[1:], filepath.Join(dir, "out"))

	cmd := exec.Command(args[0], cmdArgs...)
	cmd.Stdin, cmd.Stdout, cmd.Stderr = os.Stdin, os.Stdout, os.Stderr
	err = cmd.Run()
	var exit *exec.ExitError
	if errors.As(err, &exit) {
		return exit.ExitCode(), nil
	}
	if err != nil {
		return 1, err
	}
	return 0, nil
}

func main() {
	code, err := run(os.Args[1:])
	if err != nil {
		fmt.Fprintf(os.Stderr, "edison-cc: %v\n", err)
	}
	os.Exit(code)
}
