import os
import os.path
import sys
from subprocess import check_call

import mozunit
import pytest
from moztest.selftest import fixtures

MOZ_AUTOMATION = bool(os.getenv("MOZ_AUTOMATION", "0") == "1")


def test_grizzly_smoke():
    ffbin = fixtures.binary()

    if MOZ_AUTOMATION:
        assert os.path.exists(
            ffbin
        ), "Missing Firefox build. Build it, or set GECKO_BINARY_PATH"

    elif not os.path.exists(ffbin):
        pytest.skip("Missing Firefox build. Build it, or set GECKO_BINARY_PATH")

    check_call(
        [
            sys.executable,
            "-m",
            "grizzly",
            ffbin,
            "no-op",
            "--headless",
            "--smoke-test",
            "--limit",
            "10",
            "--relaunch",
            "5",
        ],
    )


if __name__ == "__main__":
    mozunit.main()
