# Claude — Bytes Project Guide

## What is Bytes?

Bytes is a terminal-based multiplayer game platform written in C11. It uses ncurses for rendering and TCP sockets for networking. Players can host, join, or spectate games over a LAN. The first game is Pong; more can be added through the game registry.

## Build & Run

```bash
make          # build -> bin/bytes
make clean    # remove obj/ and bin/
./bin/bytes                # default port 7500
./bin/bytes --port 8080    # custom port
```

Dependencies: `gcc`, `ncursesw`, `pthreads`, `libm` (all POSIX/Linux standard).

## Architecture Overview

```
main.c        — entry point, menu loop, host/join/watch flows
game.c        — game registry, session lifecycle, server/client/spectator loops
pong.c        — Pong implementation (init, input, update, render, pack/unpack)
network.c     — TCP server/client, send/recv with length-prefix framing
protocol.c    — message pack/unpack (little-endian wire format)
ui.c          — ncurses menus, overlays, countdown, game-over screens
stats.c       — persistent win/loss tracking (~/.bytes_stats)
```

### Key abstractions

- **`game_def_t`** (include/game.h) — vtable for any game: init, handle_input, update, render, pack_state, unpack_state, is_over, get_winner. New games implement this interface and register in `game_registry[]` in game.c.
- **`game_session_t`** — runtime state for a match: who's playing, which game, server or client role.
- **Protocol** — 3-byte header (1 type + 2 LE payload length) followed by payload. Message types: HELLO, WELCOME, GAME_START, INPUT, STATE, GAME_OVER, PAUSE, RESUME, QUIT.
- **Network** — server uses non-blocking accept with poll(); clients connect blocking then use poll for recv. TCP_NODELAY enabled for low latency.

### Tick model

Server runs the game loop at 30 Hz (`TICK_RATE_HZ`). Each tick: read input, update state, render locally, broadcast serialized state to all clients. Clients send input messages and apply received state snapshots.

### Reconnect

If the remote player disconnects mid-game, the server pauses for `RECONNECT_TIMEOUT_SEC` (30s). A new client connecting as a player during this window resumes the match. Spectators can join at any time.

## Conventions

- C11 with `-Wall -Wextra -Werror -pedantic`. No warnings allowed.
- All structs use `__attribute__((packed))` for wire-format types only.
- Names: `snake_case` everywhere. Types end in `_t`. Constants are `UPPER_SNAKE`.
- Header guards: `BYTES_<MODULE>_H`.
- Memory: game state is heap-allocated via `calloc` in `game_session_init`, freed in `game_session_cleanup`. Everything else is stack or static.
- No dynamic allocation in hot paths (game loop).
- Unicode box-drawing characters for UI borders and game elements (requires locale + ncursesw).

## Adding a New Game

1. Create `include/<game>.h` and `src/<game>.c`.
2. Define a `<game>_state_t` struct and implement all `game_def_t` function pointers.
3. Export a `const game_def_t <game>_game_def`.
4. Add the enum value to `game_type_t` in common.h.
5. Register `&<game>_game_def` in `game_registry[]` in game.c.
6. The session, networking, and protocol layers handle the rest automatically.

## Common Pitfalls

- `net_recv` returns the full framed message (header + payload). Always unpack the header first, then use `payload_len` to read the payload starting at `buf + MSG_HEADER_SIZE`.
- `pack_state`/`unpack_state` work with the raw payload buffer, not the full message. The protocol layer wraps it in a MSG_STATE message.
- Terminal dimensions can change. The Pong unpack_state re-derives field dimensions from `getmaxyx` if `rows == 0`, but paddle positions are absolute — resizing mid-game will cause visual artifacts.
- `SIGPIPE` is ignored globally. Send failures return -1; handle them.
