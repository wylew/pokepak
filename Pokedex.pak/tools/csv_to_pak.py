#!/usr/bin/env python3
"""
csv_to_pak.py — Pokédex CSV → Pokedex.pak game folder converter
================================================================
Reads a user-supplied pokemon.csv and game_meta.csv and produces
a ready-to-deploy game folder for Pokedex.pak.

Usage
-----
  python3 csv_to_pak.py --game my-game-slug [options]

  --game SLUG          Folder name (must match slug in game_meta.csv)
  --csv-dir PATH       Directory containing your CSVs (default: current dir)
  --out-dir PATH       Where to write the output folder (default: ./output)
  --validate-only      Run validation without writing any files
  --no-color           Disable colored terminal output

Output
------
  output/
  └── my-game-slug/
      ├── game.conf       ← metadata for the pak
      └── pokemon.tsv     ← tab-separated data for on-device use

Copy the output folder into:
  Pokedex.pak/data/games/my-game-slug/

Dependencies
------------
  Python 3.6+  (no third-party packages required)
"""

import argparse
import csv
import os
import re
import sys
import textwrap
from pathlib import Path
from typing import Optional

# ── ANSI colours ──────────────────────────────────────────────────────────────
USE_COLOR = True

def c(code, text):
    return f"\033[{code}m{text}\033[0m" if USE_COLOR else text

OK    = lambda t: c("32", t)
WARN  = lambda t: c("33", t)
ERR   = lambda t: c("31", t)
BOLD  = lambda t: c("1",  t)
DIM   = lambda t: c("2",  t)
CYAN  = lambda t: c("36", t)

# ── Known valid values ─────────────────────────────────────────────────────────
VALID_TYPES = {
    "Normal", "Fire", "Water", "Electric", "Grass", "Ice",
    "Fighting", "Poison", "Ground", "Flying", "Psychic", "Bug",
    "Rock", "Ghost", "Dragon", "Dark", "Steel", "Fairy",
}

VALID_FORM_TAGS = {
    "Mega", "Regional", "Form", "Legendary", "Mythical", "Starter", "",
}

VALID_GAME_TYPES = {"retail", "fan"}

REQUIRED_META_COLS = [
    "slug", "name", "short", "year", "platform",
    "type", "dex_label", "color", "has_stats", "has_location", "has_national",
]

REQUIRED_POKEMON_COLS = ["dex_id", "name", "type1"]

ALL_POKEMON_COLS = [
    "dex_id", "national_id", "name", "type1", "type2", "form_tag",
    "hp", "atk", "def", "spa", "spd", "spe",
    "ability1", "ability2", "hidden_ability",
    "evo_from", "evo_method", "evo_into",
    "location", "catch_rate", "gender_ratio",
    "egg_group1", "egg_group2", "description",
]

# TSV column order written to disk (always the same regardless of what CSV provides)
TSV_COLS = ALL_POKEMON_COLS


# ── Validation helpers ─────────────────────────────────────────────────────────

class ValidationError(Exception):
    pass


def validate_hex_color(value: str, field: str) -> str:
    """Validate and normalise a hex color string."""
    v = value.strip()
    if not re.match(r'^#[0-9A-Fa-f]{6}$', v):
        raise ValidationError(
            f"{field}: '{v}' is not a valid hex colour. "
            "Use format #RRGGBB, e.g. #CC0000"
        )
    return v


def validate_type(value: str, field: str, row_num: int) -> str:
    """Warn on unrecognised types but don't block (fan games may have custom types)."""
    v = value.strip()
    if v and v not in VALID_TYPES:
        print(WARN(f"  Row {row_num}: {field} '{v}' is not a standard type — will still work but won't have a colour."))
    return v


def validate_stat(value: str, field: str, row_num: int) -> str:
    """Validate a base stat is a non-negative integer."""
    v = value.strip()
    if not v:
        return ""
    try:
        n = int(v)
        if n < 0 or n > 999:
            print(WARN(f"  Row {row_num}: {field}={v} is outside expected range 0–999."))
        return str(n)
    except ValueError:
        print(WARN(f"  Row {row_num}: {field}='{v}' is not an integer — leaving blank."))
        return ""


