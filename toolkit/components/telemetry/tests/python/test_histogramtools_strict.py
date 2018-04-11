# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.import json

import mozunit
import sys
import unittest
from os import path
from test_histogramtools_non_strict import load_histogram

TELEMETRY_ROOT_PATH = path.abspath(path.join(path.dirname(__file__), path.pardir, path.pardir))
sys.path.append(TELEMETRY_ROOT_PATH)
from shared_telemetry_utils import ParserError
import parse_histograms


class TestParser(unittest.TestCase):
    def test_valid_histogram(self):
        SAMPLE_HISTOGRAM = {
            "TEST_VALID_HISTOGRAM": {
                "record_in_processes": ["main", "content"],
                "alert_emails": ["team@mozilla.xyz"],
                "bug_numbers": [1383793],
                "expires_in_version": "never",
                "kind": "boolean",
                "description": "Test histogram"
            }
        }
        histograms = load_histogram(SAMPLE_HISTOGRAM)
        parse_histograms.load_whitelist()

        hist = parse_histograms.Histogram('TEST_VALID_HISTOGRAM',
                                          histograms['TEST_VALID_HISTOGRAM'],
                                          strict_type_checks=True)

        ParserError.exit_func()
        self.assertTrue(hist.expiration(), "never")
        self.assertTrue(hist.kind(), "boolean")
        self.assertTrue(hist.record_in_processes, ["main", "content"])

    def test_missing_bug_numbers(self):
        SAMPLE_HISTOGRAM = {
            "TEST_HISTOGRAM_WHITELIST_BUG_NUMBERS": {
                "record_in_processes": ["main", "content"],
                "alert_emails": ["team@mozilla.xyz"],
                "expires_in_version": "never",
                "kind": "boolean",
                "description": "Test histogram"
            }
        }
        histograms = load_histogram(SAMPLE_HISTOGRAM)
        parse_histograms.load_whitelist()

        parse_histograms.Histogram('TEST_HISTOGRAM_WHITELIST_BUG_NUMBERS',
                                   histograms['TEST_HISTOGRAM_WHITELIST_BUG_NUMBERS'],
                                   strict_type_checks=True)

        self.assertRaises(SystemExit, ParserError.exit_func)

        # Set global whitelists for parse_histograms.
        parse_histograms.whitelists = {
            "alert_emails": [],
            "bug_numbers": [
                "TEST_HISTOGRAM_WHITELIST_BUG_NUMBERS"
            ],
            "n_buckets": [],
            "expiry_default": [],
            "kind": []
        }

        hist = parse_histograms.Histogram('TEST_HISTOGRAM_WHITELIST_BUG_NUMBERS',
                                          histograms['TEST_HISTOGRAM_WHITELIST_BUG_NUMBERS'],
                                          strict_type_checks=True)

        ParserError.exit_func()
        self.assertEqual(hist.expiration(), 'never')
        self.assertEqual(hist.kind(), 'boolean')
        self.assertEqual(hist.record_in_processes(), ["main", "content"])
        self.assertEqual(hist.keyed(), False)

        parse_histograms.whitelists = None

    def test_missing_alert_emails(self):
        SAMPLE_HISTOGRAM = {
            "TEST_HISTOGRAM_WHITELIST_ALERT_EMAILS": {
                "record_in_processes": ["main", "content"],
                "bug_numbers": [1383793],
                "expires_in_version": "never",
                "kind": "boolean",
                "description": "Test histogram",
            }
        }
        histograms = load_histogram(SAMPLE_HISTOGRAM)
        parse_histograms.load_whitelist()

        parse_histograms.Histogram('TEST_HISTOGRAM_WHITELIST_ALERT_EMAILS',
                                   histograms['TEST_HISTOGRAM_WHITELIST_ALERT_EMAILS'],
                                   strict_type_checks=True)

        self.assertRaises(SystemExit, ParserError.exit_func)

        # Set global whitelists for parse_histograms.
        parse_histograms.whitelists = {
            "alert_emails": [
                "TEST_HISTOGRAM_WHITELIST_ALERT_EMAILS"
            ],
            "bug_numbers": [],
            "n_buckets": [],
            "expiry_default": [],
            "kind": []
        }

        hist = parse_histograms.Histogram('TEST_HISTOGRAM_WHITELIST_ALERT_EMAILS',
                                          histograms['TEST_HISTOGRAM_WHITELIST_ALERT_EMAILS'],
                                          strict_type_checks=True)

        ParserError.exit_func()
        self.assertEqual(hist.expiration(), 'never')
        self.assertEqual(hist.kind(), 'boolean')
        self.assertEqual(hist.record_in_processes(), ["main", "content"])
        self.assertEqual(hist.keyed(), False)

        parse_histograms.whitelists = None

    def test_high_n_buckets(self):
        SAMPLE_HISTOGRAM = {
            "TEST_HISTOGRAM_WHITELIST_N_BUCKETS": {
                "record_in_processes": ["main", "content"],
                "alert_emails": ["team@mozilla.xyz"],
                "bug_numbers": [1383793],
                "expires_in_version": "never",
                "kind": "exponential",
                "low": 1024,
                "high": 16777216,
                "n_buckets": 200,
                "description": "Test histogram",
            }
        }
        histograms = load_histogram(SAMPLE_HISTOGRAM)
        parse_histograms.load_whitelist()

        parse_histograms.Histogram('TEST_HISTOGRAM_WHITELIST_N_BUCKETS',
                                   histograms['TEST_HISTOGRAM_WHITELIST_N_BUCKETS'],
                                   strict_type_checks=True)

        self.assertRaises(SystemExit, ParserError.exit_func)

        # Set global whitelists for parse_histograms.
        parse_histograms.whitelists = {
            "alert_emails": [],
            "bug_numbers": [],
            "n_buckets": [
                "TEST_HISTOGRAM_WHITELIST_N_BUCKETS"
            ],
            "expiry_default": [],
            "kind": []
        }

        hist = parse_histograms.Histogram('TEST_HISTOGRAM_WHITELIST_N_BUCKETS',
                                          histograms['TEST_HISTOGRAM_WHITELIST_N_BUCKETS'],
                                          strict_type_checks=True)

        ParserError.exit_func()
        self.assertEqual(hist.expiration(), 'never')
        self.assertEqual(hist.kind(), 'exponential')
        self.assertEqual(hist.record_in_processes(), ["main", "content"])
        self.assertEqual(hist.keyed(), False)
        self.assertEqual(hist.low(), 1024)
        self.assertEqual(hist.high(), 16777216)
        self.assertEqual(hist.n_buckets(), 200)

        parse_histograms.whitelists = None

    def test_expiry_default(self):
        SAMPLE_HISTOGRAM = {
            "TEST_HISTOGRAM_WHITELIST_EXPIRY_DEFAULT": {
                "record_in_processes": ["main", "content"],
                "expires_in_version": "default",
                "alert_emails": ["team@mozilla.xyz"],
                "bug_numbers": [1383793],
                "kind": "boolean",
                "description": "Test histogram",
            }
        }
        histograms = load_histogram(SAMPLE_HISTOGRAM)
        parse_histograms.load_whitelist()

        parse_histograms.Histogram('TEST_HISTOGRAM_WHITELIST_EXPIRY_DEFAULT',
                                   histograms['TEST_HISTOGRAM_WHITELIST_EXPIRY_DEFAULT'],
                                   strict_type_checks=True)

        self.assertRaises(SystemExit, ParserError.exit_func)

        # Set global whitelists for parse_histograms.
        parse_histograms.whitelists = {
            "alert_emails": [],
            "bug_numbers": [],
            "n_buckets": [],
            "expiry_default": [
                "TEST_HISTOGRAM_WHITELIST_EXPIRY_DEFAULT"
            ],
            "kind": []
        }

        hist = parse_histograms.Histogram('TEST_HISTOGRAM_WHITELIST_EXPIRY_DEFAULT',
                                          histograms['TEST_HISTOGRAM_WHITELIST_EXPIRY_DEFAULT'],
                                          strict_type_checks=True)

        ParserError.exit_func()
        self.assertEqual(hist.expiration(), 'default')
        self.assertEqual(hist.kind(), 'boolean')
        self.assertEqual(hist.record_in_processes(), ["main", "content"])
        self.assertEqual(hist.keyed(), False)

        parse_histograms.whitelists = None

    def test_unsupported_kind_count(self):
        SAMPLE_HISTOGRAM = {
            "TEST_HISTOGRAM_WHITELIST_KIND": {
                "record_in_processes": ["main", "content"],
                "expires_in_version": "never",
                "kind": "count",
                "releaseChannelCollection": "opt-out",
                "alert_emails": ["team@mozilla.xyz"],
                "bug_numbers": [1383793],
                "description": "Test histogram",
            }
        }
        histograms = load_histogram(SAMPLE_HISTOGRAM)
        parse_histograms.load_whitelist()

        self.assertRaises(SystemExit, parse_histograms.Histogram,
                          'TEST_HISTOGRAM_WHITELIST_KIND',
                          histograms['TEST_HISTOGRAM_WHITELIST_KIND'],
                          strict_type_checks=True)

        # Set global whitelists for parse_histograms.
        parse_histograms.whitelists = {
            "alert_emails": [],
            "bug_numbers": [],
            "n_buckets": [],
            "expiry_default": [],
            "kind": [
                "TEST_HISTOGRAM_WHITELIST_KIND"
            ]
        }

        hist = parse_histograms.Histogram('TEST_HISTOGRAM_WHITELIST_KIND',
                                          histograms['TEST_HISTOGRAM_WHITELIST_KIND'],
                                          strict_type_checks=True)

        ParserError.exit_func()
        self.assertEqual(hist.expiration(), 'never')
        self.assertEqual(hist.kind(), 'count')
        self.assertEqual(hist.record_in_processes(), ["main", "content"])
        self.assertEqual(hist.keyed(), False)

        parse_histograms.whitelists = None

    def test_unsupported_kind_flag(self):
        SAMPLE_HISTOGRAM = {
            "TEST_HISTOGRAM_WHITELIST_KIND": {
                "record_in_processes": ["main", "content"],
                "expires_in_version": "never",
                "kind": "flag",
                "alert_emails": ["team@mozilla.xyz"],
                "bug_numbers": [1383793],
                "description": "Test histogram",
            }
        }
        histograms = load_histogram(SAMPLE_HISTOGRAM)
        parse_histograms.load_whitelist()

        self.assertRaises(SystemExit, parse_histograms.Histogram,
                          'TEST_HISTOGRAM_WHITELIST_KIND',
                          histograms['TEST_HISTOGRAM_WHITELIST_KIND'],
                          strict_type_checks=True)

        # Set global whitelists for parse_histograms.
        parse_histograms.whitelists = {
            "alert_emails": [],
            "bug_numbers": [],
            "n_buckets": [],
            "expiry_default": [],
            "kind": [
                "TEST_HISTOGRAM_WHITELIST_KIND"
            ]
        }

        hist = parse_histograms.Histogram('TEST_HISTOGRAM_WHITELIST_KIND',
                                          histograms['TEST_HISTOGRAM_WHITELIST_KIND'],
                                          strict_type_checks=True)

        ParserError.exit_func()
        self.assertEqual(hist.expiration(), 'never')
        self.assertEqual(hist.kind(), 'flag')
        self.assertEqual(hist.record_in_processes(), ["main", "content"])
        self.assertEqual(hist.keyed(), False)

        parse_histograms.whitelists = None


if __name__ == '__main__':
    mozunit.main()
