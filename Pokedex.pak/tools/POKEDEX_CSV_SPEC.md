# Pokédex CSV Specification
## Pokedex.pak — Universal Game Data Format

This document defines the CSV format used to add any Pokémon game
(retail, fan ROM hack, or custom) to the Pokedex.pak.

---

## Workflow

```
You fill in:              Tool converts:           Pak reads:
pokemon.csv    →→→   csv_to_pak.py    →→→    pokemon.tsv
game_meta.csv          (on your PC)            game.conf
```

1. Fill in `pokemon.csv` and `game_meta.csv` using the spec below
2. Run: `python3 csv_to_pak.py --game my-game-slug`
3. Copy the output folder into `Pokedex.pak/data/games/` on your SD card

---

## File 1: `game_meta.csv`

A single-row CSV (plus header) describing the game itself.

### Columns

| Column         | Required | Description                                              | Example                  |
|----------------|----------|----------------------------------------------------------|--------------------------|
| `slug`         | ✅       | Unique folder name, lowercase, hyphens only              | `pokemon-emerald`        |
| `name`         | ✅       | Full display name                                        | `Pokemon Emerald`        |
| `short`        | ✅       | Short name for tight spaces                              | `Emerald`                |
| `year`         | ✅       | Release year                                             | `2004`                   |
| `platform`     | ✅       | Platform name                                            | `Game Boy Advance`       |
| `type`         | ✅       | `retail` or `fan`                                        | `retail`                 |
| `dex_label`    | ✅       | Pokédex region name                                      | `Hoenn Dex`              |
| `color`        | ✅       | Hex color for game select row background                 | `#006400`                |
| `has_stats`    | ✅       | `yes` if pokemon.csv includes base stats, `no` otherwise | `yes`                    |
| `has_location` | ✅       | `yes` if pokemon.csv includes location data              | `no`                     |
| `has_national` | ✅       | `yes` if game uses both a regional and national dex #    | `no`                     |
| `note`         |          | Optional note (not displayed on-device)                  | `Expanded movepool hack` |

### Example

```csv
slug,name,short,year,platform,type,dex_label,color,has_stats,has_location,has_national,note
pokemon-emerald,Pokemon Emerald,Emerald,2004,Game Boy Advance,retail,Hoenn Dex,#006400,yes,no,no,
pokemon-lazarus,Pokemon Lazarus,Lazarus,2024,GBA ROM Hack,fan,Ilios Dex,#1a3a5c,no,yes,yes,430 unique Pokemon + forms
```

---

## File 2: `pokemon.csv`

One row per Pokémon entry (including alternate forms, regional variants, and Megas
as separate rows — each with its own `dex_id`).

### All Columns

| Column        | Required | Description                                                      | Example values                         |
|---------------|----------|------------------------------------------------------------------|----------------------------------------|
| `dex_id`      | ✅       | Regional dex number for this game (repeated for alternate forms) | `1`, `25`, `006`                       |
| `national_id` |          | Official National Dex number (leave blank if not applicable)     | `722`, `25`                            |
| `name`        | ✅       | Pokémon name, including form prefix if applicable                | `Pikachu`, `Alolan Raichu`, `Mega Gengar` |
| `type1`       | ✅       | Primary type                                                     | `Fire`, `Water`, `Ghost`               |
| `type2`       |          | Secondary type (blank if single-type)                            | `Flying`, `Dark`                       |
| `form_tag`    |          | Form category tag — see **Form Tags** below                      | `Mega`, `Regional`, `Form`, `Legendary`, `Mythical` |
| `hp`          |          | Base HP stat                                                     | `45`, `250`                            |
| `atk`         |          | Base Attack stat                                                 | `49`, `110`                            |
| `def`         |          | Base Defense stat                                                 | `49`, `130`                            |
| `spa`         |          | Base Special Attack stat                                         | `65`, `154`                            |
| `spd`         |          | Base Special Defense stat                                        | `65`, `90`                             |
| `spe`         |          | Base Speed stat                                                  | `45`, `130`                            |
| `ability1`    |          | Primary ability name                                             | `Overgrow`, `Static`                   |
| `ability2`    |          | Secondary ability name (blank if none)                           | `Chlorophyll`, `Lightning Rod`         |
| `hidden_ability` |       | Hidden ability name (blank if none)                              | `Drought`, `Surge Surfer`              |
| `evo_from`    |          | Pokémon this evolves from (blank if base form)                   | `Pichu`, `Charmander`                  |
| `evo_method`  |          | How evolution is triggered                                       | `Level 16`, `Thunder Stone`, `Trade`   |
| `evo_into`    |          | What this Pokémon evolves into (blank if final form)             | `Raichu`, `Charmeleon`, `Vaporeon/Jolteon/Flareon` |
| `location`    |          | Where to find this Pokémon in-game                               | `Viridian Forest (wild)`, `Starter`    |
| `catch_rate`  |          | Catch rate value (0–255)                                         | `45`, `3`                              |
| `gender_ratio`|          | Gender ratio descriptor                                          | `M87.5/F12.5`, `Genderless`, `M100`   |
| `egg_group1`  |          | First egg group                                                  | `Monster`, `Fairy`, `Undiscovered`     |
| `egg_group2`  |          | Second egg group (blank if only one)                             | `Dragon`, `Field`                      |
| `description` |          | Pokédex flavor text (wrap in quotes if it contains commas)       | `"A strange seed was planted on its back at birth."` |

### Which columns are truly required

Only `dex_id`, `name`, and `type1` are strictly required. Every other column
is optional — the pak will simply omit that section from the detail view if
the data is blank. This means you can start with a minimal CSV and fill in
more detail over time.

