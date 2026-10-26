#!/usr/bin/env python3
"""Fail if the built shared library drops a required public ABI symbol.

ELF and Mach-O libraries are inspected with ``nm``. Windows PE DLLs are
inspected through the export table using GNU ``objdump`` (MinGW) or
Microsoft ``dumpbin``. A successful tool invocation is not considered enough:
the output must contain at least one parsed symbol before it is accepted.
"""
from __future__ import annotations

import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CANDIDATES = [
    ROOT / "build/lib/libm-mdy-m-log.so.0.1.0",
    ROOT / "build/lib/libm-mdy-m-log.dylib",
    ROOT / "build/lib/libm-mdy-m-log.1.dylib",
    ROOT / "build-cmake-shared/libm-mdy-m-log.so",
    ROOT / "build-shared/libm-mdy-m-log.so",
    ROOT / "build-shared/libm-mdy-m-log.so.1",
    ROOT / "build-shared/libm-mdy-m-log.so.0.1.0",
    ROOT / "build-shared/libm-mdy-m-log.dylib",
    ROOT / "build-shared/libm-mdy-m-log.1.dylib",
    ROOT / "build-shared/m-mdy-m-log.dll",
    ROOT / "build-shared/libm-mdy-m-log.dll",
    ROOT / "build-shared/Release/m-mdy-m-log.dll",
    ROOT / "build-shared/Release/libm-mdy-m-log.dll",
]
REQUIRED = {
    "mmlog_abi_version", "mmlog_level_name", "mmlog_status_name",
    "mmlog_logger_config_init", "mmlog_fields_init", "mmlog_event_init",
    "mmlog_logger_create", "mmlog_logger_destroy", "mmlog_logger_set_min_level",
    "mmlog_logger_set_color_mode", "mmlog_logger_set_quiet", "mmlog_logger_add_file",
    "mmlog_logger_add_callback", "mmlog_logger_flush", "mmlog_logger_write", "mmlog_logf",
}


def normalize_symbol(name: str) -> str:
    """Normalize common C symbol spelling differences (notably Mach-O)."""
    name = name.strip()
    while name.startswith("_"):
        name = name[1:]
    return name


def parse_nm_symbols(output: str) -> set[str]:
    """Read symbol names from GNU/BSD/macOS ``nm`` output."""
    symbols: set[str] = set()
    for line in output.splitlines():
        fields = line.split()
        # Typical nm rows are: ADDRESS TYPE NAME; undefined imports are often
        # just "                 U symbol" and must not satisfy an export check.
        if len(fields) < 3:
            continue
        kind = fields[-2]
        candidate = fields[-1]
        if kind.upper() == "U" or candidate.endswith(":"):
            continue
        normalized = normalize_symbol(candidate)
        if normalized:
            symbols.add(normalized)
    return symbols


def parse_objdump_exports(output: str) -> set[str]:
    """Read named exports from GNU objdump's PE DLL export table only."""
    marker = "[Ordinal/Name Pointer] Table"
    start = output.find(marker)
    if start < 0:
        return set()

    symbols: set[str] = set()
    for line in output[start + len(marker):].splitlines():
        # The named export table is followed by another PE directory heading.
        if line.startswith("The ") or line.startswith("There is "):
            if symbols:
                break
        match = re.match(r"^\s*\[\s*\d+\]\s+(\S+)\s*$", line)
        if match:
            symbols.add(normalize_symbol(match.group(1)))
    return symbols


def parse_dumpbin_exports(output: str) -> set[str]:
    """Read named exports from Microsoft ``dumpbin /exports`` output."""
    symbols: set[str] = set()
    in_table = False
    for line in output.splitlines():
        if re.search(r"\bordinal\s+hint\s+RVA\s+name\b", line, re.IGNORECASE):
            in_table = True
            continue
        if not in_table:
            continue
        match = re.match(
            r"^\s*\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(\S+)\s*$", line
        )
        if match:
            symbols.add(normalize_symbol(match.group(1)))
        elif symbols and line.strip():
            break
    return symbols


def _run(command: list[str]) -> subprocess.CompletedProcess[str] | None:
    try:
        return subprocess.run(command, check=True, capture_output=True, text=True)
    except (OSError, subprocess.CalledProcessError):
        return None


def choose_best_symbol_table(tables: list[set[str]]) -> set[str]:
    """Choose the parsed table that names the most required public symbols.

    Some PE tool versions emit auxiliary symbol tables before the actual DLL
    export table. Do not stop at the first non-empty result: prefer the output
    that best matches the library's required ABI, while still allowing the
    caller to report a useful missing-symbol list when no tool finds all of it.
    """
    if not tables:
        return set()
    return max(tables, key=lambda table: len(REQUIRED.intersection(table)))


def exported_symbols(library: Path) -> set[str]:
    """Inspect a library with format-appropriate tools and choose best output."""
    if library.suffix.lower() == ".dll":
        attempts = [
            ("objdump", ["objdump", "-p", str(library)], parse_objdump_exports),
            ("dumpbin", ["dumpbin", "/exports", str(library)], parse_dumpbin_exports),
            ("nm", ["nm", "-g", str(library)], parse_nm_symbols),
        ]
    else:
        attempts = [
            ("nm", ["nm", "-D", "--defined-only", str(library)], parse_nm_symbols),
            ("nm", ["nm", "-gU", str(library)], parse_nm_symbols),
            ("nm", ["nm", "-g", str(library)], parse_nm_symbols),
        ]

    errors: list[str] = []
    parsed_tables: list[set[str]] = []
    for label, command, parser in attempts:
        if shutil.which(command[0]) is None:
            errors.append(f"{label}: executable not found")
            continue
        result = _run(command)
        if result is None:
            errors.append(f"{label}: command failed")
            continue
        symbols = parser(result.stdout)
        if not symbols:
            errors.append(f"{label}: output contained no parseable symbols")
            continue
        parsed_tables.append(symbols)
        if REQUIRED.issubset(symbols):
            return symbols

    if parsed_tables:
        return choose_best_symbol_table(parsed_tables)
    raise RuntimeError("; ".join(errors))


def main() -> int:
    candidates = list(CANDIDATES)
    # Include common versioned suffixes produced by BSD/macOS and build dirs.
    for directory in (ROOT / "build-shared", ROOT / "build", ROOT / "build-cmake-shared"):
        if directory.is_dir():
            candidates.extend(sorted(directory.glob("libm-mdy-m-log.so*")))
            candidates.extend(sorted(directory.glob("libm-mdy-m-log*.dylib")))
            candidates.extend(sorted(directory.glob("m-mdy-m-log.dll")))
            candidates.extend(sorted(directory.glob("libm-mdy-m-log.dll")))
            candidates.extend(sorted((directory / "Release").glob("m-mdy-m-log.dll")))
            candidates.extend(sorted((directory / "Release").glob("libm-mdy-m-log.dll")))

    library = next((path for path in candidates if path.exists() and path.is_file()), None)
    if library is None:
        print("ABI symbol check: no shared library found; build the shared target first", file=sys.stderr)
        return 2

    try:
        symbols = exported_symbols(library)
    except RuntimeError as exc:
        print(f"ABI symbol check: could not inspect {library}: {exc}", file=sys.stderr)
        return 2

    missing = sorted(REQUIRED - symbols)
    if missing:
        print(f"ABI symbol check: FAIL ({library})", file=sys.stderr)
        print("Missing exports: " + ", ".join(missing), file=sys.stderr)
        return 1

    print(f"ABI symbol check: PASS ({library.name}; {len(REQUIRED)} required symbols)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
