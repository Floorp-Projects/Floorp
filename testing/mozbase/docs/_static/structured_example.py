import argparse
import sys
import traceback
import types

from mozlog import commandline, get_default_logger


class TestAssertion(Exception):
    pass


def assert_equals(a, b):
    if a != b:
        raise TestAssertion("%r not equal to %r" % (a, b))


def expected(status):
    def inner(f):
        def test_func():
            f()
        test_func.__name__ = f.__name__
        test_func._expected = status
        return test_func
    return inner


def test_that_passes():
    assert_equals(1, int("1"))


def test_that_fails():
    assert_equals(1, int("2"))


def test_that_has_an_error():
    assert_equals(2, 1 + "1")


@expected("FAIL")
def test_expected_fail():
    assert_equals(2 + 2, 5)


class TestRunner(object):

    def __init__(self):
        self.logger = get_default_logger(component='TestRunner')

    def gather_tests(self):
        for item in globals().itervalues():
            if isinstance(item, types.FunctionType) and item.__name__.startswith("test_"):
                yield item.__name__, item

    def run(self):
        tests = list(self.gather_tests())

        self.logger.suite_start(tests=[name for name, func in tests])
        self.logger.info("Running tests")
        for name, func in tests:
            self.run_test(name, func)
        self.logger.suite_end()

    def run_test(self, name, func):
        self.logger.test_start(name)
        status = None
        message = None
        expected = func._expected if hasattr(func, "_expected") else "PASS"
        try:
            func()
        except TestAssertion as e:
            status = "FAIL"
            message = e.message
        except:
            status = "ERROR"
            message = traceback.format_exc()
        else:
            status = "PASS"
        self.logger.test_end(name, status=status, expected=expected, message=message)


def get_parser():
    parser = argparse.ArgumentParser()
    return parser


def main():
    parser = get_parser()
    commandline.add_logging_group(parser)

    args = parser.parse_args()

    logger = commandline.setup_logging("structured-example", args, {"raw": sys.stdout})

    runner = TestRunner()
    try:
        runner.run()
    except:
        logger.critical("Error during test run:\n%s" % traceback.format_exc())


if __name__ == "__main__":
    main()
