# Agents — Bytes Execution Model

## Server Agent

The server (host) is the single source of truth. It runs the authoritative game loop:

1. **Accept connections** — non-blocking accept via `poll()`. Players go through a HELLO/WELCOME handshake; spectators receive WELCOME + GAME_START so they can join mid-match.
2. **Read remote input** — poll the player's socket each tick. Deserialize MSG_INPUT and feed it into `def->handle_input(state, player_id, key)`.
3. **Read local input** — `getch()` with timeout. Feed into `handle_input` for player 1.
4. **Tick** — call `def->update(state)` at 30 Hz. Physics, collision, scoring all happen here.
5. **Broadcast** — serialize state via `def->pack_state`, wrap in MSG_STATE, send to all connected clients (players + spectators).
6. **Detect game over** — if `def->is_over(state)`, send MSG_GAME_OVER to all and break.

### Pause/Reconnect sub-agent

When the remote player disconnects:
- Server enters paused state, sends MSG_PAUSE to remaining clients.
- Polls for new connections for up to 30 seconds.
- If a player reconnects (sends HELLO with ROLE_PLAYER), resumes and broadcasts MSG_RESUME.
- If timeout expires, declares the present player the winner via MSG_GAME_OVER.

## Client Agent

The client is a thin input sender + state renderer:

1. **Send input** — on each `getch()`, pack MSG_INPUT and send to server.
2. **Receive state** — poll for MSG_STATE, unpack, render. Also handles MSG_GAME_OVER, MSG_PAUSE, MSG_RESUME, MSG_QUIT.
3. **No local simulation** — the client does not run `update()`. It applies server state snapshots directly.

## Spectator Agent

Identical to the client agent but:
- Sends HELLO with `ROLE_SPECTATOR` instead of `ROLE_PLAYER`.
- Never sends MSG_INPUT.
- Renders with `is_spectator = true` (shows "[SPECTATING]" label).
- Can only quit locally; has no gameplay effect.

## Protocol Agent (Wire Format)

All messages follow: `[type:u8][payload_len:u16le][payload:...]`

| Type | ID | Direction | Purpose |
|------|-----|-----------|---------|
| HELLO | 1 | C->S | Client announces name + role |
| WELCOME | 2 | S->C | Server confirms, assigns player ID |
| GAME_START | 3 | S->C | Announces game type + player names |
| INPUT | 4 | C->S | Keyboard input from client |
| STATE | 5 | S->C | Full game state snapshot |
| GAME_OVER | 6 | S->C | Winner announcement |
| PAUSE | 7 | S->C | Game paused (player disconnect) |
| RESUME | 8 | S->C | Game resumed (player reconnected) |
| QUIT | 9 | Either | Graceful disconnect |

## Stats Agent

Runs locally after each game session. Tracks per-game played/won/lost counts in `~/.bytes_stats` (simple `key=value` flat file). Loaded on startup, saved after each recorded game.

## Game Registry

The `game_def_t` vtable acts as a pluggable agent contract. Each game implementation provides its own init/input/update/render/serialize/deserialize/win-check functions. The engine calls them polymorphically — adding a game requires zero changes to the networking, protocol, or session management code.
