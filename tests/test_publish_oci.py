#!/usr/bin/env python3
"""Unit tests for the log.c OCI target inventory and archive selection."""
from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from publish_oci import (  # noqa: E402
    PACKAGE_NAME,
    TARGETS,
    ZIP_MEDIA_TYPE,
    TAR_MEDIA_TYPE,
    PublishError,
    resolve_archives,
    validate_version,
)


class OciTargetInventoryTests(unittest.TestCase):
    def test_package_identity_is_project_specific(self) -> None:
        self.assertEqual(PACKAGE_NAME, "m-mdy-m-log")

    def test_inventory_matches_all_native_release_targets(self) -> None:
        self.assertEqual(
            [target.id for target in TARGETS],
            [
                "linux-x86_64",
                "linux-aarch64",
                "macos-x86_64",
                "macos-arm64",
                "windows-x86_64-ucrt64",
                "freebsd-x86_64",
                "openbsd-x86_64",
            ],
        )

    def test_windows_uses_zip_media_type_and_unix_targets_use_tar_gzip(self) -> None:
        for target in TARGETS:
            expected = ZIP_MEDIA_TYPE if target.id.startswith("windows-") else TAR_MEDIA_TYPE
            self.assertEqual(target.media_type, expected)

    def test_archive_selection_requires_every_platform(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for target in TARGETS:
                (root / target.archive_name("0.1.1")).write_bytes(b"native sdk")
            result = resolve_archives(root, "0.1.1")
            self.assertEqual(len(result), 7)
            self.assertTrue(all(path.is_file() for path in result.values()))

    def test_archive_selection_rejects_a_missing_platform(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for target in TARGETS[:-1]:
                (root / target.archive_name("0.1.1")).write_bytes(b"native sdk")
            with self.assertRaisesRegex(PublishError, "openbsd-x86_64"):
                resolve_archives(root, "0.1.1")

    def test_archive_selection_rejects_empty_assets(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for target in TARGETS:
                (root / target.archive_name("0.1.1")).write_bytes(b"native sdk")
            (root / TARGETS[-1].archive_name("0.1.1")).write_bytes(b"")
            with self.assertRaisesRegex(PublishError, "empty openbsd-x86_64"):
                resolve_archives(root, "0.1.1")

    def test_archive_selection_rejects_symlink_assets(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for target in TARGETS:
                (root / target.archive_name("0.1.1")).write_bytes(b"native sdk")
            linked = root / TARGETS[-1].archive_name("0.1.1")
            linked.unlink()
            try:
                linked.symlink_to(root / TARGETS[0].archive_name("0.1.1"))
            except (OSError, NotImplementedError):
                self.skipTest("symbolic links unavailable")
            with self.assertRaisesRegex(PublishError, "missing or unsafe openbsd-x86_64"):
                resolve_archives(root, "0.1.1")

    def test_version_validator_accepts_release_and_prerelease_versions(self) -> None:
        self.assertEqual(validate_version("0.1.1"), "0.1.1")
        self.assertEqual(validate_version("0.2.0-rc.1"), "0.2.0-rc.1")

    def test_version_validator_rejects_tag_prefix_and_malformed_versions(self) -> None:
        for value in ("v0.1.1", "latest", "0.1", "0.1.1-rc", "01.1.1"):
            with self.subTest(value=value):
                with self.assertRaises(PublishError):
                    validate_version(value)


if __name__ == "__main__":
    unittest.main(verbosity=2)
