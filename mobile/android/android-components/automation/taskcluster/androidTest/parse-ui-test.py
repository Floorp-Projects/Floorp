#!/usr/bin/python3

from __future__ import print_function

import argparse
import json
import sys
from pathlib import Path

import yaml


def parse_args(cmdln_args):
    parser = argparse.ArgumentParser(description="Parse UI test logs an results")
    parser.add_argument(
        "--output-md",
        type=argparse.FileType("w", encoding="utf-8"),
        help="Output markdown file.",
        required=True,
    )
    parser.add_argument(
        "--log",
        type=argparse.FileType("r", encoding="utf-8"),
        help="Log output of flank.",
        required=True,
    )
    parser.add_argument(
        "--results", type=Path, help="Directory containing flank results", required=True
    )
    parser.add_argument(
        "--exit-code", type=int, help="Exit code of flank.", required=True
    )
    parser.add_argument("--device-type", help="Type of device ", required=True)
    parser.add_argument(
        "--report-treeherder-failures",
        help="Report failures in treeherder format.",
        required=False,
        action="store_true",
    )

    return parser.parse_args(args=cmdln_args)


def extract_android_args(log):
    return yaml.safe_load(log.split("AndroidArgs\n")[1].split("RunTests\n")[0])


def main():
    args = parse_args(sys.argv[1:])

    log = args.log.read()
    matrix_ids = json.loads(args.results.joinpath("matrix_ids.json").read_text())
    # with args.results.joinpath("flank.yml") as f:
    #    flank_config = yaml.safe_load(f)

    android_args = extract_android_args(log)

    print = args.output_md.write

    print("# Devices\n")
    print(yaml.safe_dump(android_args["gcloud"]["device"]))

    print("# Results\n")
    print("| matrix | result | logs | details \n")
    print("| --- | --- | --- | --- |\n")
    for matrix, matrix_result in matrix_ids.items():
        print(
            "| {matrixId} | {outcome} | [logs]({webLink}) | {axes[0][details]}\n".format(
                **matrix_result
            )
        )
        if (
            args.report_treeherder_failures
            and matrix_result["outcome"] != "success"
            and matrix_result["outcome"] != "flaky"
        ):
            # write failures to test log in format known to treeherder logviewer
            sys.stdout.write(
                f"TEST-UNEXPECTED-FAIL | {matrix_result['outcome']} | {matrix_result['webLink']} | {matrix_result['axes'][0]['details']}\n"
            )

    print("---\n")
    print("# References & Documentation\n")
    print(
        "* [Automated UI Testing Documentation](https://github.com/mozilla-mobile/shared-docs/blob/main/android/ui-testing.md)\n"
    )
    print(
        "* Mobile Test Engineering on [Confluence](https://mozilla-hub.atlassian.net/wiki/spaces/MTE/overview) | [Slack](https://mozilla.slack.com/archives/C02KDDS9QM9) | [Alerts](https://mozilla.slack.com/archives/C0134KJ4JHL)\n"
    )


if __name__ == "__main__":
    main()
