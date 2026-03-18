# Adding a New Game to Pokedex.pak

## Quick Start

1. Create a folder under `data/games/`:
   ```
   data/games/my-game-slug/
   ├── game.conf
   └── pokemon.tsv
   ```

2. The pak auto-detects the folder and adds it to the game select menu.

---

## game.conf reference

```
name=Pokemon FireRed
short=FireRed
year=2004
platform=Game Boy Advance
type=retail
dex_label=Kanto Dex
entry_count=386
color=#CC4400
```

| Key           | Description                                      |
|---------------|--------------------------------------------------|
| `name`        | Full display name shown in the game select list  |
| `short`       | Short label (unused in UI, for your reference)   |
| `year`        | Release year                                     |
| `platform`    | Platform string                                  |
| `type`        | `retail` or `fan`  (shown as [OG] or [FAN])      |
| `dex_label`   | Pokédex region name (e.g. "Kanto Dex")           |
| `entry_count` | Total entries in your TSV                        |
| `color`       | Background color hex for the game select row     |
| `note`        | Optional note (not displayed, for your records)  |

---

## pokemon.tsv — Two supported formats

### Format 1: "gen1" — for retail games with base stats

Header row (tab-separated, exact column names matter):

```
id	name	type1	type2	hp	atk	def	spa	spd	spe	evolution	description
```

- `type2` can be `—` or blank for single-type Pokémon
- `evolution` is a plain text string
- `description` is Pokédex flavor text

Example row:
```
25	Pikachu	Electric	—	35	55	40	50	50	90	Evolves into Raichu (Thunder Stone)	When several gather, their electricity causes lightning storms.
```

### Format 2: "lazarus" — for ROM hacks with regional variants, megas, and forms

Header row:

```
ilios_id	national_id	name	type1	type2	form_note	location
```

- `ilios_id` is the ROM hack's internal Pokédex number
- `national_id` is the official National Dex number
- `form_note` describes the variant type: `Mega`, `Regional`, `Form`, or blank for base
- `location` is the in-game location string

Example rows:
```
71	88	Grimer	Poison		        Acrisia City (wild)
71	88	Alolan Grimer	Poison	Dark	Regional	Acrisia City (wild)
12	652	Mega Chesnaught	Grass	Fighting	Mega	Mega Evolution of Chesnaught
```

---

## Format auto-detection

The pak reads the first line of your TSV and checks whether it contains
`ilios_id`. If yes, it uses the "lazarus" format. Otherwise it uses "gen1".

You can use either format for any game — the naming is just convention.

---

## Notes on TSV files

- Fields must be separated by real tab characters, not spaces
- Do not use quotes around fields
- Blank type2 is fine — just leave the column empty
- Add as many rows as you like — there's no entry limit
- Files are read sequentially, so order determines list order

---

## Folder naming

The folder name under `data/games/` becomes the slug used for save data.
Avoid spaces or special characters. Examples: `gen1-red-blue`, `lazarus`,
`emerald-seaglass`, `pokemon-gaia`.
