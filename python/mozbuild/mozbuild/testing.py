# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import json
import os
import sys

import mozpack.path as mozpath

from mozpack.copier import FileCopier
from mozpack.manifests import InstallManifest

from .base import MozbuildObject
from .util import OrderedDefaultDict
from collections import defaultdict

import manifestparser

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
                test_data, manifest_support_files = json.load(fh)

                for path, tests in test_data.items():
                    for metadata in tests:
                        # Many tests inherit their "support-files" from a manifest
                        # level default, so we store these separately to save
                        # disk space, and propagate them to each test when
                        # de-serializing here.
                        manifest = metadata['manifest']
                        support_files = manifest_support_files.get(manifest)
                        if support_files and 'support-files' not in metadata:
                            metadata['support-files'] = support_files
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

        # If installing tests is going to result in re-generating the build
        # backend, we need to do this here, so that the updated contents of
        # all-tests.json make it to the set of tests to run.
        self._run_make(target='run-tests-deps', pass_thru=True,
                       print_directory=False)

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

    # marionette tests are run from the srcdir
    # TODO(ato): make packaging work as for other test suites
    MARIONETTE=('marionette', 'marionette', '.', False),
    MARIONETTE_LOOP=('marionette', 'marionette', '.', False),
    MARIONETTE_UNIT=('marionette', 'marionette', '.', False),
    MARIONETTE_UPDATE=('marionette', 'marionette', '.', False),
    MARIONETTE_WEBAPI=('marionette', 'marionette', '.', False),

    METRO_CHROME=('metro-chrome', 'testing/mochitest', 'metro', True),
    MOCHITEST=('mochitest', 'testing/mochitest', 'tests', True),
    MOCHITEST_CHROME=('chrome', 'testing/mochitest', 'chrome', True),
    WEBRTC_SIGNALLING_TEST=('steeplechase', 'steeplechase', '.', True),
    XPCSHELL_TESTS=('xpcshell', 'xpcshell', '.', True),
)

# Reftests have their own manifest format and are processed separately.
REFTEST_FLAVORS = ('crashtest', 'reftest')

# Web platform tests have their own manifest format and are processed separately.
WEB_PLATFORM_TESTS_FLAVORS = ('web-platform-tests',)

def all_test_flavors():
    return ([v[0] for v in TEST_MANIFESTS.values()] +
            list(REFTEST_FLAVORS) +
            list(WEB_PLATFORM_TESTS_FLAVORS) +
            ['python'])

class TestInstallInfo(object):
    def __init__(self):
        self.seen = set()
        self.pattern_installs = []
        self.installs = []
        self.external_installs = set()
        self.deferred_installs = set()

    def __ior__(self, other):
        self.pattern_installs.extend(other.pattern_installs)
        self.installs.extend(other.installs)
        self.external_installs |= other.external_installs
        self.deferred_installs |= other.deferred_installs
        return self

class SupportFilesConverter(object):
    """Processes a "support-files" entry from a test object, either from
    a parsed object from a test manifests or its representation in
    moz.build and returns the installs to perform for this test object.

    Processing the same support files multiple times will not have any further
    effect, and the structure of the parsed objects from manifests will have a
    lot of repeated entries, so this class takes care of memoizing.
    """
    def __init__(self):
        self._fields = (('head', set()),
                        ('tail', set()),
                        ('support-files', set()),
                        ('generated-files', set()))

    def convert_support_files(self, test, install_root, manifest_dir, out_dir):
        # Arguments:
        #  test - The test object to process.
        #  install_root - The directory under $objdir/_tests that will contain
        #                 the tests for this harness (examples are "testing/mochitest",
        #                 "xpcshell").
        #  manifest_dir - Absoulute path to the (srcdir) directory containing the
        #                 manifest that included this test
        #  out_dir - The path relative to $objdir/_tests used as the destination for the
        #            test, based on the relative path to the manifest in the srcdir,
        #            the install_root, and 'install-to-subdir', if present in the manifest.
        info = TestInstallInfo()
        for field, seen in self._fields:
            value = test.get(field, '')
            for pattern in value.split():

                # We track uniqueness locally (per test) where duplicates are forbidden,
                # and globally, where they are permitted. If a support file appears multiple
                # times for a single test, there are unnecessary entries in the manifest. But
                # many entries will be shared across tests that share defaults.
                # We need to memoize on the basis of both the path and the output
                # directory for the benefit of tests specifying 'install-to-subdir'.
                key = field, pattern, out_dir
                if key in info.seen:
                    raise ValueError("%s appears multiple times in a test manifest under a %s field,"
                                     " please omit the duplicate entry." % (pattern, field))
                info.seen.add(key)
                if key in seen:
                    continue
                seen.add(key)

                if field == 'generated-files':
                    info.external_installs.add(mozpath.normpath(mozpath.join(out_dir, pattern)))
                # '!' indicates our syntax for inter-directory support file
                # dependencies. These receive special handling in the backend.
                elif pattern[0] == '!':
                    info.deferred_installs.add(pattern)
                # We only support globbing on support-files because
                # the harness doesn't support * for head and tail.
                elif '*' in pattern and field == 'support-files':
                    info.pattern_installs.append((manifest_dir, pattern, out_dir))
                # "absolute" paths identify files that are to be
                # placed in the install_root directory (no globs)
                elif pattern[0] == '/':
                    full = mozpath.normpath(mozpath.join(manifest_dir,
                                                         mozpath.basename(pattern)))
                    info.installs.append((full, mozpath.join(install_root, pattern[1:])))
                else:
                    full = mozpath.normpath(mozpath.join(manifest_dir, pattern))
                    dest_path = mozpath.join(out_dir, pattern)

                    # If the path resolves to a different directory
                    # tree, we take special behavior depending on the
                    # entry type.
                    if not full.startswith(manifest_dir):
                        # If it's a support file, we install the file
                        # into the current destination directory.
                        # This implementation makes installing things
                        # with custom prefixes impossible. If this is
                        # needed, we can add support for that via a
                        # special syntax later.
                        if field == 'support-files':
                            dest_path = mozpath.join(out_dir,
                                                     os.path.basename(pattern))
                        # If it's not a support file, we ignore it.
                        # This preserves old behavior so things like
                        # head files doesn't get installed multiple
                        # times.
                        else:
                            continue
                    info.installs.append((full, mozpath.normpath(dest_path)))
        return info

