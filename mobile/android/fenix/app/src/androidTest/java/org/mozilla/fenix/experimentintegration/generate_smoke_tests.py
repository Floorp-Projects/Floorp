import subprocess
from pathlib import Path

import yaml


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

        for count, item in enumerate(code):
            if "class" in item or "@SmokeTest" in item:
                locations.append(count)

        for location in locations:
            if len(test_names) == 0:
                class_name = code[location + 1]
                test_names.append(class_name)
            else:
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
def test_smoke_{test_name}(setup_experiment, gradlewbuild, load_branches, check_ping_for_experiment):
    setup_experiment(load_branches)
    gradlewbuild.test("{test}", smoke=True)
    assert check_ping_for_experiment
"""
        )
    with open(pytest_file, "a") as file:
        for item in tests:
            file.writelines(f"{item}")


if __name__ == "__main__":
    test_modules = None
    create_test_file()
    with open("variables.yaml", "r") as file:
        test_modules = yaml.safe_load(file)
    for item in test_modules.get("smoke_tests"):
        tests = search_for_smoke_tests(item)
        generate_smoke_tests(tests)
