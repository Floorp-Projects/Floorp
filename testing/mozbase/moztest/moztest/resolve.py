# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import fnmatch
import os
import pickle
import sys

import six

from abc import ABCMeta, abstractmethod
from collections import defaultdict

import mozpack.path as mozpath
from manifestparser import combine_fields, TestManifest
from mozbuild.base import MozbuildObject
from mozbuild.testing import TEST_MANIFESTS, REFTEST_FLAVORS
from mozbuild.util import OrderedDefaultDict
from mozpack.files import FileFinder

here = os.path.abspath(os.path.dirname(__file__))

MOCHITEST_CHUNK_BY_DIR = 4
MOCHITEST_TOTAL_CHUNKS = 5


def WebglSuite(name):
    return {
        'aliases': (name,),
        'build_flavor': 'mochitest',
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'plain', 'subsuite': name, 'test_paths': None},
        'task_regex': ['mochitest-' + name + '($|.*(-1|[^0-9])$)',
                       'test-verify($|.*(-1|[^0-9])$)'],
    }


TEST_SUITES = {
    'cppunittest': {
        'aliases': ('cpp',),
        'mach_command': 'cppunittest',
        'kwargs': {'test_file': None},
    },
    'crashtest': {
        'aliases': ('c', 'rc'),
        'build_flavor': 'crashtest',
        'mach_command': 'crashtest',
        'kwargs': {'test_file': None},
        'task_regex': ['crashtest($|.*(-1|[^0-9])$)',
                       'test-verify($|.*(-1|[^0-9])$)'],
    },
    'firefox-ui-functional': {
        'aliases': ('fxfn',),
        'mach_command': 'firefox-ui-functional',
        'kwargs': {},
    },
    'firefox-ui-update': {
        'aliases': ('fxup',),
        'mach_command': 'firefox-ui-update',
        'kwargs': {},
    },
    'check-spidermonkey': {
        'aliases': ('sm',),
        'mach_command': 'check-spidermonkey',
        'kwargs': {'valgrind': False},
    },
    # TODO(ato): integrate geckodriver tests with moz.build
    'geckodriver': {
        'aliases': ('testing/geckodriver',),
        'mach_command': 'geckodriver-test',
        'kwargs': {},
    },
    'marionette': {
        'aliases': ('mn',),
        'mach_command': 'marionette-test',
        'kwargs': {'tests': None},
        'task_regex': ['marionette($|.*(-1|[^0-9])$)'],
    },
    'mochitest-a11y': {
        'aliases': ('a11y', 'ally'),
        'build_flavor': 'a11y',
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'a11y', 'test_paths': None, 'e10s': False},
        'task_regex': ['mochitest-a11y($|.*(-1|[^0-9])$)',
                       'test-verify($|.*(-1|[^0-9])$)'],
    },
    'mochitest-browser-chrome': {
        'aliases': ('bc', 'browser'),
        'build_flavor': 'browser-chrome',
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'browser-chrome', 'test_paths': None},
        'task_regex': ['mochitest-browser-chrome($|.*(-1|[^0-9])$)',
                       'test-verify($|.*(-1|[^0-9])$)'],
    },
    'mochitest-browser-chrome-screenshots': {
        'aliases': ('ss', 'screenshots-chrome'),
        'build_flavor': 'browser-chrome',
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'browser-chrome', 'subsuite': 'screenshots', 'test_paths': None},
        'task_regex': ['browser-screenshots($|.*(-1|[^0-9])$)'],
    },
    'mochitest-chrome': {
        'aliases': ('mc',),
        'build_flavor': 'chrome',
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'chrome', 'test_paths': None, 'e10s': False},
        'task_regex': ['mochitest-chrome($|.*(-1|[^0-9])$)',
                       'test-verify($|.*(-1|[^0-9])$)'],
    },
    'mochitest-chrome-gpu': {
        'aliases': ('gpu',),
        'build_flavor': 'chrome',
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'chrome', 'subsuite': 'gpu', 'test_paths': None, 'e10s': False},
        'task_regex': ['mochitest-gpu($|.*(-1|[^0-9])$)',
                       'test-verify($|.*(-1|[^0-9])$)'],
    },
    'mochitest-devtools-chrome': {
        'aliases': ('dt', 'devtools'),
        'build_flavor': 'browser-chrome',
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'browser-chrome', 'subsuite': 'devtools', 'test_paths': None},
        'task_regex': ['mochitest-devtools-chrome($|.*(-1|[^0-9])$)',
                       'test-verify($|.*(-1|[^0-9])$)'],
    },
    'mochitest-media': {
        'aliases': ('mpm', 'plain-media'),
        'build_flavor': 'mochitest',
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'plain', 'subsuite': 'media', 'test_paths': None},
        'task_regex': ['mochitest-media($|.*(-1|[^0-9])$)',
                       'test-verify($|.*(-1|[^0-9])$)'],
    },
    'mochitest-plain': {
        'aliases': ('mp', 'plain',),
        'build_flavor': 'mochitest',
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'plain', 'test_paths': None},
        'task_regex': ['mochitest-plain($|.*(-1|[^0-9])$)',  # noqa
                       'test-verify($|.*(-1|[^0-9])$)'],
    },
    'mochitest-plain-gpu': {
        'aliases': ('gpu',),
        'build_flavor': 'mochitest',
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'plain', 'subsuite': 'gpu', 'test_paths': None},
        'task_regex': ['mochitest-gpu($|.*(-1|[^0-9])$)',
                       'test-verify($|.*(-1|[^0-9])$)'],
    },
    'mochitest-remote': {
        'aliases': ('remote',),
        'build_flavor': 'browser-chrome',
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'browser-chrome', 'subsuite': 'remote', 'test_paths': None},
        'task_regex': ['mochitest-remote($|.*(-1|[^0-9])$)',
                       'test-verify($|.*(-1|[^0-9])$)'],
    },
    'mochitest-webgl1-core': WebglSuite('webgl1-core'),
    'mochitest-webgl1-ext': WebglSuite('webgl1-ext'),
    'mochitest-webgl2-core': WebglSuite('webgl2-core'),
    'mochitest-webgl2-ext': WebglSuite('webgl2-ext'),
    'mochitest-webgl2-deqp': WebglSuite('webgl2-deqp'),
    'mochitest-webgpu': WebglSuite('webgpu'),
    'puppeteer': {
        'aliases': ('remote/test/puppeteer',),
        'mach_command': 'puppeteer-test',
        'kwargs': {'headless': False},
    },
    'python': {
        'build_flavor': 'python',
        'mach_command': 'python-test',
        'kwargs': {'tests': None},
    },
    'telemetry-tests-client': {
        'aliases': ('ttc',),
        'build_flavor': 'telemetry-tests-client',
        'mach_command': 'telemetry-tests-client',
        'kwargs': {},
        'task_regex': ['telemetry-tests-client($|.*(-1|[^0-9])$)'],
    },
    'reftest': {
        'aliases': ('rr',),
        'build_flavor': 'reftest',
        'mach_command': 'reftest',
        'kwargs': {'tests': None},
        'task_regex': ['(opt|debug)-reftest($|.*(-1|[^0-9])$)',
                       'test-verify-gpu($|.*(-1|[^0-9])$)'],
    },
    'robocop': {
        'mach_command': 'robocop',
        'kwargs': {'test_paths': None},
        'task_regex': ['robocop($|.*(-1|[^0-9])$)'],
    },
    'web-platform-tests': {
        'aliases': ('wpt',),
        'mach_command': 'web-platform-tests',
        'build_flavor': 'web-platform-tests',
        'kwargs': {'subsuite': 'testharness'},
        'task_regex': ['web-platform-tests($|.*(-1|[^0-9])$)',
                       'test-verify-wpt'],
    },
    'web-platform-tests-crashtest': {
        'aliases': ('wpt',),
        'mach_command': 'web-platform-tests',
        'kwargs': {'include': []},
        'task_regex': ['web-platform-tests-crashtest($|.*(-1|[^0-9])$)',
                       'test-verify-wpt'],
    },
    'web-platform-tests-testharness': {
        'aliases': ('wpt',),
        'mach_command': 'web-platform-tests',
        'kwargs': {'include': []},
        'task_regex': ['web-platform-tests(?!-reftest|-wdspec)($|.*(-1|[^0-9])$)',
                       'test-verify-wpt'],
    },
    'web-platform-tests-reftest': {
        'aliases': ('wpt',),
        'mach_command': 'web-platform-tests',
        'kwargs': {'include': []},
        'task_regex': ['web-platform-tests-reftest($|.*(-1|[^0-9])$)',
                       'test-verify-wpt'],
    },
    'web-platform-tests-wdspec': {
        'aliases': ('wpt',),
        'mach_command': 'web-platform-tests',
        'kwargs': {'include': []},
        'task_regex': ['web-platform-tests-wdspec($|.*(-1|[^0-9])$)',
                       'test-verify-wpt'],
    },
    'valgrind': {
        'aliases': ('v',),
        'mach_command': 'valgrind-test',
        'kwargs': {},
    },
    'xpcshell': {
        'aliases': ('x',),
        'build_flavor': 'xpcshell',
        'mach_command': 'xpcshell-test',
        'kwargs': {'test_file': 'all'},
        'task_regex': ['xpcshell($|.*(-1|[^0-9])$)',
                       'test-verify($|.*(-1|[^0-9])$)'],
    },
}
"""Definitions of all test suites and the metadata needed to run and process
them. Each test suite definition can contain the following keys.

Arguments:
    aliases (tuple): A tuple containing shorthands used to refer to this suite.
    build_flavor (str): The flavor assigned to this suite by the build system
                        in `mozbuild.testing.TEST_MANIFESTS` (or similar).
    mach_command (str): Name of the mach command used to run this suite.
    kwargs (dict): Arguments needed to pass into the mach command.
    task_regex (list): A list of regexes used to filter task labels that run
                       this suite.
"""