def _resolve_installs(paths, topobjdir, manifest):
    """Using the given paths as keys, find any unresolved installs noted
    by the build backend corresponding to those keys, and add them
    to the given manifest.
    """
    filename = os.path.join(topobjdir, 'test-installs.json')
    with open(filename, 'r') as fh:
        resolved_installs = json.load(fh)

    for path in paths:
        path = path[2:]
        if path not in resolved_installs:
            raise Exception('A cross-directory support file path noted in a '
                'test manifest does not appear in any other manifest.\n "%s" '
                'must appear in another test manifest to specify an install '
                'for "!/%s".' % (path, path))
        installs = resolved_installs[path]
        for install_info in installs:
            try:
                if len(install_info) == 3:
                    manifest.add_pattern_symlink(*install_info)
                if len(install_info) == 2:
                    manifest.add_symlink(*install_info)
            except ValueError:
                # A duplicate value here is pretty likely when running
                # multiple directories at once, and harmless.
                pass

def install_test_files(topsrcdir, topobjdir, tests_root, test_objs):
    """Installs the requested test files to the objdir. This is invoked by
    test runners to avoid installing tens of thousands of test files when
    only a few tests need to be run.
    """
    flavor_info = {flavor: (root, prefix, install)
                   for (flavor, root, prefix, install) in TEST_MANIFESTS.values()}
    objdir_dest = mozpath.join(topobjdir, tests_root)

    converter = SupportFilesConverter()
    install_info = TestInstallInfo()
    for o in test_objs:
        flavor = o['flavor']
        if flavor not in flavor_info:
            # This is a test flavor that isn't installed by the build system.
            continue
        root, prefix, install = flavor_info[flavor]
        if not install:
            # This flavor isn't installed to the objdir.
            continue

        manifest_path = o['manifest']
        manifest_dir = mozpath.dirname(manifest_path)

        out_dir = mozpath.join(root, prefix, manifest_dir[len(topsrcdir) + 1:])
        file_relpath = o['file_relpath']
        source = mozpath.join(topsrcdir, file_relpath)
        dest = mozpath.join(root, prefix, file_relpath)
        if 'install-to-subdir' in o:
            out_dir = mozpath.join(out_dir, o['install-to-subdir'])
            manifest_relpath = mozpath.relpath(source, mozpath.dirname(manifest_path))
            dest = mozpath.join(out_dir, manifest_relpath)

        install_info.installs.append((source, dest))
        install_info |= converter.convert_support_files(o, root,
                                                        manifest_dir,
                                                        out_dir)

    manifest = InstallManifest()

    for source, dest in set(install_info.installs):
        if dest in install_info.external_installs:
            continue
        manifest.add_symlink(source, dest)
    for base, pattern, dest in install_info.pattern_installs:
        manifest.add_pattern_symlink(base, pattern, dest)

    _resolve_installs(install_info.deferred_installs, topobjdir, manifest)

    # Harness files are treated as a monolith and installed each time we run tests.
    # Fortunately there are not very many.
    manifest |= InstallManifest(mozpath.join(topobjdir,
                                             '_build_manifests',
                                             'install', tests_root))
    copier = FileCopier()
    manifest.populate_registry(copier)
    copier.copy(objdir_dest,
                remove_unaccounted=False)


# Convenience methods for test manifest reading.
def read_manifestparser_manifest(context, manifest_path):
    path = mozpath.normpath(mozpath.join(context.srcdir, manifest_path))
    return manifestparser.TestManifest(manifests=[path], strict=True,
                                       rootdir=context.config.topsrcdir,
                                       finder=context._finder)

def read_reftest_manifest(context, manifest_path):
    import reftest
    path = mozpath.normpath(mozpath.join(context.srcdir, manifest_path))
    manifest = reftest.ReftestManifest(finder=context._finder)
    manifest.load(path)
    return manifest

def read_wpt_manifest(context, paths):
    manifest_path, tests_root = paths
    full_path = mozpath.normpath(mozpath.join(context.srcdir, manifest_path))
    old_path = sys.path[:]
    try:
        # Setup sys.path to include all the dependencies required to import
        # the web-platform-tests manifest parser. web-platform-tests provides
        # a the localpaths.py to do the path manipulation, which we load,
        # providing the __file__ variable so it can resolve the relative
        # paths correctly.
        paths_file = os.path.join(context.config.topsrcdir, "testing",
                                  "web-platform", "tests", "tools", "localpaths.py")
        _globals = {"__file__": paths_file}
        execfile(paths_file, _globals)
        import manifest as wptmanifest
    finally:
        sys.path = old_path
        f = context._finder.get(full_path)
        return wptmanifest.manifest.load(tests_root, f)
