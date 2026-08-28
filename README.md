# Snake

Simple terminal Snake game. Uses `ncurses` on Linux/macOS and PDCurses (wincon backend) on Windows.

![screenshot](https://github.com/user-attachments/assets/10eded69-6200-4022-a1b0-1159d821ec16)

## Play

Prebuilt binaries for Linux, Windows, and macOS (universal) are on the [Releases page](../../releases). Each binary is signed with a GitHub-issued build-provenance attestation; verify with:

```
gh attestation verify <downloaded-file> --repo <owner>/<repo>
```

### Linux

```
chmod +x snake-linux-x86_64
./snake-linux-x86_64
```

Needs `libncursesw6` (preinstalled on essentially every modern desktop distro).

### Windows

Double-click `snake-windows-x86_64.exe`, or run it from a terminal. No runtime dependencies. SmartScreen may warn about an unrecognized publisher on first run - click **More info → Run anyway**.

### macOS

The binary is unsigned, so Gatekeeper quarantines it on first download. Clear it once:

```
chmod +x snake-macos-universal
xattr -d com.apple.quarantine snake-macos-universal
./snake-macos-universal
```

## Build from source

Requires CMake ≥ 3.16 and Ninja.

```
# Linux
sudo apt install libncurses-dev ninja-build
cmake --preset linux && cmake --build --preset linux

# macOS (uses Homebrew ncurses if present, falls back to system)
brew install ninja ncurses
cmake --preset mac && cmake --build --preset mac

# Windows cross-compile from Linux (MinGW-w64)
sudo apt install mingw-w64 ninja-build
cmake --preset windows-mingw && cmake --build --preset windows-mingw
```

Output binary lands in `build/snake` (or `build-windows/snake.exe`). Native Windows builds via MSYS2 mingw64 also work - install `mingw-w64-x86_64-cmake` and `mingw-w64-x86_64-ninja` and use the `linux` preset (compiler is native, no toolchain file needed).

## Controls

Configurable at the pre-game options screen:

- `W`/`A`/`S`/`D` or arrow keys to move
- `P` to pause
- `Q` to quit

Options cycle with `1`/`2`/`3` and start with `Enter`. Options reopen between rounds so speed, wraparound, and control scheme can be tuned mid-session.

## History

This is one of my first C projects, kept around for sentiment. The recent commits are a polish pass: CMake build, cross-platform CI, and a handful of fixes to long-standing bugs in growth, item spawning, and the render loop.
