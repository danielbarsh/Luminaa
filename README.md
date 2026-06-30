# Vector Siege

A small Asteroids-style arcade game in C++20 / SFML 3, built less as "a game"
and more as a sandbox for the kind of low-level work I'm interested in:
object pooling instead of per-frame heap churn, a small persistent thread
pool for parallel collision checks, and a runtime profiler overlay (`F1`)
that times each subsystem and graphs frame time live.

## Why these design choices

- **No allocations during gameplay.** `ObjectPool<T, Capacity>` is a fixed
  flat array with a free-index stack. Asteroids and bullets are
  acquired/released from these pools instead of `new`/`delete`, so frame
  time stays predictable even with a few hundred entities on screen.
- **Parallel collision pass.** `ThreadPool::parallelFor` splits the
  bullet-vs-asteroid broad phase across worker threads. Each chunk collects
  its own local hit list and only takes a lock once, when merging — not per
  pair — to avoid turning a perf win into a perf loss from contention.
- **Built-in profiler.** `Profiler` records per-category millisecond samples
  into ring buffers and draws a live frame-time graph plus per-subsystem
  averages/max, similar in spirit to the kind of instrumentation you'd want
  around an emulator or SoC functional model. Press `F1` in-game to toggle it.

## Screenshots

![Shooting State](ScreenShots/shooting.PNG) 
![Idle State](ScreenShots/idle.PNG)


## Controls

| Key | Action |
|---|---|
| Left/Right or A/D | Rotate ship |
| Up or W | Thrust |
| Space | Fire |
| F1 | Toggle profiler overlay |
| R | Restart after game over |
| Esc | Quit |

## Building

Requires CMake 3.16+ and a C++20 compiler. SFML 3 is pulled automatically
via `FetchContent` — no manual install needed.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/VectorSiege
```

On Linux you'll need SFML's runtime dependencies if they're not already on
your system (X11, FreeType, etc.):

```bash
sudo apt install libxrandr-dev libxcursor-dev libxi-dev libudev-dev \
    libfreetype-dev libflac-dev libvorbis-dev libgl1-mesa-dev libegl1-mesa-dev
```

Drop a `.ttf` file into `assets/font.ttf` for the HUD/profiler text (see
`assets/README.txt`) — the game runs without it, just without on-screen text.

## Layout

```
include/   ObjectPool, ThreadPool, Profiler, Entities, Game (headers)
src/       Game.cpp (simulation + render), main.cpp
assets/    font.ttf goes here
```
