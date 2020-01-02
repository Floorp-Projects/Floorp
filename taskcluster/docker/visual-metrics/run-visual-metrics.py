#!/usr/bin/env python3
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Instrument visualmetrics.py to run in parallel.
"""

import argparse
import os
import json
import shutil
import sys
import tarfile
from concurrent.futures import ProcessPoolExecutor, ThreadPoolExecutor
from functools import partial
from multiprocessing import cpu_count
from pathlib import Path

import attr
import requests
import structlog
import subprocess

from jsonschema import validate
from voluptuous import Required, Schema

#: The workspace directory where files will be downloaded, etc.
WORKSPACE_DIR = Path("/", "builds", "worker", "workspace")

#: The directory where job artifacts will be stored.
WORKSPACE_JOBS_DIR = WORKSPACE_DIR / "jobs"

#: The directory where artifacts from this job will be placed.
OUTPUT_DIR = Path("/", "builds", "worker", "artifacts")

#: A job to process through visualmetrics.py
@attr.s
class Job:
    #: The name of the test.
    test_name = attr.ib(type=str)

    #: The directory for all the files pertaining to the job.
    job_dir = attr.ib(type=Path)

    #: json_path: The path to the ``browsertime.json`` file on disk.
    json_path = attr.ib(type=Path)

    #: json_location: The location or URL of the ``browsertime.json`` file.
    json_location = attr.ib(type=str)

    #: video_path: The path of the video file on disk.
    video_path = attr.ib(type=Path)

    #: video_location: The path or URL of the video file.
    video_location = attr.ib(type=str)


# NB: Keep in sync with try_task_config_schema in
#     taskcluster/taskgraph.decision.py
#: The schema for validating jobs.
JOB_SCHEMA = Schema(
    {
        Required("jobs"): [
            {
                Required("test_name"): str,
                Required("json_location"): str,
                Required("video_location"): str,
            }
        ]
    }
)

#: The schema for validating application data.
APP_SCHEMA = Schema(
    {
        Required("application"): {
            Required("name"): str,
            Required("version"): str,
        }
    }
)

PERFHERDER_SCHEMA = Path("/", "builds", "worker", "performance-artifact-schema.json")
with PERFHERDER_SCHEMA.open() as f:
    PERFHERDER_SCHEMA = json.loads(f.read())


def run_command(log, cmd):
    """Run a command using subprocess.check_output

    Args:
        log: The structlog logger instance.
        cmd: the command to run as a list of strings.

    Returns:
        A tuple of the process' exit status and standard output.
    """
    log.info("Running command", cmd=cmd)
    try:
        res = subprocess.check_output(cmd)
        log.info("Command succeeded", result=res)
        return 0, res
    except subprocess.CalledProcessError as e:
        log.info("Command failed", cmd=cmd, status=e.returncode, output=e.output)
        return e.returncode, e.output


def append_result(log, suites, test_name, app_name, name, result):
    """Appends a ``name`` metrics result in the ``test_name`` suite.

    Args:
        log: The structlog logger instance.
        suites: A mapping containing the suites.
        test_name: The name of the test.
        app_name: The name of the browser application used in the test.
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
    if test_name not in suites:
        # TODO: Once Bug 1593198 lands, perfherder will recognize the 'application'; then
        # it won't be necessary anymore to have the application name added to 'extraOptions'.
        # So when Bug 1593198 lands, remove the 'extraOptions' key below.
        suites[test_name] = {"name": test_name, "subtests": {}, "extraOptions": [app_name]}

    subtests = suites[test_name]["subtests"]
    if name not in subtests:
        subtests[name] = {
            "name": name,
            "replicates": [result],
            "lowerIsBetter": True,
            "unit": "ms",
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
    series = subtest["replicates"][1:]
    subtest["value"] = float(sum(series)) / float(len(series))
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
        json_path: Filename and path of json file to parse.
        schema: Schema to validate the json file against.

    Returns:
        The read json content (dictionary).
    """
    try:
        with open(str(json_path), "r") as f:
            read_json = json.load(f)
    except Exception as e:
        log.error(
            "Could not read json file: %s" % e, path=json_path, exc_info=True
        )
        return 1
    log.info("Loaded json from file", path=json_path, read_json=read_json)

    try:
        schema(read_json)
    except Exception as e:
        log.error("Failed to parse json: %s" % e)
        return 1
    return read_json


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

    visualmetrics_path = Path(fetch_dir) / "visualmetrics.py"
    if not visualmetrics_path.exists():
        log.error(
            "Could not locate visualmetrics.py", expected_path=str(visualmetrics_path)
        )
        return 1

    results_path = Path(args.browsertime_results).parent
    try:
        with tarfile.open(str(args.browsertime_results)) as tar:
            tar.extractall(path=str(results_path))
    except Exception:
        log.error(
            "Could not read extract browsertime results archive",
            path=args.browsertime_results,
            exc_info=True,
        )
        return 1
    log.info("Extracted browsertime results", path=args.browsertime_results)

    jobs_json_path = results_path / "browsertime-results" / "jobs.json"
    jobs_json = read_json(jobs_json_path, JOB_SCHEMA)

    app_json_path = results_path / "browsertime-results" / "application.json"
    app_json = read_json(app_json_path, APP_SCHEMA)

    try:
        downloaded_jobs, failed_downloads = download_inputs(log, jobs_json["jobs"])
    except Exception as e:
        log.error("Failed to download jobs: %s" % e, exc_info=True)
        return 1

    failed_runs = 0
    suites = {}

    with ProcessPoolExecutor(max_workers=cpu_count()) as executor:
        for job, result in zip(
            downloaded_jobs,
            executor.map(
                partial(
                    run_visual_metrics,
                    visualmetrics_path=visualmetrics_path,
                    options=args.visual_metrics_options,
                ),
                downloaded_jobs,
            ),
        ):
            returncode, res = result
            if returncode != 0:
                log.error(
                    "Failed to run visualmetrics.py",
                    video_location=job.video_location,
                    error=res,
                )
                failed_runs += 1
            else:
                # Python 3.5 requires a str object (not 3.6+)
                res = json.loads(res.decode("utf8"))
                for name, value in res.items():
                    append_result(log, suites, job.test_name, app_json["application"]["name"],
                                  name, value)

    suites = [get_suite(suite) for suite in suites.values()]

    perf_data = {
        "framework": {"name": "browsertime"},
        "application": app_json["application"],
        "type": "vismet",
        "suites": suites,
    }

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
                "total_jobs": len(downloaded_jobs) + len(failed_downloads),
                "successful_runs": len(downloaded_jobs) - failed_runs,
                "failed_runs": failed_runs,
                "failed_downloads": [
                    {
                        "video_location": job.video_location,
                        "json_location": job.json_location,
                        "test_name": job.test_name,
                    }
                    for job in failed_downloads
                ],
            },
            f,
        )

    # If there's one failure along the way, we want to return > 0
    # to trigger a red job in TC.
    return len(failed_downloads) + failed_runs


def download_inputs(log, raw_jobs):
    """Download the inputs for all jobs in parallel.

    Args:
        log: The structlog logger instance.
        raw_jobs: The list of unprocessed jobs from the ``jobs.json`` input file.

    Returns:
        A tuple of the successfully downloaded jobs and the failed to download jobs.
    """
    WORKSPACE_DIR.mkdir(parents=True, exist_ok=True)

    pending_jobs = []
    for i, job in enumerate(raw_jobs):
        job_dir = WORKSPACE_JOBS_DIR / str(i)
        job_dir.mkdir(parents=True, exist_ok=True)
        pending_jobs.append(
            Job(
                job["test_name"],
                job_dir,
                job_dir / "browsertime.json",
                job["json_location"],
                job_dir / "video",
                job["video_location"],
            )
        )

    downloaded_jobs = []
    failed_jobs = []

    with ThreadPoolExecutor(max_workers=8) as executor:
        for job, success in executor.map(partial(download_job, log), pending_jobs):
            if success:
                downloaded_jobs.append(job)
            else:
                job.job_dir.rmdir()
                failed_jobs.append(job)

    return downloaded_jobs, failed_jobs


def download_job(log, job):
    """Download the files for a given job.

    Args:
        log: The structlog logger instance.
        job: The job to download.

    Returns:
        A tuple of the job and whether or not the download was successful.

        The returned job will be updated so that it's :attr:`Job.video_path`
        attribute is updated to match the file path given by the video file
        in the ``browsertime.json`` file.
    """
    fetch_dir = Path(os.getenv("MOZ_FETCHES_DIR"))
    log = log.bind(json_location=job.json_location)
    try:
        download_or_copy(fetch_dir / job.video_location, job.video_path)
        download_or_copy(fetch_dir / job.json_location, job.json_path)
    except Exception as e:
        log.error(
            "Failed to download files for job: %s" % e,
            video_location=job.video_location,
            exc_info=True,
        )
        return job, False

    try:
        with job.json_path.open("r", encoding="utf-8") as f:
            browsertime_json = json.load(f)
    except OSError as e:
        log.error("Could not read browsertime.json: %s" % e)
        return job, False
    except ValueError as e:
        log.error("Could not parse browsertime.json as JSON: %s" % e)
        return job, False

    try:
        video_path = job.job_dir / browsertime_json[0]["files"]["video"][0]
    except KeyError:
        log.error("Could not read video path from browsertime.json file")
        return job, False

    video_path.parent.mkdir(parents=True, exist_ok=True)

    job.video_path.rename(video_path)
    job.video_path = video_path

    return job, True


def download_or_copy(url_or_location, path):
    """Download the resource at the given URL or path to the local path.

    Args:
        url_or_location: The URL or path of the resource to download or copy.
        path: The local path to download or copy the resource to.

    Raises:
        OSError:
            Raised if an IO error occurs while writing the file.

        requests.exceptions.HTTPError:
            Raised when an HTTP error (including e.g., HTTP 404) occurs.
    """
    url_or_location = str(url_or_location)
    if os.path.exists(url_or_location):
        shutil.copyfile(url_or_location, str(path))
        return
    elif not url_or_location.startswith("http"):
        raise IOError(
            "%s does not seem to be an URL or an existing file" % url_or_location
        )

    download(url_or_location, path)


def download(url, path):
    """Download the resource at the given URL to the local path.

    Args:
        url: The URL of the resource to download.
        path: The local path to download the resource to.

    Raises:
        OSError:
            Raised if an IO error occurs while writing the file.

        requests.exceptions.HTTPError:
            Raised when an HTTP error (including e.g., HTTP 404) occurs.
    """
    request = requests.get(url, stream=True)
    request.raise_for_status()

    path.parent.mkdir(parents=True, exist_ok=True)

    with path.open("wb") as f:
        for chunk in request:
            f.write(chunk)


def run_visual_metrics(job, visualmetrics_path, options):
    """Run visualmetrics.py on the input job.

    Returns:
       A returncode and a string containing the output of visualmetrics.py
    """
    cmd = ["/usr/bin/python", str(visualmetrics_path), "--video", str(job.video_path)]
    cmd.extend(options)
    return run_command(log, cmd)


if __name__ == "__main__":
    structlog.configure(
        processors=[
            structlog.processors.TimeStamper(fmt="iso"),
            structlog.processors.format_exc_info,
            structlog.dev.ConsoleRenderer(colors=False),
        ],
        cache_logger_on_first_use=True,
    )

    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument(
        "--browsertime-results",
        type=Path,
        metavar="PATH",
        help="The path to the browsertime results tarball.",
        required=True,
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
