#!/usr/bin/python3

import argparse
import sys
import xml
from pathlib import Path

from beautifultable import BeautifulTable
from junitparser import Attr, Failure, JUnitXml, TestCase, TestSuite


def parse_args(cmdln_args):
    parser = argparse.ArgumentParser(
        description="Parse and print UI test JUnit results"
    )
    parser.add_argument(
        "--results",
        type=Path,
        help="Directory containing task artifact results",
        required=True,
    )
    return parser.parse_args(args=cmdln_args)


class test_suite(TestSuite):
    flakes = Attr()


class test_case(TestCase):
    flaky = Attr()


def parse_print_failure_results(results):
    """
    Parses the given JUnit test results and prints a formatted table of failures and flaky tests.

    Args:
        results (JUnitXml): Parsed JUnit XML results.

    Returns:
        int: The number of test failures.

    The function processes each test suite and each test case within the suite.
    If a test case has a result that is an instance of Failure, it is added to the table.
    The test case is marked as 'Flaky' if the flaky attribute is set to "true", otherwise it is marked as 'Failure'.

    Example of possible JUnit XML (FullJUnitReport.xml):
    <testsuites>
        <testsuite name="ExampleSuite" tests="2" failures="1" flakes="1" time="0.003">
            <testcase classname="example.TestClass" name="testSuccess" flaky="true" time="0.001">
                <failure message="Assertion failed">Expected true but was false</failure>
            </testcase>
            <testcase classname="example.TestClass" name="testFailure" time="0.002">
                <failure message="Assertion failed">Expected true but was false</failure>
                <failure message="Assertion failed">Expected true but was false</failure>
            </testcase>
        </testsuite>
    </testsuites>
    """

    table = BeautifulTable(maxwidth=256)
    table.columns.header = ["UI Test", "Outcome", "Details"]
    table.columns.alignment = BeautifulTable.ALIGN_LEFT
    table.set_style(BeautifulTable.STYLE_GRID)

    failure_count = 0

    # Dictionary to store the last seen failure details for each test case
    last_seen_failures = {}

    for suite in results:
        cur_suite = test_suite.fromelem(suite)
        for case in cur_suite:
            cur_case = test_case.fromelem(case)
            if cur_case.result:
                for entry in case.result:
                    if isinstance(entry, Failure):
                        flaky_status = getattr(cur_case, "flaky", "false") == "true"
                        if flaky_status:
                            test_id = "%s#%s" % (case.classname, case.name)
                            details = (
                                entry.text.replace("\t", " ") if entry.text else ""
                            )
                            # Check if the current failure details are different from the last seen ones
                            if details != last_seen_failures.get(test_id, ""):
                                table.rows.append(
                                    [
                                        test_id,
                                        "Flaky",
                                        details,
                                    ]
                                )
                            last_seen_failures[test_id] = details
                        else:
                            test_id = "%s#%s" % (case.classname, case.name)
                            details = (
                                entry.text.replace("\t", " ") if entry.text else ""
                            )
                            # Check if the current failure details are different from the last seen ones
                            if details != last_seen_failures.get(test_id, ""):
                                table.rows.append(
                                    [
                                        test_id,
                                        "Failure",
                                        details,
                                    ]
                                )
                                print(f"TEST-UNEXPECTED-FAIL | {test_id} | {details}")
                                failure_count += 1
                            # Update the last seen failure details for this test case
                            last_seen_failures[test_id] = details

    print(table)
    return failure_count


def load_results_file(filename):
    ret = None
    try:
        f = open(filename, "r")
        try:
            ret = JUnitXml.fromfile(f)
        except xml.etree.ElementTree.ParseError as e:
            print(f"Error parsing {filename} file: {e}")
        finally:
            f.close()
    except IOError as e:
        print(e)

    return ret


def main():
    args = parse_args(sys.argv[1:])

    failure_count = 0
    junitxml = load_results_file(args.results.joinpath("FullJUnitReport.xml"))
    if junitxml:
        failure_count = parse_print_failure_results(junitxml)
    return failure_count


if __name__ == "__main__":
    sys.exit(main())
