# Poképak

A custom Pokédex application for the **TRIMUI Brick** and **TrimUI Smart Pro** (tg5040), built with C and SDL2. Optimized for the 1024x768 display and Mali GPU.

## Features
- **GBA Aesthetic**: Banded gradients and classic Pokédex UI.
- **Multi-Game Support**: Seamlessly switch between different Pokémon games/regional dexes.
- **Custom Overrides**: Support for game-specific sprites and data.
- **Handheld Optimized**: Native ARM64 performance with OpenGL ES 2.0 rendering.

---

## Build Instructions

To ensure compatibility with the TRIMUI devices (Glibc 2.31), use the legacy build environment:

```bash
make legacy
```

This will:
1. Create a Debian Bullseye Distrobox container.
2. Cross-compile the application for ARM64.
3. Output the binary to `Pokedex.pak/bin/arm64/pokedex`.

---

## Adding Custom Game Data

Adding a new game is as simple as creating a new folder in `Pokedex.pak/data/games/`.

### 1. Directory Structure
```
Pokedex.pak/data/games/
└── my-custom-game/
    ├── game.conf           # Game display settings
    ├── pokemon.tsv         # Pokémon data table (Tab Separated)
    ├── caught.txt          # (Auto-generated) list of caught Pokémon IDs
    └── custom_sprites/     # (Optional) Sprites specific to this game
```

### 2. `game.conf` Format
Create a `game.conf` file with the following keys:
```ini
name=My Custom ROM Hack
```
*(Currently, only `name=` is parsed by the C application, but you can include others for future-proofing.)*

### 3. Pokémon Data Table (.tsv)
The application expects a **Tab-Separated** file with a header row. You can name this file anything as long as it ends in **`.tsv`** (e.g. `lazarus.tsv` or `national_dex.tsv`).

| Column | Name | Description |
| :--- | :--- | :--- |
| 1 | `ilios_id` | Internal ID used for list sorting. |
| 2 | `national_id` | **Primary Sprite Match**. Used for ID-based fallback (e.g. `122.png`). |
| 3 | `name` | Pokémon Name (e.g. `Bulbasaur`). |
| 4 | `type1` | Primary Type (e.g. `Grass`). |
| 5 | `type2` | Secondary Type (empty if single-type). |
| 6 | `form_note` | (Optional) Note like `Mega` or `Alolan`. |

**Note:** Use real Tab characters, not spaces. Do not wrap fields in quotes.

### 4. Sprite Conventions
Sprites are loaded from two locations in order:
1. **Game Specific**: `data/games/<slug>/custom_sprites/`
2. **Global Library**: `Pokedex.pak/data/sprites/`

#### Filename Matching Logic:
For a Pokémon named `"Hisuian Decidueye"` with National ID `1025`, the app checks:
1.  **Original Case**: `Hisuian_Decidueye.png` (Spaces replaced by underscores)
2.  **lowercase**: `hisuian_decidueye.png`
3.  **UPPERCASE**: `HISUIAN_DECIDUEYE.PNG`
4.  **National ID**: `1025.png`

---

## Installation
1. Build the binary using `make legacy`.
2. Copy the entire `Pokedex.pak` folder to your SD card's `/Tools/tg5040/` directory.
3. Launch via the **Tools** menu on your device.

---

## Controls (TRIMUI Brick)
- **D-Pad**: Navigate lists.
- **B**: Confirm / Select Game / Toggle Caught status.
- **A**: Go Back / Exit.
- **D-Pad Left/Right**: Quick-scroll Pokédex list by 10 entries.

---

## Credits
Built with SDL2, SDL_image, and SDL_ttf. 
Pokémon © Nintendo / Game Freak / Creatures Inc.