def validate_dex_id(value: str, row_num: int) -> str:
    """Validate dex_id is a positive integer."""
    v = value.strip()
    if not v:
        raise ValidationError(f"Row {row_num}: dex_id is required but blank.")
    try:
        n = int(v)
        if n <= 0:
            raise ValidationError(f"Row {row_num}: dex_id must be a positive integer, got {v}")
        return str(n)
    except ValueError:
        raise ValidationError(f"Row {row_num}: dex_id='{v}' is not a valid integer.")


def sanitise_text(value: str) -> str:
    """Remove characters that would break TSV or minui-presenter display."""
    # Strip tabs (would break TSV), control chars, and bare backslashes
    v = value.strip()
    v = v.replace('\t', ' ')
    v = v.replace('\r', '')
    # Collapse multiple internal newlines to one
    v = re.sub(r'\n{3,}', '\n\n', v)
    return v


# ── CSV readers ────────────────────────────────────────────────────────────────

def read_game_meta(csv_path: Path, target_slug: str) -> dict:
    """Read game_meta.csv and return the row matching target_slug."""
    if not csv_path.exists():
        raise ValidationError(f"game_meta.csv not found at: {csv_path}")

    with open(csv_path, newline='', encoding='utf-8-sig') as f:
        reader = csv.DictReader(f)
        cols = reader.fieldnames or []

        missing = [c for c in REQUIRED_META_COLS if c not in cols]
        if missing:
            raise ValidationError(
                f"game_meta.csv is missing required columns: {', '.join(missing)}\n"
                f"  Found columns: {', '.join(cols)}"
            )

        rows = list(reader)

    match = [r for r in rows if r.get("slug", "").strip() == target_slug]
    if not match:
        available = [r.get("slug", "?") for r in rows]
        raise ValidationError(
            f"No row with slug='{target_slug}' found in game_meta.csv.\n"
            f"  Available slugs: {', '.join(available)}"
        )
    if len(match) > 1:
        print(WARN(f"  Multiple rows for slug='{target_slug}' — using first."))

    meta = {k: v.strip() for k, v in match[0].items()}

    # Validate required meta fields
    for col in REQUIRED_META_COLS:
        if not meta.get(col):
            raise ValidationError(f"game_meta.csv: '{col}' is required but blank for slug='{target_slug}'.")

    # Type
    if meta["type"] not in VALID_GAME_TYPES:
        raise ValidationError(
            f"game_meta.csv: type='{meta['type']}' — must be 'retail' or 'fan'."
        )

    # Color
    meta["color"] = validate_hex_color(meta["color"], "game_meta.csv: color")

    # Boolean flags
    for flag in ("has_stats", "has_location", "has_national"):
        v = meta.get(flag, "").lower()
        if v not in ("yes", "no"):
            raise ValidationError(
                f"game_meta.csv: {flag}='{meta[flag]}' — must be 'yes' or 'no'."
            )
        meta[flag] = (v == "yes")

    return meta


