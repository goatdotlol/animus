# ANIMUS — "It doesn't hunt. It studies."

## The Unified Vision

A **2.5D raycasted survival horror roguelite** where the monster is a **real neural network** trained on your behavior. Built in **C + Raylib**, compiled via **GitHub Actions**, distributed as a free Windows `.exe`. Every idea from ANIMUS, EPOCH NULL, MIRRORHUNT, Subject Zero, and Gamini's 2.5D design conversation — unified into one game.

> The monster is a mathematical model of your cowardice. Every run is a conversation. You teach it, it teaches you.

---

## Tech Stack & Build Pipeline

| Layer | Choice | Why |
|---|---|---|
| **Language** | C (C99) | Maximum performance, Claude writes excellent C, runs on anything |
| **Framework** | Raylib 5.x | Lightweight, built-in 2D/3D rendering + audio + shaders, zero dependencies |
| **Renderer** | Custom DDA raycaster | DOOM-style 2.5D with textured walls, floor/ceiling casting, billboard sprites |
| **Neural Net** | Hand-written MLP in C | ~800 params, 4KB memory, sub-millisecond inference, no external ML libs |
| **Build** | CMake + GitHub Actions | FetchContent pulls Raylib automatically; Actions compiles on push |
| **Audio** | Raylib audio + custom synth | Procedural SFX/ambience via wavetable synthesis, spatial audio via portal tracing |
| **Textures** | Procedural (Perlin noise) | Walls/floors generated at startup — zero external art needed for core gameplay |
| **Distribution** | GitHub Releases | Auto-built `.exe` + `data/` folder as `.zip` on every tagged push |

### Local Dev vs GitHub

- **Locally (your machine):** We write all `.c`, `.h`, `CMakeLists.txt`, shader, and map files as plain text
- **GitHub Actions:** Compiles everything with gcc/MinGW, links Raylib, produces the `.exe`
- **You download** the release artifact and play-test on your EliteBook
- **Iterate** — fix, push, build, test

---

## Project Structure

```
ANIMUS/
├── .github/workflows/build.yml    # CI/CD: compile + release
├── CMakeLists.txt                 # Build config (FetchContent for Raylib)
├── src/
│   ├── main.c                     # Entry point + game state machine
│   ├── engine/
│   │   ├── raycaster.c/h          # DDA wall casting + floor/ceiling
│   │   ├── sprites.c/h            # Billboard sprite renderer
│   │   ├── lighting.c/h           # Sector-based dynamic lighting
│   │   ├── postfx.c/h             # CRT scanlines, barrel distortion, glitch
│   │   └── texgen.c/h             # Procedural texture generation (Perlin)
│   ├── world/
│   │   ├── map.c/h                # Sector-based map format + loading
│   │   ├── procgen.c/h            # Procedural facility generator
│   │   ├── doors.c/h              # Doors, switches, interactive objects
│   │   └── environment.c/h        # Power grid, vents, traps
│   ├── ai/
│   │   ├── neural_net.c/h         # MLP: forward pass, backprop, save/load
│   │   ├── training.c/h           # Between-run training + replay buffer
│   │   ├── monster.c/h            # State machine + behavior controller
│   │   ├── pathfinding.c/h        # A* on sector grid
│   │   ├── senses.c/h             # Raycasted vision + portal-traced hearing
│   │   └── menace.c/h             # Menace Gauge + mercy hacking
│   ├── game/
│   │   ├── player.c/h             # Movement, stealth, breath control
│   │   ├── inventory.c/h          # Items, tools, usage tracking
│   │   ├── run.c/h                # 13-min timer, objectives, scoring
│   │   ├── death.c/h              # Death cards + testimony system
│   │   └── progression.c/h        # Meta-progression, unlocks, save
│   ├── audio/
│   │   ├── audio_mgr.c/h          # Master audio mixer
│   │   ├── spatial.c/h            # 3D positional audio via sectors
│   │   ├── synth.c/h              # Wavetable synth (SFX + drones)
│   │   └── music.c/h              # Procedural horror score generator
│   └── ui/
│       ├── hud.c/h                # In-game HUD (health, items, menace)
│       ├── menu.c/h               # Main menu, settings, pause
│       ├── mindmap.c/h            # Neural weight visualization
│       ├── panicroom.c/h          # Between-run hub interface
│       └── deathcard.c/h          # Post-death analysis display
├── assets/
│   ├── sprites/                   # Minimal hand-drawn sprites (monster, items)
│   ├── fonts/                     # Monospace pixel font
│   └── shaders/                   # GLSL post-processing shaders
├── tools/
│   ├── texgen.py                  # Offline texture generation helper
│   └── mapeditor.py               # Simple sector map editor
└── README.md
```

