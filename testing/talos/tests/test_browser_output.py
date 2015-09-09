#!/usr/bin/env python

"""
test talos browser output parsing
"""

import os
import unittest

from talos.results import BrowserLogResults
from talos.results import PageloaderResults
from talos.utils import TalosError

here = os.path.dirname(os.path.abspath(__file__))

class TestBrowserOutput(unittest.TestCase):

    def test_ts_format(self):

        # output file
        browser_ts = os.path.join(here, 'browser_output.ts.txt')

        # parse the results
        browser_log = BrowserLogResults(browser_ts)

        # ensure the results meet what we expect
        self.assertEqual(browser_log.format, 'tsformat')
        self.assertEqual(browser_log.browser_results.strip(), '392')
        self.assertEqual(browser_log.startTime, 1333663595953)
        self.assertEqual(browser_log.endTime, 1333663596551)

    def test_tsvg_format(self):

        # output file
        browser_tsvg = os.path.join(here, 'browser_output.tsvg.txt')

        # parse the results
        browser_log = BrowserLogResults(browser_tsvg)

        # ensure the results meet what we expect
        self.assertEqual(browser_log.format, 'tpformat')
        self.assertEqual(browser_log.startTime, 1333666702130)
        self.assertEqual(browser_log.endTime, 1333666702743)

        # we won't validate the exact string because it is long
        raw_report = browser_log.browser_results.strip()
        raw_report.startswith('_x_x_mozilla_page_load')
        raw_report.endswith('|11;hixie-007.xml;1629;1651;1648;1652;1649')

        # but we will ensure that it is parseable
        pageloader_results = PageloaderResults(raw_report)
        self.assertEqual(len(pageloader_results.results), 12)
        indices = [i['index'] for i in pageloader_results.results]
        self.assertEqual(indices, range(12))

        # test hixie-001.xml just as a spot-check
        hixie_001 = pageloader_results.results[5]
        expected_values = [45643, 14976, 17807, 14971, 17235]
        self.assertEqual(hixie_001['runs'], expected_values)
        self.assertEqual(hixie_001['page'], 'hixie-001.xml')

    def test_garbage(self):
        """
        send in garbage input and ensure the output is the
        inability to find the report
        """

        garbage = "hjksdfhkhasdfjkhsdfkhdfjklasd"
        self.compare_error_message(garbage, "Could not find report")

    def test_missing_end_report(self):
        """what if you're not done with a report?"""
        garbage = "hjksdfhkhasdfjkhsdfkhdfjklasd"

        input = self.start_report()
        input += garbage
        self.compare_error_message(input, "Could not find end token: '__end_report'")

    def test_double_end_report(self):
        """double end report tokens"""

        garbage = "hjksdfhkhasdfjkhsdfkhdfjklasd"
        input = self.start_report() + garbage + self.end_report() + self.end_report()
        self.compare_error_message(input, "Unmatched number of tokens")

    def test_end_report_before_start_report(self):
        """the end report token occurs before the start report token"""

        garbage = "hjksdfhkhasdfjkhsdfkhdfjklasd"
        input = self.end_report() + garbage + self.start_report()
        self.compare_error_message(input, "End token '%s' occurs before start token" % self.end_report())

    def test_missing_timestamps(self):
        """what if the timestamps are missing?"""

        # make a bogus report but missing the timestamps
        garbage = "hjksdfhkhasdfjkhsdfkhdfjklasd"
        input = self.start_report() + garbage + self.end_report()

        # it will fail
        self.compare_error_message(input, "Could not find startTime in browser output")

    def test_wrong_order(self):
        """what happens if you mix up the token order?"""

        # i've secretly put the AfterTerminationTimestamp before
        # the BeforeLaunchTimestamp
        # Let's see if the parser notices
        bad_report = """__start_report392__end_report

Failed to load native module at path '/home/jhammel/firefox/components/libmozgnome.so': (80004005) libnotify.so.1: cannot open shared object file: No such file or directory
Could not read chrome manifest 'file:///home/jhammel/firefox/extensions/%7B972ce4c6-7e08-4474-a285-3208198ce6fd%7D/chrome.manifest'.
[JavaScript Warning: "Use of enablePrivilege is deprecated.  Please use code that runs with the system principal (e.g. an extension) instead." {file: "http://localhost:15707/startup_test/startup_test.html?begin=1333663595557" line: 0}]
__startTimestamp1333663595953__endTimestamp
__startAfterTerminationTimestamp1333663596551__endAfterTerminationTimestamp
__startBeforeLaunchTimestamp1333663595557__endBeforeLaunchTimestamp
"""

        self.compare_error_message(bad_report, "] found before ('__startBeforeLaunchTimestamp', '__endBeforeLaunchTimestamp') [character position:")

    def test_multiple_reports(self):
        """you're only allowed to have one report in a file"""

        # this one works fine
        good_report = """__start_report392__end_report

Failed to load native module at path '/home/jhammel/firefox/components/libmozgnome.so': (80004005) libnotify.so.1: cannot open shared object file: No such file or directory
Could not read chrome manifest 'file:///home/jhammel/firefox/extensions/%7B972ce4c6-7e08-4474-a285-3208198ce6fd%7D/chrome.manifest'.
[JavaScript Warning: "Use of enablePrivilege is deprecated.  Please use code that runs with the system principal (e.g. an extension) instead." {file: "http://localhost:15707/startup_test/startup_test.html?begin=1333663595557" line: 0}]
__startTimestamp1333663595953__endTimestamp
__startBeforeLaunchTimestamp1333663595557__endBeforeLaunchTimestamp
__startAfterTerminationTimestamp1333663596551__endAfterTerminationTimestamp
"""

        b = BrowserLogResults(results_raw=good_report)

        # but there's no hope for this one
        bad_report = good_report + good_report # interesting math

        self.compare_error_message(bad_report, "Multiple matches for %s,%s" % (self.start_report(), self.end_report()))

    def start_report(self):
        """return a start report token"""
        return BrowserLogResults.report_tokens[0][1][0] # start token

    def end_report(self):
        """return a start report token"""
        return BrowserLogResults.report_tokens[0][1][-1] # end token

    def compare_error_message(self, browser_log, substr):
        """
        ensures that exceptions give correct error messages
        - browser_log : a browser log file
        - substr : substring of the error message
        """

        error = None
        try:
            BrowserLogResults(results_raw=browser_log)
        except TalosError, e:
            if substr not in str(e):
                import pdb; pdb.set_trace()
            self.assertTrue(substr in str(e))
class TestTalosError(unittest.TestCase):
    """
    test TalosError class
    """
    def test_browser_log_results(self):
        #an example that should fail
        #passing invalid value for argument result_raw
        with self.assertRaises(TalosError):
            BrowserLogResults(results_raw = "__FAIL<bad test>__FAIL")

if __name__ == '__main__':
    unittest.main()
