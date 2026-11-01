"""Wrap src/index.html into src/index_html.h (raw string literal for the firmware).

Usage: python tools/gen_html.py
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src" / "index.html"
DST = ROOT / "src" / "index_html.h"

html = SRC.read_text(encoding="utf-8")
if ")HTML\"" in html:
    raise SystemExit("index.html contains the raw-string terminator )HTML\"")

DST.write_text(
    "#pragma once\n\nstatic constexpr char kIndexHtml[] = R\"HTML(\n" + html + ")HTML\";\n",
    encoding="utf-8",
)
print(f"wrote {DST} ({len(html)} chars)")