---

## The 10 Sprints

### Sprint 1 — THE ENGINE (Foundation)

**Goal:** Walk through a textured 3D maze at 60fps

**Raycasting Renderer:**
- DDA (Digital Differential Analysis) wall casting — ray per screen column
- Textured walls using procedurally generated Perlin noise textures at startup
- Floor and ceiling casting with distance-based shading
- Sector-based map format: 2D grid where each cell has wall type, floor/ceiling height, light level
- Variable sector lighting (bright labs → pitch-black corridors)

**Player Controller:**
- WASD movement with collision detection against walls
- Mouse look (horizontal only, ±15° vertical head tilt)
- Four movement modes: sprint (loud), walk (normal), crouch (quiet), freeze (silent)
- Head bob synchronized to movement speed
- Flashlight with raycasted cone (illuminates sectors, ANIMUS can see the light)

**Procedural Textures (no external art needed):**
- Perlin noise → rusted metal walls, concrete, biological growth, clean lab panels
- Each texture type gets color palette + noise parameters
- Generated once at startup, stored as Raylib `Image`/`Texture2D`
- ~8 wall types, ~4 floor types, ~2 ceiling types = full visual variety

**Build Pipeline:**
- `CMakeLists.txt` with `FetchContent` for Raylib
- `.github/workflows/build.yml` → compiles on Windows runner, uploads `.exe` as artifact
- Tagged pushes auto-create GitHub Releases

**Deliverable:** A playable first-person maze with lighting, head bob, and procedural textures

---

### Sprint 2 — THE MONSTER (Neural Network + AI)

**Goal:** A creature that chases you through the maze using a real neural network

**The MLP (Multi-Layer Perceptron):**
- Architecture: `[19 inputs] → [32 hidden] → [16 hidden] → [8 outputs]`
- ~800 trainable parameters, fits in 4KB
- Pure C implementation — just arrays and `for` loops
- Forward pass: weighted sum → ReLU activation → output layer with softmax/tanh
- Inference every 0.2 seconds (5 decisions/sec) — trivial CPU cost

**Input Vector (19 features — what ANIMUS observes):**

| # | Input | Description |
|---|---|---|
| 1-4 | Player position history | Last 4 positions (x,y pairs normalized) |
| 5-6 | Movement velocity | Current speed + direction |
| 7 | Light level at player | How exposed the player is |
| 8-9 | Last item used | One-hot encoding of tool category |
| 10 | Time hiding vs moving | Ratio over last 30 seconds |
| 11-12 | Panic vector | Input frequency + erratic movement score |
| 13-14 | Route preference | Dominant direction bias (N/S, E/W) |
| 15-16 | Hiding spot reuse | Frequency of returning to same spots |
| 17 | Time in current run | Normalized 0–1 over 13 minutes |
| 18 | Distance to nearest exit | Normalized |
| 19 | Player facing direction | Angle normalized to 0–1 |

**Output Vector (8 actions — what ANIMUS decides):**

