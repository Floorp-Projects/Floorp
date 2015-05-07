# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

from argparse import Namespace
import logging
import mozpack.path as mozpath
import os
import shutil
import sys
import warnings
import which

from mozbuild.base import (
    MachCommandBase,
    MachCommandConditions as conditions,
    MozbuildObject,
)

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

here = os.path.abspath(os.path.dirname(__file__))


ADB_NOT_FOUND = '''
The %s command requires the adb binary to be on your path.

If you have a B2G build, this can be found in
'%s/out/host/<platform>/bin'.
'''.lstrip()

GAIA_PROFILE_NOT_FOUND = '''
The %s command requires a non-debug gaia profile. Either pass in --profile,
or set the GAIA_PROFILE environment variable.

If you do not have a non-debug gaia profile, you can build one:
    $ git clone https://github.com/mozilla-b2g/gaia
    $ cd gaia
    $ make

The profile should be generated in a directory called 'profile'.
'''.lstrip()

GAIA_PROFILE_IS_DEBUG = '''
The %s command requires a non-debug gaia profile. The specified profile,
%s, is a debug profile.

If you do not have a non-debug gaia profile, you can build one:
    $ git clone https://github.com/mozilla-b2g/gaia
    $ cd gaia
    $ make

The profile should be generated in a directory called 'profile'.
'''.lstrip()

ENG_BUILD_REQUIRED = '''
The %s command requires an engineering build. It may be the case that
VARIANT=user or PRODUCTION=1 were set. Try re-building with VARIANT=eng:

    $ VARIANT=eng ./build.sh

There should be an app called 'test-container.gaiamobile.org' located in
%s.
'''.lstrip()

# Maps test flavors to mochitest suite type.
FLAVORS = {
    'mochitest': 'plain',
    'chrome': 'chrome',
    'browser-chrome': 'browser',
    'jetpack-package': 'jetpack-package',
    'jetpack-addon': 'jetpack-addon',
    'a11y': 'a11y',
    'webapprt-chrome': 'webapprt-chrome',
}


