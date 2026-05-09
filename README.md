# EREBUS

**"It doesn't hunt. It studies."**

A 2.5D raycasted survival horror roguelite where the monster is a **real neural network** trained on your behavior. Built in C with Raylib. Runs on anything.

---

## The Concept

You are trapped in a collapsed research facility with **EREBUS** — a neural network given flesh. It begins blind, stupid, walking into walls. But every time you hide in a locker, sprint around a corner, or throw a decoy, its weights update. By run 5, it checks your hiding spots. By run 15, it predicts your inputs before you press them.

**It is a literal neural network — and you are its training data.**

## Features

- 🧠 **Real Neural Network** — A genuine MLP (~800 parameters) that trains on your behavioral patterns between runs
- 👁️ **2.5D Raycasted Engine** — DOOM-style rendering with procedural textures, distance fog, and sector lighting
- 🎭 **The Monster Evolves** — From blind wanderer (run 1) to psychological predator (run 15+)
- 🔬 **Menace Gauge** — Dynamic tension system the AI learns to exploit
- 🧪 **Synaptic Surgery** — Manipulate the monster's neural architecture between runs
- 🎵 **Procedural Audio** — Every sound generated at runtime, no external files
- 💻 **Runs on Anything** — Targets 60fps on Intel HD 520 integrated graphics

## Building

The game builds automatically via GitHub Actions on every push. Download the latest build from the [Actions tab](../../actions) or [Releases](../../releases).

### Building Locally (requires CMake + MinGW/GCC)

```bash
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/erebus
```

## Controls

| Key | Action |
|-----|--------|
| WASD | Move |
| Mouse | Look |
| Shift | Sprint (loud) |
| Ctrl | Crouch (quiet) |
| Space | Freeze (silent) |
| F | Toggle flashlight |
| Tab | Toggle minimap |
| F3 | Debug info |
| Esc | Pause |

## Tech Stack

| Component | Technology |
|-----------|-----------|
| Language | C (C99) |
| Framework | Raylib 5.5 |
| Renderer | Custom DDA raycaster |
| Neural Net | Hand-written MLP in C |
| Build | CMake + GitHub Actions |
| Textures | 100% procedural (Perlin noise) |

## Development Roadmap

- [x] **Sprint 1** — Raycasting engine, procedural textures, player movement
- [ ] **Sprint 2** — Neural network + monster AI
- [ ] **Sprint 3** — Game loop (objectives, items, death cards)
- [ ] **Sprint 4** — Learning brain (between-run training, weight persistence)
- [ ] **Sprint 5** — Panic Room (between-run hub, synaptic surgery)
- [ ] **Sprint 6** — Advanced AI (Menace Gauge, overfitting boss, deception)
- [ ] **Sprint 7** — Procedural audio engine
- [ ] **Sprint 8** — Visual polish (CRT shaders, sprite morphing)
- [ ] **Sprint 9** — Narrative & endings
- [ ] **Sprint 10** — Release

## Credits

Designed by FiveSaw & Gamini Vijayasingha.

## License

MIT
