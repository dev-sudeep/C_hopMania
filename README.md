# 🐔 Hop Mania (Crossy Chicken TUI)

> A vibrant, fast-paced terminal replica of **Hop Mania / Crossy Road** written in pure C99 with colorful ANSI escape sequences and flicker-free double-buffered rendering.

```text
 ╔═════════════════════════════════════════════════════════════════════════╗
 ║  🐔 HOP MANIA :: CROSSY CHICKEN TUI                  [ANSI C EDITION]   ║
 ╚═════════════════════════════════════════════════════════════════════════╝
```

---

## 🎮 Features

- **Procedural Infinite Terrain**:
  - **Lush Safe Grasslands**: Safe checkpoints with decorative flora, trees, and collectible golden seeds/coins (`$`).
  - **Highways (up to 5 Lanes Wide)**: Multi-lane roads with traffic flowing in alternating directions at varying speeds. Features sports cars, sedans, city buses, and heavy semi-trucks.
  - **Rushing Rivers & Floating Logs**: Rivers up to 4 lanes wide where the chicken must hop across floating logs. Standing on a log drifts the player along with the water current!
  - **Railroad Crossings**: High-speed train tracks with flashing red warning signals (`[!TRAIN!]`) before a superfast express train zooms across.
- **Classic Crossy Mechanics**:
  - **Forward & Backward Hopping**: Use `w` to hop forward and `s` to hop backward.
  - **Eagle Stagnation Danger**: Idle in one spot for too long and a hungry eagle swoops down from above!
  - **Drifting Physics**: Riding logs carries the player dynamically; falling into the water or drifting off-screen ends the run.
- **Rich Visuals & ANSI Double Buffering**:
  - 24-bit TrueColor and high-visibility ANSI escape sequences.
  - Double-buffered in-memory rendering grid with atomic single `write()` updates for **zero-flicker 30 FPS gameplay**.
  - Dynamic particle effects: feather explosions on vehicle collision, water splashes on drowning, and golden sparkles on coin pickup.
  - Live side dashboard tracking distance, record score, coins, current zone, and an eagle countdown gauge.
- **Persistent High Scores**: High scores are automatically saved to `~/.c_hop_highscore`.

---

## 🕹️ Controls

| Key | Action | Description |
| :---: | :---: | :--- |
| **`w`** / `↑` | **Hop Forward** | Advance forward (+1 distance score) |
| **`s`** / `↓` | **Hop Backward** | Step backward |
| **`a`** / `←` | **Move Left** | Lateral hop to the left |
| **`d`** / `→` | **Move Right** | Lateral hop to the right |
| **`p`** | **Pause / Resume** | Toggle pause modal |
| **`r`** | **Restart** | Reset run immediately |
| **`q`** / `ESC` | **Quit** | Cleanly exit to shell |

---

## 🚀 Quick Start & Building

### Prerequisites
- GCC or Clang
- POSIX-compliant terminal (Linux, macOS, WSL)
- Standard terminal size of at least **80x24** columns

### Build & Run
```bash
# Clone repository
git clone https://github.com/dev-sudeep/C_hopMania.git
cd C_hopMania

# Build executable
make

# Run the game
make run
# or directly:
./bin/hopmania
```

### Running Test Suite
```bash
make test
```
The automated test suite verifies:
- Multi-lane road generation (validating up to 5 lanes)
- River and log floating physics & drifting mechanics
- Drowning vs. log riding collision checks
- Vehicle hitboxes and road splat detection
- Idle stagnation and eagle attack trigger
- Headless off-screen rendering integrity

---

## 📁 Architecture & File Structure

```text
C_hop/
├── include/
│   ├── entities.h   # Entity models (Player, Vehicles, Logs, Train, Particles)
│   ├── game.h       # Master game state, procedural world generation, collision logic
│   ├── render.h     # ANSI escape sequence macros, screen buffer layout
│   └── terminal.h   # POSIX termios raw mode, non-blocking input, signals
├── src/
│   ├── game.c       # Simulation loop, physics, row spawning, highscore I/O
│   ├── main.c       # Game entrypoint, delta timing, FPS regulator (~30 FPS)
│   ├── render.c     # Double-buffered ANSI renderer, sprites, UI dashboard
│   └── terminal.c   # Terminal raw mode setup, key sequence decoder
├── tests/
│   ├── test_game.c  # Unit & physics simulation test suite
│   └── test_render.c# Headless rendering & buffer stress test
├── Makefile         # Build automation (all, clean, test, run, install)
├── README.md        # Documentation and guide
└── .gitignore       # Git exclusion rules
```

---

## 📜 License

Distributed under the MIT License.
