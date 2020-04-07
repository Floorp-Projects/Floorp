# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import sys

try:
    import cPickle as pickle
except ImportError:
    import pickle


import manifestparser
import mozpack.path as mozpath
from mozpack.copier import FileCopier
from mozpack.manifests import InstallManifest

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
#    (flavor, install_root, install_subdir, package_tests)
#
# flavor identifies the flavor of this test.
# install_root is the path prefix to install the files starting from the root
#     directory and not as specified by the manifest location. (bug 972168)
# install_subdir is the path of where to install the files in
#     the tests directory.
# package_tests indicates whether to package test files into the test
#     package; suites that compile the test files should not install
#     them into the test package.
#
TEST_MANIFESTS = dict(
    A11Y=('a11y', 'testing/mochitest', 'a11y', True),
    BROWSER_CHROME=('browser-chrome', 'testing/mochitest', 'browser', True),
    ANDROID_INSTRUMENTATION=('instrumentation', 'instrumentation', '.', False),
    FIREFOX_UI_FUNCTIONAL=('firefox-ui-functional', 'firefox-ui', '.', False),
    FIREFOX_UI_UPDATE=('firefox-ui-update', 'firefox-ui', '.', False),
    PYTHON_UNITTEST=('python', 'python', '.', False),
    CRAMTEST=('cram', 'cram', '.', False),
    TELEMETRY_TESTS_CLIENT=(
        'telemetry-tests-client',
        'toolkit/components/telemetry/tests/marionette/', '.', False),

    # marionette tests are run from the srcdir
    # TODO(ato): make packaging work as for other test suites
    MARIONETTE=('marionette', 'marionette', '.', False),
    MARIONETTE_UNIT=('marionette', 'marionette', '.', False),
    MARIONETTE_WEBAPI=('marionette', 'marionette', '.', False),

    MOCHITEST=('mochitest', 'testing/mochitest', 'tests', True),
    MOCHITEST_CHROME=('chrome', 'testing/mochitest', 'chrome', True),
    WEBRTC_SIGNALLING_TEST=('steeplechase', 'steeplechase', '.', True),
    XPCSHELL_TESTS=('xpcshell', 'xpcshell', '.', True),
)

# reftests, wpt, and puppeteer all have their own manifest formats
# and are processed separately
REFTEST_FLAVORS = ('crashtest', 'reftest')
PUPPETEER_FLAVORS = ('puppeteer',)
WEB_PLATFORM_TESTS_FLAVORS = ('web-platform-tests',)


def all_test_flavors():
    return ([v[0] for v in TEST_MANIFESTS.values()] +
            list(REFTEST_FLAVORS) +
            list(PUPPETEER_FLAVORS) +
            list(WEB_PLATFORM_TESTS_FLAVORS))


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
        #            test, based on the relative path to the manifest in the srcdir and
        #            the install_root.
        info = TestInstallInfo()
        for field, seen in self._fields:
            value = test.get(field, '')
            for pattern in value.split():

                # We track uniqueness locally (per test) where duplicates are forbidden,
                # and globally, where they are permitted. If a support file appears multiple
                # times for a single test, there are unnecessary entries in the manifest. But
                # many entries will be shared across tests that share defaults.
                key = field, pattern, out_dir
                if key in info.seen:
                    raise ValueError(
                        "%s appears multiple times in a test manifest under a %s field,"
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
                # the harness doesn't support * for head.
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
                    info.installs.append((full, mozpath.normpath(dest_path)))
        return info


def _resolve_installs(paths, topobjdir, manifest):
    """Using the given paths as keys, find any unresolved installs noted
    by the build backend corresponding to those keys, and add them
    to the given manifest.
    """
    filename = os.path.join(topobjdir, 'test-installs.pkl')
    with open(filename, 'rb') as fh:
        resolved_installs = pickle.load(fh)

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
                    manifest.add_pattern_link(*install_info)
                if len(install_info) == 2:
                    manifest.add_link(*install_info)
            except ValueError:
                # A duplicate value here is pretty likely when running
                # multiple directories at once, and harmless.
                pass


def _make_install_manifest(topsrcdir, topobjdir, test_objs):

    flavor_info = {flavor: (root, prefix, install)
                   for (flavor, root, prefix, install) in TEST_MANIFESTS.values()}

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
        install_info.installs.append((source, dest))
        install_info |= converter.convert_support_files(o, root,
                                                        manifest_dir,
                                                        out_dir)

    manifest = InstallManifest()

    for source, dest in set(install_info.installs):
        if dest in install_info.external_installs:
            continue
        manifest.add_link(source, dest)
    for base, pattern, dest in install_info.pattern_installs:
        manifest.add_pattern_link(base, pattern, dest)

    _resolve_installs(install_info.deferred_installs, topobjdir, manifest)

    return manifest


def install_test_files(topsrcdir, topobjdir, tests_root, test_objs):
    """Installs the requested test files to the objdir. This is invoked by
    test runners to avoid installing tens of thousands of test files when
    only a few tests need to be run.
    """

    if test_objs:
        manifest = _make_install_manifest(topsrcdir, topobjdir, test_objs)
    else:
        # If we don't actually have a list of tests to install we install
        # test and support files wholesale.
        manifest = InstallManifest(mozpath.join(topobjdir, '_build_manifests',
                                                'install', '_test_files'))

    harness_files_manifest = mozpath.join(topobjdir, '_build_manifests',
                                          'install', tests_root)

    if os.path.isfile(harness_files_manifest):
        # If the backend has generated an install manifest for test harness
        # files they are treated as a monolith and installed each time we
        # run tests. Fortunately there are not very many.
        manifest |= InstallManifest(harness_files_manifest)

    copier = FileCopier()
    manifest.populate_registry(copier)
    copier.copy(mozpath.join(topobjdir, tests_root),
                remove_unaccounted=False)


# Convenience methods for test manifest reading.
def read_manifestparser_manifest(context, manifest_path):
    path = manifest_path.full_path
    return manifestparser.TestManifest(manifests=[path], strict=True,
                                       rootdir=context.config.topsrcdir,
                                       finder=context._finder,
                                       handle_defaults=False)


def read_reftest_manifest(context, manifest_path):
    import reftest
    path = manifest_path.full_path
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
        try:
            rv = wptmanifest.manifest.load(tests_root, f)
        except wptmanifest.manifest.ManifestVersionMismatch:
            # If we accidentially end up with a committed manifest that's the wrong
            # version, then return an empty manifest here just to not break the build
            rv = wptmanifest.manifest.Manifest()
        return rv