for i in range(1, MOCHITEST_TOTAL_CHUNKS + 1):
    TEST_SUITES['mochitest-%d' % i] = {
        'aliases': ('m%d' % i,),
        'mach_command': 'mochitest',
        'kwargs': {
            'flavor': 'mochitest',
            'subsuite': 'default',
            'chunk_by_dir': MOCHITEST_CHUNK_BY_DIR,
            'total_chunks': MOCHITEST_TOTAL_CHUNKS,
            'this_chunk': i,
            'test_paths': None,
        },
    }

_test_flavors = {
    'a11y': 'mochitest-a11y',
    'browser-chrome': 'mochitest-browser-chrome',
    'chrome': 'mochitest-chrome',
    'crashtest': 'crashtest',
    'firefox-ui-functional': 'firefox-ui-functional',
    'firefox-ui-update': 'firefox-ui-update',
    'marionette': 'marionette',
    'mochitest': 'mochitest-plain',
    'puppeteer': 'puppeteer',
    'python': 'python',
    'reftest': 'reftest',
    'steeplechase': '',
    'telemetry-tests-client': 'telemetry-tests-client',
    'web-platform-tests': 'web-platform-tests',
    'xpcshell': 'xpcshell',
}

_test_subsuites = {
    ('browser-chrome', 'devtools'): 'mochitest-devtools-chrome',
    ('browser-chrome', 'remote'): 'mochitest-remote',
    ('browser-chrome', 'screenshots'): 'mochitest-browser-chrome-screenshots',
    ('chrome', 'gpu'): 'mochitest-chrome-gpu',
    ('mochitest', 'gpu'): 'mochitest-plain-gpu',
    ('mochitest', 'media'): 'mochitest-media',
    ('mochitest', 'robocop'): 'robocop',
    ('mochitest', 'webgl1-core'): 'mochitest-webgl1-core',
    ('mochitest', 'webgl1-ext'): 'mochitest-webgl1-ext',
    ('mochitest', 'webgl2-core'): 'mochitest-webgl2-core',
    ('mochitest', 'webgl2-ext'): 'mochitest-webgl2-ext',
    ('mochitest', 'webgl2-deqp'): 'mochitest-webgl2-deqp',
    ('mochitest', 'webgpu'): 'mochitest-webgpu',
    ('web-platform-tests', 'testharness'): 'web-platform-tests-testharness',
    ('web-platform-tests', 'crashtest'): 'web-platform-tests-crashtest',
    ('web-platform-tests', 'reftest'): 'web-platform-tests-reftest',
    ('web-platform-tests', 'wdspec'): 'web-platform-tests-wdspec',
}