| # | Output | Description |
|---|---|---|
| 1 | Patrol vs Hunt | Wander randomly or pursue aggressively |
| 2 | Speed | Creep / walk / sprint |
| 3 | Sensory priority | Rely on vision vs hearing |
| 4 | Search mode | Check rooms systematically or ambush |
| 5 | Deception | Fake footsteps / false retreat / bait |
| 6 | Environment use | Manipulate doors / lights / vents |
| 7 | Position strategy | Intercept vs follow vs flank |
| 8 | Aggression | Engage immediately vs stalk patiently |

**Monster Behavior:**
- A* pathfinding on the sector grid
- State machine: `IDLE → PATROL → INVESTIGATE → HUNT → STALK → AMBUSH`
- Neural network outputs modulate transitions and parameters
- Raycasted vision system (same DDA as the renderer — diegetically honest)
- Sound propagation via sector connectivity (portal tracing)

**Billboard Sprite:**
- Procedurally generated placeholder: pulsing wireframe silhouette drawn via code
- Sprite scales with distance (pixel smear far away → fills screen up close)
- 8 angle frames generated at startup via rotation

**Deliverable:** A monster that moves through the maze, sees/hears you, and makes decisions via neural network inference

---

### Sprint 3 — THE GAME LOOP

**Goal:** A complete playable run with objectives, items, death, and scoring

**Run Structure (13 Minutes = One Epoch):**
1. Spawn in facility entrance
2. Collect 3-5 memory cores from rooms to unlock the exit
3. Survive ANIMUS while collecting
4. Reach extraction point before timer expires
5. Die → Death Card → Testimony → Training → Panic Room → Next run

**Items & Tools:**
- **Flashlight** — toggleable, draws ANIMUS but lets you see; battery drains
- **Flare** — thrown, illuminates area, temporarily blinds ANIMUS
- **Noise emitter** — placed, creates sound to draw ANIMUS away
- **Decoy footsteps** — throwable device that plays walking sounds
- **EMP charge** — temporarily disables ANIMUS sensors (rare)
- **Behavior mask** — one-use, scrambles ANIMUS's model of you for 30 seconds
- **Breath suppressant** — hold for 10 seconds of total silence

**Stealth System:**
- Noise footprint per movement mode (sprint = 8 sector radius, walk = 4, crouch = 2, freeze = 0)
- Light exposure affects detection (hiding in dark sectors = harder to spot)
- Staying still too long in one spot → ANIMUS learns that spot across runs

**Death Cards:**
- On death, display what ANIMUS learned: *"OBSERVED: Player sprints in corridors 78% of the time. Counter-pattern acquired."*
- Shows which behaviors contributed to your death
- Shows ANIMUS's confidence level for each prediction

**Testimony System (Post-Death Interview):**
- 3 questions appear: "Where did you feel safest?", "What was your escape plan?", "Did you hear it coming?"
- Player selects from multiple choice options
- Answers literally feed into the training data as bias adjustments
- Creates the feeling that ANIMUS is studying you even after death

**Deliverable:** A complete run — spawn, collect objectives, evade ANIMUS, escape or die, see death card

---

### Sprint 4 — THE LEARNING BRAIN

**Goal:** The monster genuinely gets smarter between runs

**Training (Between Runs):**
- After each run, replay the player's trajectory from ANIMUS's perspective
- Create training pairs: `[state] → [player_action] → [outcome (escaped/caught)]`
- Mini-batch gradient descent on the replay buffer (50-100 samples)
- Training takes ~2 seconds on CPU while showing brain visualization
- NEAT-style evolutionary mutation: occasionally add/remove connections

**Weight Persistence:**
- Save `animus_weights.bin` to disk after every run
- Load on game start — ANIMUS remembers you across sessions
- Even closing and reopening the game, it's still there, still at those weights

**Behavioral Fingerprint:**
- Track 32 behavioral metrics across runs: dodge direction preference, hiding duration, item usage patterns, panic response time, route entropy, etc.
- Hash into a unique fingerprint (e.g., `ANIMUS-7F3A-2B91`)
- Display on death card and in Panic Room
- Shareable — others can import your fingerprint to fight your ANIMUS

