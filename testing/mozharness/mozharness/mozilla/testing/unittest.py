#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

from __future__ import absolute_import
import re
import os

from mozharness.mozilla.testing.errors import TinderBoxPrintRe
from mozharness.base.log import OutputParser, WARNING, INFO, CRITICAL, ERROR
from mozharness.mozilla.automation import TBPL_WARNING, TBPL_FAILURE, TBPL_RETRY
from mozharness.mozilla.automation import TBPL_SUCCESS, TBPL_WORST_LEVEL_TUPLE

SUITE_CATEGORIES = ["mochitest", "reftest", "xpcshell"]


def tbox_print_summary(
    pass_count, fail_count, known_fail_count=None, crashed=False, leaked=False
):
    emphasize_fail_text = '<em class="testfail">%s</em>'

    if (
        pass_count < 0
        or fail_count < 0
        or (known_fail_count is not None and known_fail_count < 0)
    ):
        summary = emphasize_fail_text % "T-FAIL"
    elif (
        pass_count == 0
        and fail_count == 0
        and (known_fail_count == 0 or known_fail_count is None)
    ):
        summary = emphasize_fail_text % "T-FAIL"
    else:
        str_fail_count = str(fail_count)
        if fail_count > 0:
            str_fail_count = emphasize_fail_text % str_fail_count
        summary = "%d/%s" % (pass_count, str_fail_count)
        if known_fail_count is not None:
            summary += "/%d" % known_fail_count
    # Format the crash status.
    if crashed:
        summary += "&nbsp;%s" % emphasize_fail_text % "CRASH"
    # Format the leak status.
    if leaked is not False:
        summary += "&nbsp;%s" % emphasize_fail_text % ((leaked and "LEAK") or "L-FAIL")
    return summary


class TestSummaryOutputParserHelper(OutputParser):
    def __init__(self, regex=re.compile(r"(passed|failed|todo): (\d+)"), **kwargs):
        self.regex = regex
        self.failed = 0
        self.passed = 0
        self.todo = 0
        self.last_line = None
        self.tbpl_status = TBPL_SUCCESS
        self.worst_log_level = INFO
        super(TestSummaryOutputParserHelper, self).__init__(**kwargs)

    def parse_single_line(self, line):
        super(TestSummaryOutputParserHelper, self).parse_single_line(line)
        self.last_line = line
        m = self.regex.search(line)
        if m:
            try:
                setattr(self, m.group(1), int(m.group(2)))
            except ValueError:
                # ignore bad values
                pass

    def evaluate_parser(self, return_code, success_codes=None, previous_summary=None):
        # TestSummaryOutputParserHelper is for Marionette, which doesn't support test-verify
        # When it does we can reset the internal state variables as needed
        joined_summary = previous_summary

        if return_code == 0 and self.passed > 0 and self.failed == 0:
            self.tbpl_status = TBPL_SUCCESS
        elif return_code == 10 and self.failed > 0:
            self.tbpl_status = TBPL_WARNING
        else:
            self.tbpl_status = TBPL_FAILURE
            self.worst_log_level = ERROR

        return (self.tbpl_status, self.worst_log_level, joined_summary)

    def print_summary(self, suite_name):
        # generate the TinderboxPrint line for TBPL
        emphasize_fail_text = '<em class="testfail">%s</em>'
        failed = "0"
        if self.passed == 0 and self.failed == 0:
            self.tsummary = emphasize_fail_text % "T-FAIL"
        else:
            if self.failed > 0:
                failed = emphasize_fail_text % str(self.failed)
            self.tsummary = "%d/%s/%d" % (self.passed, failed, self.todo)

        self.info("TinderboxPrint: %s<br/>%s\n" % (suite_name, self.tsummary))

    def append_tinderboxprint_line(self, suite_name):
        self.print_summary(suite_name)


