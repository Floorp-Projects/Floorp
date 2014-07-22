import argparse
import optparse
import os
import unittest
import StringIO
import json

from mozlog.structured import (
    commandline,
    reader,
    structuredlog,
    stdadapter,
    handlers,
)


class TestHandler(object):
    def __init__(self):
        self.items = []

    def __call__(self, data):
        self.items.append(data)

    @property
    def last_item(self):
        return self.items[-1]


class BaseStructuredTest(unittest.TestCase):
    def setUp(self):
        self.logger = structuredlog.StructuredLogger("test")
        self.handler = TestHandler()
        self.logger.add_handler(self.handler)

    @property
    def last_item(self):
        return self.handler.items.pop()

    def assert_log_equals(self, expected, actual=None):
        if actual is None:
            actual = self.last_item

        all_expected = {"pid": os.getpid(),
                        "thread": "MainThread",
                        "source": "test"}
        specials = set(["time"])

        all_expected.update(expected)
        for key, value in all_expected.iteritems():
            self.assertEqual(actual[key], value)

        self.assertEquals(set(all_expected.keys()) | specials, set(actual.keys()))


class TestStructuredLog(BaseStructuredTest):
    def test_suite_start(self):
        self.logger.suite_start(["test"])
        self.assert_log_equals({"action": "suite_start",
                                "tests":["test"]})

    def test_suite_end(self):
        self.logger.suite_end()
        self.assert_log_equals({"action": "suite_end"})

    def test_start(self):
        self.logger.test_start("test1")
        self.assert_log_equals({"action": "test_start",
                                "test":"test1"})

        self.logger.test_start(("test1", "==", "test1-ref"))
        self.assert_log_equals({"action": "test_start",
                                "test":("test1", "==", "test1-ref")})

    def test_start_inprogress(self):
        self.logger.test_start("test1")
        self.logger.test_start("test1")
        self.assert_log_equals({"action": "log",
                                "message": "test_start for test1 logged while in progress.",
                                "level": "ERROR"})

    def test_status(self):
        self.logger.test_start("test1")
        self.logger.test_status("test1", "subtest name", "fail", expected="FAIL", message="Test message")
        self.assert_log_equals({"action": "test_status",
                                "subtest": "subtest name",
                                "status": "FAIL",
                                "message": "Test message",
                                "test":"test1"})
        self.logger.test_end("test1", "OK")

    def test_status_1(self):
        self.logger.test_start("test1")
        self.logger.test_status("test1", "subtest name", "fail")
        self.assert_log_equals({"action": "test_status",
                                "subtest": "subtest name",
                                "status": "FAIL",
                                "expected": "PASS",
                                "test":"test1"})
        self.logger.test_end("test1", "OK")

    def test_status_2(self):
        self.assertRaises(ValueError, self.logger.test_status, "test1", "subtest name", "XXXUNKNOWNXXX")

    def test_status_extra(self):
        self.logger.test_start("test1")
        self.logger.test_status("test1", "subtest name", "FAIL", expected="PASS", extra={"data": 42})
        self.assert_log_equals({"action": "test_status",
                                "subtest": "subtest name",
                                "status": "FAIL",
                                "expected": "PASS",
                                "test": "test1",
                                "extra": {"data":42}
                            })
        self.logger.test_end("test1", "OK")

    def test_status_stack(self):
        self.logger.test_start("test1")
        self.logger.test_status("test1", "subtest name", "FAIL", expected="PASS", stack="many\nlines\nof\nstack")
        self.assert_log_equals({"action": "test_status",
                                "subtest": "subtest name",
                                "status": "FAIL",
                                "expected": "PASS",
                                "test": "test1",
                                "stack": "many\nlines\nof\nstack"
                            })
        self.logger.test_end("test1", "OK")

    def test_status_not_started(self):
        self.logger.test_status("test_UNKNOWN", "subtest", "PASS")
        self.assertTrue(self.last_item["message"].startswith(
            "test_status for test_UNKNOWN logged while not in progress. Logged with data: {"))

    def test_end(self):
        self.logger.test_start("test1")
        self.logger.test_end("test1", "fail", message="Test message")
        self.assert_log_equals({"action": "test_end",
                                "status": "FAIL",
                                "expected": "OK",
                                "message": "Test message",
                                "test":"test1"})

    def test_end_1(self):
        self.logger.test_start("test1")
        self.logger.test_end("test1", "PASS", expected="PASS", extra={"data":123})
        self.assert_log_equals({"action": "test_end",
                                "status": "PASS",
                                "extra": {"data": 123},
                                "test":"test1"})

    def test_end_2(self):
        self.assertRaises(ValueError, self.logger.test_end, "test1", "XXXUNKNOWNXXX")

    def test_end_stack(self):
        self.logger.test_start("test1")
        self.logger.test_end("test1", "PASS", expected="PASS", stack="many\nlines\nof\nstack")
        self.assert_log_equals({"action": "test_end",
                                "status": "PASS",
                                "test": "test1",
                                "stack": "many\nlines\nof\nstack"
                            })

    def test_end_no_start(self):
        self.logger.test_end("test1", "PASS", expected="PASS")
        self.assertTrue(self.last_item["message"].startswith(
            "test_end for test1 logged while not in progress. Logged with data: {"))

    def test_end_twice(self):
        self.logger.test_start("test2")
        self.logger.test_end("test2", "PASS", expected="PASS")
        self.assert_log_equals({"action": "test_end",
                                "status": "PASS",
                                "test": "test2"})
        self.logger.test_end("test2", "PASS", expected="PASS")
        self.assertTrue(self.last_item["message"].startswith(
            "test_end for test2 logged while not in progress. Logged with data: {"))


    def test_process(self):
        self.logger.process_output(1234, "test output")
        self.assert_log_equals({"action": "process_output",
                                "process": 1234,
                                "data": "test output"})

    def test_log(self):
        for level in ["critical", "error", "warning", "info", "debug"]:
            getattr(self.logger, level)("message")
            self.assert_log_equals({"action": "log",
                                    "level": level.upper(),
                                    "message": "message"})

    def test_logging_adapter(self):
        import logging
        logging.basicConfig(level="DEBUG")
        old_level = logging.root.getEffectiveLevel()
        logging.root.setLevel("DEBUG")

        std_logger = logging.getLogger("test")
        std_logger.setLevel("DEBUG")

        logger = stdadapter.std_logging_adapter(std_logger)

        try:
            for level in ["critical", "error", "warning", "info", "debug"]:
                getattr(logger, level)("message")
                self.assert_log_equals({"action": "log",
                                        "level": level.upper(),
                                        "message": "message"})
        finally:
            logging.root.setLevel(old_level)

    def test_add_remove_handlers(self):
        handler = TestHandler()
        self.logger.add_handler(handler)
        self.logger.info("test1")

        self.assert_log_equals({"action": "log",
                                "level": "INFO",
                                "message": "test1"})

        self.assert_log_equals({"action": "log",
                                "level": "INFO",
                                "message": "test1"}, actual=handler.last_item)

        self.logger.remove_handler(handler)
        self.logger.info("test2")

        self.assert_log_equals({"action": "log",
                                "level": "INFO",
                                "message": "test2"})

        self.assert_log_equals({"action": "log",
                                "level": "INFO",
                                "message": "test1"}, actual=handler.last_item)

    def test_wrapper(self):
        file_like = structuredlog.StructuredLogFileLike(self.logger)

        file_like.write("line 1")

        self.assert_log_equals({"action": "log",
                                "level": "INFO",
                                "message": "line 1"})

        file_like.write("line 2\n")

        self.assert_log_equals({"action": "log",
                                "level": "INFO",
                                "message": "line 2"})

        file_like.write("line 3\r")

        self.assert_log_equals({"action": "log",
                                "level": "INFO",
                                "message": "line 3"})

        file_like.write("line 4\r\n")

        self.assert_log_equals({"action": "log",
                                "level": "INFO",
                                "message": "line 4"})

