# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
from contextlib import contextmanager

import pytest
import smoke

JS = os.path.join(os.path.dirname(__file__), "js.py")


@contextmanager
def fake_js():
    os.environ["JSSHELL"] = JS
    try:
        yield
    finally:
        del os.environ["JSSHELL"]


def test_run_no_jsshell():
    with pytest.raises(FileNotFoundError):
        smoke.run_jsshell("--fuzzing-safe -e 'print(\"PASSED\")'")


def test_run_jsshell_set():
    with fake_js():
        smoke.run_jsshell("--fuzzing-safe -e 'print(\"PASSED\")'")


def test_smoke_test():
    with fake_js():
        smoke.smoke_test()
