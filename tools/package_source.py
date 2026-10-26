#!/usr/bin/env python3
"""Create clean source .tar.gz and .zip archives for m-mdy-m/log.c releases."""
from __future__ import annotations
import argparse
import tarfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXCLUDED_DIRS = {".git", "build", "build-cmake", "build-cmake-release", "build-cmake-static", "build-static", "build-shared", "stage", "stage-e2e", "__pycache__", ".pytest_cache", ".mypy_cache", ".ruff_cache", "dist"}
EXCLUDED_NAMES = {"service.log", "m-mdy-m-log-test-output.tmp"}


def included(path: Path) -> bool:
    relative = path.relative_to(ROOT)
    if any(part in EXCLUDED_DIRS or part.startswith("build-cmake-") or part.startswith("stage-e2e") for part in relative.parts):
        return False
    if path.name in EXCLUDED_NAMES or path.suffix == ".pyc":
        return False
    return path.is_file()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", required=True)
    parser.add_argument("--outdir", default="build/packages")
    args = parser.parse_args()
    output = Path(args.outdir).resolve()
    output.mkdir(parents=True, exist_ok=True)
    prefix = f"m-mdy-m-log-v{args.version}"
    paths = sorted(path for path in ROOT.rglob("*") if included(path) and output not in path.parents)
    tar_path = output / f"{prefix}-source.tar.gz"
    zip_path = output / f"{prefix}-source.zip"
    with tarfile.open(tar_path, "w:gz", format=tarfile.PAX_FORMAT) as archive:
        for path in paths:
            archive.add(path, arcname=f"{prefix}/{path.relative_to(ROOT).as_posix()}", recursive=False)
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in paths:
            archive.write(path, f"{prefix}/{path.relative_to(ROOT).as_posix()}")
    print(f"Created {tar_path} ({len(paths)} files)")
    print(f"Created {zip_path} ({len(paths)} files)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
