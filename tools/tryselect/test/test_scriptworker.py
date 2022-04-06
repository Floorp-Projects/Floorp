# Any copyright is dedicated to the Public Domain.
# https://creativecommons.org/publicdomain/zero/1.0/

import re
from textwrap import dedent

import mozunit


def test_release(run_mach, capfd):
    cmd = [
        "try",
        "scriptworker",
        "--no-push",
        "tree",
    ]
    assert run_mach(cmd) == 0

    output = capfd.readouterr().out
    print(output)

    expected = re.compile(
        dedent(
            r"""
        Pushed via `mach try scriptworker`
        Calculated try_task_config.json:
        {
            "parameters": {
                "app_version": "\d+\.\d+",
                "build_number": \d+,
    """
        ).lstrip(),
        re.MULTILINE,
    )
    assert expected.search(output)


if __name__ == "__main__":
    mozunit.main()