def read_pokemon_csv(csv_path: Path, meta: dict) -> list:
    """Read pokemon.csv, validate all rows, return list of cleaned dicts."""
    if not csv_path.exists():
        raise ValidationError(f"pokemon.csv not found at: {csv_path}")

    with open(csv_path, newline='', encoding='utf-8-sig') as f:
        reader = csv.DictReader(f)
        cols = reader.fieldnames or []
        rows = list(reader)

    # Check required columns
    missing = [c for c in REQUIRED_POKEMON_COLS if c not in cols]
    if missing:
        raise ValidationError(
            f"pokemon.csv is missing required columns: {', '.join(missing)}\n"
            f"  Found columns: {', '.join(cols)}"
        )

    # Warn about any completely unknown columns
    known = set(ALL_POKEMON_COLS)
    extra = [c for c in cols if c not in known]
    if extra:
        print(WARN(f"  pokemon.csv: Unrecognised columns will be ignored: {', '.join(extra)}"))

    if not rows:
        raise ValidationError("pokemon.csv has no data rows.")

    errors = []
    warnings = []
    cleaned = []

    for i, raw in enumerate(rows, start=2):  # start=2 to account for header
        row_num = i

        # Skip fully blank rows
        if not any(v.strip() for v in raw.values()):
            continue

        entry = {col: raw.get(col, "").strip() for col in ALL_POKEMON_COLS}

        # --- Required fields ---
        try:
            entry["dex_id"] = validate_dex_id(entry["dex_id"], row_num)
        except ValidationError as e:
            errors.append(str(e))
            continue

        if not entry["name"]:
            errors.append(f"Row {row_num}: name is required but blank.")
            continue

        if not entry["type1"]:
            errors.append(f"Row {row_num} ({entry['name']}): type1 is required but blank.")
            continue

        # --- Type validation ---
        entry["type1"] = validate_type(entry["type1"], "type1", row_num)
        if entry["type2"]:
            entry["type2"] = validate_type(entry["type2"], "type2", row_num)

        # --- Form tag ---
        ft = entry.get("form_tag", "").strip()
        if ft and ft not in VALID_FORM_TAGS:
            # Custom form tags are fine — just note it
            print(DIM(f"  Row {row_num} ({entry['name']}): custom form_tag='{ft}' — will display as-is."))

        # --- Stats (only validate if provided) ---
        for stat in ("hp", "atk", "def", "spa", "spd", "spe"):
            entry[stat] = validate_stat(entry[stat], stat, row_num)

        # --- Catch rate ---
        if entry["catch_rate"]:
            entry["catch_rate"] = validate_stat(entry["catch_rate"], "catch_rate", row_num)

        # --- Text sanitisation ---
        for field in ("name", "description", "evo_from", "evo_method", "evo_into",
                      "location", "ability1", "ability2", "hidden_ability",
                      "gender_ratio", "egg_group1", "egg_group2"):
            entry[field] = sanitise_text(entry.get(field, ""))

        # --- Stats consistency check ---
        stat_fields = [entry[s] for s in ("hp", "atk", "def", "spa", "spd", "spe")]
        provided_stats = [s for s in stat_fields if s]
        if provided_stats and len(provided_stats) < 6:
            warnings.append(
                f"Row {row_num} ({entry['name']}): Only {len(provided_stats)}/6 stats provided. "
                "Partial stats will be shown as blank."
            )

        # --- National ID ---
        if entry["national_id"]:
            try:
                int(entry["national_id"])
            except ValueError:
                warnings.append(
                    f"Row {row_num} ({entry['name']}): national_id='{entry['national_id']}' is not an integer — clearing."
                )
                entry["national_id"] = ""

        cleaned.append(entry)

    if warnings:
        print()
        print(WARN(f"  {len(warnings)} warning(s):"))
        for w in warnings:
            print(WARN(f"    ⚠  {w}"))

    if errors:
        print()
        print(ERR(f"  {len(errors)} error(s) found:"))
        for e in errors:
            print(ERR(f"    ✗  {e}"))
        raise ValidationError(f"{len(errors)} validation error(s) in pokemon.csv — see above.")

    return cleaned


# ── Output writers ─────────────────────────────────────────────────────────────

def write_game_conf(out_dir: Path, meta: dict, entry_count: int):
    """Write game.conf from meta dict."""
    conf_path = out_dir / "game.conf"
    lines = [
        f"name={meta['name']}",
        f"short={meta['short']}",
        f"year={meta['year']}",
        f"platform={meta['platform']}",
        f"type={meta['type']}",
        f"dex_label={meta['dex_label']}",
        f"entry_count={entry_count}",
        f"color={meta['color']}",
        f"has_stats={'yes' if meta['has_stats'] else 'no'}",
        f"has_location={'yes' if meta['has_location'] else 'no'}",
        f"has_national={'yes' if meta['has_national'] else 'no'}",
    ]
    if meta.get("note"):
        lines.append(f"note={meta['note']}")
    conf_path.write_text('\n'.join(lines) + '\n', encoding='utf-8')


def write_pokemon_tsv(out_dir: Path, rows: list):
    """Write pokemon.tsv with consistent tab-separated columns."""
    tsv_path = out_dir / "pokemon.tsv"
    with open(tsv_path, 'w', encoding='utf-8', newline='') as f:
        # Header
        f.write('\t'.join(TSV_COLS) + '\n')
        # Data rows
        for row in rows:
            values = [row.get(col, "") for col in TSV_COLS]
            f.write('\t'.join(values) + '\n')


# ── Summary printer ────────────────────────────────────────────────────────────

