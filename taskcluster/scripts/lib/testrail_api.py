#!/usr/bin/env python3

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
This module provides a TestRail class for interfacing with the TestRail API, enabling the creation and management of test milestones, test runs, and updating test cases. It facilitates automation and integration of TestRail functionalities into testing workflows, particularly for projects requiring automated test management and reporting.

The TestRail class encapsulates methods to interact with TestRail's API, including creating milestones and test runs, updating test cases, and checking the existence of milestones. It also features a method to retry API calls, enhancing the robustness of network interactions.

Key Components:
- TestRail Class: A class providing methods for interacting with TestRail's API.
  - create_milestone: Create a new milestone in a TestRail project.
  - create_milestone_and_test_runs: Create a milestone and associated test runs for multiple devices in a project.
  - create_test_run: Create a test run within a TestRail project.
  - does_milestone_exist: Check if a milestone already exists in a TestRail project.
  - update_test_cases_to_passed: Update the status of test cases to 'passed' in a test run.
- Private Methods: Utility methods for internal use to fetch test cases, update test run results, and retrieve milestones.
- Retry Mechanism: A method to retry API calls with a specified number of attempts and delay, improving reliability in case of intermittent network issues.

Usage:
This module is intended to be used as part of a larger automated testing system, where integration with TestRail is required for test management and reporting.

"""

import os
import sys
import time

# Ensure the directory containing this script is in Python's search path
script_directory = os.path.dirname(os.path.abspath(__file__))
if script_directory not in sys.path:
    sys.path.append(script_directory)

from testrail_conn import APIClient


class TestRail:
    def __init__(self, host, username, password):
        self.client = APIClient(host)
        self.client.user = username
        self.client.password = password

    # Public Methods

    def create_milestone(self, testrail_project_id, title, description):
        data = {"name": title, "description": description}
        return self.client.send_post(f"add_milestone/{testrail_project_id}", data)

    def create_milestone_and_test_runs(
        self, project_id, milestone_name, milestone_description, devices, test_suite_id
    ):
        # Create milestone
        milestone_id = self._retry_api_call(
            self.create_milestone, project_id, milestone_name, milestone_description
        )["id"]

        # Create test runs for each device
        for device in devices:
            test_run_id = self._retry_api_call(
                self.create_test_run, project_id, milestone_id, device, test_suite_id
            )["id"]
            self._retry_api_call(
                self.update_test_cases_to_passed, project_id, test_run_id, test_suite_id
            )

        return milestone_id

    def create_test_run(
        self, testrail_project_id, testrail_milestone_id, name_run, testrail_suite_id
    ):
        data = {
            "name": name_run,
            "milestone_id": testrail_milestone_id,
            "suite_id": testrail_suite_id,
        }
        return self.client.send_post(f"add_run/{testrail_project_id}", data)

    def does_milestone_exist(self, testrail_project_id, milestone_name):
        num_of_milestones_to_check = 10  # check last 10 milestones
        milestones = self._get_milestones(
            testrail_project_id
        )  # returns reverse chronological order
        for milestone in milestones[
            -num_of_milestones_to_check:
        ]:  # check last 10 api responses
            if milestone_name == milestone["name"]:
                return True
        return False

    def update_test_cases_to_passed(
        self, testrail_project_id, testrail_run_id, testrail_suite_id
    ):
        test_cases = self._get_test_cases(testrail_project_id, testrail_suite_id)
        data = {
            "results": [
                {"case_id": test_case["id"], "status_id": 1} for test_case in test_cases
            ]
        }
        return self._update_test_run_results(testrail_run_id, data)

    # Private Methods

    def _get_test_cases(self, testrail_project_id, testrail_test_suite_id):
        return self.client.send_get(
            f"get_cases/{testrail_project_id}&suite_id={testrail_test_suite_id}"
        )

    def _update_test_run_results(self, testrail_run_id, data):
        return self.client.send_post(f"add_results_for_cases/{testrail_run_id}", data)

    def _get_milestones(self, testrail_project_id):
        return self.client.send_get(f"get_milestones/{testrail_project_id}")

    def _retry_api_call(self, api_call, *args, max_retries=3, delay=5):
        """
        Retries the given API call up to max_retries times with a delay between attempts.

        :param api_call: The API call method to retry.
        :param args: Arguments to pass to the API call.
        :param max_retries: Maximum number of retries.
        :param delay: Delay between retries in seconds.
        """
        for attempt in range(max_retries):
            try:
                return api_call(*args)
            except Exception:
                if attempt == max_retries - 1:
                    raise  # Reraise the last exception
                time.sleep(delay)
