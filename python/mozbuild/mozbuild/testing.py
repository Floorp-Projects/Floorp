# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import json
import os

import mozpack.path as mozpath

from .base import MozbuildObject
from .util import OrderedDefaultDict
from collections import defaultdict


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

    def __init__(self, filename=None):
        self._tests_by_path = OrderedDefaultDict(list)
        self._tests_by_flavor = defaultdict(set)
        self._test_dirs = set()

        if filename:
            with open(filename, 'rt') as fh:
                d = json.load(fh)

                for path, tests in d.items():
                    for metadata in tests:
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
                   if (flavor == 'devtools' and test.get('flavor') != 'browser-chrome') or \
                      (flavor != 'devtools' and test.get('flavor') != flavor):
                    continue

                if subsuite and test.get('subsuite') != subsuite:
                    continue

                if tags and not (tags & set(test.get('tags', '').split())):
                    continue

                if under_path \
                    and not test['file_relpath'].startswith(under_path):
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

        self._tests = TestMetadata(filename=os.path.join(self.topobjdir,
            'all-tests.json'))
        self._test_rewrites = {
            'a11y': os.path.join(self.topobjdir, '_tests', 'testing',
                'mochitest', 'a11y'),
            'browser-chrome': os.path.join(self.topobjdir, '_tests', 'testing',
                'mochitest', 'browser'),
            'jetpack-package': os.path.join(self.topobjdir, '_tests', 'testing',
                'mochitest', 'jetpack-package'),
            'jetpack-addon': os.path.join(self.topobjdir, '_tests', 'testing',
                'mochitest', 'jetpack-addon'),
            'chrome': os.path.join(self.topobjdir, '_tests', 'testing',
                'mochitest', 'chrome'),
            'mochitest': os.path.join(self.topobjdir, '_tests', 'testing',
                'mochitest', 'tests'),
            'webapprt-chrome': os.path.join(self.topobjdir, '_tests', 'testing',
                'mochitest', 'webapprtChrome'),
            'webapprt-content': os.path.join(self.topobjdir, '_tests', 'testing',
                'mochitest', 'webapprtContent'),
            'web-platform-tests': os.path.join(self.topobjdir, '_tests', 'testing',
                                               'web-platform'),
            'xpcshell': os.path.join(self.topobjdir, '_tests', 'xpcshell'),
        }

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

            result = self._tests.resolve_tests(under_path=reldir,
                **kwargs)

        else:
            result = self._tests.resolve_tests(**kwargs)

        for test in result:
            rewrite_base = self._test_rewrites.get(test['flavor'], None)

            if rewrite_base:
                yield rewrite_test_base(test, rewrite_base,
                    honor_install_to_subdir=True)
            else:
                yield test

# These definitions provide a single source of truth for modules attempting
# to get a view of all tests for a build. Used by the emitter to figure out
# how to read/install manifests and by test dependency annotations in Files()
# entries to enumerate test flavors.

# While there are multiple test manifests, the behavior is very similar
# across them. We enforce this by having common handling of all
# manifests and outputting a single class type with the differences
# described inside the instance.
#
# Keys are variable prefixes and values are tuples describing how these
# manifests should be handled:
#
#    (flavor, install_prefix, package_tests)
#
# flavor identifies the flavor of this test.
# install_prefix is the path prefix of where to install the files in
#     the tests directory.
# package_tests indicates whether to package test files into the test
#     package; suites that compile the test files should not install
#     them into the test package.
#
TEST_MANIFESTS = dict(
    A11Y=('a11y', 'testing/mochitest', 'a11y', True),
    BROWSER_CHROME=('browser-chrome', 'testing/mochitest', 'browser', True),
    ANDROID_INSTRUMENTATION=('instrumentation', 'instrumentation', '.', False),
    JETPACK_PACKAGE=('jetpack-package', 'testing/mochitest', 'jetpack-package', True),
    JETPACK_ADDON=('jetpack-addon', 'testing/mochitest', 'jetpack-addon', False),
    METRO_CHROME=('metro-chrome', 'testing/mochitest', 'metro', True),
    MOCHITEST=('mochitest', 'testing/mochitest', 'tests', True),
    MOCHITEST_CHROME=('chrome', 'testing/mochitest', 'chrome', True),
    MOCHITEST_WEBAPPRT_CONTENT=('webapprt-content', 'testing/mochitest', 'webapprtContent', True),
    MOCHITEST_WEBAPPRT_CHROME=('webapprt-chrome', 'testing/mochitest', 'webapprtChrome', True),
    WEBRTC_SIGNALLING_TEST=('steeplechase', 'steeplechase', '.', True),
    XPCSHELL_TESTS=('xpcshell', 'xpcshell', '.', True),
)

# Reftests have their own manifest format and are processed separately.
REFTEST_FLAVORS = ('crashtest', 'reftest')

# Web platform tests have their own manifest format and are processed separately.
WEB_PATFORM_TESTS_FLAVORS = ('web-platform-tests',)

def all_test_flavors():
    return ([v[0] for v in TEST_MANIFESTS.values()] +
            list(REFTEST_FLAVORS) +
            list(WEB_PATFORM_TESTS_FLAVORS))
