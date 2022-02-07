# Any copyright is dedicated to the Public Domain.
# https://creativecommons.org/publicdomain/zero/1.0/

from textwrap import dedent

import mozunit


def test_release(run_mach, capfd):
    cmd = [
        "try",
        "release",
        "--no-push",
        "--version=97.0",
    ]
    assert run_mach(cmd) == 0

    output = capfd.readouterr().out
    print(output)

    expected = dedent(
        """
        Commit message:
        staging release: 97.0

        Pushed via `mach try release`
        Calculated try_task_config.json:
        {
            "parameters": {
                "optimize_target_tasks": true,
                "release_type": "release",
                "target_tasks_method": "staging_release_builds"
            },
            "version": 2
        }

    """
    ).lstrip()
    assert expected in output


if __name__ == "__main__":
    mozunit.main()
