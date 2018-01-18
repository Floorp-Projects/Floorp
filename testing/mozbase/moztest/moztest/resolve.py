# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import cPickle as pickle
import os
import sys
from collections import defaultdict

import manifestparser
import mozpack.path as mozpath
from mozbuild.base import MozbuildObject
from mozbuild.util import OrderedDefaultDict
from mozbuild.frontend.reader import BuildReader, EmptyConfig
from mozversioncontrol import get_repository_object

here = os.path.abspath(os.path.dirname(__file__))


MOCHITEST_CHUNK_BY_DIR = 4
MOCHITEST_TOTAL_CHUNKS = 5

TEST_SUITES = {
    'cppunittest': {
        'aliases': ('Cpp', 'cpp'),
        'mach_command': 'cppunittest',
        'kwargs': {'test_file': None},
    },
    'crashtest': {
        'aliases': ('C', 'Rc', 'RC', 'rc'),
        'mach_command': 'crashtest',
        'kwargs': {'test_file': None},
    },
    'firefox-ui-functional': {
        'aliases': ('Fxfn',),
        'mach_command': 'firefox-ui-functional',
        'kwargs': {},
    },
    'firefox-ui-update': {
        'aliases': ('Fxup',),
        'mach_command': 'firefox-ui-update',
        'kwargs': {},
    },
    'check-spidermonkey': {
        'aliases': ('Sm', 'sm'),
        'mach_command': 'check-spidermonkey',
        'kwargs': {'valgrind': False},
    },
    # TODO(ato): integrate geckodriver tests with moz.build
    'geckodriver': {
        'mach_command': 'geckodriver-test',
        'aliases': ('testing/geckodriver',),
        'kwargs': {},
    },
    'mochitest-a11y': {
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'a11y', 'test_paths': None},
    },
    'mochitest-browser': {
        'aliases': ('bc', 'BC', 'Bc'),
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'browser-chrome', 'test_paths': None},
    },
    'mochitest-chrome': {
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'chrome', 'test_paths': None},
    },
    'mochitest-devtools': {
        'aliases': ('dt', 'DT', 'Dt'),
        'mach_command': 'mochitest',
        'kwargs': {'subsuite': 'devtools', 'test_paths': None},
    },
    'mochitest-plain': {
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'plain', 'test_paths': None},
    },
    'python': {
        'mach_command': 'python-test',
        'kwargs': {'tests': None},
    },
    'reftest': {
        'aliases': ('RR', 'rr', 'Rr'),
        'mach_command': 'reftest',
        'kwargs': {'tests': None},
    },
    'web-platform-tests': {
        'aliases': ('wpt',),
        'mach_command': 'web-platform-tests',
        'kwargs': {}
    },
    'valgrind': {
        'aliases': ('V', 'v'),
        'mach_command': 'valgrind-test',
        'kwargs': {},
    },
    'xpcshell': {
        'aliases': ('X', 'x'),
        'mach_command': 'xpcshell-test',
        'kwargs': {'test_file': 'all'},
    },
}

# Maps test flavors to metadata on how to run that test.
TEST_FLAVORS = {
    'a11y': {
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'a11y', 'test_paths': []},
        'task_regex': 'mochitest-a11y(?:-1)?$',
    },
    'browser-chrome': {
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'browser-chrome', 'test_paths': []},
        'task_regex': 'mochitest-browser-chrome(?:-e10s)?(?:-1)?$',
    },
    'crashtest': {},
    'chrome': {
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'chrome', 'test_paths': []},
        'task_regex': 'mochitest-chrome(?:-e10s)?(?:-1)?$',
    },
    'firefox-ui-functional': {
        'mach_command': 'firefox-ui-functional',
        'kwargs': {'tests': []},
    },
    'firefox-ui-update': {
        'mach_command': 'firefox-ui-update',
        'kwargs': {'tests': []},
    },
    'marionette': {
        'mach_command': 'marionette-test',
        'kwargs': {'tests': []},
    },
    'mochitest': {
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'mochitest', 'test_paths': []},
        'task_regex': 'mochitest(?:-e10s)?(?:-1)?$',
    },
    'python': {
        'mach_command': 'python-test',
        'kwargs': {},
    },
    'reftest': {
        'mach_command': 'reftest',
        'kwargs': {'tests': []},
        'task_regex': '(opt|debug)-reftest(?:-no-accel|-gpu|-stylo)?(?:-e10s)?(?:-1)?$',
    },
    'steeplechase': {},
    'web-platform-tests': {
        'mach_command': 'web-platform-tests',
        'kwargs': {'include': []},
        'task_regex': 'web-platform-tests(?:-reftests|-wdspec)?(?:-e10s)?(?:-1)?$',
    },
    'xpcshell': {
        'mach_command': 'xpcshell-test',
        'kwargs': {'test_paths': []},
        'task_regex': 'xpcshell(?:-1)?$',
    },
}

