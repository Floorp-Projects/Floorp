#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from unittest import mock

import mozunit
import pytest

from mozbuild.base import MachCommandBase  # noqa

from mozperftest.runner import main
from mozperftest.utils import silence


def test_main():
    with pytest.raises(SystemExit), silence():
        main(["--help"])


def test_tools():
    with mock.patch(
        "mozperftest.runner._activate_mach_virtualenv", return_value="fake_path"
    ) as _:
        with pytest.raises(SystemExit), silence():
            main(["tools"])


@mock.patch("mozperftest.PerftestToolsArgumentParser")
def test_side_by_side(arg, patched_mozperftest_tools):
    with mock.patch(
        "mozperftest.runner._activate_mach_virtualenv", return_value="fake_path"
    ) as _, mock.patch(
        "mozperftest.runner._create_artifacts_dir", return_value="fake_path"
    ) as _:
        main(
            [
                "tools",
                "side-by-side",
                "-t",
                "fake-test-name",
            ]
        )


if __name__ == "__main__":
    mozunit.main()