class MochitestRunner(MozbuildObject):

    """Easily run mochitests.

    This currently contains just the basics for running mochitests. We may want
    to hook up result parsing, etc.
    """

    def get_webapp_runtime_path(self):
        import mozinfo
        app_name = 'webapprt-stub' + mozinfo.info.get('bin_suffix', '')
        app_path = os.path.join(self.distdir, 'bin', app_name)
        if sys.platform.startswith('darwin'):
            # On Mac, we copy the stub from the dist dir to the test app bundle,
            # since we have to run it from a bundle for its windows to appear.
            # Ideally, the build system would do this for us, and we should find
            # a way for it to do that.
            mac_dir_name = os.path.join(
                self.mochitest_dir,
                'webapprtChrome',
                'webapprt',
                'test',
                'chrome',
                'TestApp.app',
                'Contents',
                'MacOS')
            mac_app_name = 'webapprt' + mozinfo.info.get('bin_suffix', '')
            mac_app_path = os.path.join(mac_dir_name, mac_app_name)
            shutil.copy(app_path, mac_app_path)
            return mac_app_path
        return app_path

    # On Mac, the app invoked by runtests.py is in a different app bundle
    # (as determined by get_webapp_runtime_path above), but the XRE path should
    # still point to the browser's app bundle, so we set it here explicitly.
    def get_webapp_runtime_xre_path(self):
        if sys.platform.startswith('darwin'):
            xre_path = os.path.join(
                self.distdir,
                self.substs['MOZ_MACBUNDLE_NAME'],
                'Contents',
                'Resources')
        else:
            xre_path = os.path.join(self.distdir, 'bin')
        return xre_path

    def __init__(self, *args, **kwargs):
        MozbuildObject.__init__(self, *args, **kwargs)

        # TODO Bug 794506 remove once mach integrates with virtualenv.
        build_path = os.path.join(self.topobjdir, 'build')
        if build_path not in sys.path:
            sys.path.append(build_path)

        self.tests_dir = os.path.join(self.topobjdir, '_tests')
        self.mochitest_dir = os.path.join(
            self.tests_dir,
            'testing',
            'mochitest')
        self.bin_dir = os.path.join(self.topobjdir, 'dist', 'bin')

    def run_b2g_test(self, test_paths=None, **kwargs):
        """Runs a b2g mochitest.

        test_paths is an enumerable of paths to tests. It can be a relative path
        from the top source directory, an absolute filename, or a directory
        containing test files.
        """
        # Need to call relpath before os.chdir() below.
        test_path = ''
        if test_paths:
            if len(test_paths) > 1:
                print('Warning: Only the first test path will be used.')
            test_path = self._wrap_path_argument(test_paths[0]).relpath()

        # TODO without os.chdir, chained imports fail below
        os.chdir(self.mochitest_dir)

        # The imp module can spew warnings if the modules below have
        # already been imported, ignore them.
        with warnings.catch_warnings():
            warnings.simplefilter('ignore')

            import imp
            path = os.path.join(self.mochitest_dir, 'runtestsb2g.py')
            with open(path, 'r') as fh:
                imp.load_module('mochitest', fh, path,
                                ('.py', 'r', imp.PY_SOURCE))

            import mochitest

        options = Namespace(**kwargs)

        if test_path:
            if options.chrome:
                test_root_file = mozpath.join(
                    self.mochitest_dir,
                    'chrome',
                    test_path)
            else:
                test_root_file = mozpath.join(
                    self.mochitest_dir,
                    'tests',
                    test_path)
            if not os.path.exists(test_root_file):
                print(
                    'Specified test path does not exist: %s' %
                    test_root_file)
                return 1
            options.testPath = test_path

        if options.desktop:
            return mochitest.run_desktop_mochitests(options)

        try:
            which.which('adb')
        except which.WhichError:
            # TODO Find adb automatically if it isn't on the path
            print(ADB_NOT_FOUND % ('mochitest-remote', options.b2gPath))
            return 1

        return mochitest.run_remote_mochitests(options)

    def run_desktop_test(self, context, suite=None, test_paths=None, **kwargs):
        """Runs a mochitest.

        suite is the type of mochitest to run. It can be one of ('plain',
        'chrome', 'browser', 'metro', 'a11y', 'jetpack-package', 'jetpack-addon').

        test_paths are path to tests. They can be a relative path from the
        top source directory, an absolute filename, or a directory containing
        test files.
        """
        # Make absolute paths relative before calling os.chdir() below.
        if test_paths:
            test_paths = [self._wrap_path_argument(
                p).relpath() if os.path.isabs(p) else p for p in test_paths]

        # runtests.py is ambiguous, so we load the file/module manually.
        if 'mochitest' not in sys.modules:
            import imp
            path = os.path.join(self.mochitest_dir, 'runtests.py')
            with open(path, 'r') as fh:
                imp.load_module('mochitest', fh, path,
                                ('.py', 'r', imp.PY_SOURCE))

        import mochitest
        from manifestparser import TestManifest
        from mozbuild.testing import TestResolver

        # This is required to make other components happy. Sad, isn't it?
        os.chdir(self.topobjdir)

        # Automation installs its own stream handler to stdout. Since we want
        # all logging to go through us, we just remove their handler.
        remove_handlers = [l for l in logging.getLogger().handlers
                           if isinstance(l, logging.StreamHandler)]
        for handler in remove_handlers:
            logging.getLogger().removeHandler(handler)

        options = Namespace(**kwargs)

        flavor = suite

        if suite == 'plain':
            # Don't need additional options for plain.
            flavor = 'mochitest'
        elif suite == 'chrome':
            options.chrome = True
        elif suite == 'browser':
            options.browserChrome = True
            flavor = 'browser-chrome'
        elif suite == 'devtools':
            options.browserChrome = True
            options.subsuite = 'devtools'
        elif suite == 'jetpack-package':
            options.jetpackPackage = True
        elif suite == 'jetpack-addon':
            options.jetpackAddon = True
        elif suite == 'metro':
            options.immersiveMode = True
            options.browserChrome = True
        elif suite == 'a11y':
            options.a11y = True
        elif suite == 'webapprt-content':
            options.webapprtContent = True
            if not options.app or options.app == self.get_binary_path():
                options.app = self.get_webapp_runtime_path()
            options.xrePath = self.get_webapp_runtime_xre_path()
        elif suite == 'webapprt-chrome':
            options.webapprtChrome = True
            options.browserArgs.append("-test-mode")
            if not options.app or options.app == self.get_binary_path():
                options.app = self.get_webapp_runtime_path()
            options.xrePath = self.get_webapp_runtime_xre_path()
        else:
            raise Exception('None or unrecognized mochitest suite type.')

        if test_paths:
            resolver = self._spawn(TestResolver)

            tests = list(
                resolver.resolve_tests(
                    paths=test_paths,
                    flavor=flavor))

            if not tests:
                print('No tests could be found in the path specified. Please '
                      'specify a path that is a test file or is a directory '
                      'containing tests.')
                return 1

            manifest = TestManifest()
            manifest.tests.extend(tests)

            # XXX why is this such a special case?
            if len(tests) == 1 and options.closeWhenDone and suite == 'plain':
                options.closeWhenDone = False

            options.manifestFile = manifest

        # We need this to enable colorization of output.
        self.log_manager.enable_unstructured()
        result = mochitest.run_test_harness(options)
        self.log_manager.disable_unstructured()
        return result

    def run_android_test(self, test_path, **kwargs):
        self.tests_dir = os.path.join(self.topobjdir, '_tests')
        self.mochitest_dir = os.path.join(self.tests_dir, 'testing', 'mochitest')
        import imp
        path = os.path.join(self.mochitest_dir, 'runtestsremote.py')
        with open(path, 'r') as fh:
            imp.load_module('runtestsremote', fh, path,
                            ('.py', 'r', imp.PY_SOURCE))
        import runtestsremote

        options = Namespace(**kwargs)
        if test_path:
            options.testPath = test_path

        sys.exit(runtestsremote.run_test_harness(options))


