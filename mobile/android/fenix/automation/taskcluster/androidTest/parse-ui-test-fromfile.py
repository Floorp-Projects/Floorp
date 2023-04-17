#!/usr/bin/python3

import argparse
import sys
import xml
from pathlib import Path

from beautifultable import BeautifulTable
from junitparser import Attr, Failure, JUnitXml, TestSuite


def parse_args(cmdln_args):
    parser = argparse.ArgumentParser(description="Parse and print UI test JUnit results")
    parser.add_argument(
        "--results", type=Path,
        help="Directory containing task artifact results", required=True
    )
    return parser.parse_args(args=cmdln_args)


class test_suite(TestSuite):
    flakes = Attr()


def parse_print_failure_results(results):
    table = BeautifulTable(maxwidth=256)
    table.columns.header = (['UI Test', 'Outcome', 'Details'])
    table.columns.alignment = BeautifulTable.ALIGN_LEFT
    table.set_style(BeautifulTable.STYLE_GRID)

    for suite in results:
        cur_suite = test_suite.fromelem(suite)
        if cur_suite.flakes != '0':
            for case in suite:
                for entry in case.result:
                    if case.result:
                        table.rows.append(
                            ["%s#%s" % (case.classname, case.name), "Flaky", entry.text.replace('\t', ' ')])
                        break
        else:
            for case in suite:
                for entry in case.result:
                    if isinstance(entry, Failure):
                        table.rows.append(
                            ["%s#%s" % (case.classname, case.name), "Failure", entry.text.replace('\t', ' ')])
                        break
    print(table)


def load_results_file(filename):
    ret = None
    try:
        f = open(filename, 'r')
        try:
            ret = JUnitXml.fromfile(f)
        except xml.etree.ElementTree.ParseError as e:
            print(f'Error parsing {filename} file: {e}')
        finally:
            f.close()
    except IOError as e:
        print(e)

    return ret


def main():
    args = parse_args(sys.argv[1:])

    junitxml = load_results_file(args.results.joinpath('FullJUnitReport.xml'))
    if junitxml:
        parse_print_failure_results(junitxml)


if __name__ == '__main__':
    main()
