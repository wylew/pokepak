# Pokedex.pak

A Gen I Pokédex tool pak for NextUI / MinUI on the TrimUI Brick and TrimUI Smart Pro (tg5040).

Browse all 151 original Pokémon, view their types, base stats, evolution chains,
and Pokédex flavor text — fully offline, no internet required.

---

## Features

- Full Gen 1 Pokédex (Bulbasaur #001 → Mew #151)
- Colour-coded by primary type in the list view
- Three-slide detail view per Pokémon:
  1. Overview (name, number, types, stat summary)
  2. Full base stats breakdown
  3. Evolution info + Pokédex description
- Scroll position is remembered between sessions
- Runs entirely from a shell script — no Python, no internet

---

## Controls

| Button | Action                          |
|--------|---------------------------------|
| D-Pad  | Navigate list / slides          |
| A      | View Pokémon detail             |
| B      | Go back / Quit                  |
| L / R  | Scroll list pages               |

---

## Installation

1. Mount your NextUI SD card on your computer.
2. Download `Pokedex.pak.zip` from the releases page.
3. Extract the zip and copy the `Pokedex.pak` folder to:
   ```
   /Tools/tg5040/Pokedex.pak/
   ```
4. Confirm the file `/Tools/tg5040/Pokedex.pak/launch.sh` exists.

> **Note:** The platform folder (`tg5040`) matches the TrimUI Brick and TrimUI Smart Pro.
> If you are on a different device, copy the pak folder to the correct platform subfolder.

---

## Dependency: minui-list and minui-presenter

This pak requires the `minui-list` and `minui-presenter` binaries, compiled for
arm64 (tg5040). You need to place them at:

```
Pokedex.pak/bin/arm64/minui-list
Pokedex.pak/bin/arm64/minui-presenter
```

Pre-compiled binaries are available from:
- https://github.com/josegonzalez/minui-list/releases
- https://github.com/josegonzalez/minui-presenter/releases

Download the `tg5040` or `arm64` release assets and place the binaries in the
`bin/arm64/` folder. Make sure they are executable:
```sh
chmod +x Pokedex.pak/bin/arm64/minui-list
chmod +x Pokedex.pak/bin/arm64/minui-presenter
```

---

## File Structure

```
Pokedex.pak/
├── launch.sh              ← Main entry point
├── README.md              ← This file
├── data/
│   └── pokemon.tsv        ← Gen I Pokémon data (offline, tab-separated)
└── bin/
    ├── arm64/             ← Binaries for tg5040 (TrimUI Brick/TSP)
    │   ├── minui-list     ← (download separately — see above)
    │   └── minui-presenter← (download separately — see above)
    └── arm/               ← Binaries for 32-bit devices (optional)
```

---

## Saved Data

Your last scroll position is saved to:
```
/.userdata/shared/Pokedex/selected_index.txt
```

This persists between sessions so you pick up where you left off.

---

## Credits

- Pokémon data adapted from public Pokédex sources (Generation I)
- Shell pak structure based on the MinUI pak guide by Jose Diaz-Gonzalez
  https://josediazgonzalez.com/2025/06/16/writing-a-pak-for-the-minui-and-nextui-launchers/
- `minui-list` and `minui-presenter` by Jose Diaz-Gonzalez
  https://github.com/josegonzalez/minui-list
  https://github.com/josegonzalez/minui-presenter

Pokémon © Nintendo / Game Freak / Creatures Inc.
This pak is a fan-made tool and is not affiliated with or endorsed by Nintendo.
