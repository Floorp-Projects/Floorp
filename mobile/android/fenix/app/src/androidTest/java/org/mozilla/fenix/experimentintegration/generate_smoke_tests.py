# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import subprocess
from pathlib import Path

import yaml

parser = argparse.ArgumentParser("Options for android apk downloader")

parser.add_argument(
    "--test-files", nargs="+", help="List of test files to generate tests from"
)
args = parser.parse_args()


def search_for_smoke_tests(tests_name):
    """Searches for smoke tests within the requested test module."""
    path = Path("../ui")
    files = sorted([x for x in path.iterdir() if x.is_file()])
    locations = []
    file_name = None
    test_names = []

    for name in files:
        if tests_name in name.name:
            file_name = name
            break

    with open(file_name, "r") as file:
        code = file.read().split(" ")
        code = [item for item in code if item != ""]

        #  Gets the class name from the path e.g.: .ui/HomescreenTest.kt
        class_name = f"{file_name.name.strip('kt').strip('.').split('/')[-1]}"

        #  This is a custom annotation found in fenix/customannotations/SmokeTest.kt
        for count, item in enumerate(code):
            if "@SmokeTest" in item:
                locations.append(count)

        for location in locations:
            test_names.append(f"{class_name}#{code[location+3].strip('()')}")
    return test_names


def create_test_file():
    """Create the python file to hold the tests."""

    path = Path("tests/")
    filename = "test_smoke_scenarios.py"
    final_path = path / filename

    if final_path.exists():
        print("File Exists, you need to delete it to create a new one.")
        return
    # file exists
    subprocess.run([f"touch {final_path}"], encoding="utf8", shell=True)
    assert final_path.exists()
    with open(final_path, "w") as file:
        file.write("import pytest\n\n")


def generate_smoke_tests(tests_names=None):
    """Generate pytest code for the requested tests."""
    pytest_file = "tests/test_smoke_scenarios.py"
    tests = []

    for test in tests_names[1:]:
        test_name = test.replace("#", "_").lower()
        tests.append(
            f"""
@pytest.mark.smoke_test
def test_smoke_{test_name}(setup_experiment, gradlewbuild):
    setup_experiment()
    gradlewbuild.test("{test}", smoke=True)
"""
        )
    with open(pytest_file, "a") as file:
        for item in tests:
            file.writelines(f"{item}")


if __name__ == "__main__":
    test_modules = []
    args = parser.parse_args()
    if args.test_files:
        test_modules = args.test_files
    else:
        with open("variables.yaml", "r") as file:
            tests = yaml.safe_load(file)
            test_modules = [test for test in tests.get("smoke_tests")]
    create_test_file()
    print(f"Created tests for modules: {test_modules}")
    for item in test_modules:
        tests = search_for_smoke_tests(item)
        generate_smoke_tests(tests)
