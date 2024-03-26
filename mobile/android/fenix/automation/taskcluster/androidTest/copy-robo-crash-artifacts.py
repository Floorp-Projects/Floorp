#!/usr/bin/env python3

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
This script is designed to automate the process of fetching crash logs from Google Cloud Storage (GCS) for failed devices in a Robo test.
It is intended to be run as part of a Taskcluster job following a scheduled test task.
The script requires the presence of a `matrix_ids.json` artifact in the results directory and the availability of the `gsutil` command in the environment.

The script performs the following operations:
- Loads the `matrix_ids.json` artifact to identify the GCS paths for the crash logs.
- Identifies failed devices based on the outcomes specified in the `matrix_ids.json` artifact.
- Fetches crash logs for the failed devices from the specified GCS paths.
- Copies the fetched crash logs to the current worker artifact results directory.

The script is configured to log its operations and errors, providing visibility into its execution process.
It uses the `gsutil` command-line tool to interact with GCS, ensuring compatibility with the GCS environment.

Usage:
    python3 script_name.py

Requirements:
    - The `matrix_ids.json` artifact must be present in the results directory.
    - The `gsutil` command must be available in the environment.
    - The script should be run after a scheduled test task in a Taskcluster job.

Output:
    - Crash logs for failed devices are copied to the current worker artifact results directory.
"""

import json
import logging
import subprocess
import sys
from enum import Enum


def setup_logging():
    """Configure logging for the script."""
    log_format = "%(message)s"
    logging.basicConfig(level=logging.INFO, format=log_format)


class Worker(Enum):
    """
    Worker paths
    """

    RESULTS_DIR = "/builds/worker/artifacts/results"
    ARTIFACTS_DIR = "/builds/worker/artifacts"


class ArtifactType(Enum):
    """
    Artifact types for fetching crash logs and matrix IDs artifact.
    """

    CRASH_LOG = "data_app_crash*.txt"
    MATRIX_IDS = "matrix_ids.json"


def load_matrix_ids_artifact(matrix_file_path):
    """Load the matrix IDs artifact from the specified file path.

    Args:
        matrix_file_path (str): The file path to the matrix IDs artifact.
    Returns:
        dict: The contents of the matrix IDs artifact.
    """
    try:
        with open(matrix_file_path, "r") as f:
            return json.load(f)
    except FileNotFoundError:
        logging.error(f"Could not find matrix file: {matrix_file_path}")
        sys.exit(1)
    except json.JSONDecodeError:
        logging.error(f"Error decoding matrix file: {matrix_file_path}")
        sys.exit(1)


def get_gcs_path(matrix_artifact_file):
    """
    Extract the root GCS path from the matrix artifact file.

    Args:
        matrix_artifact_file (dict): The matrix artifact file contents.
    Returns:
        str: The root GCS path extracted from the matrix artifact file.
    """
    for matrix in matrix_artifact_file.values():
        gcs_path = matrix.get("gcsPath")
        if gcs_path:
            return gcs_path
    return None


def check_gsutil_availability():
    """
    Check the availability of the `gsutil` command in the environment.
    Exit the script if `gsutil` is not available.
    """
    try:
        result = subprocess.run(["gsutil", "--version"], capture_output=True, text=True)
        if result.returncode != 0:
            logging.error(f"Error executing gsutil: {result.stderr}")
            sys.exit(1)
    except Exception as e:
        logging.error(f"Error executing gsutil: {e}")
        sys.exit(1)


def fetch_artifacts(root_gcs_path, device, artifact_pattern):
    """
    Fetch artifacts from the specified GCS path pattern for the given device.

    Args:
        root_gcs_path (str): The root GCS path for the artifacts.
        device (str): The device name for which to fetch artifacts.
        artifact_pattern (str): The pattern to match the artifacts.
    Returns:
        list: A list of artifacts matching the specified pattern.
    """
    gcs_path_pattern = f"gs://{root_gcs_path.rstrip('/')}/{device}/{artifact_pattern}"

    try:
        result = subprocess.run(
            ["gsutil", "ls", gcs_path_pattern], capture_output=True, text=True
        )
        if result.returncode == 0:
            return result.stdout.splitlines()
        else:
            if "AccessDeniedException" in result.stderr:
                logging.error(f"Permission denied for GCS path: {gcs_path_pattern}")
            elif "network error" in result.stderr.lower():
                logging.error(f"Network error accessing GCS path: {gcs_path_pattern}")
            else:
                logging.error(f"Failed to list files: {result.stderr}")
            return []
    except Exception as e:
        logging.error(f"Error executing gsutil: {e}")
        return []


def fetch_failed_device_names(matrix_artifact_file):
    """
    Fetch the names of devices that failed based on the outcomes specified in the matrix artifact file.

    Args:
        matrix_artifact_file (dict): The matrix artifact file contents.
    Returns:
        list: A list of device names that failed in the test.
    """
    failed_devices = []
    for matrix in matrix_artifact_file.values():
        axes = matrix.get("axes", [])
        for axis in axes:
            if axis.get("outcome") == "failure":
                device = axis.get("device")
                if device:
                    failed_devices.append(device)
    return failed_devices


def gsutil_cp(artifact, dest):
    """
    Copy the specified artifact to the destination path using `gsutil`.

    Args:
        artifact (str): The path to the artifact to copy.
        dest (str): The destination path to copy the artifact to.
    Returns:
        None
    """
    logging.info(f"Copying {artifact} to {dest}")
    try:
        result = subprocess.run(
            ["gsutil", "cp", artifact, dest], capture_output=True, text=True
        )
        if result.returncode != 0:
            if "AccessDeniedException" in result.stderr:
                logging.error(f"Permission denied for GCS path: {artifact}")
            elif "network error" in result.stderr.lower():
                logging.error(f"Network error accessing GCS path: {artifact}")
            else:
                logging.error(f"Failed to list files: {result.stderr}")
    except Exception as e:
        logging.error(f"Error executing gsutil: {e}")


def process_artifacts(artifact_type):
    """
    Process the artifacts based on the specified artifact type.

    Args:
        artifact_type (ArtifactType): The type of artifact to process.
    Returns:
        None
    """
    matrix_ids_artifact = load_matrix_ids_artifact(
        Worker.RESULTS_DIR.value + "/" + ArtifactType.MATRIX_IDS.value
    )
    failed_device_names = fetch_failed_device_names(matrix_ids_artifact)
    root_gcs_path = get_gcs_path(matrix_ids_artifact)

    if not root_gcs_path:
        logging.error("Could not find root GCS path in matrix file.")
        return

    if not failed_device_names:
        return

    for device in failed_device_names:
        artifacts = fetch_artifacts(root_gcs_path, device, artifact_type.value)
        if not artifacts:
            logging.info(f"No artifacts found for device: {device}")
            continue

        for artifact in artifacts:
            gsutil_cp(artifact, Worker.RESULTS_DIR.value)


def main():
    setup_logging()
    check_gsutil_availability()
    process_artifacts(ArtifactType.CRASH_LOG)


if __name__ == "__main__":
    main()
