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
            {Required("json_location"): str, Required("video_location"): str}
        ]
    }
)


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
    try:
        with open(str(jobs_json_path), "r") as f:
            jobs_json = json.load(f)
    except Exception as e:
        log.error(
            "Could not read jobs.json file: %s" % e, path=jobs_json_path, exc_info=True
        )
        return 1

    log.info("Loaded jobs.json from file", path=jobs_json_path, jobs_json=jobs_json)
    try:
        JOB_SCHEMA(jobs_json)
    except Exception as e:
        log.error("Failed to parse jobs.json: %s" % e)
        return 1

    try:
        downloaded_jobs, failed_jobs = download_inputs(log, jobs_json["jobs"])
    except Exception as e:
        log.error("Failed to download jobs: %s" % e, exc_info=True)
        return 1

    runs_failed = 0

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
                runs_failed += 1
            else:
                path = job.job_dir / "visual-metrics.json"
                with path.open("wb") as f:
                    log.info("Writing job result", path=path)
                    f.write(res)

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    with Path(WORKSPACE_DIR, "jobs.json").open("w") as f:
        json.dump(
            {
                "successful_jobs": [
                    {
                        "video_location": job.video_location,
                        "json_location": job.json_location,
                        "path": (str(job.job_dir.relative_to(WORKSPACE_DIR)) + "/"),
                    }
                    for job in downloaded_jobs
                ],
                "failed_jobs": [
                    {
                        "video_location": job.video_location,
                        "json_location": job.json_location,
                    }
                    for job in failed_jobs
                ],
            },
            f,
        )

    archive = OUTPUT_DIR / "visual-metrics.tar.xz"
    log.info("Creating the tarfile", tarfile=archive)
    returncode, res = run_command(
        log, ["tar", "cJf", str(archive), "-C", str(WORKSPACE_DIR), "."]
    )
    if returncode != 0:
        raise Exception("Could not tar the results")

    # If there's one failure along the way, we want to return > 0
    # to trigger a red job in TC.
    return len(failed_jobs) + runs_failed


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
        with job.json_path.open("r") as f:
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
