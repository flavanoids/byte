# Bytes

Terminal-based multiplayer games over LAN. Written in C.

![C11](https://img.shields.io/badge/C11-standard-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey)

## About

Bytes is a multiplayer game platform that runs entirely in the terminal. Players can host, join, or spectate games over a local network using nothing but a compiled binary and a terminal emulator.

The first game is **Pong**. The architecture supports adding more.

## Quick Start

```bash
make
./bin/bytes
```

One player hosts, the other joins with the host's IP. That's it.

## Building

### Linux

```bash
sudo apt install build-essential libncursesw5-dev    # Debian/Ubuntu
make
```

### macOS

```bash
xcode-select --install    # if needed
brew install ncurses      # if needed
make
```

### Windows (cross-compile from Linux)

```bash
sudo apt install gcc-mingw-w64-x86-64
git clone --depth 1 https://github.com/wmcbrine/PDCurses.git deps/PDCurses
cd deps/PDCurses/wincon && make CC=x86_64-w64-mingw32-gcc AR=x86_64-w64-mingw32-ar WIDE=Y && cd ../../..
make windows
```

Produces `bin/bytes.exe` -- a statically linked Windows binary with no runtime dependencies.

### Windows (native)

Install [MSYS2](https://www.msys2.org/), then from a UCRT64 shell:

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-pdcurses make
make
```

## Usage

```
./bin/bytes                 # default port 7500
./bin/bytes --port 8080     # custom port
./bin/bytes --solo          # single player vs CPU
./bin/bytes --test-keys     # input diagnostics
```

**Host** a game, **join** by IP, or **spectate** an ongoing match. Navigate menus with arrow keys, confirm with Enter.

## Features

- LAN multiplayer over TCP
- Host, join, or spectate
- Solo play vs CPU
- 30 Hz server-authoritative game loop
- Automatic pause & reconnect on disconnect (30s window)
- Persistent local win/loss stats (`~/.bytes_stats`)
- Cross-platform: Linux, macOS, Windows
- Unicode box-drawing UI

## Architecture

```
src/
├── main.c       Entry point, menu loop, host/join/watch flows
├── game.c       Game registry, session lifecycle
├── pong.c       Pong implementation
├── network.c    TCP server/client with length-prefix framing
├── protocol.c   Message pack/unpack (little-endian wire format)
├── ui.c         ncurses menus, overlays, screens
├── stats.c      Persistent win/loss tracking
└── platform.c   OS abstraction (sockets, time, paths)
```

Games implement a simple vtable (`game_def_t`) -- init, input, update, render, serialize, deserialize, win check. The engine handles networking, protocol, and session management.

## Adding a Game

1. Create `include/<game>.h` and `src/<game>.c`
2. Implement the `game_def_t` function pointers
3. Add an enum value to `game_type_t` in `common.h`
4. Register in `game_registry[]` in `game.c`

No changes needed to networking or protocol code.

## License

MIT
