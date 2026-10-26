#!/usr/bin/env python3
"""Check local Markdown links without requiring third-party Python packages."""
from __future__ import annotations
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LINK = re.compile(r"(?<!!)\[[^\]]*\]\(([^)]+)\)")
ignored_schemes = ("https://", "http://", "mailto:", "#", "data:")
errors: list[str] = []
for md in sorted(ROOT.rglob("*.md")):
    if ".git" in md.parts or "build" in md.parts:
        continue
    body = md.read_text(encoding="utf-8")
    for target in LINK.findall(body):
        target = target.strip().split()[0].strip("<>")
        if not target or target.startswith(ignored_schemes):
            continue
        target = target.split("#", 1)[0]
        if not target:
            continue
        dest = (md.parent / target).resolve()
        if not dest.exists():
            errors.append(f"{md.relative_to(ROOT)}: missing local link: {target}")
if errors:
    print("Documentation link check: FAIL", file=sys.stderr)
    print("\n".join(errors), file=sys.stderr)
    sys.exit(1)
print("Documentation link check: PASS")
