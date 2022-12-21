# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
from argparse import Namespace

import pytest

from mozlint import LintRoller

here = os.path.abspath(os.path.dirname(__file__))


@pytest.fixture
def lint(request):
    lintargs = getattr(request.module, "lintargs", {})
    lint = LintRoller(root=here, **lintargs)

    # Add a few super powers to our lint instance
    def mock_vcs(files):
        def _fake_vcs_files(*args, **kwargs):
            return files

        setattr(lint.vcs, "get_changed_files", _fake_vcs_files)
        setattr(lint.vcs, "get_outgoing_files", _fake_vcs_files)

    setattr(lint, "vcs", Namespace())
    setattr(lint, "mock_vcs", mock_vcs)
    return lint


@pytest.fixture(scope="session")
def filedir():
    return os.path.join(here, "files")


@pytest.fixture(scope="module")
def files(filedir, request):
    suffix_filter = getattr(request.module, "files", [""])
    return [
        os.path.join(filedir, p)
        for p in os.listdir(filedir)
        if any(p.endswith(suffix) for suffix in suffix_filter)
    ]


@pytest.fixture(scope="session")
def lintdir():
    lintdir = os.path.join(here, "linters")
    sys.path.insert(0, lintdir)
    return lintdir


@pytest.fixture(scope="module")
def linters(lintdir):
    def inner(*names):
        return [
            os.path.join(lintdir, p)
            for p in os.listdir(lintdir)
            if any(os.path.splitext(p)[0] == name for name in names)
            if os.path.splitext(p)[1] == ".yml"
        ]

    return inner
