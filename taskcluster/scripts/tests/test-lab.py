#!/usr/bin/env python3

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Firebase Test Lab (Flank) test runner script for Taskcluster
# This script is used to run UI tests on Firebase Test Lab using Flank
# It requires a service account key file to authenticate with Firebase Test Lab
# It also requires the `gcloud` command line tool to be installed and configured
# Lastly it requires the `flank.jar` file to be present in the `test-tools` directory set up in the task definition
# The service account key file is stored in the `secrets` section of the task definition

# Flank: https://flank.github.io/flank/

import argparse
import logging
import os
import subprocess
import sys
from enum import Enum
from pathlib import Path
from typing import List, Optional, Union


# Worker paths and binaries
class Worker(Enum):
    JAVA_BIN = "/usr/bin/java"
    FLANK_BIN = "/builds/worker/test-tools/flank.jar"
    RESULTS_DIR = "/builds/worker/artifacts/results"
    ARTIFACTS_DIR = "/builds/worker/artifacts"


ANDROID_TEST = "./automation/taskcluster/androidTest"


def setup_logging():
    """Configure logging for the script."""
    log_format = "%(message)s"
    logging.basicConfig(level=logging.INFO, format=log_format)


def run_command(
    command: List[Union[str, bytes]], log_path: Optional[str] = None
) -> int:
    """Execute a command, log its output, and check for errors.

    Args:
        command: The command to execute
        log_path: The path to a log file to write the command output to
    Returns:
        int: The exit code of the command
    """

    with subprocess.Popen(
        command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True
    ) as process:
        if log_path:
            with open(log_path, "a") as log_file:
                for line in process.stdout:
                    sys.stdout.write(line)
                    log_file.write(line)
        else:
            for line in process.stdout:
                sys.stdout.write(line)
        process.wait()
        sys.stdout.flush()
        if process.returncode != 0:
            error_message = f"Command {' '.join(command)} failed with exit code {process.returncode}"
            logging.error(error_message)
        return process.returncode


def setup_environment():
    """Configure Google Cloud project and authenticate with the service account."""
    project_id = os.getenv("GOOGLE_PROJECT")
    credentials_file = os.getenv("GOOGLE_APPLICATION_CREDENTIALS")
    if not project_id or not credentials_file:
        logging.error(
            "Error: GOOGLE_PROJECT and GOOGLE_APPLICATION_CREDENTIALS environment variables must be set."
        )
        sys.exit(1)

    run_command(["gcloud", "config", "set", "project", project_id])
    run_command(
        ["gcloud", "auth", "activate-service-account", "--key-file", credentials_file]
    )


def execute_tests(
    flank_config: str, apk_app: Path, apk_test: Optional[Path] = None
) -> int:
    """Run UI tests on Firebase Test Lab using Flank.

    Args:
        flank_config: The YML configuration for Flank to use e.g, automation/taskcluster/androidTest/flank-<config>.yml
        apk_app: Absolute path to a Android APK application package (optional) for robo test or instrumentation test
        apk_test: Absolute path to a Android APK androidTest package
    Returns:
        int: The exit code of the command
    """

    run_command([Worker.JAVA_BIN.value, "-jar", Worker.FLANK_BIN.value, "--version"])

    flank_command = [
        Worker.JAVA_BIN.value,
        "-jar",
        Worker.FLANK_BIN.value,
        "android",
        "run",
        "--config",
        f"{ANDROID_TEST}/flank-{flank_config}.yml",
        "--app",
        str(apk_app),
        "--local-result-dir",
        Worker.RESULTS_DIR.value,
        "--project",
        os.environ.get("GOOGLE_PROJECT"),
        "--client-details",
        f'matrixLabel={os.environ.get("PULL_REQUEST_NUMBER", "None")}',
    ]

    # Add androidTest APK if provided (optional) as robo test or instrumentation test
    if apk_test:
        flank_command.extend(["--test", str(apk_test)])

    exit_code = run_command(flank_command, "flank.log")
    if exit_code == 0:
        logging.info("All UI test(s) have passed!")
    return exit_code


def process_results(flank_config: str, test_type: str = "instrumentation") -> None:
    """Process and parse test results.

    Args:
        flank_config: The YML configuration for Flank to use e.g, automation/taskcluster/androidTest/flank-<config>.yml
    """

    # Ensure directories exist and scripts are executable
    github_dir = os.path.join(Worker.ARTIFACTS_DIR.value, "github")
    os.makedirs(github_dir, exist_ok=True)

    parse_ui_test_script = os.path.join(ANDROID_TEST, "parse-ui-test.py")
    parse_ui_test_fromfile_script = os.path.join(
        ANDROID_TEST, "parse-ui-test-fromfile.py"
    )

    os.chmod(parse_ui_test_script, 0o755)
    os.chmod(parse_ui_test_fromfile_script, 0o755)

    # Run parsing scripts and check for errors

    # Process the results differently based on the test type: robo or instrumentation
    # Currently, robo test does not have a test file artifact to parse
    exit_code = 0
    if test_type == "instrumentation":
        exit_code = run_command(
            [parse_ui_test_fromfile_script, "--results", Worker.RESULTS_DIR.value],
            "flank.log",
        )

    command = [
        parse_ui_test_script,
        "--exit-code",
        str(0),
        "--log",
        "flank.log",
        "--results",
        Worker.RESULTS_DIR.value,
        "--output-md",
        os.path.join(github_dir, "customCheckRunText.md"),
        "--device-type",
        flank_config,
    ]
    if exit_code == 0:
        command.append("--report-treeherder-failures")
    run_command(
        command,
        "flank.log",
    )


def main():
    """Parse command line arguments and execute the test runner."""
    parser = argparse.ArgumentParser(
        description="Run UI tests on Firebase Test Lab using Flank as a test runner"
    )
    parser.add_argument(
        "flank_config",
        help="The YML configuration for Flank to use e.g, automation/taskcluster/androidTest/flank-<config>.yml",
    )
    parser.add_argument(
        "apk_app", help="Absolute path to a Android APK application package"
    )
    parser.add_argument(
        "--apk_test",
        help="Absolute path to a Android APK androidTest package",
        default=None,
    )
    args = parser.parse_args()

    setup_environment()

    # Only resolve apk_test if it is provided
    apk_test_path = Path(args.apk_test).resolve() if args.apk_test else None
    exit_code = execute_tests(
        flank_config=args.flank_config,
        apk_app=Path(args.apk_app).resolve(),
        apk_test=apk_test_path,
    )

    # Determine the instrumentation type to process the results differently
    instrumentation_type = "instrumentation" if args.apk_test else "robo"
    process_results(flank_config=args.flank_config, test_type=instrumentation_type)

    sys.exit(exit_code)


if __name__ == "__main__":
    setup_logging()
    main()
