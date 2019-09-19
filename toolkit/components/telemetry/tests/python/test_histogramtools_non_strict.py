# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import mozunit
import sys
import unittest
from os import path

TELEMETRY_ROOT_PATH = path.abspath(path.join(path.dirname(__file__), path.pardir, path.pardir))
sys.path.append(TELEMETRY_ROOT_PATH)
# The parsers live in a subdirectory of "build_scripts", account for that.
# NOTE: if the parsers are moved, this logic will need to be updated.
sys.path.append(path.join(TELEMETRY_ROOT_PATH, "build_scripts"))
from mozparsers import parse_histograms   # noqa: E402


def load_histogram(histograms):
    """Parse the passed Histogram and return a dictionary mapping histogram
    names to histogram parameters.

    :param histogram: Histogram as a python dictionary
    :returns: Parsed Histogram dictionary mapping histogram names to histogram parameters
    """
    def hook(ps):
        return parse_histograms.load_histograms_into_dict(ps, strict_type_checks=False)
    return json.loads(json.dumps(histograms), object_pairs_hook=hook)


class TestParser(unittest.TestCase):
    def test_unknown_field(self):
        SAMPLE_HISTOGRAM = {
            "A11Y_INSTANTIATED_FLAG": {
                "record_in_processes": ["main", "content"],
                "expires_in_version": "never",
                "kind": "flag",
                "description": "has accessibility support been instantiated",
                "new_field": "Its a new field"
                }}
        histograms = load_histogram(SAMPLE_HISTOGRAM)

        hist = parse_histograms.Histogram('A11Y_INSTANTIATED_FLAG',
                                          histograms['A11Y_INSTANTIATED_FLAG'],
                                          strict_type_checks=False)
        self.assertEqual(hist.expiration(), 'never')
        self.assertEqual(hist.kind(), 'flag')
        self.assertEqual(hist.record_in_processes(), ["main", "content"])

    def test_non_numeric_expressions(self):
        SAMPLE_HISTOGRAM = {
            "TEST_NON_NUMERIC_HISTOGRAM": {
                "kind": "linear",
                "description": "sample",
                "n_buckets": "JS::GCReason::NUM_TELEMETRY_REASONS",
                "high": "mozilla::StartupTimeline::MAX_EVENT_ID"
                }}

        histograms = load_histogram(SAMPLE_HISTOGRAM)
        hist = parse_histograms.Histogram('TEST_NON_NUMERIC_HISTOGRAM',
                                          histograms['TEST_NON_NUMERIC_HISTOGRAM'],
                                          strict_type_checks=False)

        # expected values come off parse_histograms.py
        self.assertEqual(hist.n_buckets(), 101)
        self.assertEqual(hist.high(), 12)

    def test_devtools_database_parsing(self):
        db = path.join(TELEMETRY_ROOT_PATH,
                       path.pardir,
                       path.pardir,
                       path.pardir,
                       "devtools",
                       "shared",
                       "css",
                       "generated",
                       "properties-db.js")

        histograms = list(parse_histograms.from_files([db], strict_type_checks=False))
        histograms = [h.name() for h in histograms]

        # Test a shorthand (animation)
        self.assertTrue("USE_COUNTER2_CSS_PROPERTY_Animation_DOCUMENT" in histograms)

        # Test a shorthand alias (-moz-animation).
        self.assertTrue("USE_COUNTER2_CSS_PROPERTY_MozAnimation_DOCUMENT" in histograms)

        # Test a longhand (animation-name)
        self.assertTrue("USE_COUNTER2_CSS_PROPERTY_AnimationName_DOCUMENT" in histograms)

        # Test a longhand alias (-moz-animation-name)
        self.assertTrue("USE_COUNTER2_CSS_PROPERTY_MozAnimationName_DOCUMENT" in histograms)

    def test_current_histogram(self):
        HISTOGRAMS_PATH = path.join(TELEMETRY_ROOT_PATH, "Histograms.json")
        all_histograms = list(parse_histograms.from_files([HISTOGRAMS_PATH],
                                                          strict_type_checks=False))
        test_histogram = [i for i in all_histograms if i.name() == 'TELEMETRY_TEST_FLAG'][0]

        self.assertEqual(test_histogram.expiration(), 'never')
        self.assertEqual(test_histogram.kind(), 'flag')
        self.assertEqual(test_histogram.record_in_processes(), ["main", "content"])
        self.assertEqual(test_histogram.keyed(), False)

    def test_no_products(self):
        SAMPLE_HISTOGRAM = {
            "TEST_EMPTY_PRODUCTS": {
                "kind": "flag",
                "description": "sample",
                }}

        histograms = load_histogram(SAMPLE_HISTOGRAM)
        hist = parse_histograms.Histogram('TEST_EMPTY_PRODUCTS',
                                          histograms['TEST_EMPTY_PRODUCTS'],
                                          strict_type_checks=False)

        self.assertEqual(hist.kind(), 'flag')
        # bug 1486072: absent `product` key becomes None instead of ["all"]
        self.assertEqual(hist.products(), None)


if __name__ == '__main__':
    mozunit.main()
