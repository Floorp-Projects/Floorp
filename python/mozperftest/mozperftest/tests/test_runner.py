#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import mozunit
import pytest

from mozperftest.runner import main
from mozperftest.utils import silence


def test_main():
    with pytest.raises(SystemExit), silence():
        main(["--help"])


if __name__ == "__main__":
    mozunit.main()
