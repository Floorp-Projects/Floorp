# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import argparse


"""
Argument parsing is going to get very complex very quickly so
we leave the parser in it's own file.
"""


def parse_args():
    parser = argparse.ArgumentParser(
        description="Process data into a customized data format "
        "and analyze it using standardized technique."
    )
    parser.add_argument(
        "--config",
        "-c",
        type=str,
        required=True,
        help="Configuration to use for processing and analyzing data.",
    )
    parser.add_argument(
        "--debug",
        action="store_true",
        default=False,
        help="Enable additional debug logging.",
    )
    parser.add_argument(
        "--no-iodide",
        "-ni",
        action="store_true",
        default=False,
        help="Run this tool without starting the iodide server at the end.",
    )
    parser.add_argument(
        "--sort-files",
        "-sf",
        action="store_true",
        default=False,
        help="Sort the entries of output json files by the name of resource files.",
    )
    return parser.parse_args()
