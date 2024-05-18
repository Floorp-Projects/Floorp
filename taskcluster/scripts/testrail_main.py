#!/usr/bin/env python3

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
This Python script automates creating milestones and test runs in TestRail and updating
test cases based on the results of automated smoke tests for different product releases.

Functionality includes:
- Reading TestRail credentials and environment variables.
- Building milestone names and descriptions.
- Interacting with the TestRail API to create milestones, test runs, and update test cases.
- Sending notifications to a specified Slack channel.
"""

import os
import sys

from lib.testrail_api import TestRail
from lib.testrail_utils import (
    build_milestone_description,
    build_milestone_name,
    get_release_type,
    get_release_version,
    load_testrail_credentials,
)
from slack_notifier import (
    get_taskcluster_options,
    send_error_notification,
    send_success_notification,
)

# Constants
SUCCESS_CHANNEL_ID = "C02KDDS9QM9"  # mobile-testeng
ERROR_CHANNEL_ID = "G016BC5FUHJ"  # mobile-alerts-sandbox


def main():
    # Load TestRail credentials
    credentials = load_testrail_credentials(".testrail_credentials.json")
    testrail = TestRail(
        credentials["host"], credentials["username"], credentials["password"]
    )

    # Read task environment variables
    try:
        shipping_product = os.environ["SHIPPING_PRODUCT"]
        testrail_product_type = os.environ["TESTRAIL_PRODUCT_TYPE"]
        testrail_project_id = os.environ["TESTRAIL_PROJECT_ID"]
        testrail_test_suite_id = os.environ["TESTRAIL_TEST_SUITE_ID"]
    except KeyError as e:
        raise ValueError(f"ERROR: Missing Environment Variable: {e}")

    # Release information
    release_version = get_release_version()
    release_type = get_release_type(release_version)

    # Build milestone information
    milestone_name = build_milestone_name(
        testrail_product_type, release_type, release_version
    )
    milestone_description = build_milestone_description(milestone_name)

    # Configure Taskcluster API
    options = get_taskcluster_options()

    try:
        # Check if milestone exists
        if testrail.does_milestone_exist(testrail_project_id, milestone_name):
            print(f"Milestone for {milestone_name} already exists. Exiting script...")
            sys.exit()

        # Create milestone and test runs
        devices = ["Google Pixel 3(Android11)", "Google Pixel 2(Android11)"]
        milestone = testrail.create_milestone(
            testrail_project_id, milestone_name, milestone_description
        )

        for device in devices:
            test_run = testrail.create_test_run(
                testrail_project_id, milestone["id"], device, testrail_test_suite_id
            )
            testrail.update_test_run_tests(test_run["id"], 1)  # 1 = Passed
        # Send success notification
        success_values = {
            "RELEASE_TYPE": release_type,
            "RELEASE_VERSION": release_version,
            "SHIPPING_PRODUCT": shipping_product,
            "TESTRAIL_PROJECT_ID": testrail_project_id,
            "TESTRAIL_PRODUCT_TYPE": testrail_product_type,
        }
        send_success_notification(success_values, SUCCESS_CHANNEL_ID, options)

    except Exception as error_message:
        send_error_notification(str(error_message), ERROR_CHANNEL_ID, options)


if __name__ == "__main__":
    main()