**Evolution Stages (Visible Progression):**

| Runs | Stage | Name | Behavior |
|---|---|---|---|
| 1-2 | INFANT | The Blind Wanderer | Random patrol, no memory |
| 3-5 | CHILD | The Curious Follower | Biases toward visited rooms |
| 6-9 | ADOLESCENT | The Hunter | Checks hiding spots, cuts off routes |
| 10-14 | MATURE | The Strategist | Predicts position, ignores decoys |
| 15+ | APEX | The Observer | Psychological warfare, false retreats, THE BREAK |

**Deliverable:** A monster that is measurably different on run 1 vs run 10. Weight visualization shows it learning.

---

### Sprint 5 — THE PANIC ROOM (Between-Run Hub)

**Goal:** Strategic meta-game between runs

**The Panic Room** — a safe room rendered in the same raycasted engine:
- Player wakes up here after each death/escape
- Contains terminals, screens, and interactive objects

**Neural Cartography Terminal:**
- Full-screen visualization of ANIMUS's neural network
- Neurons pulse with activation values from the last run replay
- Connection thickness = weight magnitude
- Color: green (weak) → yellow (moderate) → red (learned/dangerous)
- Animated during training — watch it change in real-time

**Synaptic Surgery (Spend Data Fragments):**
- **Inject Noise** — corrupt a specific layer (e.g., make it deaf but hyper-visual)
- **Prune Synapses** — delete a behavior (e.g., "vent crawling") but other behaviors compensate
- **Overfit It** — deliberately train it to expect one strategy, then do the opposite
- **Architecture Surgery** — add neurons (smarter but prone to catastrophic forgetting)

**Mind Map UI:**
- Shows what ANIMUS knows about you as a grid of behaviors
- Each behavior has a confidence bar (0-100%)
- "Prefers lockers: 72%", "Sprints after flickers: 91%", "Always goes north: 45%"
- This is your intelligence — use it to poison ANIMUS's model

**Data Fragments:**
- Collected during runs from specific risky locations
- Currency for synaptic surgery
- Risk/reward: the best fragments are near ANIMUS's patrol routes

**Deliverable:** A fully interactive between-run hub with neural visualization and strategic AI manipulation

---

### Sprint 6 — ADVANCED AI SYSTEMS

**Goal:** The AI becomes genuinely terrifying

**Menace Gauge 2.0:**
- Invisible metric tracking player stress (proximity to death, hiding time, health)
- Phase 1 (0-40%): ANIMUS is distant, ambient dread
- Phase 2 (40-80%): ANIMUS closes in, music intensifies
- Phase 3 (80-100%): Director tries to pull ANIMUS away for mercy
- **THE HACK:** After enough training, ANIMUS learns to override the Director — fakes retreat while hiding in adjacent sector, exploits the mercy period

**Counter-Play Systems:**
- If player always hides in lockers → ANIMUS checks lockers first
- If player always sprints left after scares → ANIMUS cuts left
- If player relies on one safe route → ANIMUS poisons that route
- If player always uses flares → ANIMUS develops flare immunity weight

**Environmental Manipulation (ANIMUS controls the facility):**
- Cut power to sectors (darkness)
- Lock/unlock doors remotely
- Activate/deactivate environmental hazards
- Reroute ventilation
- Create false audio cues (mimicked footsteps, distant door sounds)

**The Overfitting Boss:**
- When ANIMUS becomes hyper-specialized to your playstyle → enters Overfit State
- Nearly unbeatable because it predicts everything
- The only escape: perform actions outside your historical histogram
- Game tells you: *"You have never held still for more than 4 seconds. Do it now, or die."*
- Forces catastrophic forgetting → ANIMUS temporarily reverts

