#!/usr/bin/env python3
"""Create the local Linux x86-64 release SDK archive from a staged install."""
from __future__ import annotations
import argparse
import platform
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def run(*args: str) -> None:
    subprocess.run(args, cwd=ROOT, check=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", required=True)
    parser.add_argument("--outdir", default="build/packages")
    args = parser.parse_args()
    if platform.system() != "Linux":
        parser.error("this local bundle target only supports Linux; use the GitHub release matrix for other platforms")
    if platform.machine().lower() not in {"x86_64", "amd64"}:
        parser.error("this local bundle target is for Linux x86-64")

    output = Path(args.outdir).resolve()
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="m-mdy-m-log-stage-") as temp:
        stage = Path(temp) / "stage"
        run("make", "all")
        run("make", "install", "PREFIX=/usr", f"DESTDIR={stage}")
        run("python3", "tools/package_binary.py", "--prefix", str(stage / "usr"),
            "--version", args.version, "--platform", "linux-x86_64", "--format", "tar.gz",
            "--outdir", str(output))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
