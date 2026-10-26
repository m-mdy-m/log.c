#!/usr/bin/env python3
"""Fail if the built shared library drops a required public ABI symbol."""
from __future__ import annotations
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
# Platform toolchains use slightly different versioned shared-library suffixes
# (especially *BSD); search the active build outputs after exact known names.
for directory in (ROOT / "build-shared", ROOT / "build", ROOT / "build-cmake-shared"):
    if directory.is_dir():
        CANDIDATES.extend(sorted(directory.glob("libm-mdy-m-log.so*")))
        CANDIDATES.extend(sorted(directory.glob("libm-mdy-m-log*.dylib")))
        CANDIDATES.extend(sorted(directory.glob("m-mdy-m-log.dll")))
        CANDIDATES.extend(sorted(directory.glob("libm-mdy-m-log.dll")))
        CANDIDATES.extend(sorted((directory / "Release").glob("m-mdy-m-log.dll")))
        CANDIDATES.extend(sorted((directory / "Release").glob("libm-mdy-m-log.dll")))
lib = next((path for path in CANDIDATES if path.exists() and path.is_file()), None)
if lib is None:
    print("ABI symbol check: no shared library found; run `make shared` first", file=sys.stderr)
    sys.exit(2)
commands = [["nm", "-D", "--defined-only", str(lib)], ["nm", "-gU", str(lib)], ["nm", "-g", str(lib)], ["dumpbin", "/exports", str(lib)]]
result = None
for command in commands:
    try:
        result = subprocess.run(command, check=True, capture_output=True, text=True)
        break
    except (OSError, subprocess.CalledProcessError):
        continue
if result is None:
    print("ABI symbol check: neither nm nor dumpbin could inspect the built library", file=sys.stderr)
    sys.exit(2)
text = result.stdout
missing = sorted(name for name in REQUIRED if name not in text and f"_{name}" not in text)
if missing:
    print(f"ABI symbol check: FAIL ({lib})", file=sys.stderr)
    print("Missing exports: " + ", ".join(missing), file=sys.stderr)
    sys.exit(1)
print(f"ABI symbol check: PASS ({lib.name}; {len(REQUIRED)} required symbols)")
