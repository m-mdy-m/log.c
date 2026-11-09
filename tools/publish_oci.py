#!/usr/bin/env python3
"""Publish the native SDK archives from one release as verified GHCR OCI packages."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

PACKAGE_NAME = "m-mdy-m-log"
COMPONENT_ID = "m-mdy-m-log"
SOURCE_REPOSITORY = "https://github.com/m-mdy-m/log.c"
TAR_MEDIA_TYPE = "application/vnd.pipeline.package.layer.v1.tar+gzip"
ZIP_MEDIA_TYPE = "application/zip"


@dataclass(frozen=True)
class Target:
    id: str
    os: str
    architecture: str
    abi: str
    archive_format: str

    @property
    def media_type(self) -> str:
        return ZIP_MEDIA_TYPE if self.archive_format == "zip" else TAR_MEDIA_TYPE

    def archive_name(self, version: str) -> str:
        return f"m-mdy-m-log-v{version}-{self.id}.{self.archive_format}"


TARGETS = (
    Target("linux-x86_64", "linux", "x86_64", "gnu", "tar.gz"),
    Target("linux-aarch64", "linux", "aarch64", "gnu", "tar.gz"),
    Target("macos-x86_64", "macos", "x86_64", "darwin", "tar.gz"),
    Target("macos-arm64", "macos", "arm64", "darwin", "tar.gz"),
    Target("windows-x86_64-ucrt64", "windows", "x86_64", "ucrt64", "zip"),
    Target("freebsd-x86_64", "freebsd", "x86_64", "freebsd", "tar.gz"),
    Target("openbsd-x86_64", "openbsd", "x86_64", "openbsd", "tar.gz"),
)


class PublishError(RuntimeError):
    """Raised when an SDK archive cannot be safely published and verified."""


def validate_version(value: str) -> str:
    pattern = (
        r"(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)"
        r"(?:-[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?"
        r"(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?"
    )
    if not re.fullmatch(pattern, value):
        raise PublishError(f"invalid package version: {value!r}")
    return value


def resolve_archives(directory: str | Path, version: str) -> dict[Target, Path]:
    root = Path(directory).expanduser()
    if root.is_symlink() or not root.is_dir():
        raise PublishError(f"release asset directory is missing or unsafe: {root}")
    root = root.absolute()
    result: dict[Target, Path] = {}
    for target in TARGETS:
        path = root / target.archive_name(version)
        if path.is_symlink() or not path.is_file():
            raise PublishError(f"missing or unsafe {target.id} archive: {path.name}")
        if path.stat().st_size <= 0:
            raise PublishError(f"empty {target.id} archive: {path.name}")
        result[target] = path
    return result


def _run(command: list[str], *, cwd: Path | None = None) -> str:
    try:
        result = subprocess.run(
            command, cwd=cwd, check=False, capture_output=True, text=True,
            encoding="utf-8", errors="replace",
        )
    except OSError as exc:
        raise PublishError(f"could not start {command[0]!r}: {exc}") from exc
    if result.returncode != 0:
        details = (result.stderr or result.stdout).strip()
        raise PublishError(
            f"command failed with exit code {result.returncode}: {' '.join(command)}\n{details}"
        )
    return result.stdout.strip()


def _pipeline_json(*arguments: str) -> dict[str, Any]:
    output = _run([sys.executable, "-m", "pipeline.oci.cli", *arguments])
    try:
        value = json.loads(output)
    except json.JSONDecodeError as exc:
        raise PublishError(f"Pipeline OCI helper returned invalid JSON: {exc}") from exc
    if not isinstance(value, dict):
        raise PublishError("Pipeline OCI helper must return a JSON object")
    return value


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def publish_target(
    *, target: Target, artifact: Path, version: str, revision: str,
    owner: str, output_root: Path,
) -> dict[str, str]:
    target_root = output_root / ".work" / target.id
    metadata_dir = target_root / "metadata"
    stage_dir = target_root / "staged"
    roundtrip_dir = target_root / "roundtrip"
    evidence_dir = output_root / "evidence" / target.id
    for directory in (metadata_dir, stage_dir, roundtrip_dir):
        directory.mkdir(parents=True, exist_ok=False)

    prepared = _pipeline_json(
        "prepare",
        "--artifact", str(artifact),
        "--output-directory", str(metadata_dir),
        "--package-name", PACKAGE_NAME,
        "--component-id", COMPONENT_ID,
        "--variant", "default",
        "--version", version,
        "--source-repository", SOURCE_REPOSITORY,
        "--source-revision", revision,
        "--target-id", target.id,
        "--target-os", target.os,
        "--architecture", target.architecture,
        "--abi", target.abi,
        "--media-type", target.media_type,
    )
    metadata_path = Path(str(prepared["metadata_path"]))
    _pipeline_json("verify", "--metadata", str(metadata_path), "--artifact", str(artifact))

    staged = _pipeline_json(
        "stage",
        "--metadata", str(metadata_path),
        "--artifact", str(artifact),
        "--output-directory", str(stage_dir),
        "--expected-metadata-sha256", str(prepared["metadata_sha256"]),
    )
    staged_artifact = Path(str(staged["artifact_path"]))
    if staged_artifact.name != artifact.name:
        raise PublishError(f"staged archive name changed for {target.id}")
    staged_metadata = stage_dir / "package-metadata.json"
    shutil.copyfile(metadata_path, staged_metadata)

    package_tag = str(prepared["tag"])
    tagged_reference = f"ghcr.io/{owner}/{PACKAGE_NAME}:{package_tag}"
    push_output = _run(
        [
            "oras", "push", tagged_reference,
            "--artifact-type", "application/vnd.pipeline.package.v1",
            "--config", f"{staged_metadata.name}:application/vnd.pipeline.package.config.v1+json",
            "--annotation", f"org.opencontainers.image.source={SOURCE_REPOSITORY}",
            "--annotation", f"org.opencontainers.image.revision={revision}",
            "--annotation", f"org.opencontainers.image.version={version}",
            "--annotation", f"org.opencontainers.image.title={PACKAGE_NAME}",
            f"{staged_artifact.name}:{target.media_type}",
            "--format", "json",
        ],
        cwd=stage_dir,
    )
    try:
        push_result = json.loads(push_output)
    except json.JSONDecodeError as exc:
        raise PublishError(f"ORAS returned invalid JSON for {target.id}: {exc}") from exc
    push_result_path = target_root / "push-result.json"
    push_result_path.write_text(json.dumps(push_result, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    receipt = _pipeline_json(
        "receipt", "--metadata", str(metadata_path),
        "--push-result", str(push_result_path), "--registry", "ghcr.io", "--owner", owner,
    )
    receipt_path = target_root / "publication-receipt.json"
    receipt_path.write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    try:
        immutable_reference = str(receipt["oci"]["immutable_reference"])
        digest = str(receipt["oci"]["digest"])
    except (KeyError, TypeError) as exc:
        raise PublishError(f"Pipeline did not return an immutable receipt for {target.id}") from exc

    _run(["oras", "pull", immutable_reference, "--output", str(roundtrip_dir)])
    pulled_artifact = roundtrip_dir / artifact.name
    _pipeline_json("verify", "--metadata", str(metadata_path), "--artifact", str(pulled_artifact))

    evidence_dir.mkdir(parents=True, exist_ok=False)
    evidence_files = [
        (f"published-{artifact.name}", staged_artifact),
        ("package-metadata.json", metadata_path),
        ("push-result.json", push_result_path),
        ("publication-receipt.json", receipt_path),
        (f"roundtrip-{artifact.name}", pulled_artifact),
    ]
    for name, source in evidence_files:
        if source.is_symlink() or not source.is_file():
            raise PublishError(f"OCI evidence source is missing or unsafe: {source}")
        shutil.copyfile(source, evidence_dir / name)
    checksums = [f"{_sha256(evidence_dir / name)}  {name}" for name, _ in evidence_files]
    (evidence_dir / "SHA256SUMS").write_text("\n".join(checksums) + "\n", encoding="utf-8")
    shutil.rmtree(target_root)

    return {
        "target": target.id,
        "tag": tagged_reference,
        "immutable_reference": immutable_reference,
        "digest": digest,
        "archive": artifact.name,
        "sha256": _sha256(artifact),
        "media_type": target.media_type,
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Publish the seven native log.c SDK archives to GHCR as OCI packages."
    )
    parser.add_argument("--assets-directory", default="release-assets")
    parser.add_argument("--version", required=True, help="plain SemVer, without the leading v")
    parser.add_argument("--source-revision", required=True, help="full Git commit SHA")
    parser.add_argument("--owner", required=True, help="lowercase GitHub Container Registry owner")
    parser.add_argument("--package-name", default=PACKAGE_NAME)
    parser.add_argument("--output-directory", default="oci-evidence")
    args = parser.parse_args()

    try:
        version = validate_version(args.version)
        if args.package_name != PACKAGE_NAME:
            raise PublishError(f"package-name is fixed for this project: {PACKAGE_NAME}")
        if not re.fullmatch(r"[0-9a-f]{40}|[0-9a-f]{64}", args.source_revision):
            raise PublishError("source-revision must be a full lowercase Git commit SHA")
        owner = args.owner.lower()
        if not re.fullmatch(r"[a-z0-9][a-z0-9-]{0,38}", owner):
            raise PublishError("owner is not a valid lowercase GHCR namespace")

        artifacts = resolve_archives(args.assets_directory, version)
        output_root = Path(args.output_directory).expanduser()
        if output_root.is_symlink() or output_root.exists():
            raise PublishError(f"refusing to overwrite existing evidence directory: {output_root}")
        output_root = output_root.absolute()
        output_root.mkdir(parents=True, exist_ok=False)

        records = [
            publish_target(
                target=target, artifact=artifacts[target], version=version,
                revision=args.source_revision, owner=owner, output_root=output_root,
            )
            for target in TARGETS
        ]
        shutil.rmtree(output_root / ".work")
        markdown = [
            f"# GHCR publication — m-mdy-m/log.c {version}", "",
            "| Target | OCI tag | Immutable digest | Media type |",
            "| --- | --- | --- | --- |",
        ]
        markdown.extend(
            f"| {row['target']} | {row['tag']} | {row['digest']} | {row['media_type']} |"
            for row in records
        )
        (output_root / "OCI-PUBLICATIONS.md").write_text(
            "\n".join(markdown) + "\n", encoding="utf-8"
        )
        print(json.dumps({"version": version, "published": records}, indent=2, sort_keys=True))
        return 0
    except (OSError, KeyError, PublishError) as exc:
        print(f"GHCR publication failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
