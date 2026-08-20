#!/usr/bin/env python3
"""One-shot generator for NPC / weapon / talisman name includes."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / "src" / "game"
AGENT = Path(
    r"C:\Users\Tim\.cursor\projects\c-dev-projects-EldenRing-StatsShare\agent-tools"
)

SKIP_SUB = (
    "Dummy",
    "BuddyStone",
    "Bonfire",
    "Talk Dummy",
    "Human",
    "Patrol Dummy",
    "Caravan Dummy",
    "Bullet Dummy",
    "Balloon Dummy",
)


def parse_lines(text: str) -> list[tuple[int, str]]:
    rows: list[tuple[int, str]] = []
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        m = re.match(r"^(\d+)\s+(.+)$", line)
        if not m:
            continue
        rows.append((int(m.group(1)), m.group(2).strip()))
    return rows


def cpp_escape(s: str) -> str:
    return s.replace("\\", "\\\\").replace('"', '\\"')


def strip_parens(name: str) -> str:
    return re.sub(r"\s*\([^)]*\)\s*$", "", name).strip()


def write_table(path: Path, header: str, rows: list[tuple[int, str]]) -> None:
    lines = [header, "{"]
    for i, n in rows:
        lines.append(f'    {{{i}, "{cpp_escape(n)}"}},')
    lines.append("}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    npc_rows = parse_lines(
        (AGENT / "1f9be9a8-23f4-491d-8796-4dd1f60c8f1f.txt").read_text(encoding="utf-8")
    )
    by_id: dict[int, str] = {}
    for i, n in npc_rows:
        if any(x in n for x in SKIP_SUB):
            continue
        cleaned = strip_parens(n)
        if not cleaned:
            continue
        prev = by_id.get(i)
        if prev is None or (len(cleaned) > len(prev) and "Unscaled" not in n):
            by_id[i] = cleaned
    write_table(
        GAME / "npc_name_table.inc",
        "// Generated from soulsmods Paramdex ER/Names/NpcParam.txt",
        sorted(by_id.items()),
    )

    wep_rows = parse_lines(
        (AGENT / "123a21a6-e700-45bc-aa3e-a24c6ee09256.txt").read_text(encoding="utf-8")
    )
    write_table(
        GAME / "weapon_name_table.inc",
        "// Generated from soulsmods Paramdex ER/Names/EquipParamWeapon.txt",
        sorted(wep_rows),
    )

    acc_rows = parse_lines((ROOT / "tools" / "talisman_names.txt").read_text(encoding="utf-8"))
    if not acc_rows:
        raise SystemExit("talisman dump not found")
    write_table(
        GAME / "talisman_name_table.inc",
        "// Generated from soulsmods Paramdex ER/Names/EquipParamAccessory.txt",
        sorted(acc_rows),
    )
    print(f"npc={len(by_id)} weapons={len(wep_rows)} talismans={len(acc_rows)}")


if __name__ == "__main__":
    main()
