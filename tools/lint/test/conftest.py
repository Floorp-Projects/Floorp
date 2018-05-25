import os
import sys
from collections import defaultdict

from mozbuild.base import MozbuildObject
from mozlint.pathutils import findobject
from mozlint.parser import Parser

import pytest

here = os.path.abspath(os.path.dirname(__file__))
build = MozbuildObject.from_environment(cwd=here)

lintdir = os.path.dirname(here)
sys.path.insert(0, lintdir)


@pytest.fixture(scope='module')
def root(request):
    """Return the root directory for the files of the linter under test.

    For example, with LINTER=flake8 this would be tools/lint/test/files/flake8.
    """
    if not hasattr(request.module, 'LINTER'):
        pytest.fail("'root' fixture used from a module that didn't set the LINTER variable")
    return os.path.join(here, 'files', request.module.LINTER)


@pytest.fixture(scope='module')
def paths(root):
    """Return a function that can resolve file paths relative to the linter
    under test.

    Can be used like `paths('foo.py', 'bar/baz')`. This will return a list of
    absolute paths under the `root` files directory.
    """
    def _inner(*paths):
        if not paths:
            return [root]
        return [os.path.normpath(os.path.join(root, p)) for p in paths]
    return _inner


@pytest.fixture(scope='module')
def config(request):
    """Finds, loads and returns the config for the linter name specified by the
    LINTER global variable in the calling module.

    This implies that each test file (that uses this fixture) should only be
    used to test a single linter. If no LINTER variable is defined, the test
    will fail.
    """
    if not hasattr(request.module, 'LINTER'):
        pytest.fail("'config' fixture used from a module that didn't set the LINTER variable")

    name = request.module.LINTER
    config_path = os.path.join(lintdir, '{}.yml'.format(name))
    parser = Parser(build.topsrcdir)
    # TODO Don't assume one linter per yaml file
    return parser.parse(config_path)[0]


@pytest.fixture(scope='module', autouse=True)
def run_setup(config):
    """Make sure that if the linter named in the LINTER global variable has a
    setup function, it gets called before running the tests.
    """
    if 'setup' not in config:
        return

    func = findobject(config['setup'])
    func(build.topsrcdir)


@pytest.fixture(scope='module')
def lint(config, root):
    """Find and return the 'lint' function for the external linter named in the
    LINTER global variable.

    This will automatically pass in the 'config' and 'root' arguments if not
    specified.
    """
    try:
        func = findobject(config['payload'])
    except (ImportError, ValueError):
        pytest.fail("could not resolve a lint function from '{}'".format(config['payload']))

    def wrapper(paths, config=config, root=root, collapse_results=False, **lintargs):
        results = func(paths, config, root=root, **lintargs)
        if not collapse_results:
            return results

        ret = defaultdict(list)
        for r in results:
            ret[r.path].append(r)
        return ret

    return wrapper


@pytest.fixture
def create_temp_file(tmpdir):
    def inner(contents, name=None):
        name = name or 'temp.py'
        path = tmpdir.join(name)
        path.write(contents)
        return path.strpath
    return inner
