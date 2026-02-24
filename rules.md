# Rules — Bytes Coding Standards

## Language & Compiler

- **C11** (`-std=c11`). No GNU extensions beyond `__attribute__((packed))` for wire structs.
- Compiled with `-Wall -Wextra -Werror -pedantic`. Every warning is an error. Fix them, don't suppress them.
- Feature test macros: `_XOPEN_SOURCE=700`, `_DEFAULT_SOURCE`.

## Naming

- All identifiers use `snake_case`.
- Types end with `_t`: `pong_state_t`, `net_server_t`, `msg_header_t`.
- Constants and macros use `UPPER_SNAKE_CASE`: `MAX_NAME_LEN`, `TICK_RATE_HZ`.
- Enum members: `UPPER_SNAKE_CASE` with a category prefix: `MSG_HELLO`, `ROLE_PLAYER`, `COLOR_P1`.
- Static (file-local) functions have no prefix. Public functions use their module prefix: `net_`, `proto_`, `ui_`, `game_`, `stats_`, `pong_`.

## File Organization

- One module = one `.c` + one `.h` pair.
- Headers use include guards: `#ifndef BYTES_<MODULE>_H`.
- Headers declare the public API only. Implementation details stay in the `.c`.
- `common.h` holds shared constants, enums, and typedefs used across modules.

## Memory

- Game state is heap-allocated once per session (`calloc` in `game_session_init`), freed once (`free` in `game_session_cleanup`).
- No `malloc`/`calloc` in the game loop. All per-tick buffers are stack-allocated.
- Wire buffers are fixed-size stack arrays: `uint8_t buf[MSG_HEADER_SIZE + MAX_MSG_PAYLOAD]`.

## Networking

- TCP with `SO_REUSEADDR` and `TCP_NODELAY`.
- Server socket is non-blocking. Client sockets are blocking with `poll()` timeouts.
- All sends loop until complete (handle `EINTR`, `EAGAIN`).
- `SIGPIPE` is ignored; send failures return -1.
- Messages are length-prefixed: 3-byte header (type + 16-bit LE payload length).
- `net_recv` reads the full message atomically (header then payload).

## Protocol

- All multi-byte fields are little-endian on the wire.
- Packed structs (`__attribute__((packed))`) are used only for documentation/sizing of message layouts — actual pack/unpack is done with explicit byte manipulation.
- Name fields are fixed `MAX_NAME_LEN` (32) bytes, null-terminated, zero-padded.

## UI / ncurses

- Initialize with `setlocale(LC_ALL, "")` for Unicode support.
- Use `ncursesw` (wide-character ncurses) for Unicode box-drawing and symbols.
- Color pairs are defined in `common.h` and initialized in `ui_init`.
- Always restore terminal state: `curs_set`, `echo`/`noecho`, `nodelay`, `keypad` are toggled carefully around input prompts.
- `ESCDELAY = 25` for responsive Escape key handling.

## Game Implementation Contract

Every game must provide a `game_def_t` with all function pointers populated:

| Function | Signature | Rule |
|----------|-----------|------|
| `init` | `(void *state, int rows, int cols)` | Zero-initialize state, set up for given terminal size |
| `handle_input` | `(void *state, int player_id, int key)` | Pure state mutation, no I/O |
| `update` | `(void *state)` | Advance one tick, no I/O |
| `render` | `(void *state, ...)` | ncurses output only, no state mutation |
| `pack_state` | `(const void *state, uint8_t *buf, size_t buflen)` | Serialize to wire format, return byte count |
| `unpack_state` | `(void *state, const uint8_t *buf, size_t len)` | Deserialize from wire format |
| `is_over` | `(const void *state)` | Pure query, no side effects |
| `get_winner` | `(const void *state)` | Returns player ID (1 or 2), 0 if no winner |

## Error Handling

- Network and I/O functions return -1 on error, 0 on timeout/no-data, >0 on success.
- Errors are reported to the user via `ui_show_message`, never via stderr (stdout is owned by ncurses).
- No `assert` in production paths. Validate inputs and fail gracefully.

## Stats

- Stored in `~/.bytes_stats` as `key=value` lines.
- Keys follow the pattern `<game>_played`, `<game>_won`, `<game>_lost`.
- File is loaded at startup and saved after each game.
