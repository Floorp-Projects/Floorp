#!/usr/bin/python3

from __future__ import print_function

import sys
import argparse
from pathlib import Path
import json
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
    return parser.parse_args(args=cmdln_args)


def extract_android_args(log):
    return yaml.safe_load(log.split("AndroidArgs\n")[1].split("RunTests\n")[0])


def main():
    args = parse_args(sys.argv[1:])

    log = args.log.read()
    matrix_ids = json.loads(args.results.joinpath("matrix_ids.json").read_text())
    #with args.results.joinpath("flank.yml") as f:
    #    flank_config = yaml.safe_load(f)

    android_args = extract_android_args(log)

    print = args.output_md.write

    print("# Devices\n")
    print(yaml.safe_dump(android_args["gcloud"]["device"]))

    print("# Results\n")
    print("| matrix | result | logs | details \n")
    print("| --- | --- | --- | --- |\n")
    for matrix, matrix_result in matrix_ids.items():
        print("| {matrixId} | {outcome} | [logs]({webLink}) | {testAxises[0][details]}\n".format(**matrix_result))


if __name__ == "__main__":
    main()

