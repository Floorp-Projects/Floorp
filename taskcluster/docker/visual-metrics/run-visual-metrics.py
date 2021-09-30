#!/usr/bin/env python3
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Instrument visualmetrics.py to run in parallel."""

import argparse
import json
import logging
import os
import statistics
import subprocess
import sys
import tarfile
import time
from concurrent.futures import ProcessPoolExecutor
from functools import partial
from multiprocessing import cpu_count
from pathlib import Path

import attr
import structlog
from jsonschema import validate
from voluptuous import ALLOW_EXTRA, Required, Schema


#: The max run time for a command (5 minutes)
MAX_TIME = 300


#: The directory where artifacts from this job will be placed.
OUTPUT_DIR = Path("/", "builds", "worker", "artifacts")


#: A job to process through visualmetrics.py
@attr.s
class Job:
    #: The name of the test.
    test_name = attr.ib(type=str)

    #: A unique number for the job.
    count = attr.ib(type=int)

    #: The extra options for this job.
    extra_options = attr.ib(type=str)

    #: If true, we allow 0's in the vismet results
    accept_zero_vismet = attr.ib(type=bool)

    #: json_path: The path to the ``browsertime.json`` file on disk.
    json_path = attr.ib(type=Path)

    #: video_path: The path of the video file on disk.
    video_path = attr.ib(type=Path)


#: The schema for validating jobs.
JOB_SCHEMA = Schema(
    {
        Required("jobs"): [
            {
                Required("test_name"): str,
                Required("browsertime_json_path"): str,
                Required("extra_options"): [str],
                Required("accept_zero_vismet"): bool,
            }
        ],
        Required("application"): {Required("name"): str, "version": str},
        Required("extra_options"): [str],
    }
)

#: A partial schema for browsertime.json files.
BROWSERTIME_SCHEMA = Schema(
    [{Required("files"): {Required("video"): [str]}}], extra=ALLOW_EXTRA
)

SHOULD_ALERT = {
    "ContentfulSpeedIndex": True,
    "FirstVisualChange": True,
    "LastVisualChange": True,
    "PerceptualSpeedIndex": True,
    "SpeedIndex": True,
    "videoRecordingStart": False,
}

with Path("/", "builds", "worker", "performance-artifact-schema.json").open() as f:
    PERFHERDER_SCHEMA = json.loads(f.read())


def run_command(log, cmd, job_count):
    """Run a command using subprocess.check_output

    Args:
        log: The structlog logger instance.
        cmd: the command to run as a list of strings.

    Returns:
        A tuple of the process' exit status and standard output.
    """
    log.info("Running command", cmd=cmd)
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    lines = []
    res = None
    start = time.time()
    while time.time() - start <= MAX_TIME:
        time.sleep(0.1)
        output = process.stdout.readline()
        if output == b"" and process.poll() is not None:
            break
        if output:
            res = output.strip()
            lines.append(res.decode("utf-8", "ignore"))
        else:
            time.sleep(5)

    if time.time() - start > MAX_TIME:
        log.error(
            "TEST-UNEXPECTED-FAIL | Timed out waiting for response from command",
            cmd=cmd,
        )
        return 1, "Timed out"

    rc = process.poll()
    job_prefix = "[JOB-" + str(job_count) + "] "
    for line in lines:
        # Some output doesn't start with the levels because it comes
        # from FFMPEG rather than the script itself
        if line.startswith(("[INFO]", "[WARNING]", "[CRITICAL]", "[ERROR]")):
            splitline = line.split(" - ")
            level = splitline[0]
            line = " - ".join(splitline[1:])
        else:
            level = "[INFO]"

        newline = job_prefix + line
        if level.strip() in ("[ERROR]", "[CRITICAL]"):
            if rc == 0:
                rc = 1
            log.error("TEST-UNEXPECTED-FAIL | " + newline)
        elif level == "[WARNING]":
            log.warning(newline)
        else:
            log.info(newline)

    return rc, res


def append_result(log, suites, test_name, name, result, extra_options):
    """Appends a ``name`` metrics result in the ``test_name`` suite.

    Args:
        log: The structlog logger instance.
        suites: A mapping containing the suites.
        test_name: The name of the test.
        name: The name of the metrics.
        result: The value to append.
    """
    if name.endswith("Progress"):
        return
    try:
        result = int(result)
    except ValueError:
        log.error("Could not convert value", name=name)
        log.error("%s" % result)
        result = 0

    orig_test_name = test_name
    if test_name in suites and suites[test_name]["extraOptions"] != extra_options:
        missing = set(extra_options) - set(suites[test_name]["extraOptions"])
        test_name = test_name + "-".join(list(missing))

    subtests = suites.setdefault(
        test_name,
        {
            "name": orig_test_name,
            "tags": extra_options + ["visual"],
            "subtests": {},
            "extraOptions": extra_options,
        },
    )["subtests"]

    if name not in subtests:
        subtests[name] = {
            "name": name,
            "replicates": [result],
            "lowerIsBetter": True,
            "unit": "ms",
            "shouldAlert": SHOULD_ALERT.get(name, False),
        }
    else:
        subtests[name]["replicates"].append(result)


def compute_median(subtest):
    """Adds in the subtest the ``value`` field, which is the average of all
    replicates.

    Args:
        subtest: The subtest containing all replicates.

    Returns:
        The subtest.
    """
    if "replicates" not in subtest:
        return subtest
    subtest["value"] = statistics.median(subtest["replicates"])
    return subtest


def get_suite(suite):
    """Returns the suite with computed medians in its subtests.

    Args:
        suite: The suite to convert.

    Returns:
        The suite.
    """
    suite["subtests"] = [
        compute_median(subtest) for subtest in suite["subtests"].values()
    ]
    return suite


def read_json(json_path, schema):
    """Read the given json file and verify against the provided schema.

    Args:
        json_path: Path of json file to parse.
        schema: A callable to validate the JSON's schema.

    Returns:
        The contents of the file at ``json_path`` interpreted as JSON.
    """
    try:
        with open(str(json_path), "r", encoding="utf-8", errors="ignore") as f:
            data = json.load(f)
    except Exception:
        log.error("Could not read JSON file", path=json_path, exc_info=True)
        raise

    log.info("Loaded JSON from file", path=json_path)

    try:
        schema(data)
    except Exception:
        log.error("JSON failed to validate", exc_info=True)
        raise

    return data


def main(log, args):
    """Run visualmetrics.py in parallel.

    Args:
        log: The structlog logger instance.
        args: The parsed arguments from the argument parser.

    Returns:
        The return code that the program will exit with.
    """
    fetch_dir = os.getenv("MOZ_FETCHES_DIR")
    if not fetch_dir:
        log.error("Expected MOZ_FETCHES_DIR environment variable.")
        return 1

    fetch_dir = Path(fetch_dir)

    visualmetrics_path = fetch_dir / "visualmetrics.py"
    if not visualmetrics_path.exists():
        log.error(
            "Could not locate visualmetrics.py", expected_path=str(visualmetrics_path)
        )
        return 1

    browsertime_results_path = fetch_dir / "browsertime-results.tgz"

    try:
        with tarfile.open(str(browsertime_results_path)) as tar:
            tar.extractall(path=str(fetch_dir))
    except Exception:
        log.error(
            "Could not read/extract browsertime results archive",
            path=browsertime_results_path,
            exc_info=True,
        )
        return 1
    log.info("Extracted browsertime results", path=browsertime_results_path)

    try:
        jobs_json_path = fetch_dir / "browsertime-results" / "jobs.json"
        jobs_json = read_json(jobs_json_path, JOB_SCHEMA)
    except Exception:
        log.error(
            "Could not open the jobs.json file", path=jobs_json_path, exc_info=True
        )
        return 1

    jobs = []
    count = 0

    for job in jobs_json["jobs"]:
        browsertime_json_path = fetch_dir / job["browsertime_json_path"]

        try:
            browsertime_json = read_json(browsertime_json_path, BROWSERTIME_SCHEMA)
        except Exception:
            log.error(
                "Could not open a browsertime.json file",
                path=browsertime_json_path,
                exc_info=True,
            )
            return 1

        for site in browsertime_json:
            for video in site["files"]["video"]:
                count += 1
                jobs.append(
                    Job(
                        test_name=job["test_name"],
                        extra_options=len(job["extra_options"]) > 0
                        and job["extra_options"]
                        or jobs_json["extra_options"],
                        accept_zero_vismet=job["accept_zero_vismet"],
                        json_path=browsertime_json_path,
                        video_path=browsertime_json_path.parent / video,
                        count=count,
                    )
                )

    failed_runs = 0
    suites = {}

    with ProcessPoolExecutor(max_workers=cpu_count()) as executor:
        for job, result in zip(
            jobs,
            executor.map(
                partial(
                    run_visual_metrics,
                    visualmetrics_path=visualmetrics_path,
                    options=args.visual_metrics_options,
                ),
                jobs,
            ),
        ):
            returncode, res = result
            if returncode != 0:
                log.error(
                    "Failed to run visualmetrics.py",
                    video_path=job.video_path,
                    error=res,
                )
                failed_runs += 1
            else:
                for name, value in res.items():
                    append_result(
                        log, suites, job.test_name, name, value, job.extra_options
                    )

    suites = [get_suite(suite) for suite in suites.values()]

    perf_data = {
        "framework": {"name": "browsertime"},
        "application": jobs_json["application"],
        "type": "pageload",
        "suites": suites,
    }

    # TODO: Try to get the similarity for all possible tests, this means that we
    # will also get a comparison of recorded vs. live sites to check the on-going
    # quality of our recordings.
    # Bug 1674927 - Similarity metric is disabled until we figure out
    # why it had a huge increase in run time.

    # Validates the perf data complies with perfherder schema.
    # The perfherder schema uses jsonschema so we can't use voluptuous here.
    validate(perf_data, PERFHERDER_SCHEMA)

    raw_perf_data = json.dumps(perf_data)
    with Path(OUTPUT_DIR, "perfherder-data.json").open("w") as f:
        f.write(raw_perf_data)
    # Prints the data in logs for Perfherder to pick it up.
    log.info("PERFHERDER_DATA: %s" % raw_perf_data)

    # Lists the number of processed jobs, failures, and successes.
    with Path(OUTPUT_DIR, "summary.json").open("w") as f:
        json.dump(
            {
                "total_jobs": len(jobs),
                "successful_runs": len(jobs) - failed_runs,
                "failed_runs": failed_runs,
            },
            f,
        )

    # If there's one failure along the way, we want to return > 0
    # to trigger a red job in TC.
    return failed_runs


def run_visual_metrics(job, visualmetrics_path, options):
    """Run visualmetrics.py on the input job.

    Returns:
       A returncode and a string containing the output of visualmetrics.py
    """
    cmd = [
        "/usr/bin/python",
        str(visualmetrics_path),
        "-vvv",
        "--logformat",
        "[%(levelname)s] - %(message)s",
        "--video",
        str(job.video_path),
    ]
    cmd.extend(options)
    rc, res = run_command(log, cmd, job.count)

    if rc == 0:
        # Python 3.5 requires a str object (not 3.6+)
        res = json.loads(res.decode("utf8"))

        failed_tests = []
        if not job.accept_zero_vismet:
            # Ensure that none of these values are at 0 which
            # is indicative of a failling test
            monitored_tests = [
                "contentfulspeedindex",
                "lastvisualchange",
                "perceptualspeedindex",
                "speedindex",
            ]
            for metric, val in res.items():
                if metric.lower() in monitored_tests and val == 0:
                    failed_tests.append(metric)

        if failed_tests:
            log.error(
                "TEST-UNEXPECTED-FAIL | Some visual metrics have an erroneous value of 0."
            )
            log.info("Tests which failed: %s" % str(failed_tests))
            rc += 1

    return rc, res


if __name__ == "__main__":
    logging.basicConfig(format="%(levelname)s - %(message)s", level=logging.INFO)
    structlog.configure(
        processors=[
            structlog.processors.format_exc_info,
            structlog.dev.ConsoleRenderer(colors=False),
        ],
        logger_factory=structlog.stdlib.LoggerFactory(),
        cache_logger_on_first_use=True,
    )

    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument(
        "visual_metrics_options",
        type=str,
        metavar="VISUAL-METRICS-OPTIONS",
        help="Options to pass to visualmetrics.py",
        nargs="*",
    )

    args = parser.parse_args()
    log = structlog.get_logger()

    try:
        sys.exit(main(log, args))
    except Exception as e:
        log.error("Unhandled exception: %s" % e, exc_info=True)
        sys.exit(1)
