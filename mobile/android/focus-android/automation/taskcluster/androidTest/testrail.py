"""
This Python script is designed to automate the process of creating milestones
and test runs in TestRail, and updating test cases based on the results of
automated smoke tests for different product releases.

Below is a summary of its functionality in order of execution:

1. Environment and Credentials Setup: 
    - Imports necessary libraries and modules.
    - Loads environmental variables from "execution_metadata.env".
    - Reads and processes TestRail credentials from '.testrail_credentials.json'.

2. Environment Variables Validation: 
    - Retrieves and validates several environment variables like `PRODUCT_TYPE`,
        `RELEASE_TYPE`, `VERSION_NUMBER`, and `TEST_STATUS`.
    - Ensures `TEST_STATUS` is either 'PASS' or 'FAIL'.

3. Utility Functions: 
    - `parse_release_number()`: Parses the version number to extract a specific part.
    - `build_milestone_name()`: Constructs a milestone name based on product type,
        release type, and version number.
    - `build_milestone_description()`: Creates a detailed description for the milestone
        including the current date and placeholders for various testing statuses.

4. TestRail Integration: 
    - Defines a `TestRail` class that handles interactions with the TestRail API.
    - Includes methods to create milestones, create test runs, and update test cases.

5. Main Execution: 
    - Checks if `TEST_STATUS` is 'PASS'. If not, it raises an error to trigger a Slack notification.
    - Sets parameters for a demo TestRail project.
    - Instantiates the `TestRail` class.
    - Creates a milestone in TestRail and retrieves its ID.
    - Creates test runs for each device/API combination (currently hardcoded for phase 1 testing)
        and updates test cases to 'passed' status.

6. Phase 1 and Phase 2 Notes: 
    - The script is currently in Phase 1, where certain values are hardcoded for testing.
    - In Phase 2, these hardcoded values will be parameterized for broader usage.
"""


import json
import os
import textwrap
from lib.testrail_conn import APIClient
from dotenv import load_dotenv
from datetime import datetime

try:
    load_dotenv("execution_metadata.env")  # Attempt to load .env file
except FileNotFoundError:
    raise FileNotFoundError("The .env file was not found.")
except Exception as e:
    raise Exception(f"An error occurred while loading the .env file: {e}")

try:
    with open('.testrail_credentials.json', 'r') as file:
        secret = json.load(file)
        TESTRAIL_HOST = secret['host']
        TESTRAIL_USERNAME = secret['username']
        TESTRAIL_PASSWORD = secret['password']
except json.JSONDecodeError as e:
    raise ValueError("Failed to load testrail credentials : {e}")

try:
    PRODUCT_TYPE = os.environ["PRODUCT_TYPE"]
    RELEASE_TYPE = os.environ["RELEASE_TYPE"]
    VERSION_NUMBER = os.environ["MOBILE_HEAD_REF"]
    TEST_STATUS = os.environ["TEST_STATUS"]

    if TEST_STATUS not in ('PASS', 'FAIL'):
        raise ValueError(f"ERROR: Invalid TEST_STATUS value: {TEST_STATUS}")
except KeyError as e:
    raise ValueError(f"ERROR: Missing Environment Variable: {e}")

def parse_release_number(VERSION_NUMBER):
    parts = VERSION_NUMBER.split('_')
    return parts[1]

def build_milestone_name(product_type, release_type, version_number):
    return f"Automated smoke testing sign-off - {product_type} {release_type} {version_number}"

def build_milestone_description(milestone_name):
    current_date = datetime.now()
    formatted_date = current_date = current_date.strftime("%B %d, %Y")
    return textwrap.dedent(f"""
        RELEASE: {milestone_name}\n\n\
        RELEASE_TAG_URL: https://github.com/mozilla-mobile/firefox-android/releases\n\n\
        RELEASE_DATE: {formatted_date}\n\n\
        TESTING_STATUS: [ TBD ]\n\n\
        QA_RECOMMENDATION:[ TBD ]\n\n\
        QA_RECOMENTATION_VERBOSE: \n\n\
        TESTING_SUMMARY\n\n\
        Known issues: n/a\n\
        New issue: n/a\n\
        Verified issue: 
    """)

class TestRail():

    def __init__(self):
        try:
            self.client = APIClient(TESTRAIL_HOST)
            self.client.user = TESTRAIL_USERNAME
            self.client.password = TESTRAIL_PASSWORD
        except KeyError as e:
            raise ValueError(f"ERROR: Missing Testrail Env Var: {e}")
    
    # Public Methods

    def create_milestone(self, testrail_project_id, title, description):
        data = {"name": title, "description": description}
        return self.client.send_post(f'add_milestone/{testrail_project_id}', data)
    
    def create_test_run(self, testrail_project_id, testrail_milestone_id, name_run, testrail_suite_id):
        data = {"name": name_run, "milestone_id": testrail_milestone_id, "suite_id": testrail_suite_id}
        return self.client.send_post(f'add_run/{testrail_project_id}', data)
    
    def update_test_cases_to_passed(self, testrail_project_id, testrail_run_id, testrail_suite_id):
        test_cases = self._get_test_cases(testrail_project_id, testrail_suite_id)
        data = { "results": [{"case_id": test_case['id'], "status_id": 1} for test_case in test_cases]}
        return testrail._update_test_run_results(testrail_run_id, data)

    # Private Methods

    def _get_test_cases(self, testrail_project_id, testrail_test_suite_id):
        return self.client.send_get(f'get_cases/{testrail_project_id}&suite_id={testrail_test_suite_id}')
    
    def _update_test_run_results(self, testrail_run_id, data):
        return self.client.send_post(f'add_results_for_cases/{testrail_run_id}', data)
    
if __name__ == "__main__":
    if TEST_STATUS != 'PASS':
        raise ValueError("Tests failed. Sending Slack Notification....")

    # There are for a dummy Testrail project used for Phase 1 testing of this script
    # They will be parameterized during Phase 2 of script hardening
    PROJECT_ID = 53 # Firefox for FireTV
    TEST_SUITE_ID = 45442 # Demo Test Suite

    testrail = TestRail()
    milestone_name = build_milestone_name(PRODUCT_TYPE, RELEASE_TYPE, parse_release_number(VERSION_NUMBER))
    milestone_description = build_milestone_description(milestone_name)

    # Create milestone for 'Firefox for FireTV' and store the ID
    milestone_id = testrail.create_milestone(PROJECT_ID, milestone_name, milestone_description)['id']

    # Create test run for each Device/API and update test cases to 'passed'
    # The Firebase Test devices are temporarily hard-coded during testing 
    # and will be parameterized in Phase 2 of hardening
    for test_run_name in ['Google Pixel 32(Android11)', 'Google Pixel2(Android9)']:
        test_run_id = testrail.create_test_run(PROJECT_ID, milestone_id, test_run_name, TEST_SUITE_ID)['id']
        testrail.update_test_cases_to_passed(PROJECT_ID, test_run_id, TEST_SUITE_ID)