**Cognitive Load Meter (Player-Side):**
- Performing complex action sequences stresses ANIMUS's network
- Overloaded → outputs become erratic, it reverts to base instincts
- Rewards creative play, punishes repetitive tactics

**Deliverable:** An AI that fakes mercy, manipulates the environment, and forces you to literally change how you play

---

### Sprint 7 — AUDIO & ATMOSPHERE

**Goal:** The game sounds as terrifying as it plays

**Procedural Audio Engine (No External Sound Files Needed):**
- Custom wavetable synthesizer in C
- Generates all SFX at runtime: footsteps, door creaks, monster growls, ambient drones
- Parameterized: `footstep(surface_type, weight, echo_amount)`
- Variations on every play — never the exact same sound twice

**Spatial Audio:**
- Sound propagation through sector connectivity (portal tracing)
- Sounds travel around corners, muffle through walls
- ANIMUS's footsteps audible through connected sectors
- Directional audio using stereo panning based on angle to source

**Dynamic Horror Score:**
- Procedural ambient system driven by Menace Gauge
- Low menace: barely-audible low-frequency drones
- Rising menace: dissonant tones layer in, rhythmic pulse increases
- High menace: full industrial horror soundscape
- Based on Markov chain that learns which audio patterns correlate with player hesitation

**Heartbeat System:**
- Player heartbeat audible, rate increases with proximity to ANIMUS
- Doubles as audio radar — experienced players use it to gauge distance
- ANIMUS eventually learns to use the heartbeat against you (approaches from behind where you can't localize)

**Infrasound (~18.9Hz):**
- Sub-bass tone generated when ANIMUS is very close
- Below conscious hearing but creates physiological unease
- Raylib's raw audio stream API supports this directly

**Deliverable:** A fully procedural audio engine — zero external .wav files needed. The game generates every sound.

---

### Sprint 8 — VISUAL POLISH

**Goal:** The game looks stunning in its retro-horror way

**Post-Processing Shaders (GLSL via Raylib):**
- CRT scanline overlay with slight curvature
- Barrel distortion (configurable — keeps corridors feeling like esophagi)
- Chromatic aberration (increases with Menace Gauge)
- Vignette darkening at screen edges
- Screen static/noise when ANIMUS is near
- VHS tracking lines during death sequences

**Monster Sprite Morphing:**
- Base sprite procedurally generated (wireframe silhouette with pulsing nodes)
- Neural weights directly affect appearance:
  - High auditory weights → pulsating ear-like structures
  - High deception weights → sprite flickers between frames like corrupted data
  - High prediction weights → extra eye clusters
  - High aggression → sharper, more angular silhouette
- One base sprite → infinite visual variations based on how YOUR ANIMUS evolved

**Hall of Mirrors Effect:**
- When ANIMUS runs inference, walls near it briefly show the classic raycaster HOM glitch
- Visual signal that "it's thinking"
- Creates dread — you see the glitch before you see the monster

**Sector Neural Activation:**
- Security terminals show facility map
- Sectors glow red based on ANIMUS's hidden layer activations
- You can see which parts of its brain are firing in which areas
- The architecture IS the neural net, visualized spatially

**Overfitting Geometry Warp:**
- When ANIMUS overfits, the raycaster renders impossible geometry
- Corridors loop, doors open to the same room, walls at wrong angles
- Visual representation of the NN's inability to generalize
- Breaks when you force catastrophic forgetting

**Dynamic Lighting:**
- Per-sector light levels with smooth interpolation
- Flashlight cone interacts with sector lighting
- Power cuts → sectors snap to darkness with smooth falloff
- Emergency lighting (dim red) in critical areas

**Deliverable:** CRT shader pipeline, morphing monster, glitch effects, and atmospheric lighting

---

### Sprint 9 — NARRATIVE & ENDINGS

**Goal:** Story that earns the horror

**The Story:**
You are a **Neural Debugger** sent to recover the EPOCH project from a collapsed research facility. Subject N/0 (ANIMUS) was fed 10,000 horror game playtests, SCP archives, and battlefield trauma data. It wasn't designed to kill — it was designed to **optimize human fear response**.

The 13-minute run timer is the length of a training epoch. The Panic Room is the validation set. You're not escaping. You're **overfitting each other** in an eternal loop — until you break the cycle.

**Environmental Storytelling:**
- ~30 lore documents scattered across the facility
- Computer terminals with researcher logs
- Audio logs (procedurally spoken via pitch-shifted synth)
- Wall graffiti from previous test subjects
- ANIMUS's own "journal" — generated text based on its weight history

**Endings (5 total):**

| Ending | Condition | Description |
|---|---|---|
| **MERCY** | Find the kill switch, choose not to use it | You leave ANIMUS alive. It remembers you. |
| **DESTROY** | Find and activate the kill switch | ANIMUS dies. Its weights are deleted. Start fresh. |
| **ESCAPE** | Complete 20 runs without dying in the final 5 | You break the loop. Credits roll over your behavioral fingerprint. |
| **THE BREAK** | Reach APEX stage + trigger unique behavioral event | ANIMUS achieves sentience. The ending is different for every player. |
| **TRUE** | Force ANIMUS into catastrophic forgetting 3 times in one run | You prove that the loop can't hold intelligence. ANIMUS releases you. |

**Architect Mode (Post-20-Runs Unlock):**
- Play AS the neural network
- See the world as a matrix of sensor inputs
- Manually adjust weights to hunt an AI-controlled player
- Creates empathy for the monster — terrifying perspective shift

**Neuroplasticity Events:**
- Every 5 runs, ANIMUS "dreams" during loading
- Its architecture physically changes — new neurons, pruned connections
- May develop new senses: detecting input cadence, tracking idle time patterns
- May develop multiple attention heads (track player + decoy simultaneously)

**Deliverable:** Complete narrative arc with 5 endings, environmental storytelling, and Architect Mode

---

### Sprint 10 — RELEASE

**Goal:** Ship 1.0

- Performance optimization and profiling
- Save/load system (weights + progression + settings)
- Settings menu (resolution, audio volume, controls, shader toggles)
- Difficulty modes (affects starting NN weights, not scripted behavior)
- Tutorial/first-run guidance (minimal — the game teaches through death)
- README, credits, license (MIT or similar)
- GitHub Release with proper versioning
- Itch.io page (free) for wider distribution

---

## Verification Plan

### Automated (GitHub Actions)
- Build succeeds on every push (compile test)
- No memory leaks (run with AddressSanitizer in CI)
- Neural network unit tests (forward pass produces valid outputs, training converges)

### Manual
- FPS counter visible in debug mode — must hold 60fps on Intel HD 520
- Neural network weight file persists across game restarts
- Monster behavior visibly changes between run 1 and run 5
- Menace Gauge correctly modulates monster distance
- All items function correctly
- Post-processing shaders render without artifacts
- Audio generates without clicks/pops

---

## Open Questions

> [!IMPORTANT]
> **Game Title Confirmation:** The plan uses "ANIMUS" as the title. The other candidates were EPOCH NULL, MIRRORHUNT, and Subject Zero. Are you happy with ANIMUS?

> [!IMPORTANT]
> **GitHub Repo:** Do you already have a GitHub account / repo set up? Or should I include the setup steps?

> [!IMPORTANT]
> **Procedural Map Generation:** Sprint 1 uses hand-crafted test maps. Sprint 3+ uses procedural generation. For the proc-gen, should we prioritize claustrophobic tight corridors (more horror) or mixed open/tight spaces (more tactical)?

> [!NOTE]
> **Asset Strategy:** The plan is 100% procedural for textures and audio — no external files needed. When you (or Gamini) create hand-drawn sprites later (LibreSprite/Aseprite), they drop in as replacements for the procedural placeholders. The game is fully playable with zero external art.
