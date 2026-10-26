#!/usr/bin/env python3
"""Regression tests for ABI export-table parsing across binary formats."""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import check_abi  # noqa: E402


class ExportParserTests(unittest.TestCase):
    def test_gnu_nm(self) -> None:
        output = """
0000000000001100 T mmlog_abi_version
0000000000001200 T mmlog_logger_create
                 U mmlog_not_exported
"""
        self.assertEqual(
            check_abi.parse_nm_symbols(output),
            {"mmlog_abi_version", "mmlog_logger_create"},
        )

    def test_macho_nm_leading_underscores(self) -> None:
        output = "0000000000001100 T _mmlog_abi_version\n"
        self.assertIn("mmlog_abi_version", check_abi.parse_nm_symbols(output))

    def test_gnu_objdump_reads_only_pe_named_export_table(self) -> None:
        output = """
The Export Tables (interpreted .edata section contents)
[Ordinal/Name Pointer] Table
        [   0] mmlog_abi_version
        [   1] mmlog_logger_create
The Function Table (interpreted .pdata section contents)
0000000180001000 0000000180001010 0000000180002000
"""
        self.assertEqual(
            check_abi.parse_objdump_exports(output),
            {"mmlog_abi_version", "mmlog_logger_create"},
        )

    def test_objdump_without_named_exports_returns_empty_set(self) -> None:
        self.assertEqual(check_abi.parse_objdump_exports("The Export Tables\nNo exports\n"), set())

    def test_dumpbin_exports(self) -> None:
        output = """
ordinal hint RVA      name
      1    0 00001000 mmlog_abi_version
      2    1 00001020 mmlog_logger_create
Summary
"""
        self.assertEqual(
            check_abi.parse_dumpbin_exports(output),
            {"mmlog_abi_version", "mmlog_logger_create"},
        )

    def test_best_table_prefers_required_exports_over_auxiliary_symbols(self) -> None:
        auxiliary = {"__head_lib64_libkernel32_a", "__imp_GetLastError"}
        exports = {"mmlog_abi_version", "mmlog_logger_create", "mmlog_logger_write"}
        self.assertEqual(
            check_abi.choose_best_symbol_table([auxiliary, exports]),
            exports,
        )

    def test_best_table_keeps_table_with_most_required_symbols(self) -> None:
        smaller = {"mmlog_abi_version"}
        larger = {"mmlog_abi_version", "mmlog_logger_create"}
        self.assertEqual(
            check_abi.choose_best_symbol_table([smaller, larger]),
            larger,
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