class TestCommandline(unittest.TestCase):
    def test_setup_logging(self):
        parser = argparse.ArgumentParser()
        commandline.add_logging_group(parser)
        args = parser.parse_args(["--log-raw=-"])
        logger = commandline.setup_logging("test", args, {})
        self.assertEqual(len(logger.handlers), 1)

    def test_setup_logging_optparse(self):
        parser = optparse.OptionParser()
        commandline.add_logging_group(parser)
        args, _ = parser.parse_args(["--log-raw=-"])
        logger = commandline.setup_logging("test_optparse", args, {})
        self.assertEqual(len(logger.handlers), 1)
        self.assertIsInstance(logger.handlers[0], handlers.StreamHandler)

class TestReader(unittest.TestCase):
    def to_file_like(self, obj):
        data_str = "\n".join(json.dumps(item) for item in obj)
        return StringIO.StringIO(data_str)

    def test_read(self):
        data = [{"action": "action_0", "data": "data_0"},
                {"action": "action_1", "data": "data_1"}]

        f = self.to_file_like(data)
        self.assertEquals(data, list(reader.read(f)))

    def test_imap_log(self):
        data = [{"action": "action_0", "data": "data_0"},
                {"action": "action_1", "data": "data_1"}]

        f = self.to_file_like(data)

        def f_action_0(item):
            return ("action_0", item["data"])

        def f_action_1(item):
            return ("action_1", item["data"])

        res_iter = reader.imap_log(reader.read(f),
                                   {"action_0": f_action_0,
                                    "action_1": f_action_1})
        self.assertEquals([("action_0", "data_0"), ("action_1", "data_1")],
                          list(res_iter))

    def test_each_log(self):
        data = [{"action": "action_0", "data": "data_0"},
                {"action": "action_1", "data": "data_1"}]

        f = self.to_file_like(data)

        count = {"action_0":0,
                 "action_1":0}

        def f_action_0(item):
            count[item["action"]] += 1

        def f_action_1(item):
            count[item["action"]] += 2

        reader.each_log(reader.read(f),
                        {"action_0": f_action_0,
                         "action_1": f_action_1})

        self.assertEquals({"action_0":1, "action_1":2}, count)

    def test_handler(self):
        data = [{"action": "action_0", "data": "data_0"},
                {"action": "action_1", "data": "data_1"}]

        f = self.to_file_like(data)

        test = self
        class ReaderTestHandler(reader.LogHandler):
            def __init__(self):
                self.action_0_count = 0
                self.action_1_count = 0

            def action_0(self, item):
                test.assertEquals(item["action"], "action_0")
                self.action_0_count += 1

            def action_1(self, item):
                test.assertEquals(item["action"], "action_1")
                self.action_1_count += 1

        handler = ReaderTestHandler()
        reader.handle_log(reader.read(f), handler)

        self.assertEquals(handler.action_0_count, 1)
        self.assertEquals(handler.action_1_count, 1)

if __name__ == "__main__":
    unittest.main()