def get_suite_definition(flavor, subsuite=None, strict=False):
    """Return a suite definition given a flavor and optional subsuite.

    If strict is True, a subsuite must have its own entry in TEST_SUITES.
    Otherwise, the entry for 'flavor' will be returned with the 'subsuite'
    keyword arg set.

    With or without strict mode, an empty dict will be returned if no
    matching suite definition was found.
    """
    if not subsuite:
        suite_name = _test_flavors.get(flavor)
        return suite_name, TEST_SUITES.get(suite_name, {}).copy()

    suite_name = _test_subsuites.get((flavor, subsuite))
    if suite_name or strict:
        return suite_name, TEST_SUITES.get(suite_name, {}).copy()

    suite_name = _test_flavors.get(flavor)
    if suite_name not in TEST_SUITES:
        return suite_name, {}

    suite = TEST_SUITES[suite_name].copy()
    suite.setdefault('kwargs', {})
    suite['kwargs']['subsuite'] = subsuite
    return suite_name, suite


def rewrite_test_base(test, new_base):
    """Rewrite paths in a test to be under a new base path.

    This is useful for running tests from a separate location from where they
    were defined.
    """
    test['here'] = mozpath.join(new_base, test['dir_relpath'])
    test['path'] = mozpath.join(new_base, test['file_relpath'])
    return test