def print_summary(meta: dict, rows: list):
    """Print a breakdown of what was found in the data."""
    total = len(rows)
    with_stats = sum(1 for r in rows if r.get("hp"))
    with_location = sum(1 for r in rows if r.get("location"))
    with_desc = sum(1 for r in rows if r.get("description"))
    with_abilities = sum(1 for r in rows if r.get("ability1"))

    form_counts = {}
    for r in rows:
        ft = r.get("form_tag", "") or "Base"
        form_counts[ft] = form_counts.get(ft, 0) + 1

    print()
    print(BOLD("  Pokédex Summary"))
    print(f"    Game:          {meta['name']} ({meta['year']}) [{meta['type'].upper()}]")
    print(f"    Platform:      {meta['platform']}")
    print(f"    Dex:           {meta['dex_label']}")
    print(f"    Total entries: {total}")
    print(f"    With stats:    {with_stats}")
    print(f"    With location: {with_location}")
    print(f"    With desc:     {with_desc}")
    print(f"    With ability:  {with_abilities}")
    print()
    print(BOLD("  Entry breakdown by form_tag:"))
    for tag, count in sorted(form_counts.items(), key=lambda x: -x[1]):
        label = tag if tag else "Base (no tag)"
        print(f"    {label:<20} {count}")


# ── Main ───────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Convert Pokédex CSV files into a Pokedex.pak game folder.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=textwrap.dedent("""
            Examples:
              python3 csv_to_pak.py --game pokemon-emerald
              python3 csv_to_pak.py --game pokemon-lazarus --csv-dir ~/Desktop/lazarus-data
              python3 csv_to_pak.py --game my-hack --validate-only
        """),
    )
    parser.add_argument("--game",          required=True,  help="Game slug (must match slug in game_meta.csv)")
    parser.add_argument("--csv-dir",       default=".",    help="Directory containing pokemon.csv and game_meta.csv (default: .)")
    parser.add_argument("--out-dir",       default="output", help="Output directory (default: ./output)")
    parser.add_argument("--validate-only", action="store_true", help="Validate CSVs without writing output files")
    parser.add_argument("--no-color",      action="store_true", help="Disable coloured output")
    args = parser.parse_args()

    global USE_COLOR
    if args.no_color:
        USE_COLOR = False

    csv_dir  = Path(args.csv_dir).expanduser()
    out_base = Path(args.out_dir).expanduser()
    slug     = args.game

    print()
    print(BOLD(f"  Pokedex.pak CSV Converter"))
    print(DIM( f"  Slug: {slug}"))
    print(DIM( f"  CSV dir: {csv_dir}"))
    if not args.validate_only:
        print(DIM(f"  Output: {out_base / slug}"))
    print()

    # ── Read & validate ──
    try:
        print(CYAN("  Reading game_meta.csv ..."))
        meta = read_game_meta(csv_dir / "game_meta.csv", slug)
        print(OK(f"  ✓ Game meta OK: {meta['name']}"))

        print(CYAN("  Reading pokemon.csv ..."))
        rows = read_pokemon_csv(csv_dir / "pokemon.csv", meta)
        print(OK(f"  ✓ {len(rows)} entries validated"))

    except ValidationError as e:
        print()
        print(ERR(f"  ✗ Validation failed:"))
        print(ERR(f"    {e}"))
        print()
        sys.exit(1)

    print_summary(meta, rows)

    if args.validate_only:
        print()
        print(OK("  ✓ Validation complete — no files written (--validate-only)."))
        print()
        sys.exit(0)

    # ── Write output ──
    out_dir = out_base / slug
    out_dir.mkdir(parents=True, exist_ok=True)

    write_game_conf(out_dir, meta, entry_count=len(rows))
    write_pokemon_tsv(out_dir, rows)

    conf_path = out_dir / "game.conf"
    tsv_path  = out_dir / "pokemon.tsv"
    tsv_kb    = tsv_path.stat().st_size // 1024

    print()
    print(OK("  ✓ Files written:"))
    print(f"    {conf_path}")
    print(f"    {tsv_path}  ({tsv_kb} KB)")
    print()
    print(BOLD("  Next step:"))
    print(f"    Copy  {out_dir}  →  Pokedex.pak/data/games/{slug}/")
    print()


if __name__ == "__main__":
    main()