# parser

def TestPathArg(func):
    test_paths = CommandArgument('test_paths', nargs='*', metavar='TEST', default=None,
        help='Test to run. Can be a single test file or a directory of tests to '
             '(run recursively). If omitted, the entire suite is run.')
    return test_paths(func)


def setup_argument_parser():
    build_obj = MozbuildObject.from_environment(cwd=here)

    build_path = os.path.join(build_obj.topobjdir, 'build')
    if build_path not in sys.path:
        sys.path.append(build_path)

    mochitest_dir = os.path.join(build_obj.topobjdir, '_tests', 'testing', 'mochitest')

    with warnings.catch_warnings():
        warnings.simplefilter('ignore')

        import imp
        path = os.path.join(build_obj.topobjdir, mochitest_dir, 'runtests.py')
        with open(path, 'r') as fh:
            imp.load_module('mochitest', fh, path,
                            ('.py', 'r', imp.PY_SOURCE))

        from mochitest_options import MochitestArgumentParser

    return MochitestArgumentParser()


# condition filters

def is_platform_in(*platforms):
    def is_platform_supported(cls):
        for p in platforms:
            c = getattr(conditions, 'is_{}'.format(p), None)
            if c and c(cls):
                return True
        return False

    is_platform_supported.__doc__ = 'Must have a {} build.'.format(
        ' or '.join(platforms))
    return is_platform_supported


def verify_host_bin():
    # validate MOZ_HOST_BIN environment variables for Android tests
    MOZ_HOST_BIN = os.environ.get('MOZ_HOST_BIN')
    if not MOZ_HOST_BIN:
        print('environment variable MOZ_HOST_BIN must be set to a directory containing host xpcshell')
        return 1
    elif not os.path.isdir(MOZ_HOST_BIN):
        print('$MOZ_HOST_BIN does not specify a directory')
        return 1
    elif not os.path.isfile(os.path.join(MOZ_HOST_BIN, 'xpcshell')):
        print('$MOZ_HOST_BIN/xpcshell does not exist')
        return 1
    return 0


