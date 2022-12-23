import unittest

from mozharness.base.log import INFO, WARNING
from mozharness.mozilla.automation import TBPL_SUCCESS, TBPL_WARNING
from mozharness.mozilla.mozbase import MozbaseMixin
from mozharness.mozilla.structuredlog import StructuredOutputParser
from mozlog.handlers.statushandler import RunSummary

success_summary = RunSummary(
    unexpected_statuses={},
    expected_statuses={"PASS": 3, "OK": 1, "FAIL": 1},
    known_intermittent_statuses={"FAIL": 1},
    log_level_counts={"info": 5},
    action_counts={"test_status": 4, "test_end": 1, "suite_end": 1},
)

failure_summary = RunSummary(
    unexpected_statuses={"FAIL": 2},
    expected_statuses={"PASS": 2, "OK": 1},
    known_intermittent_statuses={},
    log_level_counts={"warning": 2, "info": 3},
    action_counts={"test_status": 3, "test_end": 2, "suite_end": 1},
)


class TestParser(MozbaseMixin, StructuredOutputParser):
    def __init__(self, *args, **kwargs):
        super(TestParser, self).__init__(*args, **kwargs)
        self.config = {}


class TestStructuredOutputParser(unittest.TestCase):
    def setUp(self):
        self.parser = TestParser()

    def test_evaluate_parser_success(self):
        self.parser.handler.expected_statuses = {"PASS": 3, "OK": 1, "FAIL": 1}
        self.parser.handler.log_level_counts = {"info": 5}
        self.parser.handler.action_counts = {
            "test_status": 4,
            "test_end": 1,
            "suite_end": 1,
        }
        self.parser.handler.known_intermittent_statuses = {"FAIL": 1}
        result = self.parser.evaluate_parser(
            return_code=TBPL_SUCCESS, success_codes=[TBPL_SUCCESS]
        )
        tbpl_status, worst_log_level, joined_summary = result
        self.assertEqual(tbpl_status, TBPL_SUCCESS)
        self.assertEqual(worst_log_level, INFO)
        self.assertEqual(joined_summary, success_summary)

    def test_evaluate_parser_failure(self):
        self.parser.handler.unexpected_statuses = {"FAIL": 2}
        self.parser.handler.expected_statuses = {"PASS": 2, "OK": 1}
        self.parser.handler.log_level_counts = {"warning": 2, "info": 3}
        self.parser.handler.action_counts = {
            "test_status": 3,
            "test_end": 2,
            "suite_end": 1,
        }
        result = self.parser.evaluate_parser(
            return_code=TBPL_SUCCESS, success_codes=[TBPL_SUCCESS]
        )
        tbpl_status, worst_log_level, joined_summary = result
        self.assertEqual(tbpl_status, TBPL_WARNING)
        self.assertEqual(worst_log_level, WARNING)
        self.assertEqual(joined_summary, failure_summary)
