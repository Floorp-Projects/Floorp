# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
import unittest
from os import path

import mozunit
from test_histogramtools_non_strict import load_histogram

TELEMETRY_ROOT_PATH = path.abspath(
    path.join(path.dirname(__file__), path.pardir, path.pardir)
)
sys.path.append(TELEMETRY_ROOT_PATH)
# The parsers live in a subdirectory of "build_scripts", account for that.
# NOTE: if the parsers are moved, this logic will need to be updated.
sys.path.append(path.join(TELEMETRY_ROOT_PATH, "build_scripts"))
from mozparsers import parse_histograms
from mozparsers.shared_telemetry_utils import ParserError


class TestParser(unittest.TestCase):
    def setUp(self):
        def mockexit(x):
            raise SystemExit(x)

        self.oldexit = os._exit
        os._exit = mockexit

    def tearDown(self):
        os._exit = self.oldexit

    def test_valid_histogram(self):
        SAMPLE_HISTOGRAM = {
            "TEST_VALID_HISTOGRAM": {
                "record_in_processes": ["main", "content", "socket", "utility"],
                "alert_emails": ["team@mozilla.xyz"],
                "bug_numbers": [1383793],
                "expires_in_version": "never",
                "kind": "boolean",
                "products": ["firefox"],
                "description": "Test histogram",
            }
        }
        histograms = load_histogram(SAMPLE_HISTOGRAM)
        parse_histograms.load_allowlist()

        hist = parse_histograms.Histogram(
            "TEST_VALID_HISTOGRAM",
            histograms["TEST_VALID_HISTOGRAM"],
            strict_type_checks=True,
        )

        ParserError.exit_func()
        self.assertTrue(hist.expiration(), "never")
        self.assertTrue(hist.kind(), "boolean")
        self.assertTrue(hist.record_in_processes, ["main", "content"])
        self.assertTrue(hist.record_into_store, ["main"])

    def test_missing_bug_numbers(self):
        SAMPLE_HISTOGRAM = {
            "TEST_HISTOGRAM_ALLOWLIST_BUG_NUMBERS": {
                "record_in_processes": ["main", "content"],
                "alert_emails": ["team@mozilla.xyz"],
                "expires_in_version": "never",
                "kind": "boolean",
                "products": ["firefox"],
                "description": "Test histogram",
            }
        }
        histograms = load_histogram(SAMPLE_HISTOGRAM)
        parse_histograms.load_allowlist()

        parse_histograms.Histogram(
            "TEST_HISTOGRAM_ALLOWLIST_BUG_NUMBERS",
            histograms["TEST_HISTOGRAM_ALLOWLIST_BUG_NUMBERS"],
            strict_type_checks=True,
        )

        self.assertRaises(SystemExit, ParserError.exit_func)

        # Set global allowlists for parse_histograms.
        parse_histograms.allowlists = {
            "alert_emails": [],
            "bug_numbers": ["TEST_HISTOGRAM_ALLOWLIST_BUG_NUMBERS"],
            "n_buckets": [],
            "expiry_default": [],
            "kind": [],
        }

        hist = parse_histograms.Histogram(
            "TEST_HISTOGRAM_ALLOWLIST_BUG_NUMBERS",
            histograms["TEST_HISTOGRAM_ALLOWLIST_BUG_NUMBERS"],
            strict_type_checks=True,
        )

        ParserError.exit_func()
        self.assertEqual(hist.expiration(), "never")
        self.assertEqual(hist.kind(), "boolean")
        self.assertEqual(hist.record_in_processes(), ["main", "content"])
        self.assertEqual(hist.keyed(), False)

        parse_histograms.allowlists = None

    def test_missing_alert_emails(self):
        SAMPLE_HISTOGRAM = {
            "TEST_HISTOGRAM_ALLOWLIST_ALERT_EMAILS": {
                "record_in_processes": ["main", "content"],
                "bug_numbers": [1383793],
                "expires_in_version": "never",
                "kind": "boolean",
                "products": ["firefox"],
                "description": "Test histogram",
            }
        }
        histograms = load_histogram(SAMPLE_HISTOGRAM)
        parse_histograms.load_allowlist()

        parse_histograms.Histogram(
            "TEST_HISTOGRAM_ALLOWLIST_ALERT_EMAILS",
            histograms["TEST_HISTOGRAM_ALLOWLIST_ALERT_EMAILS"],
            strict_type_checks=True,
        )

        self.assertRaises(SystemExit, ParserError.exit_func)

        # Set global allowlists for parse_histograms.
        parse_histograms.allowlists = {
            "alert_emails": ["TEST_HISTOGRAM_ALLOWLIST_ALERT_EMAILS"],
            "bug_numbers": [],
            "n_buckets": [],
            "expiry_default": [],
            "kind": [],
        }

        hist = parse_histograms.Histogram(
            "TEST_HISTOGRAM_ALLOWLIST_ALERT_EMAILS",
            histograms["TEST_HISTOGRAM_ALLOWLIST_ALERT_EMAILS"],
            strict_type_checks=True,
        )

        ParserError.exit_func()
        self.assertEqual(hist.expiration(), "never")
        self.assertEqual(hist.kind(), "boolean")
        self.assertEqual(hist.record_in_processes(), ["main", "content"])
        self.assertEqual(hist.keyed(), False)

        parse_histograms.allowlists = None

    def test_high_value(self):
        SAMPLE_HISTOGRAM = {
            "TEST_HISTOGRAM_ALLOWLIST_N_BUCKETS": {
                "record_in_processes": ["main", "content"],
                "alert_emails": ["team@mozilla.xyz"],
                "bug_numbers": [1383793],
                "expires_in_version": "never",
                "kind": "exponential",
                "low": 1024,
                "high": 2**64,
                "n_buckets": 100,
                "products": ["firefox"],
                "description": "Test histogram",
            }
        }
        histograms = load_histogram(SAMPLE_HISTOGRAM)
        parse_histograms.load_allowlist()

        parse_histograms.Histogram(
            "TEST_HISTOGRAM_ALLOWLIST_N_BUCKETS",
            histograms["TEST_HISTOGRAM_ALLOWLIST_N_BUCKETS"],
            strict_type_checks=True,
        )

        self.assertRaises(SystemExit, ParserError.exit_func)

    def test_high_n_buckets(self):
        SAMPLE_HISTOGRAM = {
            "TEST_HISTOGRAM_ALLOWLIST_N_BUCKETS": {
                "record_in_processes": ["main", "content"],
                "alert_emails": ["team@mozilla.xyz"],
                "bug_numbers": [1383793],
                "expires_in_version": "never",
                "kind": "exponential",
                "low": 1024,
                "high": 16777216,
                "n_buckets": 200,
                "products": ["firefox"],
                "description": "Test histogram",
            }
        }
        histograms = load_histogram(SAMPLE_HISTOGRAM)
        parse_histograms.load_allowlist()

        parse_histograms.Histogram(
            "TEST_HISTOGRAM_ALLOWLIST_N_BUCKETS",
            histograms["TEST_HISTOGRAM_ALLOWLIST_N_BUCKETS"],
            strict_type_checks=True,
        )

        self.assertRaises(SystemExit, ParserError.exit_func)

        # Set global allowlists for parse_histograms.
        parse_histograms.allowlists = {
            "alert_emails": [],
            "bug_numbers": [],
            "n_buckets": ["TEST_HISTOGRAM_ALLOWLIST_N_BUCKETS"],
            "expiry_default": [],
            "kind": [],
        }

        hist = parse_histograms.Histogram(
            "TEST_HISTOGRAM_ALLOWLIST_N_BUCKETS",
            histograms["TEST_HISTOGRAM_ALLOWLIST_N_BUCKETS"],
            strict_type_checks=True,
        )

        ParserError.exit_func()
        self.assertEqual(hist.expiration(), "never")
        self.assertEqual(hist.kind(), "exponential")
        self.assertEqual(hist.record_in_processes(), ["main", "content"])
        self.assertEqual(hist.keyed(), False)
        self.assertEqual(hist.low(), 1024)
        self.assertEqual(hist.high(), 16777216)
        self.assertEqual(hist.n_buckets(), 200)

        parse_histograms.allowlists = None

    def test_expiry_default(self):
        SAMPLE_HISTOGRAM = {
            "TEST_HISTOGRAM_ALLOWLIST_EXPIRY_DEFAULT": {
                "record_in_processes": ["main", "content"],
                "expires_in_version": "default",
                "alert_emails": ["team@mozilla.xyz"],
                "bug_numbers": [1383793],
                "kind": "boolean",
                "products": ["firefox"],
                "description": "Test histogram",
            }
        }
        histograms = load_histogram(SAMPLE_HISTOGRAM)
        parse_histograms.load_allowlist()

        parse_histograms.Histogram(
            "TEST_HISTOGRAM_ALLOWLIST_EXPIRY_DEFAULT",
            histograms["TEST_HISTOGRAM_ALLOWLIST_EXPIRY_DEFAULT"],
            strict_type_checks=True,
        )

        self.assertRaises(SystemExit, ParserError.exit_func)

        # Set global allowlists for parse_histograms.
        parse_histograms.allowlists = {
            "alert_emails": [],
            "bug_numbers": [],
            "n_buckets": [],
            "expiry_default": ["TEST_HISTOGRAM_ALLOWLIST_EXPIRY_DEFAULT"],
            "kind": [],
        }

        hist = parse_histograms.Histogram(
            "TEST_HISTOGRAM_ALLOWLIST_EXPIRY_DEFAULT",
            histograms["TEST_HISTOGRAM_ALLOWLIST_EXPIRY_DEFAULT"],
            strict_type_checks=True,
        )

        ParserError.exit_func()
        self.assertEqual(hist.expiration(), "default")
        self.assertEqual(hist.kind(), "boolean")
        self.assertEqual(hist.record_in_processes(), ["main", "content"])
        self.assertEqual(hist.keyed(), False)

        parse_histograms.allowlists = None

    def test_unsupported_kind_count(self):
        SAMPLE_HISTOGRAM = {
            "TEST_HISTOGRAM_ALLOWLIST_KIND": {
                "record_in_processes": ["main", "content"],
                "expires_in_version": "never",
                "kind": "count",
                "releaseChannelCollection": "opt-out",
                "alert_emails": ["team@mozilla.xyz"],
                "bug_numbers": [1383793],
                "products": ["firefox"],
                "description": "Test histogram",
            }
        }
        histograms = load_histogram(SAMPLE_HISTOGRAM)
        parse_histograms.load_allowlist()

        self.assertRaises(
            SystemExit,
            parse_histograms.Histogram,
            "TEST_HISTOGRAM_ALLOWLIST_KIND",
            histograms["TEST_HISTOGRAM_ALLOWLIST_KIND"],
            strict_type_checks=True,
        )

        # Set global allowlists for parse_histograms.
        parse_histograms.allowlists = {
            "alert_emails": [],
            "bug_numbers": [],
            "n_buckets": [],
            "expiry_default": [],
            "kind": ["TEST_HISTOGRAM_ALLOWLIST_KIND"],
        }

        hist = parse_histograms.Histogram(
            "TEST_HISTOGRAM_ALLOWLIST_KIND",
            histograms["TEST_HISTOGRAM_ALLOWLIST_KIND"],
            strict_type_checks=True,
        )

        ParserError.exit_func()
        self.assertEqual(hist.expiration(), "never")
        self.assertEqual(hist.kind(), "count")
        self.assertEqual(hist.record_in_processes(), ["main", "content"])
        self.assertEqual(hist.keyed(), False)

        parse_histograms.allowlists = None

    def test_unsupported_kind_flag(self):
        SAMPLE_HISTOGRAM = {
            "TEST_HISTOGRAM_ALLOWLIST_KIND": {
                "record_in_processes": ["main", "content"],
                "expires_in_version": "never",
                "kind": "flag",
                "alert_emails": ["team@mozilla.xyz"],
                "bug_numbers": [1383793],
                "products": ["firefox"],
                "description": "Test histogram",
            }
        }
        histograms = load_histogram(SAMPLE_HISTOGRAM)
        parse_histograms.load_allowlist()

        self.assertRaises(
            SystemExit,
            parse_histograms.Histogram,
            "TEST_HISTOGRAM_ALLOWLIST_KIND",
            histograms["TEST_HISTOGRAM_ALLOWLIST_KIND"],
            strict_type_checks=True,
        )

        # Set global allowlists for parse_histograms.
        parse_histograms.allowlists = {
            "alert_emails": [],
            "bug_numbers": [],
            "n_buckets": [],
            "expiry_default": [],
            "kind": ["TEST_HISTOGRAM_ALLOWLIST_KIND"],
        }

        hist = parse_histograms.Histogram(
            "TEST_HISTOGRAM_ALLOWLIST_KIND",
            histograms["TEST_HISTOGRAM_ALLOWLIST_KIND"],
            strict_type_checks=True,
        )

        ParserError.exit_func()
        self.assertEqual(hist.expiration(), "never")
        self.assertEqual(hist.kind(), "flag")
        self.assertEqual(hist.record_in_processes(), ["main", "content"])
        self.assertEqual(hist.keyed(), False)

        parse_histograms.allowlists = None

    def test_multistore(self):
        SAMPLE_HISTOGRAM = {
            "TEST_VALID_HISTOGRAM": {
                "record_in_processes": ["main", "content"],
                "alert_emails": ["team@mozilla.xyz"],
                "bug_numbers": [1383793],
                "expires_in_version": "never",
                "kind": "boolean",
                "description": "Test histogram",
                "products": ["firefox"],
                "record_into_store": ["main", "sync"],
            }
        }
        histograms = load_histogram(SAMPLE_HISTOGRAM)
        parse_histograms.load_allowlist()

        hist = parse_histograms.Histogram(
            "TEST_VALID_HISTOGRAM",
            histograms["TEST_VALID_HISTOGRAM"],
            strict_type_checks=True,
        )

        ParserError.exit_func()
        self.assertTrue(hist.expiration(), "never")
        self.assertTrue(hist.kind(), "boolean")
        self.assertTrue(hist.record_into_store, ["main", "sync"])

    def test_multistore_empty(self):
        SAMPLE_HISTOGRAM = {
            "TEST_HISTOGRAM_EMPTY_MULTISTORE": {
                "record_in_processes": ["main", "content"],
                "alert_emails": ["team@mozilla.xyz"],
                "bug_numbers": [1383793],
                "expires_in_version": "never",
                "kind": "boolean",
                "description": "Test histogram",
                "products": ["firefox"],
                "record_into_store": [],
            }
        }
        histograms = load_histogram(SAMPLE_HISTOGRAM)
        parse_histograms.load_allowlist()

        parse_histograms.Histogram(
            "TEST_HISTOGRAM_EMPTY_MULTISTORE",
            histograms["TEST_HISTOGRAM_EMPTY_MULTISTORE"],
            strict_type_checks=True,
        )
        self.assertRaises(SystemExit, ParserError.exit_func)

    def test_products_absent(self):
        SAMPLE_HISTOGRAM = {
            "TEST_NO_PRODUCTS": {
                "record_in_processes": ["main", "content"],
                "alert_emails": ["team@mozilla.xyz"],
                "bug_numbers": [1383793],
                "expires_in_version": "never",
                "kind": "boolean",
                "description": "Test histogram",
            }
        }
        histograms = load_histogram(SAMPLE_HISTOGRAM)
        parse_histograms.load_allowlist()

        def test_parse():
            return parse_histograms.Histogram(
                "TEST_NO_PRODUCTS",
                histograms["TEST_NO_PRODUCTS"],
                strict_type_checks=True,
            )

        self.assertRaises(SystemExit, test_parse)

    def test_products_empty(self):
        SAMPLE_HISTOGRAM = {
            "TEST_EMPTY_PRODUCTS": {
                "record_in_processes": ["main", "content"],
                "alert_emails": ["team@mozilla.xyz"],
                "bug_numbers": [1383793],
                "expires_in_version": "never",
                "kind": "boolean",
                "description": "Test histogram",
                "products": [],
            }
        }
        histograms = load_histogram(SAMPLE_HISTOGRAM)
        parse_histograms.load_allowlist()

        def test_parse():
            return parse_histograms.Histogram(
                "TEST_EMPTY_PRODUCTS",
                histograms["TEST_EMPTY_PRODUCTS"],
                strict_type_checks=True,
            )

        self.assertRaises(SystemExit, test_parse)

    def test_products_all(self):
        SAMPLE_HISTOGRAM = {
            "TEST_HISTOGRAM_ALL_PRODUCTS": {
                "record_in_processes": ["main", "content"],
                "alert_emails": ["team@mozilla.xyz"],
                "bug_numbers": [1383793],
                "expires_in_version": "never",
                "kind": "boolean",
                "description": "Test histogram",
                "products": ["all"],
            }
        }
        histograms = load_histogram(SAMPLE_HISTOGRAM)
        parse_histograms.load_allowlist()

        parse_histograms.Histogram(
            "TEST_HISTOGRAM_ALL_PRODUCTS",
            histograms["TEST_HISTOGRAM_ALL_PRODUCTS"],
            strict_type_checks=True,
        )
        self.assertRaises(SystemExit, ParserError.exit_func)

    def test_gv_streaming_unsupported_kind(self):
        SAMPLE_HISTOGRAM = {
            "TEST_HISTOGRAM_GV_STREAMING": {
                "record_in_processes": ["main", "content"],
                "alert_emails": ["team@mozilla.xyz"],
                "bug_numbers": [1383793],
                "expires_in_version": "never",
                "kind": "boolean",
                "description": "Test histogram",
                "products": ["geckoview_streaming"],
            }
        }
        histograms = load_histogram(SAMPLE_HISTOGRAM)
        parse_histograms.load_allowlist()
        parse_histograms.Histogram(
            "TEST_HISTOGRAM_GV_STREAMING",
            histograms["TEST_HISTOGRAM_GV_STREAMING"],
            strict_type_checks=True,
        )
        self.assertRaises(SystemExit, ParserError.exit_func)

    def test_gv_streaming_keyed(self):
        SAMPLE_HISTOGRAM = {
            "TEST_HISTOGRAM_GV_STREAMING": {
                "record_in_processes": ["main", "content"],
                "alert_emails": ["team@mozilla.xyz"],
                "bug_numbers": [1383793],
                "expires_in_version": "never",
                "kind": "exponential",
                "low": 1024,
                "high": 2**64,
                "n_buckets": 100,
                "keyed": "true",
                "description": "Test histogram",
                "products": ["geckoview_streaming"],
            }
        }
        histograms = load_histogram(SAMPLE_HISTOGRAM)
        parse_histograms.load_allowlist()
        parse_histograms.Histogram(
            "TEST_HISTOGRAM_GV_STREAMING",
            histograms["TEST_HISTOGRAM_GV_STREAMING"],
            strict_type_checks=True,
        )

        self.assertRaises(SystemExit, ParserError.exit_func)

    def test_enumerated_histogram_with_100_buckets(self):
        SAMPLE_HISTOGRAM = {
            "TEST_100_BUCKETS_HISTOGRAM": {
                "record_in_processes": ["main", "content", "socket", "utility"],
                "alert_emails": ["team@mozilla.xyz"],
                "bug_numbers": [1383793],
                "expires_in_version": "never",
                "kind": "enumerated",
                "n_values": 100,
                "products": ["firefox"],
                "description": "Test histogram",
            }
        }
        histograms = load_histogram(SAMPLE_HISTOGRAM)
        parse_histograms.load_allowlist()

        hist = parse_histograms.Histogram(
            "TEST_100_BUCKETS_HISTOGRAM",
            histograms["TEST_100_BUCKETS_HISTOGRAM"],
            strict_type_checks=True,
        )

        ParserError.exit_func()
        self.assertTrue(hist.expiration(), "never")
        self.assertTrue(hist.kind(), "enumerated")
        self.assertTrue(hist.n_buckets(), 101)
        self.assertTrue(hist.record_in_processes, ["main", "content"])
        self.assertTrue(hist.record_into_store, ["main"])


if __name__ == "__main__":
    mozunit.main()
