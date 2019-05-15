from __future__ import absolute_import

import json
import os
import sys

import pytest

from argparse import Namespace

# need this so raptor imports work both from /raptor and via mach
here = os.path.abspath(os.path.dirname(__file__))
if os.environ.get('SCRIPTSPATH') is not None:
    # in production it is env SCRIPTS_PATH
    mozharness_dir = os.environ['SCRIPTSPATH']
else:
    # locally it's in source tree
    mozharness_dir = os.path.join(here, '../../mozharness')
sys.path.insert(0, mozharness_dir)

from raptor.raptor import RaptorDesktopFirefox


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
    return RaptorDesktopFirefox(**options)


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


@pytest.fixture
def create_args():
    args = Namespace(app='firefox',
                     test='raptor-tp6-1',
                     binary='path/to/binary',
                     gecko_profile=False,
                     debug_mode=False,
                     page_cycles=None,
                     page_timeout=None,
                     host=None,
                     run_local=True)

    def inner(**kwargs):
        for next_arg in kwargs:
            print next_arg
            print kwargs[next_arg]
            setattr(args, next_arg, kwargs[next_arg])
        return args

    return inner
