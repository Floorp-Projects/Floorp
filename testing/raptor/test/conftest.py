from __future__ import absolute_import

import json
import os
import sys

import pytest

# need this so raptor imports work both from /raptor and via mach
here = os.path.abspath(os.path.dirname(__file__))
if os.environ.get('SCRIPTSPATH', None) is not None:
    # in production it is env SCRIPTS_PATH
    mozharness_dir = os.environ['SCRIPTSPATH']
else:
    # locally it's in source tree
    mozharness_dir = os.path.join(here, '../../mozharness')
sys.path.insert(0, mozharness_dir)

from raptor.raptor import Raptor


@pytest.fixture(scope='function')
def options(request):
    opts = {
        'app': 'firefox',
        'binary': 'path/to/dummy/browser',
    }

    if hasattr(request.module, 'OPTIONS'):
        opts.update(request.module.OPTIONS)
    return opts


@pytest.fixture(scope='function')
def raptor(options):
    return Raptor(**options)


@pytest.fixture(scope='session')
def get_prefs():
    def _inner(browser):
        import raptor
        prefs_dir = os.path.join(raptor.__file__, 'preferences')
        with open(os.path.join(prefs_dir, '{}.json'.format(browser)), 'r') as fh:
            return json.load(fh)


@pytest.fixture(scope='session')
def filedir():
    return os.path.join(here, 'files')


@pytest.fixture
def get_binary():
    from moztest.selftest import fixtures

    def inner(app):
        if app != 'firefox':
            pytest.xfail(reason="{} support not implemented".format(app))

        binary = fixtures.binary()
        if not binary:
            pytest.skip("could not find a {} binary".format(app))
        return binary

    return inner