---

## Form Tags

The `form_tag` column categorizes alternate entries. Recognized values:

| Tag         | Meaning                                              | Display in list      |
|-------------|------------------------------------------------------|----------------------|
| `Mega`      | Mega Evolution                                       | `(Mega)`             |
| `Regional`  | Regional variant (Alolan, Galarian, Hisuian, Paldean)| `(Regional)`         |
| `Form`      | Alternate form (Oricorio styles, Lycanroc variants)  | `(Form)`             |
| `Legendary` | Legendary Pokémon                                    | `(Legendary)`        |
| `Mythical`  | Mythical Pokémon                                     | `(Mythical)`         |
| `Starter`   | Starter Pokémon                                      | `(Starter)`          |
| *(blank)*   | Standard base form — no badge shown                  |                      |

You can also use custom tags — anything you put here will appear in
parentheses next to the name in the list and detail views.

---

## Type Values

Recognized type names (capitalized exactly as shown):

`Normal`, `Fire`, `Water`, `Electric`, `Grass`, `Ice`, `Fighting`, `Poison`,
`Ground`, `Flying`, `Psychic`, `Bug`, `Rock`, `Ghost`, `Dragon`, `Dark`,
`Steel`, `Fairy`

Any unrecognized type will use a neutral gray color. This is intentional —
fan games with custom types (like `Nuclear` in Pokémon Uranium) will still work.

---

## CSV Rules

- Use standard comma separation
- Wrap any field in double quotes if it contains a comma: `"Evolve Nincada, leaves behind a shell"`
- Fields may be left blank — just use consecutive commas: `25,,,Fire,,`
- The header row is required and must match the column names exactly
- UTF-8 encoding recommended (for names like `Flabébé`, `Nidoran♀`)
- Boolean fields (`has_stats`, etc. in game_meta) use `yes` / `no`
- Dex IDs do not need to be zero-padded — the pak handles display formatting

---

## Example `pokemon.csv` (mixed retail + fan game styles)

```csv
dex_id,national_id,name,type1,type2,form_tag,hp,atk,def,spa,spd,spe,ability1,ability2,hidden_ability,evo_from,evo_method,evo_into,location,catch_rate,gender_ratio,egg_group1,egg_group2,description
1,,Bulbasaur,Grass,Poison,Starter,45,49,49,65,65,45,Overgrow,,Chlorophyll,,,Ivysaur,Route 1 / Professor Oak,45,M87.5/F12.5,Monster,Grass,"A strange seed was planted on its back at birth. The plant sprouts and grows with this Pokemon."
2,,Ivysaur,Grass,Poison,,60,62,63,80,80,60,Overgrow,,Chlorophyll,Bulbasaur,Level 16,Venusaur,Evolve Bulbasaur,45,M87.5/F12.5,Monster,Grass,"When the bulb on its back grows large, it appears to lose the ability to stand on its hind legs."
3,,Venusaur,Grass,Poison,,80,82,83,100,100,80,Overgrow,,Chlorophyll,Ivysaur,Level 32,,Evolve Ivysaur,45,M87.5/F12.5,Monster,Grass,The plant blooms when it is absorbing solar energy. It stays on the move to seek sunlight.
3,,Mega Venusaur,Grass,Poison,Mega,80,100,123,122,120,80,Thick Fat,,,Venusaur,Mega Stone,,Evolve Ivysaur,,,,,
25,25,Pikachu,Electric,,,,35,55,40,50,50,90,Static,,Lightning Rod,Pichu,High Friendship,Raichu,Pallet Town (Gift),190,M50/F50,Field,Fairy,"When several of these Pokemon gather, their electricity could build and cause lightning storms."
26,26,Raichu,Electric,,,60,90,55,90,80,110,Static,,Lightning Rod,Pikachu,Thunder Stone,,Evolve Pikachu,75,M50/F50,Field,Fairy,Its long tail serves as a ground to protect itself from its own high voltage power.
26,26,Alolan Raichu,Electric,Psychic,Regional,60,85,50,95,85,110,Surge Surfer,,,Pikachu,Thunder Stone (Alola),,Evolve Pikachu in Alola,75,M50/F50,Field,Fairy,It's said that the overflowing psychic power it stored to prevent itself from shocking itself has given it the ability to control psychic power.
144,144,Articuno,Ice,Flying,Legendary,90,85,100,95,125,85,Pressure,,Snow Cloak,,,,"Seafoam Islands",3,Genderless,Undiscovered,,"A legendary bird Pokemon. It can create blizzards by freezing moisture in the air."
150,150,Mewtwo,Psychic,,Legendary,106,110,90,154,90,130,Pressure,,Unnerve,,,,"Cerulean Cave",3,Genderless,Undiscovered,,"It was created by a scientist after years of horrific gene splicing and DNA engineering experiments."
```

---

## Minimal CSV (name + types only — everything else optional)

```csv
dex_id,national_id,name,type1,type2,form_tag,hp,atk,def,spa,spd,spe,ability1,ability2,hidden_ability,evo_from,evo_method,evo_into,location,catch_rate,gender_ratio,egg_group1,egg_group2,description
1,,Rowlet,Grass,Flying,Starter,,,,,,,,,,,,,Starter,,,,
2,,Dartrix,Grass,Flying,,,,,,,,,,,,Rowlet,Level 17,Decidueye,Evolve Rowlet,,,,,
3,,Decidueye,Grass,Ghost,,,,,,,,,,,,Dartrix,Level 34,,,,,
```