for i in range(1, MOCHITEST_TOTAL_CHUNKS + 1):
    TEST_SUITES['mochitest-%d' % i] = {
        'aliases': ('M%d' % i, 'm%d' % i),
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


def rewrite_test_base(test, new_base, honor_install_to_subdir=False):
    """Rewrite paths in a test to be under a new base path.

    This is useful for running tests from a separate location from where they
    were defined.

    honor_install_to_subdir and the underlying install-to-subdir field are a
    giant hack intended to work around the restriction where the mochitest
    runner can't handle single test files with multiple configurations. This
    argument should be removed once the mochitest runner talks manifests
    (bug 984670).
    """
    test['here'] = mozpath.join(new_base, test['dir_relpath'])

    if honor_install_to_subdir and test.get('install-to-subdir'):
        manifest_relpath = mozpath.relpath(test['path'],
                                           mozpath.dirname(test['manifest']))
        test['path'] = mozpath.join(new_base, test['dir_relpath'],
                                    test['install-to-subdir'], manifest_relpath)
    else:
        test['path'] = mozpath.join(new_base, test['file_relpath'])

    return test


class TestMetadata(object):
    """Holds information about tests.

    This class provides an API to query tests active in the build
    configuration.
    """

    def __init__(self, all_tests, test_defaults=None):
        self._tests_by_path = OrderedDefaultDict(list)
        self._tests_by_flavor = defaultdict(set)
        self._test_dirs = set()

        with open(all_tests, 'rb') as fh:
            test_data = pickle.load(fh)
        defaults = None
        if test_defaults:
            with open(test_defaults, 'rb') as fh:
                defaults = pickle.load(fh)
        for path, tests in test_data.items():
            for metadata in tests:
                if defaults:
                    defaults_manifests = [metadata['manifest']]

                    ancestor_manifest = metadata.get('ancestor-manifest')
                    if ancestor_manifest:
                        defaults_manifests.append(ancestor_manifest)

                    for manifest in defaults_manifests:
                        manifest_defaults = defaults.get(manifest)
                        if manifest_defaults:
                            metadata = manifestparser.combine_fields(manifest_defaults,
                                                                     metadata)
                self._tests_by_path[path].append(metadata)
                self._test_dirs.add(os.path.dirname(path))
                flavor = metadata.get('flavor')
                self._tests_by_flavor[flavor].add(path)

    def tests_with_flavor(self, flavor):
        """Obtain all tests having the specified flavor.

        This is a generator of dicts describing each test.
        """
        for path in sorted(self._tests_by_flavor.get(flavor, [])):
            yield self._tests_by_path[path]

    def resolve_tests(self, paths=None, flavor=None, subsuite=None, under_path=None,
                      tags=None):
        """Resolve tests from an identifier.

        This is a generator of dicts describing each test.

        ``paths`` can be an iterable of values to use to identify tests to run.
        If an entry is a known test file, tests associated with that file are
        returned (there may be multiple configurations for a single file). If
        an entry is a directory, or a prefix of a directory containing tests,
        all tests in that directory are returned. If the string appears in a
        known test file, that test file is considered. If the path contains
        a wildcard pattern, tests matching that pattern are returned.

        If ``under_path`` is a string, it will be used to filter out tests that
        aren't in the specified path prefix relative to topsrcdir or the
        test's installed dir.

        If ``flavor`` is a string, it will be used to filter returned tests
        to only be the flavor specified. A flavor is something like
        ``xpcshell``.

        If ``subsuite`` is a string, it will be used to filter returned tests
        to only be in the subsuite specified.

        If ``tags`` are specified, they will be used to filter returned tests
        to only those with a matching tag.
        """
        if tags:
            tags = set(tags)

        def fltr(tests):
            for test in tests:
                if flavor:
                    if flavor == 'devtools' and test.get('flavor') != 'browser-chrome':
                        continue
                    if flavor != 'devtools' and test.get('flavor') != flavor:
                        continue

                if subsuite and test.get('subsuite') != subsuite:
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

        for path in sorted(paths):
            if path is None:
                candidate_paths |= set(self._tests_by_path.keys())
                continue

            if '*' in path:
                candidate_paths |= {p for p in self._tests_by_path
                                    if mozpath.match(p, path)}
                continue

            # If the path is a directory, or the path is a prefix of a directory
            # containing tests, pull in all tests in that directory.
            if (path in self._test_dirs or
                any(p.startswith(path) for p in self._tests_by_path)):
                candidate_paths |= {p for p in self._tests_by_path
                                    if p.startswith(path)}
                continue

            # If it's a test file, add just that file.
            candidate_paths |= {p for p in self._tests_by_path if path in p}

        for p in sorted(candidate_paths):
            tests = self._tests_by_path[p]

            for test in fltr(tests):
                yield test


class TestResolver(MozbuildObject):
    """Helper to resolve tests from the current environment to test files."""

    def __init__(self, *args, **kwargs):
        MozbuildObject.__init__(self, *args, **kwargs)

        # If installing tests is going to result in re-generating the build
        # backend, we need to do this here, so that the updated contents of
        # all-tests.pkl make it to the set of tests to run.
        self._run_make(
            target='backend.TestManifestBackend', pass_thru=True, print_directory=False,
            filename=mozpath.join(self.topsrcdir, 'build', 'rebuild-backend.mk'),
            append_env={
                b'PYTHON': self.virtualenv_manager.python_path,
                b'BUILD_BACKEND_FILES': b'backend.TestManifestBackend',
                b'BACKEND_GENERATION_SCRIPT': mozpath.join(
                    self.topsrcdir, 'build', 'gen_test_backend.py'),
            },
        )

        self._tests = TestMetadata(os.path.join(self.topobjdir,
                                                'all-tests.pkl'),
                                   test_defaults=os.path.join(self.topobjdir,
                                                              'test-defaults.pkl'))

        self._test_rewrites = {
            'a11y': os.path.join(self.topobjdir, '_tests', 'testing',
                                 'mochitest', 'a11y'),
            'browser-chrome': os.path.join(self.topobjdir, '_tests', 'testing',
                                           'mochitest', 'browser'),
            'chrome': os.path.join(self.topobjdir, '_tests', 'testing',
                                   'mochitest', 'chrome'),
            'mochitest': os.path.join(self.topobjdir, '_tests', 'testing',
                                      'mochitest', 'tests'),
            'web-platform-tests': os.path.join(self.topobjdir, '_tests', 'testing',
                                               'web-platform'),
            'xpcshell': os.path.join(self.topobjdir, '_tests', 'xpcshell'),
        }
        self._vcs = None
        self.verbose = False

    @property
    def vcs(self):
        if not self._vcs:
            self._vcs = get_repository_object(self.topsrcdir)
        return self._vcs

    def resolve_tests(self, cwd=None, **kwargs):
        """Resolve tests in the context of the current environment.

        This is a more intelligent version of TestMetadata.resolve_tests().

        This function provides additional massaging and filtering of low-level
        results.

        Paths in returned tests are automatically translated to the paths in
        the _tests directory under the object directory.

        If cwd is defined, we will limit our results to tests under the
        directory specified. The directory should be defined as an absolute
        path under topsrcdir or topobjdir for it to work properly.
        """
        rewrite_base = None

        if cwd:
            norm_cwd = mozpath.normpath(cwd)
            norm_srcdir = mozpath.normpath(self.topsrcdir)
            norm_objdir = mozpath.normpath(self.topobjdir)

            reldir = None

            if norm_cwd.startswith(norm_objdir):
                reldir = norm_cwd[len(norm_objdir)+1:]
            elif norm_cwd.startswith(norm_srcdir):
                reldir = norm_cwd[len(norm_srcdir)+1:]

            result = self._tests.resolve_tests(under_path=reldir, **kwargs)

        else:
            result = self._tests.resolve_tests(**kwargs)

        for test in result:
            rewrite_base = self._test_rewrites.get(test['flavor'], None)

            if rewrite_base:
                yield rewrite_test_base(test, rewrite_base,
                                        honor_install_to_subdir=True)
            else:
                yield test

    def get_outgoing_metadata(self):
        paths, tags, flavors = set(), set(), set()
        changed_files = self.vcs.get_outgoing_files('AM')
        if changed_files:
            config = EmptyConfig(self.topsrcdir)
            reader = BuildReader(config)
            files_info = reader.files_info(changed_files)

            for path, info in files_info.items():
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
            for suite, v in TEST_SUITES.items():
                if entry in v.get('aliases', []):
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
