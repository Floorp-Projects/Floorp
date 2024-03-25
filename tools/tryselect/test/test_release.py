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

    expected_part1 = dedent(
        """
        Commit message:
        staging release: 97.0
        """
    ).lstrip()

    # The output now features a display of the `mach try` command run here
    # that will vary based on the user and invocation, so ignore that part.

    expected_part2 = dedent(
        """
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
    assert expected_part1 in output
    assert expected_part2 in output


if __name__ == "__main__":
    mozunit.main()
