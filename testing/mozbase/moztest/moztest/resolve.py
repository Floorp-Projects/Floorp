# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import fnmatch
import os
import pickle
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


def WebglSuite(name):
    return {
        'aliases': (name,),
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
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'a11y', 'test_paths': None, 'e10s': False},
        'task_regex': ['mochitest-a11y($|.*(-1|[^0-9])$)',
                       'test-verify($|.*(-1|[^0-9])$)'],
    },
    'mochitest-browser-chrome': {
        'aliases': ('bc', 'browser'),
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'browser-chrome', 'test_paths': None},
        'task_regex': ['mochitest-browser-chrome($|.*(-1|[^0-9])$)',
                       'test-verify($|.*(-1|[^0-9])$)'],
    },
    'mochitest-browser-chrome-screenshots': {
        'aliases': ('ss', 'screenshots-chrome'),
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'browser-chrome', 'subsuite': 'screenshots', 'test_paths': None},
        'task_regex': ['browser-screenshots($|.*(-1|[^0-9])$)'],
    },
    'mochitest-chrome': {
        'aliases': ('mc',),
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'chrome', 'test_paths': None, 'e10s': False},
        'task_regex': ['mochitest-chrome($|.*(-1|[^0-9])$)',
                       'test-verify($|.*(-1|[^0-9])$)'],
    },
    'mochitest-chrome-gpu': {
        'aliases': ('gpu',),
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'chrome', 'subsuite': 'gpu', 'test_paths': None, 'e10s': False},
        'task_regex': ['mochitest-gpu($|.*(-1|[^0-9])$)',
                       'test-verify($|.*(-1|[^0-9])$)'],
    },
    'mochitest-devtools-chrome': {
        'aliases': ('dt', 'devtools'),
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'browser-chrome', 'subsuite': 'devtools', 'test_paths': None},
        'task_regex': ['mochitest-devtools-chrome($|.*(-1|[^0-9])$)',
                       'test-verify($|.*(-1|[^0-9])$)'],
    },
    'mochitest-media': {
        'aliases': ('mpm', 'plain-media'),
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'plain', 'subsuite': 'media', 'test_paths': None},
        'task_regex': ['mochitest-media($|.*(-1|[^0-9])$)',
                       'test-verify($|.*(-1|[^0-9])$)'],
    },
    'mochitest-plain': {
        'aliases': ('mp', 'plain',),
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'plain', 'test_paths': None},
        'task_regex': ['mochitest(?!-a11y|-browser|-chrome|-devtools|-gpu|-harness|-media|-screen|-webgl)($|.*(-1|[^0-9])$)',  # noqa
                       'test-verify($|.*(-1|[^0-9])$)'],
    },
    'mochitest-plain-gpu': {
        'aliases': ('gpu',),
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'plain', 'subsuite': 'gpu', 'test_paths': None},
        'task_regex': ['mochitest-gpu($|.*(-1|[^0-9])$)',
                       'test-verify($|.*(-1|[^0-9])$)'],
    },
    'mochitest-remote': {
        'aliases': ('remote',),
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
    'puppeteer': {
        'aliases': ('remote/test/puppeteer',),
        'mach_command': 'puppeteer-test',
        'kwargs': {},
    },
    'python': {
        'mach_command': 'python-test',
        'kwargs': {'tests': None},
    },
    'telemetry-tests-client': {
        'aliases': ('ttc',),
        'mach_command': 'telemetry-tests-client',
        'kwargs': {},
        'task_regex': ['telemetry-tests-client($|.*(-1|[^0-9])$)'],
    },
    'reftest': {
        'aliases': ('rr',),
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
        'kwargs': {'include': []},
        'task_regex': ['web-platform-tests($|.*(-1|[^0-9])$)',
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
        'task_regex': ['web-platform-tests-reftests($|.*(-1|[^0-9])$)',
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
        'mach_command': 'xpcshell-test',
        'kwargs': {'test_file': 'all'},
        'task_regex': ['xpcshell($|.*(-1|[^0-9])$)',
                       'test-verify($|.*(-1|[^0-9])$)'],
    },
}

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
    ('browser-chrome', 'gpu'): 'mochitest-browser-chrome-gpu',
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
    ('web-platform-tests', 'testharness'): 'web-platform-tests-testharness',
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

    def __init__(self, all_tests, srcdir, test_defaults=None):
        self._tests_by_path = OrderedDefaultDict(list)
        self._tests_by_flavor = defaultdict(set)
        self._test_dirs = set()
        self._objdir = os.path.abspath(os.path.join(all_tests, os.pardir))
        self._wpt_loaded = False
        self._srcdir = srcdir

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
                        # The (ancestor manifest, included manifest) tuple
                        # contains the defaults of the included manifest, so
                        # use it instead of [metadata['manifest']].
                        defaults_manifests[0] = (ancestor_manifest, metadata['manifest'])
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

        if flavor in (None, 'puppeteer') and any(self.is_puppeteer_path(p) for p in paths):
            self.add_puppeteer_manifest_data()
        if flavor in (None, 'web-platform-tests') and any(self.is_wpt_path(p) for p in paths):
            self.add_wpt_manifest_data()

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

    def is_puppeteer_path(self, path):
        return mozpath.match(path, "remote/test/puppeteer/test/**")

    def add_puppeteer_manifest_data(self):
        test_path = os.path.join(self._srcdir, "remote", "test", "puppeteer", "test")
        for root, dirs, paths in os.walk(test_path):
            for filename in fnmatch.filter(paths, "*.spec.js"):
                path = os.path.join(root, filename)
                self._tests_by_path[path].append({
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

        wpt_path = os.path.join(self._srcdir, "testing", "web-platform")
        sys.path = [wpt_path] + sys.path

        import manifestupdate
        # Set up a logger that will drop all the output
        import logging
        logger = logging.getLogger("manifestupdate")
        logger.propogate = False

        manifests = manifestupdate.run(self._srcdir, self._objdir, rebuild=False, download=True,
                                       config_path=None, rewrite_config=True, update=True,
                                       logger=logger)
        if not manifests:
            print("Loading wpt manifest failed")
            return

        for manifest, data in manifests.iteritems():
            tests_root = data["tests_path"]
            for test_type, path, tests in manifest:
                full_path = os.path.join(tests_root, path)
                src_path = os.path.relpath(full_path, self._srcdir)
                if test_type not in ["testharness", "reftest", "wdspec"]:
                    continue
                for test in tests:
                    self._tests_by_path[src_path].append({
                        "path": full_path,
                        "flavor": "web-platform-tests",
                        "here": os.path.dirname(path),
                        "manifest": data["manifest_path"],
                        "name": test.id,
                        "file_relpath": path,
                        "head": "",
                        "support-files": "",
                        "subsuite": test_type,
                        "dir_relpath": os.path.dirname(src_path),
                        "srcdir_relpath": src_path,
                        })

        self._wpt_loaded = True


class TestResolver(MozbuildObject):
    """Helper to resolve tests from the current environment to test files."""

    def __init__(self, *args, **kwargs):
        MozbuildObject.__init__(self, *args, **kwargs)

        # If installing tests is going to result in re-generating the build
        # backend, we need to do this here, so that the updated contents of
        # all-tests.pkl make it to the set of tests to run.
        if self.backend_out_of_date(mozpath.join(self.topobjdir,
                                                 'backend.TestManifestBackend'
                                                 )):
            print("Test configuration changed. Regenerating backend.")
            from mozbuild.gen_test_backend import gen_test_backend
            gen_test_backend()

        self._tests = TestMetadata(os.path.join(self.topobjdir,
                                                'all-tests.pkl'),
                                   self.topsrcdir,
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
