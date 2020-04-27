#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import mozunit
import json

from mozperftest.metrics.utils import open_file
from mozperftest.tests.support import temp_file


def test_open_file():
    data = json.dumps({"1": 2})

    with temp_file(name="data.json", content=data) as f:
        res = open_file(f)
        assert res == {"1": 2}

    with temp_file(name="data.txt", content="yeah") as f:
        assert open_file(f) == "yeah"


if __name__ == "__main__":
    mozunit.main()
