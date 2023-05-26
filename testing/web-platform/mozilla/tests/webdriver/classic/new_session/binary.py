import os
import sys

from tests.support.asserts import assert_error, assert_success


def test_bad_binary(new_session):
    # skipif annotations are forbidden in wpt
    if os.path.exists("/bin/echo"):
        response, _ = new_session(
            {
                "capabilities": {
                    "alwaysMatch": {"moz:firefoxOptions": {"binary": "/bin/echo"}}
                }
            }
        )
        assert_error(response, "invalid argument")


def test_shell_script_binary(new_session, configuration):
    # skipif annotations are forbidden in wpt
    if sys.platform == "linux":
        capabilities = configuration["capabilities"].copy()
        binary = configuration["browser"]["binary"]
        script = f"""#!/bin/bash

{binary} $@"""
        path = os.path.abspath("firefox.sh")
        assert not os.path.exists(path)
        try:
            with open("firefox.sh", "w") as f:
                f.write(script)
            os.chmod(path, 0o744)
            capabilities["moz:firefoxOptions"]["binary"] = path
            response, _ = new_session({"capabilities": capabilities})
            assert_success(response)
        finally:
            os.unlink(path)
