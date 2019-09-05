#!/usr/bin/env python3
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Instrument visualmetrics.py to run in parallel.

Environment variables:

  VISUAL_METRICS_JOBS_JSON:
    A JSON blob containing the job descriptions.

    Can be overridden with the --jobs-json-path option set to a local file
    path.
"""

import argparse
import os
import json
import sys
from concurrent.futures import ProcessPoolExecutor, ThreadPoolExecutor
from functools import partial
from multiprocessing import cpu_count
from pathlib import Path

import attr
import requests
import structlog
import subprocess
from voluptuous import Required, Schema, Url

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

    #: json_url: The URL of the ``browsertime.json`` file.
    json_url = attr.ib(type=str)

    #: video_path: The path of the video file on disk.
    video_path = attr.ib(type=Path)

    #: video_url: The URl of the video file.
    video_url = attr.ib(type=str)


# NB: Keep in sync with try_task_config_schema in
#     taskcluster/taskgraph.decision.py
#: The schema for validating jobs.
JOB_SCHEMA = Schema(
    {
        Required("jobs"): [
            {
                Required("browsertime_json_url"): Url(),
                Required("video_url"): Url(),
            }
        ]
    }
)


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
            "Could not locate visualmetrics.py: expected it at %s"
            % visualmetrics_path
        )
        return 1

    if args.jobs_json_path:
        try:
            with open(args.jobs_json_path, "r") as f:
                jobs_json = json.load(f)
        except Exception as e:
            log.error(
                "Could not read jobs.json file: %s" % e,
                path=args.jobs_json_path,
                exc_info=True,
            )
            return 1

        log.info(
            "Loaded jobs.json from file",
            path=args.jobs_json_path,
            jobs_json=jobs_json,
        )

    else:
        raw_jobs_json = os.getenv("VISUAL_METRICS_JOBS_JSON")
        if raw_jobs_json is not None and isinstance(raw_jobs_json, bytes):
            raw_jobs_json = raw_jobs_json.decode("utf-8")
        elif raw_jobs_json is None:
            log.error(
                "Expected one of --jobs-json-path or "
                "VISUAL_METRICS_JOBS_JSON environment variable."
            )
            return 1

        try:
            jobs_json = json.loads(raw_jobs_json)
        except (TypeError, ValueError) as e:
            log.error(
                "Failed to decode VISUAL_METRICS_JOBS_JSON environment "
                "variable: %s" % e,
                value=raw_jobs_json,
            )
            return 1

        log.info("Parsed jobs.json from environment", jobs_json=jobs_json)

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
            if isinstance(result, Exception):
                log.error(
                    "Failed to run visualmetrics.py",
                    video_url=job.video_url,
                    error=result,
                )
            else:
                with (job.job_dir / "visual-metrics.json").open("wb") as f:
                    f.write(result)

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    with Path(WORKSPACE_DIR, "jobs.json").open("w") as f:
        json.dump(
            {
                "successful_jobs": [
                    {
                        "video_url": job.video_url,
                        "browsertime_json_url": job.json_url,
                        "path": (
                            str(job.job_dir.relative_to(WORKSPACE_DIR)) + "/"
                        ),
                    }
                    for job in downloaded_jobs
                ],
                "failed_jobs": [
                    {
                        "video_url": job.video_url,
                        "browsertime_json_url": job.json_url,
                    }
                    for job in failed_jobs
                ],
            },
            f,
        )

    subprocess.check_output(
        [
            "tar",
            "cJf",
            str(OUTPUT_DIR / "visual-metrics.tar.xz"),
            "-C",
            str(WORKSPACE_DIR),
            ".",
        ]
    )


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
                job["browsertime_json_url"],
                job_dir / "video",
                job["video_url"],
            )
        )

    downloaded_jobs = []
    failed_jobs = []

    with ThreadPoolExecutor(max_workers=8) as executor:
        for job, success in executor.map(
            partial(download_job, log), pending_jobs
        ):
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
    log = log.bind(json_url=job.json_url)
    try:
        download(job.video_url, job.video_path)
        download(job.json_url, job.json_path)
    except Exception as e:
        log.error(
            "Failed to download files for job: %s" % e,
            video_url=job.video_url,
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
       Either a string containing the JSON output of visualmetrics.py or an
       exception raised by :func:`subprocess.check_output`.
    """
    cmd = [
        "/usr/bin/python",
        str(visualmetrics_path),
        "--video",
        str(job.video_path),
    ]

    cmd.extend(options)

    try:
        return subprocess.check_output(cmd)
    except subprocess.CalledProcessError as e:
        return e


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
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--jobs-json-path",
        type=Path,
        metavar="PATH",
        help=(
            "The path to the jobs.josn file. If not present, the "
            "VISUAL_METRICS_JOBS_JSON environment variable will be used "
            "instead."
        ),
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
