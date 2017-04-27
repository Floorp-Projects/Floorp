# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

import pytest

from mozlint import LintRoller


here = os.path.abspath(os.path.dirname(__file__))


@pytest.fixture
def lint(request):
    lintargs = getattr(request.module, 'lintargs', {})
    return LintRoller(root=here, **lintargs)


@pytest.fixture(scope='session')
def filedir():
    return os.path.join(here, 'files')


@pytest.fixture(scope='module')
def files(filedir, request):
    suffix_filter = getattr(request.module, 'files', [''])
    return [os.path.join(filedir, p) for p in os.listdir(filedir)
            if any(p.endswith(suffix) for suffix in suffix_filter)]


@pytest.fixture(scope='session')
def lintdir():
    return os.path.join(here, 'linters')


@pytest.fixture(scope='module')
def linters(lintdir, request):
    suffix_filter = getattr(request.module, 'linters', ['.lint.py'])
    return [os.path.join(lintdir, p) for p in os.listdir(lintdir)
            if any(p.endswith(suffix) for suffix in suffix_filter)]
