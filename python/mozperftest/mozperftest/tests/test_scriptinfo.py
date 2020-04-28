#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import mozunit

from mozperftest.scriptinfo import ScriptInfo
from mozperftest.tests.support import EXAMPLE_TEST


def test_scriptinfo():
    info = ScriptInfo(EXAMPLE_TEST)
    assert info["author"] == "N/A"

    display = str(info)
    assert "appropriate android app" in display


if __name__ == "__main__":
    mozunit.main()
