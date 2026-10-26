#!/usr/bin/env python3
"""Create a platform SDK archive from a CMake install prefix."""
from __future__ import annotations
import argparse
import shutil
import tarfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COPY_ROOT_FILES = ("README.md", "LICENSE", "NOTICE", "CHANGELOG.md")


def copy_if_exists(source: Path, destination: Path) -> None:
    if source.is_dir():
        shutil.copytree(source, destination, dirs_exist_ok=True, ignore=shutil.ignore_patterns("__pycache__", "*.pyc", "build", "build-cmake"))
    elif source.is_file():
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--prefix", required=True, help="prefix populated by `cmake --install`")
    parser.add_argument("--version", required=True)
    parser.add_argument("--platform", required=True, help="e.g. linux-x86_64 or windows-x86_64-ucrt64")
    parser.add_argument("--format", choices=("tar.gz", "zip"), required=True)
    parser.add_argument("--outdir", default="dist")
    args = parser.parse_args()
    installed = Path(args.prefix).resolve()
    if not (installed / "include/m-mdy-m/log.h").is_file():
        parser.error(f"install prefix does not contain include/m-mdy-m/log.h: {installed}")
    if not (installed / "lib/libm-mdy-m-log.a").is_file():
        parser.error(f"install prefix does not contain lib/libm-mdy-m-log.a: {installed}")
    output = Path(args.outdir).resolve()
    output.mkdir(parents=True, exist_ok=True)
    archive_name = f"m-mdy-m-log-v{args.version}-{args.platform}.{args.format}"
    top = f"m-mdy-m-log-v{args.version}-{args.platform}"
    package_root = output / top
    if package_root.exists():
        shutil.rmtree(package_root)
    package_root.mkdir(parents=True)

    for subdir in ("include", "lib", "bin", "share"):
        copy_if_exists(installed / subdir, package_root / subdir)
    for name in COPY_ROOT_FILES:
        copy_if_exists(ROOT / name, package_root / name)
    copy_if_exists(ROOT / "docs", package_root / "docs")
    copy_if_exists(ROOT / "examples", package_root / "examples")
    copy_if_exists(ROOT / "packaging/RELEASE-INSTALL.md", package_root / "INSTALL.md")
    if args.platform.startswith("windows-"):
        copy_if_exists(ROOT / "packaging/install.ps1", package_root / "install.ps1")
    else:
        copy_if_exists(ROOT / "packaging/install.sh", package_root / "install.sh")
        (package_root / "install.sh").chmod(0o755)

    header = (installed / "include/m-mdy-m/log.h").read_text(encoding="utf-8")
    abi_major = abi_minor = "unknown"
    for line in header.splitlines():
        if line.startswith("#define MMLLOG_ABI_MAJOR "):
            abi_major = line.split()[-1].rstrip("uU")
        elif line.startswith("#define MMLLOG_ABI_MINOR "):
            abi_minor = line.split()[-1].rstrip("uU")
    (package_root / "BUILD-INFO.txt").write_text(
        f"Project: m-mdy-m/log.c\nPackage version: {args.version}\nC ABI version: {abi_major}.{abi_minor}\nTarget: {args.platform}\nFormat: {args.format}\n",
        encoding="utf-8",
    )

    archive_path = output / archive_name
    if args.format == "tar.gz":
        with tarfile.open(archive_path, "w:gz", format=tarfile.PAX_FORMAT) as archive:
            archive.add(package_root, arcname=top, recursive=True)
    else:
        with zipfile.ZipFile(archive_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for path in sorted(package_root.rglob("*")):
                archive.write(path, f"{top}/{path.relative_to(package_root).as_posix()}")
    # Keep the output directory release-ready: only publishable archives belong in it.
    shutil.rmtree(package_root)
    print(f"Created {archive_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