@six.add_metaclass(ABCMeta)
class TestLoader(MozbuildObject):

    @abstractmethod
    def __call__(self):
        """Generate test metadata."""


class BuildBackendLoader(TestLoader):
    def __call__(self):
        """Loads the test metadata generated by the TestManifest build backend.

        The data is stored in two files:

            - <objdir>/all-tests.pkl
            - <objdir>/test-defaults.pkl

        The 'all-tests.pkl' file is a mapping of source path to test objects. The
        'test-defaults.pkl' file maps manifests to their DEFAULT configuration.
        These manifest defaults will be merged into the test configuration of the
        contained tests.
        """
        # If installing tests is going to result in re-generating the build
        # backend, we need to do this here, so that the updated contents of
        # all-tests.pkl make it to the set of tests to run.
        if self.backend_out_of_date(mozpath.join(self.topobjdir,
                                                 'backend.TestManifestBackend'
                                                 )):
            print("Test configuration changed. Regenerating backend.")
            from mozbuild.gen_test_backend import gen_test_backend
            gen_test_backend()

        all_tests = os.path.join(self.topobjdir, 'all-tests.pkl')
        test_defaults = os.path.join(self.topobjdir, 'test-defaults.pkl')

        with open(all_tests, 'rb') as fh:
            test_data = pickle.load(fh)

        with open(test_defaults, 'rb') as fh:
            defaults = pickle.load(fh)

        for path, tests in six.iteritems(test_data):
            for metadata in tests:
                defaults_manifests = [metadata['manifest']]

                ancestor_manifest = metadata.get('ancestor_manifest')
                if ancestor_manifest:
                    # The (ancestor manifest, included manifest) tuple
                    # contains the defaults of the included manifest, so
                    # use it instead of [metadata['manifest']].
                    ancestor_manifest = os.path.join(self.topsrcdir, ancestor_manifest)
                    defaults_manifests[0] = (ancestor_manifest, metadata['manifest'])
                    defaults_manifests.append(ancestor_manifest)

                for manifest in defaults_manifests:
                    manifest_defaults = defaults.get(manifest)
                    if manifest_defaults:
                        metadata = combine_fields(manifest_defaults, metadata)

                yield metadata


