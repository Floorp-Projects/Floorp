import os
import sys

base = os.path.dirname(__file__)
webdriver_path = os.path.abspath(
    os.path.join(base, "..", "..", "..", "tests", "webdriver")
)
sys.path.insert(0, os.path.join(webdriver_path))

pytest_plugins = [
    "tests.support.fixtures",
    "tests.support.fixtures_http",
]