@CommandProvider
class MachCommands(MachCommandBase):

    def __init__(self, context):
        MachCommandBase.__init__(self, context)

        for attr in ('device_name', 'target_out'):
            setattr(self, attr, getattr(context, attr, None))

    @Command(
        'mochitest-plain',
        category='testing',
        conditions=[is_platform_in('firefox', 'mulet', 'b2g', 'b2g_desktop', 'android')],
        description='Run a plain mochitest (integration test, plain web page).',
        parser=setup_argument_parser)
    @TestPathArg
    def run_mochitest_plain(self, test_paths, **kwargs):
        if is_platform_in('firefox', 'mulet')(self):
            return self.run_mochitest(test_paths, 'plain', **kwargs)
        elif conditions.is_emulator(self):
            return self.run_mochitest_remote(test_paths, **kwargs)
        elif conditions.is_b2g_desktop(self):
            return self.run_mochitest_b2g_desktop(test_paths, **kwargs)
        elif conditions.is_android(self):
            return self.run_mochitest_android(test_paths, **kwargs)

    @Command(
        'mochitest-chrome',
        category='testing',
        conditions=[is_platform_in('firefox', 'emulator', 'android')],
        description='Run a chrome mochitest (integration test with some XUL).',
        parser=setup_argument_parser)
    @TestPathArg
    def run_mochitest_chrome(self, test_paths, **kwargs):
        kwargs['chrome'] = True
        if conditions.is_firefox(self):
            return self.run_mochitest(test_paths, 'chrome', **kwargs)
        elif conditions.is_b2g(self) and conditions.is_emulator(self):
            return self.run_mochitest_remote(test_paths, **kwargs)
        elif conditions.is_android(self):
            return self.run_mochitest_android(test_paths, **kwargs)

    @Command(
        'mochitest-browser',
        category='testing',
        conditions=[conditions.is_firefox],
        description='Run a mochitest with browser chrome (integration test with a standard browser).',
        parser=setup_argument_parser)
    @TestPathArg
    def run_mochitest_browser(self, test_paths, **kwargs):
        return self.run_mochitest(test_paths, 'browser', **kwargs)

    @Command(
        'mochitest-devtools',
        category='testing',
        conditions=[conditions.is_firefox],
        description='Run a devtools mochitest with browser chrome (integration test with a standard browser with the devtools frame).',
        parser=setup_argument_parser)
    @TestPathArg
    def run_mochitest_devtools(self, test_paths, **kwargs):
        return self.run_mochitest(test_paths, 'devtools', **kwargs)

    @Command('jetpack-package', category='testing',
             conditions=[conditions.is_firefox],
             description='Run a jetpack package test.',
             parser=setup_argument_parser)
    @TestPathArg
    def run_mochitest_jetpack_package(self, test_paths, **kwargs):
        return self.run_mochitest(test_paths, 'jetpack-package', **kwargs)

    @Command('jetpack-addon', category='testing',
             conditions=[conditions.is_firefox],
             description='Run a jetpack addon test.',
             parser=setup_argument_parser)
    @TestPathArg
    def run_mochitest_jetpack_addon(self, test_paths, **kwargs):
        return self.run_mochitest(test_paths, 'jetpack-addon', **kwargs)

    @Command(
        'mochitest-metro',
        category='testing',
        conditions=[conditions.is_firefox],
        description='Run a mochitest with metro browser chrome (tests for Windows touch interface).',
        parser=setup_argument_parser)
    @TestPathArg
    def run_mochitest_metro(self, test_paths, **kwargs):
        return self.run_mochitest(test_paths, 'metro', **kwargs)

    @Command('mochitest-a11y', category='testing',
             conditions=[conditions.is_firefox],
             description='Run an a11y mochitest (accessibility tests).',
             parser=setup_argument_parser)
    @TestPathArg
    def run_mochitest_a11y(self, test_paths, **kwargs):
        return self.run_mochitest(test_paths, 'a11y', **kwargs)

    @Command(
        'webapprt-test-chrome',
        category='testing',
        conditions=[conditions.is_firefox],
        description='Run a webapprt chrome mochitest (Web App Runtime with the browser chrome).',
        parser=setup_argument_parser)
    @TestPathArg
    def run_mochitest_webapprt_chrome(self, test_paths, **kwargs):
        return self.run_mochitest(test_paths, 'webapprt-chrome', **kwargs)

    @Command(
        'webapprt-test-content',
        category='testing',
        conditions=[conditions.is_firefox],
        description='Run a webapprt content mochitest (Content rendering of the Web App Runtime).',
        parser=setup_argument_parser)
    @TestPathArg
    def run_mochitest_webapprt_content(self, test_paths, **kwargs):
        return self.run_mochitest(test_paths, 'webapprt-content', **kwargs)

    @Command('mochitest', category='testing',
             conditions=[conditions.is_firefox],
             description='Run any flavor of mochitest (integration test).',
             parser=setup_argument_parser)
    @CommandArgument('-f', '--flavor', choices=FLAVORS.keys(),
                     help='Only run tests of this flavor.')
    @TestPathArg
    def run_mochitest_general(self, test_paths, flavor=None, test_objects=None,
                              **kwargs):
        self._preruntest()

        from mozbuild.testing import TestResolver

        if test_objects:
            tests = test_objects
        else:
            resolver = self._spawn(TestResolver)
            tests = list(resolver.resolve_tests(paths=test_paths,
                                                cwd=self._mach_context.cwd))

        # Our current approach is to group the tests by suite and then perform
        # an invocation for each suite. Ideally, this would be done
        # automatically inside of core mochitest code. But it wasn't designed
        # to do that.
        #
        # This does mean our output is less than ideal. When running tests from
        # multiple suites, we see redundant summary lines. Hopefully once we
        # have better machine readable output coming from mochitest land we can
        # aggregate that here and improve the output formatting.

        suites = {}
        for test in tests:
            # Filter out non-mochitests.
            if test['flavor'] not in FLAVORS:
                continue

            if flavor and test['flavor'] != flavor:
                continue

            suite = FLAVORS[test['flavor']]
            suites.setdefault(suite, []).append(test)

        mochitest = self._spawn(MochitestRunner)
        overall = None
        for suite, tests in sorted(suites.items()):
            result = mochitest.run_desktop_test(
                self._mach_context,
                test_paths=[
                    test['file_relpath'] for test in tests],
                suite=suite,
                **kwargs)
            if result:
                overall = result

        return overall

    def _preruntest(self):
        from mozbuild.controller.building import BuildDriver

        self._ensure_state_subdir_exists('.')

        driver = self._spawn(BuildDriver)
        driver.install_tests(remove=False)

    def run_mochitest(self, test_paths, flavor, **kwargs):
        self._preruntest()

        mochitest = self._spawn(MochitestRunner)

        return mochitest.run_desktop_test(
            self._mach_context,
            test_paths=test_paths,
            suite=flavor,
            **kwargs)

    def run_mochitest_remote(self, test_paths, **kwargs):
        if self.target_out:
            host_webapps_dir = os.path.join(
                self.target_out,
                'data',
                'local',
                'webapps')
            if not os.path.isdir(
                os.path.join(
                    host_webapps_dir,
                    'test-container.gaiamobile.org')):
                print(
                    ENG_BUILD_REQUIRED %
                    ('mochitest-remote', host_webapps_dir))
                return 1

        from mozbuild.controller.building import BuildDriver

        self._ensure_state_subdir_exists('.')

        driver = self._spawn(BuildDriver)
        driver.install_tests(remove=False)

        mochitest = self._spawn(MochitestRunner)
        return mochitest.run_b2g_test(
            test_paths=test_paths,
            **kwargs)

    def run_mochitest_b2g_desktop(self, test_paths, **kwargs):
        kwargs['profile'] = kwargs.get(
            'profile') or os.environ.get('GAIA_PROFILE')
        if not kwargs['profile'] or not os.path.isdir(kwargs['profile']):
            print(GAIA_PROFILE_NOT_FOUND % 'mochitest-b2g-desktop')
            return 1

        if os.path.isfile(os.path.join(kwargs['profile'], 'extensions',
                                       'httpd@gaiamobile.org')):
            print(GAIA_PROFILE_IS_DEBUG % ('mochitest-b2g-desktop',
                                           kwargs['profile']))
            return 1

        from mozbuild.controller.building import BuildDriver

        self._ensure_state_subdir_exists('.')

        driver = self._spawn(BuildDriver)
        driver.install_tests(remove=False)

        mochitest = self._spawn(MochitestRunner)
        return mochitest.run_b2g_test(test_paths=test_paths, **kwargs)

    def run_mochitest_android(self, test_paths, **kwargs):
        host_ret = verify_host_bin()
        if host_ret != 0:
            return host_ret

        test_path = None
        if test_paths:
            if len(test_paths) > 1:
                print('Warning: Only the first test path will be used.')
            test_path = self._wrap_path_argument(test_paths[0]).relpath()

        mochitest = self._spawn(MochitestRunner)
        return mochitest.run_android_test(test_path, **kwargs)


@CommandProvider
class AndroidCommands(MachCommandBase):

    @Command('robocop', category='testing',
             conditions=[conditions.is_android],
             description='Run a Robocop test.',
             parser=setup_argument_parser)
    @CommandArgument(
        'test_path',
        default=None,
        nargs='?',
        metavar='TEST',
        help='Test to run. Can be specified as a Robocop test name (like "testLoad"), '
        'or omitted. If omitted, the entire test suite is executed.')
    def run_robocop(self, test_path, **kwargs):
        host_ret = verify_host_bin()
        if host_ret != 0:
            return host_ret

        if not kwargs.get('robocopIni'):
            kwargs['robocopIni'] = os.path.join(self.topobjdir, '_tests', 'testing',
                                                'mochitest', 'robocop.ini')

        if not kwargs.get('robocopApk'):
            kwargs['robocopApk'] = os.path.join(self.topobjdir, 'build', 'mobile',
                                                'robocop', 'robocop-debug.apk')
        mochitest = self._spawn(MochitestRunner)
        return mochitest.run_android_test(test_path, **kwargs)