class TestManifestLoader(TestLoader):
    def __init__(self, *args, **kwargs):
        super(TestManifestLoader, self).__init__(*args, **kwargs)
        self.finder = FileFinder(self.topsrcdir)
        self.reader = self.mozbuild_reader(config_mode="empty")
        self.variables = {
            '{}_MANIFESTS'.format(k): v[0] for k, v in six.iteritems(TEST_MANIFESTS)
        }
        self.variables.update({
            '{}_MANIFESTS'.format(f.upper()): f for f in REFTEST_FLAVORS
        })

    def _load_manifestparser_manifest(self, mpath):
        mp = TestManifest(manifests=[mpath], strict=True, rootdir=self.topsrcdir,
                          finder=self.finder, handle_defaults=True)
        return (test for test in mp.tests)

    def _load_reftest_manifest(self, mpath):
        import reftest
        manifest = reftest.ReftestManifest(finder=self.finder)
        manifest.load(mpath)

        for test in sorted(manifest.tests):
            test['manifest_relpath'] = test['manifest'][len(self.topsrcdir)+1:]
            yield test

    def __call__(self):
        for path, name, key, value in self.reader.find_variables_from_ast(self.variables):
            mpath = os.path.join(self.topsrcdir, os.path.dirname(path), value)
            flavor = self.variables[name]

            if name.rsplit('_', 1)[0].lower() in REFTEST_FLAVORS:
                tests = self._load_reftest_manifest(mpath)
            else:
                tests = self._load_manifestparser_manifest(mpath)

            for test in tests:
                path = mozpath.normpath(test['path'])
                assert mozpath.basedir(path, [self.topsrcdir])
                relpath = path[len(self.topsrcdir)+1:]

                # Add these keys for compatibility with the build backend loader.
                test['flavor'] = flavor
                test['file_relpath'] = relpath
                test['srcdir_relpath'] = relpath
                test['dir_relpath'] = mozpath.dirname(relpath)

                yield test


