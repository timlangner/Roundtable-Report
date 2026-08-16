import re
from pathlib import Path

src = Path(
    r"C:\Users\Tim\.cursor\projects\c-dev-projects-EldenRing-StatsShare\agent-tools\f45a49d5-ddcb-442f-abe8-60a58d3f8518.txt"
).read_text(encoding="utf-8")
pat = re.compile(r'\[Annotation\(Name = "([^"]+)"[^\]]*\)\]\s*\w+ = (\d+)', re.M)
fixes = {
    "Three,Path Cross": "Three-Path Cross",
    "Greatbridge, North": "Greatbridge North",
}
seen: dict[int, str] = {}
for name, id_s in pat.findall(src):
    seen[int(id_s)] = fixes.get(name, name)

out = Path(r"C:\dev\projects\EldenRing_StatsShare\src\game\grace_table.inc")
lines = ["// Generated from SoulMemory Grace.cs. Do not edit by hand.\n"]
for grace_id, name in sorted(seen.items()):
    escaped = name.replace("\\", "\\\\").replace('"', '\\"')
    lines.append(f'    {{{grace_id}, "{escaped}"}},\n')
out.write_text("".join(lines), encoding="utf-8")
print(f"wrote {len(seen)} graces to {out}")
