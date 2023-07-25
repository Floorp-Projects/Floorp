# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import unittest
from collections.abc import Iterable
from pathlib import Path
from typing import List, Optional, Union

import pytest
from buildconfig import topsrcdir

try:
    from StringIO import StringIO
except ImportError:
    # TODO io.StringIO causes failures with Python 2 (needs to be sorted out)
    from io import StringIO

from mach.command_util import load_commands_from_entry_point, load_commands_from_file
from mach.main import Mach

PROVIDER_DIR = Path(__file__).resolve().parent / "providers"


@pytest.fixture(scope="class")
def get_mach(request):
    def _populate_context(key):
        if key == "topdir":
            return topsrcdir

    def inner(
        provider_files: Optional[Union[Path, List[Path]]] = None,
        entry_point=None,
        context_handler=None,
    ):
        m = Mach(str(Path.cwd()))
        m.define_category("testing", "Mach unittest", "Testing for mach core", 10)
        m.define_category("misc", "Mach misc", "Testing for mach core", 20)
        m.populate_context_handler = context_handler or _populate_context

        if provider_files:
            if not isinstance(provider_files, Iterable):
                provider_files = [provider_files]

            for path in provider_files:
                load_commands_from_file(PROVIDER_DIR / path)

        if entry_point:
            load_commands_from_entry_point(entry_point)

        return m

    if request.cls and issubclass(request.cls, unittest.TestCase):
        request.cls.get_mach = lambda cls, *args, **kwargs: inner(*args, **kwargs)
    return inner


@pytest.fixture(scope="class")
def run_mach(request, get_mach):
    def inner(argv, *args, **kwargs):
        m = get_mach(*args, **kwargs)

        stdout = StringIO()
        stderr = StringIO()

        if sys.version_info < (3, 0):
            stdout.encoding = "UTF-8"
            stderr.encoding = "UTF-8"

        try:
            result = m.run(argv, stdout=stdout, stderr=stderr)
        except SystemExit:
            result = None

        return (result, stdout.getvalue(), stderr.getvalue())

    if request.cls and issubclass(request.cls, unittest.TestCase):
        request.cls._run_mach = lambda cls, *args, **kwargs: inner(*args, **kwargs)
    return inner


@pytest.mark.usefixtures("get_mach", "run_mach")
class TestBase(unittest.TestCase):
    pass