class TestResolver(MozbuildObject):
    """Helper to resolve tests from the current environment to test files."""
    test_rewrites = {
        'a11y': '_tests/testing/mochitest/a11y',
        'browser-chrome': '_tests/testing/mochitest/browser',
        'chrome': '_tests/testing/mochitest/chrome',
        'mochitest': '_tests/testing/mochitest/tests',
        'xpcshell': '_tests/xpcshell',
    }

    def __init__(self, *args, **kwargs):
        loader_cls = kwargs.pop('loader_cls', BuildBackendLoader)
        super(TestResolver, self).__init__(*args, **kwargs)

        self.load_tests = self._spawn(loader_cls)
        self._tests = []
        self._reset_state()

        # These suites aren't registered in moz.build so require special handling.
        self._puppeteer_loaded = False
        self._tests_loaded = False
        self._wpt_loaded = False

    def _reset_state(self):
        self._tests_by_path = OrderedDefaultDict(list)
        self._tests_by_flavor = defaultdict(set)
        self._tests_by_manifest = defaultdict(list)
        self._test_dirs = set()

    @property
    def tests(self):
        if not self._tests_loaded:
            self._reset_state()
            for test in self.load_tests():
                self._tests.append(test)
            self._tests_loaded = True
        return self._tests

    @property
    def tests_by_path(self):
        if not self._tests_by_path:
            for test in self.tests:
                self._tests_by_path[test['file_relpath']].append(test)
        return self._tests_by_path

    @property
    def tests_by_flavor(self):
        if not self._tests_by_flavor:
            for test in self.tests:
                self._tests_by_flavor[test['flavor']].add(test['file_relpath'])
        return self._tests_by_flavor

    @property
    def tests_by_manifest(self):
        if not self._tests_by_manifest:
            for test in self.tests:
                relpath = mozpath.relpath(test['path'], mozpath.dirname(test['manifest']))
                self._tests_by_manifest[test['manifest_relpath']].append(relpath)
        return self._tests_by_manifest

    @property
    def test_dirs(self):
        if not self._test_dirs:
            for test in self.tests:
                self._test_dirs.add(test['dir_relpath'])
        return self._test_dirs

    def _resolve(self, paths=None, flavor=None, subsuite=None, under_path=None, tags=None):
        if tags:
            tags = set(tags)

        def fltr(tests):
            for test in tests:
                if flavor:
                    if flavor == 'devtools' and test.get('flavor') != 'browser-chrome':
                        continue
                    if flavor != 'devtools' and test.get('flavor') != flavor:
                        continue

                if subsuite and test.get('subsuite', 'undefined') != subsuite:
                    continue

                if tags and not (tags & set(test.get('tags', '').split())):
                    continue

                if under_path and not test['file_relpath'].startswith(under_path):
                    continue

                # Make a copy so modifications don't change the source.
                yield dict(test)

        paths = paths or []
        paths = [mozpath.normpath(p) for p in paths]
        if not paths:
            paths = [None]

        candidate_paths = set()

        if flavor in (None, 'puppeteer') and any(self.is_puppeteer_path(p) for p in paths):
            self.add_puppeteer_manifest_data()

        if flavor in (None, 'web-platform-tests') and any(self.is_wpt_path(p) for p in paths):
            self.add_wpt_manifest_data()

        for path in sorted(paths):
            if path is None:
                candidate_paths |= set(self.tests_by_path.keys())
                continue

            if '*' in path:
                candidate_paths |= {p for p in self.tests_by_path
                                    if mozpath.match(p, path)}
                continue

            # If the path is a directory, or the path is a prefix of a directory
            # containing tests, pull in all tests in that directory.
            if (path in self.test_dirs or
                any(p.startswith(path) for p in self.tests_by_path)):
                candidate_paths |= {p for p in self.tests_by_path
                                    if p.startswith(path)}
                continue

            # If the path is a manifest, add all tests defined in that manifest.
            if any(path.endswith(e) for e in ('.ini', '.list')):
                key = 'manifest' if os.path.isabs(path) else 'manifest_relpath'
                candidate_paths |= {t['file_relpath'] for t in self.tests
                                    if mozpath.normpath(t[key]) == path}
                continue

            # If it's a test file, add just that file.
            candidate_paths |= {p for p in self.tests_by_path if path in p}

        for p in sorted(candidate_paths):
            tests = self.tests_by_path[p]

            for test in fltr(tests):
                yield test

    def is_puppeteer_path(self, path):
        if path is None:
            return True
        return mozpath.match(path, "remote/test/puppeteer/test/**")

    def add_puppeteer_manifest_data(self):
        if self._puppeteer_loaded:
            return

        self._reset_state()

        test_path = os.path.join(self.topsrcdir, "remote", "test", "puppeteer", "test")
        for root, dirs, paths in os.walk(test_path):
            for filename in fnmatch.filter(paths, "*.spec.js"):
                path = os.path.join(root, filename)
                self._tests.append({
                    "path": os.path.abspath(path),
                    "flavor": "puppeteer",
                    "here": os.path.dirname(path),
                    "manifest": None,
                    "name": path,
                    "file_relpath": path,
                    "head": "",
                    "support-files": "",
                    "subsuite": "puppeteer",
                    "dir_relpath": os.path.dirname(path),
                    "srcdir_relpath": path,
                })

        self._puppeteer_loaded = True

    def is_wpt_path(self, path):
        if path is None:
            return True
        if mozpath.match(path, "testing/web-platform/tests/**"):
            return True
        if mozpath.match(path, "testing/web-platform/mozilla/tests/**"):
            return True
        return False

    def add_wpt_manifest_data(self):
        if self._wpt_loaded:
            return

        self._reset_state()

        wpt_path = os.path.join(self.topsrcdir, "testing", "web-platform")
        sys.path = [wpt_path] + sys.path

        import manifestupdate
        # Set up a logger that will drop all the output
        import logging
        logger = logging.getLogger("manifestupdate")
        logger.propogate = False

        manifests = manifestupdate.run(self.topsrcdir, self.topobjdir, rebuild=False,
                                       download=True, config_path=None, rewrite_config=True,
                                       update=True, logger=logger)
        if not manifests:
            print("Loading wpt manifest failed")
            return

        for manifest, data in six.iteritems(manifests):
            tests_root = data["tests_path"]
            for test_type, path, tests in manifest:
                full_path = mozpath.join(tests_root, path)
                src_path = mozpath.relpath(full_path, self.topsrcdir)
                if test_type not in ["testharness", "reftest", "wdspec", "crashtest"]:
                    continue
                for test in tests:
                    self._tests.append({
                        "path": full_path,
                        "flavor": "web-platform-tests",
                        "here": mozpath.dirname(path),
                        "manifest": data["manifest_path"],
                        "name": test.id,
                        "file_relpath": src_path,
                        "head": "",
                        "support-files": "",
                        "subsuite": test_type,
                        "dir_relpath": mozpath.dirname(src_path),
                        "srcdir_relpath": src_path,
                    })

        self._wpt_loaded = True

    def resolve_tests(self, cwd=None, **kwargs):
        """Resolve tests from an identifier.

        This is a generator of dicts describing each test. All arguments are
        optional.

        Paths in returned tests are automatically translated to the paths in
        the _tests directory under the object directory.

        Args:
            cwd (str):
                If specified, we will limit our results to tests under this
                directory. The directory should be defined as an absolute path
                under topsrcdir or topobjdir.

            paths (list):
                An iterable of values to use to identify tests to run. If an
                entry is a known test file, tests associated with that file are
                returned (there may be multiple configurations for a single
                file). If an entry is a directory, or a prefix of a directory
                containing tests, all tests in that directory are returned. If
                the string appears in a known test file, that test file is
                considered. If the path contains a wildcard pattern, tests
                matching that pattern are returned.

            under_path (str):
                If specified, will be used to filter out tests that aren't in
                the specified path prefix relative to topsrcdir or the test's
                installed dir.

            flavor (str):
                If specified, will be used to filter returned tests to only be
                the flavor specified. A flavor is something like ``xpcshell``.

            subsuite (str):
                If specified will be used to filter returned tests to only be
                in the subsuite specified. To filter only tests that *don't*
                have any subsuite, pass the string 'undefined'.

            tags (list):
                If specified, will be used to filter out tests that don't contain
                a matching tag.
        """
        if cwd:
            norm_cwd = mozpath.normpath(cwd)
            norm_srcdir = mozpath.normpath(self.topsrcdir)
            norm_objdir = mozpath.normpath(self.topobjdir)

            reldir = None

            if norm_cwd.startswith(norm_objdir):
                reldir = norm_cwd[len(norm_objdir)+1:]
            elif norm_cwd.startswith(norm_srcdir):
                reldir = norm_cwd[len(norm_srcdir)+1:]

            kwargs['under_path'] = reldir

        rewrite_base = None
        for test in self._resolve(**kwargs):
            rewrite_base = self.test_rewrites.get(test['flavor'], None)

            if rewrite_base:
                rewrite_base = os.path.join(self.topobjdir, os.path.normpath(rewrite_base))
                yield rewrite_test_base(test, rewrite_base)
            else:
                yield test

    def get_outgoing_metadata(self):
        paths, tags, flavors = set(), set(), set()
        changed_files = self.repository.get_outgoing_files('AM')
        if changed_files:
            reader = self.mozbuild_reader(config_mode='empty')
            files_info = reader.files_info(changed_files)

            for path, info in six.iteritems(files_info):
                paths |= info.test_files
                tags |= info.test_tags
                flavors |= info.test_flavors

        return {
            'paths': paths,
            'tags': tags,
            'flavors': flavors,
        }

    def resolve_metadata(self, what):
        """Resolve tests based on the given metadata. If not specified, metadata
        from outgoing files will be used instead.
        """
        # Parse arguments and assemble a test "plan."
        run_suites = set()
        run_tests = []

        for entry in what:
            # If the path matches the name or alias of an entire suite, run
            # the entire suite.
            if entry in TEST_SUITES:
                run_suites.add(entry)
                continue
            suitefound = False
            for suite, v in six.iteritems(TEST_SUITES):
                if entry.lower() in v.get('aliases', []):
                    run_suites.add(suite)
                    suitefound = True
            if suitefound:
                continue

            # Now look for file/directory matches in the TestResolver.
            relpath = self._wrap_path_argument(entry).relpath()
            tests = list(self.resolve_tests(paths=[relpath]))
            run_tests.extend(tests)

            if not tests:
                print('UNKNOWN TEST: %s' % entry, file=sys.stderr)

        if not what:
            res = self.get_outgoing_metadata()
            paths, tags, flavors = (res[key] for key in ('paths', 'tags', 'flavors'))

            # This requires multiple calls to resolve_tests, because the test
            # resolver returns tests that match every condition, while we want
            # tests that match any condition. Bug 1210213 tracks implementing
            # more flexible querying.
            if tags:
                run_tests = list(self.resolve_tests(tags=tags))
            if paths:
                run_tests += [t for t in self.resolve_tests(paths=paths)
                              if not (tags & set(t.get('tags', '').split()))]
            if flavors:
                run_tests = [
                    t for t in run_tests if t['flavor'] not in flavors]
                for flavor in flavors:
                    run_tests += list(self.resolve_tests(flavor=flavor))

        return run_suites, run_tests
