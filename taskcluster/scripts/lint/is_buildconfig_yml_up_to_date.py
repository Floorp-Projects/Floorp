#!/usr/bin/env python3

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import logging
import os
import subprocess
import sys

from update_buildconfig_from_gradle import main as update_build_config

CURRENT_DIR = os.path.dirname(os.path.realpath(__file__))
PROJECT_DIR = os.path.realpath(os.path.join(CURRENT_DIR, "..", "..", ".."))
OUTPUT_DIR = os.path.join(PROJECT_DIR, "artifacts")
BUILDCONFIG_DIFF_FILE_NAME = "buildconfig.diff"
BUILDCONFIG_DIFF_FILE = os.path.join(OUTPUT_DIR, BUILDCONFIG_DIFF_FILE_NAME)
BUILDCONFIG_FILE_NAME = ".buildconfig.yml"

logger = logging.getLogger(__name__)


def _buildconfig_files_diff():
    cmd = [
        "hg",
        "diff",
        "--rev",
        "draft() and ancestors(.)",
        "-I",
        "**/.buildconfig.yml",
    ]
    p = subprocess.run(cmd, capture_output=True, universal_newlines=True)
    return p.stdout


def _execute_taskcluster_steps(diff, task_id):
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    with open(BUILDCONFIG_DIFF_FILE, mode="w") as f:
        f.write(diff)
    tc_root_url = os.environ["TASKCLUSTER_ROOT_URL"]
    artifact_url = f"{tc_root_url}/api/queue/v1/task/{task_id}/artifacts/public%2F{BUILDCONFIG_DIFF_FILE_NAME}"  # noqa E501
    message = f"""{BUILDCONFIG_FILE_NAME} file changed! Please update it by running:

curl --location --compressed {artifact_url} | git apply

Then commit and push!
"""
    logger.error(message)


def _execute_local_steps():
    logger.error(f"{BUILDCONFIG_FILE_NAME} file updated! Please commit these changes.")


def main():
    update_build_config()
    diff = _buildconfig_files_diff()
    if diff:
        task_id = os.environ.get("TASK_ID")
        if task_id:
            _execute_taskcluster_steps(diff, task_id)
        else:
            _execute_local_steps()
        sys.exit(1)

    logger.info(f"All good! {BUILDCONFIG_FILE_NAME} is up-to-date with gradle.")


__name__ == "__main__" and main()
