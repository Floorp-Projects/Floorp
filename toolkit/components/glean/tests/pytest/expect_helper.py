# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import inspect


def expect(path, actual):
    """
    Assert that the content of the file at `path` contains `actual`.

    If the environment variable `UPDATE_EXPECT` is set, the path content is updated to `actual`.
    This allows to update the file contents easily.
    """

    callerframerecord = inspect.stack()[1]
    frame = callerframerecord[0]
    info = inspect.getframeinfo(frame)
    msg = f"""
Unexpected content in {path} (at {info.filename}:{info.lineno})

If the code generation was changed,
run the test suite again with `UPDATE_EXPECT=1` set to update the test files.
""".strip()

    if "UPDATE_EXPECT" in os.environ:
        with open(path, "w") as file:
            file.write(actual)

    expected = None
    with open(path, "r") as file:
        expected = file.read()
    assert actual == expected, msg
