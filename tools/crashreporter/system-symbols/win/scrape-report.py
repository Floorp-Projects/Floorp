#!/usr/bin/env python
#
# Copyright 2016 Mozilla
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import csv
import json
import logging
import os
import sys

import requests
import urlparse

log = logging.getLogger()


def fetch_missing_symbols_from_crash(file_or_crash):
    if os.path.isfile(file_or_crash):
        log.info("Fetching missing symbols from JSON file: %s" % file_or_crash)
        j = {"json_dump": json.load(open(file_or_crash, "rb"))}
    else:
        if "report/index/" in file_or_crash:
            crash_id = urlparse.urlparse(file_or_crash).path.split("/")[-1]
        else:
            crash_id = file_or_crash
        url = (
            "https://crash-stats.mozilla.org/api/ProcessedCrash/"
            "?crash_id={crash_id}&datatype=processed".format(crash_id=crash_id)
        )
        log.info("Fetching missing symbols from crash: %s" % url)
        r = requests.get(url)
        if r.status_code != 200:
            log.error("Failed to fetch crash %s" % url)
            return set()
        j = r.json()
    return set(
        [
            (m["debug_file"], m["debug_id"], m["filename"], m["code_id"])
            for m in j["json_dump"]["modules"]
            if "missing_symbols" in m
        ]
    )


def main():
    logging.basicConfig()
    log.setLevel(logging.DEBUG)
    urllib3_logger = logging.getLogger("urllib3")
    urllib3_logger.setLevel(logging.ERROR)

    if len(sys.argv) < 2:
        log.error("Specify a crash URL or ID")
        sys.exit(1)
    symbols = fetch_missing_symbols_from_crash(sys.argv[1])
    log.info("Found %d missing symbols" % len(symbols))
    c = csv.writer(sys.stdout)
    c.writerow(["debug_file", "debug_id", "code_file", "code_id"])
    for row in symbols:
        c.writerow(row)


if __name__ == "__main__":
    main()
