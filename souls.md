# Souls — The Spirit of Bytes

## Core Identity

Bytes is a love letter to the terminal. It proves that meaningful multiplayer gaming doesn't need a GPU, a game engine, or an electron wrapper — just a socket, a terminal, and a shared moment between two people on the same network.

## Design Philosophy

### Simplicity over sophistication

Every architectural decision favors the simplest thing that works. A vtable of function pointers instead of a plugin system. A flat key=value file instead of SQLite. Fixed-size buffers instead of dynamic allocation. TCP instead of UDP. The codebase should be readable end-to-end in a single sitting.

### The server is the truth

There is exactly one authority: the host. The server runs the simulation, the client renders what it's told. No client-side prediction, no rollback, no reconciliation. This limits Bytes to LAN play where latency is negligible, and that's fine — LAN play is the point.

### Games are just data

A game is a struct of function pointers. The engine doesn't know what Pong is. It knows how to init, tick, render, serialize, and check for winners. This means adding a new game is purely additive: write your logic, fill in the vtable, drop it in the registry. Nothing else changes.

### The terminal is the canvas

ncurses isn't a limitation, it's the medium. Unicode box-drawing characters, color pairs, bold and dim attributes — these are the palette. The constraint breeds creativity: a ball trail rendered as a dim previous-position glyph, a center line made of dashed vertical bars, paddles as solid blocks.

### Resilience is respect

If a player disconnects, the game doesn't die. It pauses, waits, and lets them come back. The reconnect window is generous (30 seconds) because real life interrupts — a SSH session drops, a laptop lid closes, a cat walks across the keyboard. The game should survive what the network doesn't.

## Emotional Goals

- **Nostalgia** — Bytes should feel like discovering a game on a BBS or a university lab machine. The ASCII title art, the menu navigation with arrow keys, the terminal bell on a score — these are deliberate callbacks.
- **Immediacy** — No accounts, no loaders, no configuration. Run the binary, type your name, play. The friction between "I want to play" and "I'm playing" should be as close to zero as a compiled C binary allows.
- **Intimacy** — This is a two-player game on a local network. You're in the same room, or at least the same building. The spectator mode exists because someone walking by should be able to watch without disrupting the match.

## What Bytes Is Not

- **Not a competitive platform.** Stats exist for personal amusement, not leaderboards.
- **Not a framework.** The game_def_t interface is internal. There's no public API, no dynamic loading, no scripting.
- **Not cross-platform.** It targets POSIX/Linux with ncursesw. It might work on macOS. It will not work on Windows without WSL. That's acceptable.
- **Not finished.** The registry holds one game. The architecture exists to hold many. The next game should be as easy to add as Pong was — if it isn't, the architecture has failed.

## Guiding Tensions

| Tension | Resolution |
|---------|------------|
| Features vs. simplicity | Add a game, not a feature. The engine should stay small. |
| Performance vs. clarity | At 30 Hz with two players, clarity wins every time. |
| Robustness vs. complexity | Handle disconnects; don't handle Byzantine faults. Trust the LAN. |
| Beauty vs. minimalism | Use Unicode and color. Skip animation frameworks. |

## The Test

If someone can clone this repo, run `make && ./bin/bytes`, and be playing Pong with a friend within 60 seconds — Bytes is doing its job.