class DesktopUnittestOutputParser(OutputParser):
    """
    A class that extends OutputParser such that it can parse the number of
    passed/failed/todo tests from the output.
    """

    def __init__(self, suite_category, **kwargs):
        # worst_log_level defined already in DesktopUnittestOutputParser
        # but is here to make pylint happy
        self.worst_log_level = INFO
        super(DesktopUnittestOutputParser, self).__init__(**kwargs)
        self.summary_suite_re = TinderBoxPrintRe.get("%s_summary" % suite_category, {})
        self.harness_error_re = TinderBoxPrintRe["harness_error"]["minimum_regex"]
        self.full_harness_error_re = TinderBoxPrintRe["harness_error"]["full_regex"]
        self.harness_retry_re = TinderBoxPrintRe["harness_error"]["retry_regex"]
        self.fail_count = -1
        self.pass_count = -1
        # known_fail_count does not exist for some suites
        self.known_fail_count = self.summary_suite_re.get("known_fail_group") and -1
        self.crashed, self.leaked = False, False
        self.tbpl_status = TBPL_SUCCESS

    def parse_single_line(self, line):
        if self.summary_suite_re:
            summary_m = self.summary_suite_re["regex"].match(line)  # pass/fail/todo
            if summary_m:
                message = " %s" % line
                log_level = INFO
                # remove all the none values in groups() so this will work
                # with all suites including mochitest browser-chrome
                summary_match_list = [
                    group for group in summary_m.groups() if group is not None
                ]
                r = summary_match_list[0]
                if self.summary_suite_re["pass_group"] in r:
                    if len(summary_match_list) > 1:
                        self.pass_count = int(summary_match_list[-1])
                    else:
                        # This handles suites that either pass or report
                        # number of failures. We need to set both
                        # pass and fail count in the pass case.
                        self.pass_count = 1
                        self.fail_count = 0
                elif self.summary_suite_re["fail_group"] in r:
                    self.fail_count = int(summary_match_list[-1])
                    if self.fail_count > 0:
                        message += "\n One or more unittests failed."
                        log_level = WARNING
                # If self.summary_suite_re['known_fail_group'] == None,
                # then r should not match it, # so this test is fine as is.
                elif self.summary_suite_re["known_fail_group"] in r:
                    self.known_fail_count = int(summary_match_list[-1])
                self.log(message, log_level)
                return  # skip harness check and base parse_single_line
        harness_match = self.harness_error_re.search(line)
        if harness_match:
            self.warning(" %s" % line)
            self.worst_log_level = self.worst_level(WARNING, self.worst_log_level)
            self.tbpl_status = self.worst_level(
                TBPL_WARNING, self.tbpl_status, levels=TBPL_WORST_LEVEL_TUPLE
            )
            full_harness_match = self.full_harness_error_re.search(line)
            if full_harness_match:
                r = full_harness_match.group(1)
                if r == "application crashed":
                    self.crashed = True
                elif r == "missing output line for total leaks!":
                    self.leaked = None
                else:
                    self.leaked = True
            return  # skip base parse_single_line
        if self.harness_retry_re.search(line):
            self.critical(" %s" % line)
            self.worst_log_level = self.worst_level(CRITICAL, self.worst_log_level)
            self.tbpl_status = self.worst_level(
                TBPL_RETRY, self.tbpl_status, levels=TBPL_WORST_LEVEL_TUPLE
            )
            return  # skip base parse_single_line
        super(DesktopUnittestOutputParser, self).parse_single_line(line)

    def evaluate_parser(self, return_code, success_codes=None, previous_summary=None):
        success_codes = success_codes or [0]

        if self.num_errors:  # mozharness ran into a script error
            self.tbpl_status = self.worst_level(
                TBPL_FAILURE, self.tbpl_status, levels=TBPL_WORST_LEVEL_TUPLE
            )

        """
          We can run evaluate_parser multiple times, it will duplicate failures
          and status which can mean that future tests will fail if a previous test fails.
          When we have a previous summary, we want to do:
            1) reset state so we only evaluate the current results
        """
        joined_summary = {"pass_count": self.pass_count}
        if previous_summary:
            self.tbpl_status = TBPL_SUCCESS
            self.worst_log_level = INFO
            self.crashed = False
            self.leaked = False

        # I have to put this outside of parse_single_line because this checks not
        # only if fail_count was more then 0 but also if fail_count is still -1
        # (no fail summary line was found)
        if self.fail_count != 0:
            self.worst_log_level = self.worst_level(WARNING, self.worst_log_level)
            self.tbpl_status = self.worst_level(
                TBPL_WARNING, self.tbpl_status, levels=TBPL_WORST_LEVEL_TUPLE
            )

        # Account for the possibility that no test summary was output.
        if (
            self.pass_count <= 0
            and self.fail_count <= 0
            and (self.known_fail_count is None or self.known_fail_count <= 0)
            and os.environ.get("TRY_SELECTOR") != "coverage"
        ):
            self.error("No tests run or test summary not found")
            self.worst_log_level = self.worst_level(WARNING, self.worst_log_level)
            self.tbpl_status = self.worst_level(
                TBPL_WARNING, self.tbpl_status, levels=TBPL_WORST_LEVEL_TUPLE
            )

        if return_code not in success_codes:
            self.tbpl_status = self.worst_level(
                TBPL_FAILURE, self.tbpl_status, levels=TBPL_WORST_LEVEL_TUPLE
            )

        # we can trust in parser.worst_log_level in either case
        return (self.tbpl_status, self.worst_log_level, joined_summary)

    def append_tinderboxprint_line(self, suite_name):
        # We are duplicating a condition (fail_count) from evaluate_parser and
        # parse parse_single_line but at little cost since we are not parsing
        # the log more then once.  I figured this method should stay isolated as
        # it is only here for tbpl highlighted summaries and is not part of
        # result status IIUC.
        summary = tbox_print_summary(
            self.pass_count,
            self.fail_count,
            self.known_fail_count,
            self.crashed,
            self.leaked,
        )
        self.info("TinderboxPrint: %s<br/>%s\n" % (suite_name, summary))
