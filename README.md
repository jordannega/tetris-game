# Tetris (C++ / raylib)

A classic Tetris clone written in C++ using [raylib](https://www.raylib.com/).
Clean, data-driven piece logic with modern quality-of-life features.

## Features

- 🎮 Classic 10×20 board
- 🔄 **SRS wall kicks** — rotate pieces near walls and other blocks
- 👻 **Ghost piece** — see where your piece will land
- 📦 **Hold** — stash a piece for later
- 🎲 **7-bag randomizer** — fair, no piece droughts
- ⚡ **Levels** — speed increases as your score climbs
- 💯 Scoring for single / double / triple / tetris, plus hard-drop bonus
- ⏸️ Pause and restart

## Controls

| Key       | Action         |
|-----------|----------------|
| ← / →     | Move left/right|
| ↑         | Rotate         |
| Space     | Hard drop      |
| C         | Hold           |
| P         | Pause          |
| R         | Restart (on game over) |

## Build

Requires a C++ compiler and raylib installed.

### Linux / macOS
`bash
g++ main.cpp -o tetris -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
./tetris

### Windows (MinGW)
`bash
g++ main.cpp -o tetris.exe -lraylib -lopengl32 -lgdi32 -lwinmm
tetris.exe
CMake (optional)
cmake
cmake_minimum_required(VERSION 3.14)
project(tetris)
set(CMAKE_CXX_STANDARD 17)
find_package(raylib REQUIRED)
add_executable(tetris main.cpp)
target_link_libraries(tetris raylib)

How it works
Piece shapes are stored in a single lookup table PIECES[block][orientation][cell],
so collision, rotation, locking, and drawing all share the same data — no
hand-written switch statements per piece.

