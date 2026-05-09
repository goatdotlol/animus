# EREBUS

**"It doesn't hunt. It studies."**

A 2.5D raycasted survival horror roguelite where the adversary is a real neural network trained on your behavior. Built in C99 with Raylib. Runs on anything.

---

## Overview

You are trapped in a collapsed research facility with EREBUS -- a neural network given form. It begins blind, walking into walls. But every time you hide, sprint, or throw a decoy, its weights update. By run 5, it checks your hiding spots. By run 15, it predicts your inputs before you press them.

The monster is a literal multi-layer perceptron. You are its training data.

## Features

**Engine**
- Custom DDA raycasting engine (DOOM-style 2.5D)
- 10 procedural textures generated via Perlin noise at startup
- Sector-based lighting, distance fog, floor/ceiling casting
- CRT post-processing (scanlines, vignette, film grain)
- Billboard sprite rendering with depth sorting

**Neural Network Adversary**
- Genuine MLP architecture (19 inputs, 32/16 hidden, 8 outputs)
- Xavier-initialized weights with sub-millisecond inference
- Behavioral tracking feeds real player data into the network
- Weight persistence between runs -- the monster remembers
- A* pathfinding with line-of-sight and hearing senses

**Gameplay**
- 13-minute timed runs with memory core objectives
- 4 movement modes: sprint (loud), walk, crouch (quiet), freeze (silent)
- Flashlight with limited battery
- Death card testimony system with full behavioral statistics
- Player profile persistence across sessions
- Items: batteries, decoys, adrenaline, keycards

**Audio**
- Procedural sound synthesis (no external audio files)
- Spatial audio with distance attenuation
- Ambient drone generation
- Dynamic footstep synthesis

## Building

The game builds automatically via GitHub Actions on every push. Download the latest build from the [Actions tab](../../actions) or [Releases](../../releases).

### Local Build (requires CMake + GCC or MSVC)

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Controls

| Key | Action |
|-----|--------|
| WASD | Move |
| Mouse | Look |
| Shift | Sprint |
| Ctrl | Crouch |
| Space | Freeze |
| F | Toggle flashlight |
| E | Use item |
| Tab | Toggle minimap |
| F3 | Debug overlay |
| Esc | Pause / Options |

## Technical Details

| Component | Implementation |
|-----------|---------------|
| Language | C (C99) |
| Graphics | Raylib 5.5 |
| Renderer | Custom DDA raycaster (384x216 internal) |
| AI | Hand-written MLP with backpropagation |
| Pathfinding | A* on sector grid |
| Textures | Procedural (Perlin noise) |
| Audio | Procedural synthesis |
| Build System | CMake + GitHub Actions |

## Development Status

- [x] Sprint 1 -- Raycasting engine, procedural textures, player movement
- [x] Sprint 2 -- Neural network, monster AI, pathfinding
- [x] Sprint 3 -- Game loop, objectives, death cards, persistence
- [x] Sprint 4 -- Learning brain, backpropagation, weight mutation
- [ ] Sprint 5 -- Panic Room hub, synaptic surgery
- [ ] Sprint 6 -- Advanced AI (Menace Gauge, overfitting boss)
- [ ] Sprint 7 -- Procedural audio engine
- [ ] Sprint 8 -- Visual polish (GPU shaders, sprite morphing)
- [ ] Sprint 9 -- Narrative, endings, Architect Mode
- [ ] Sprint 10 -- Release optimization

---

Built by FiveSaw.
