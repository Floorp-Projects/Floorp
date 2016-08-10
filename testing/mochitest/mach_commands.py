# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

from argparse import Namespace
from collections import defaultdict
from itertools import chain
import logging
import os
import shutil
import sys
import warnings

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
import mozpack.path as mozpath

here = os.path.abspath(os.path.dirname(__file__))


ENG_BUILD_REQUIRED = '''
The mochitest command requires an engineering build. It may be the case that
VARIANT=user or PRODUCTION=1 were set. Try re-building with VARIANT=eng:

    $ VARIANT=eng ./build.sh

There should be an app called 'test-container.gaiamobile.org' located in
{}.
'''.lstrip()

SUPPORTED_TESTS_NOT_FOUND = '''
The mochitest command could not find any supported tests to run! The
following flavors and subsuites were found, but are either not supported on
{} builds, or were excluded on the command line:

{}

Double check the command line you used, and make sure you are running in
context of the proper build. To switch build contexts, either run |mach|
from the appropriate objdir, or export the correct mozconfig:

    $ export MOZCONFIG=path/to/mozconfig
'''.lstrip()

TESTS_NOT_FOUND = '''
The mochitest command could not find any mochitests under the following
test path(s):

{}

Please check spelling and make sure there are mochitests living there.
'''.lstrip()

ROBOCOP_TESTS_NOT_FOUND = '''
The robocop command could not find any tests under the following
test path(s):

{}

Please check spelling and make sure the named tests exist.
'''.lstrip()

NOW_RUNNING = '''
######
### Now running mochitest-{}.
######
'''


# Maps test flavors to data needed to run them
ALL_FLAVORS = {
    'mochitest': {
        'suite': 'plain',
        'aliases': ('plain', 'mochitest'),
        'enabled_apps': ('firefox', 'b2g', 'android', 'mulet'),
        'extra_args': {
            'flavor': 'plain',
        }
    },
    'chrome': {
        'suite': 'chrome',
        'aliases': ('chrome', 'mochitest-chrome'),
        'enabled_apps': ('firefox', 'mulet', 'b2g', 'android'),
        'extra_args': {
            'flavor': 'chrome',
        }
    },
    'browser-chrome': {
        'suite': 'browser',
        'aliases': ('browser', 'browser-chrome', 'mochitest-browser-chrome', 'bc'),
        'enabled_apps': ('firefox',),
        'extra_args': {
            'flavor': 'browser',
        }
    },
    'jetpack-package': {
        'suite': 'jetpack-package',
        'aliases': ('jetpack-package', 'mochitest-jetpack-package', 'jpp'),
        'enabled_apps': ('firefox',),
        'extra_args': {
            'flavor': 'jetpack-package',
        }
    },
    'jetpack-addon': {
        'suite': 'jetpack-addon',
        'aliases': ('jetpack-addon', 'mochitest-jetpack-addon', 'jpa'),
        'enabled_apps': ('firefox',),
        'extra_args': {
            'flavor': 'jetpack-addon',
        }
    },
    'a11y': {
        'suite': 'a11y',
        'aliases': ('a11y', 'mochitest-a11y', 'accessibility'),
        'enabled_apps': ('firefox',),
        'extra_args': {
            'flavor': 'a11y',
        }
    },
}

SUPPORTED_APPS = ['firefox', 'b2g', 'android', 'mulet']
SUPPORTED_FLAVORS = list(chain.from_iterable([f['aliases'] for f in ALL_FLAVORS.values()]))
CANONICAL_FLAVORS = sorted([f['aliases'][0] for f in ALL_FLAVORS.values()])

parser = None


class MochitestRunner(MozbuildObject):

    """Easily run mochitests.

    This currently contains just the basics for running mochitests. We may want
    to hook up result parsing, etc.
    """

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

    def resolve_tests(self, test_paths, test_objects=None, cwd=None):
        if test_objects:
            return test_objects

        from mozbuild.testing import TestResolver
        resolver = self._spawn(TestResolver)
        tests = list(resolver.resolve_tests(paths=test_paths, cwd=cwd))
        return tests

    def run_b2g_test(self, context, tests=None, suite='mochitest', **kwargs):
        """Runs a b2g mochitest."""
        if context.target_out:
            host_webapps_dir = os.path.join(context.target_out, 'data', 'local', 'webapps')
            if not os.path.isdir(os.path.join(
                    host_webapps_dir, 'test-container.gaiamobile.org')):
                print(ENG_BUILD_REQUIRED.format(host_webapps_dir))
                sys.exit(1)

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

        from manifestparser import TestManifest
        if tests:
            manifest = TestManifest()
            manifest.tests.extend(tests)
            options.manifestFile = manifest

        return mochitest.run_test_harness(parser, options)

    def run_desktop_test(self, context, tests=None, suite=None, **kwargs):
        """Runs a mochitest.

        suite is the type of mochitest to run. It can be one of ('plain',
        'chrome', 'browser', 'a11y', 'jetpack-package', 'jetpack-addon').
        """
        # runtests.py is ambiguous, so we load the file/module manually.
        if 'mochitest' not in sys.modules:
            import imp
            path = os.path.join(self.mochitest_dir, 'runtests.py')
            with open(path, 'r') as fh:
                imp.load_module('mochitest', fh, path,
                                ('.py', 'r', imp.PY_SOURCE))

        import mochitest

        # This is required to make other components happy. Sad, isn't it?
        os.chdir(self.topobjdir)

        # Automation installs its own stream handler to stdout. Since we want
        # all logging to go through us, we just remove their handler.
        remove_handlers = [l for l in logging.getLogger().handlers
                           if isinstance(l, logging.StreamHandler)]
        for handler in remove_handlers:
            logging.getLogger().removeHandler(handler)

        options = Namespace(**kwargs)

        from manifestparser import TestManifest
        if tests:
            manifest = TestManifest()
            manifest.tests.extend(tests)
            options.manifestFile = manifest

            # When developing mochitest-plain tests, it's often useful to be able to
            # refresh the page to pick up modifications. Therefore leave the browser
            # open if only running a single mochitest-plain test. This behaviour can
            # be overridden by passing in --keep-open=false.
            if len(tests) == 1 and options.keep_open is None and suite == 'plain':
                options.keep_open = True

        # We need this to enable colorization of output.
        self.log_manager.enable_unstructured()
        result = mochitest.run_test_harness(parser, options)
        self.log_manager.disable_unstructured()
        return result

    def run_android_test(self, context, tests, suite=None, **kwargs):
        host_ret = verify_host_bin()
        if host_ret != 0:
            return host_ret

        import imp
        path = os.path.join(self.mochitest_dir, 'runtestsremote.py')
        with open(path, 'r') as fh:
            imp.load_module('runtestsremote', fh, path,
                            ('.py', 'r', imp.PY_SOURCE))
        import runtestsremote

        options = Namespace(**kwargs)

        from manifestparser import TestManifest
        if tests:
            manifest = TestManifest()
            manifest.tests.extend(tests)
            options.manifestFile = manifest

        return runtestsremote.run_test_harness(parser, options)

    def run_robocop_test(self, context, tests, suite=None, **kwargs):
        host_ret = verify_host_bin()
        if host_ret != 0:
            return host_ret

        import imp
        path = os.path.join(self.mochitest_dir, 'runrobocop.py')
        with open(path, 'r') as fh:
            imp.load_module('runrobocop', fh, path,
                            ('.py', 'r', imp.PY_SOURCE))
        import runrobocop

        options = Namespace(**kwargs)

        from manifestparser import TestManifest
        if tests:
            manifest = TestManifest()
            manifest.tests.extend(tests)
            options.manifestFile = manifest

        return runrobocop.run_test_harness(parser, options)

# parser


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

    if conditions.is_android(build_obj):
        # On Android, check for a connected device (and offer to start an
        # emulator if appropriate) before running tests. This check must
        # be done in this admittedly awkward place because
        # MochitestArgumentParser initialization fails if no device is found.
        from mozrunner.devices.android_device import verify_android_device
        verify_android_device(build_obj, install=True, xre=True)

    global parser
    parser = MochitestArgumentParser()
    return parser


# condition filters

def is_buildapp_in(*apps):
    def is_buildapp_supported(cls):
        for a in apps:
            c = getattr(conditions, 'is_{}'.format(a), None)
            if c and c(cls):
                return True
        return False

    is_buildapp_supported.__doc__ = 'Must have a {} build.'.format(
        ' or '.join(apps))
    return is_buildapp_supported


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
    @Command('mochitest', category='testing',
             conditions=[is_buildapp_in(*SUPPORTED_APPS)],
             description='Run any flavor of mochitest (integration test).',
             parser=setup_argument_parser)
    @CommandArgument('-f', '--flavor',
                     metavar='{{{}}}'.format(', '.join(CANONICAL_FLAVORS)),
                     choices=SUPPORTED_FLAVORS,
                     help='Only run tests of this flavor.')
    def run_mochitest_general(self, flavor=None, test_objects=None, resolve_tests=True, **kwargs):
        buildapp = None
        for app in SUPPORTED_APPS:
            if is_buildapp_in(app)(self):
                buildapp = app
                break

        flavors = None
        if flavor:
            for fname, fobj in ALL_FLAVORS.iteritems():
                if flavor in fobj['aliases']:
                    if buildapp not in fobj['enabled_apps']:
                        continue
                    flavors = [fname]
                    break
        else:
            flavors = [f for f, v in ALL_FLAVORS.iteritems() if buildapp in v['enabled_apps']]

        from mozbuild.controller.building import BuildDriver
        self._ensure_state_subdir_exists('.')

        test_paths = kwargs['test_paths']
        kwargs['test_paths'] = []

        if test_paths and buildapp == 'b2g':
            # In B2G there is often a 'gecko' directory, though topsrcdir is actually
            # elsewhere. This little hack makes test paths like 'gecko/dom' work, even if
            # GECKO_PATH is set in the .userconfig
            gecko_path = mozpath.abspath(mozpath.join(kwargs['b2gPath'], 'gecko'))
            if gecko_path != self.topsrcdir:
                new_paths = []
                for tp in test_paths:
                    if mozpath.abspath(tp).startswith(gecko_path):
                        new_paths.append(mozpath.relpath(tp, gecko_path))
                    else:
                        new_paths.append(tp)
                test_paths = new_paths

        mochitest = self._spawn(MochitestRunner)
        tests = []
        if resolve_tests:
            tests = mochitest.resolve_tests(test_paths, test_objects, cwd=self._mach_context.cwd)

        driver = self._spawn(BuildDriver)
        driver.install_tests(tests)

        subsuite = kwargs.get('subsuite')
        if subsuite == 'default':
            kwargs['subsuite'] = None

        suites = defaultdict(list)
        unsupported = set()
        for test in tests:
            # Filter out non-mochitests and unsupported flavors.
            if test['flavor'] not in ALL_FLAVORS:
                continue

            key = (test['flavor'], test['subsuite'])
            if test['flavor'] not in flavors:
                unsupported.add(key)
                continue

            if subsuite == 'default':
                # "--subsuite default" means only run tests that don't have a subsuite
                if test['subsuite']:
                    unsupported.add(key)
                    continue
            elif subsuite and test['subsuite'] != subsuite:
                unsupported.add(key)
                continue

            suites[key].append(test)

        # This is a hack to introduce an option in mach to not send
        # filtered tests to the mochitest harness. Mochitest harness will read
        # the master manifest in that case.
        if not resolve_tests:
            for flavor in flavors:
                key = (flavor, kwargs.get('subsuite'))
                suites[key] = []

        if not suites:
            # Make it very clear why no tests were found
            if not unsupported:
                print(TESTS_NOT_FOUND.format('\n'.join(
                    sorted(list(test_paths or test_objects)))))
                return 1

            msg = []
            for f, s in unsupported:
                fobj = ALL_FLAVORS[f]
                apps = fobj['enabled_apps']
                name = fobj['aliases'][0]
                if s:
                    name = '{} --subsuite {}'.format(name, s)

                if buildapp not in apps:
                    reason = 'requires {}'.format(' or '.join(apps))
                else:
                    reason = 'excluded by the command line'
                msg.append('    mochitest -f {} ({})'.format(name, reason))
            print(SUPPORTED_TESTS_NOT_FOUND.format(
                buildapp, '\n'.join(sorted(msg))))
            return 1

        if buildapp in ('b2g',):
            run_mochitest = mochitest.run_b2g_test
        elif buildapp == 'android':
            from mozrunner.devices.android_device import grant_runtime_permissions
            grant_runtime_permissions(self)
            run_mochitest = mochitest.run_android_test
        else:
            run_mochitest = mochitest.run_desktop_test

        overall = None
        for (flavor, subsuite), tests in sorted(suites.items()):
            fobj = ALL_FLAVORS[flavor]
            msg = fobj['aliases'][0]
            if subsuite:
                msg = '{} with subsuite {}'.format(msg, subsuite)
            print(NOW_RUNNING.format(msg))

            harness_args = kwargs.copy()
            harness_args['subsuite'] = subsuite
            harness_args.update(fobj.get('extra_args', {}))

            result = run_mochitest(
                self._mach_context,
                tests=tests,
                suite=fobj['suite'],
                **harness_args)

            if result:
                overall = result

        # TODO consolidate summaries from all suites
        return overall


@CommandProvider
class RobocopCommands(MachCommandBase):

    @Command('robocop', category='testing',
             conditions=[conditions.is_android],
             description='Run a Robocop test.',
             parser=setup_argument_parser)
    @CommandArgument('--serve', default=False, action='store_true',
                     help='Run no tests but start the mochi.test web server '
                     'and launch Fennec with a test profile.')
    def run_robocop(self, serve=False, **kwargs):
        if serve:
            kwargs['autorun'] = False

        if not kwargs.get('robocopIni'):
            kwargs['robocopIni'] = os.path.join(self.topobjdir, '_tests', 'testing',
                                                'mochitest', 'robocop.ini')

        if not kwargs.get('robocopApk'):
            kwargs['robocopApk'] = os.path.join(self.topobjdir, 'mobile', 'android',
                                                'tests', 'browser', 'robocop',
                                                'robocop-debug.apk')

        from mozbuild.controller.building import BuildDriver
        self._ensure_state_subdir_exists('.')

        test_paths = kwargs['test_paths']
        kwargs['test_paths'] = []

        from mozbuild.testing import TestResolver
        resolver = self._spawn(TestResolver)
        tests = list(resolver.resolve_tests(paths=test_paths, cwd=self._mach_context.cwd,
                                            flavor='instrumentation', subsuite='robocop'))
        driver = self._spawn(BuildDriver)
        driver.install_tests(tests)

        if len(tests) < 1:
            print(ROBOCOP_TESTS_NOT_FOUND.format('\n'.join(
                sorted(list(test_paths)))))
            return 1

        from mozrunner.devices.android_device import grant_runtime_permissions
        grant_runtime_permissions(self)

        mochitest = self._spawn(MochitestRunner)
        return mochitest.run_robocop_test(self._mach_context, tests, 'robocop', **kwargs)


# NOTE python/mach/mach/commands/commandinfo.py references this function
#      by name. If this function is renamed or removed, that file should
#      be updated accordingly as well.
def REMOVED(cls):
    """Command no longer exists! Use |mach mochitest| instead.

    The |mach mochitest| command will automatically detect which flavors and
    subsuites exist in a given directory. If desired, flavors and subsuites
    can be restricted using `--flavor` and `--subsuite` respectively. E.g:

        $ ./mach mochitest dom/indexedDB

    will run all of the plain, chrome and browser-chrome mochitests in that
    directory. To only run the plain mochitests:

        $ ./mach mochitest -f plain dom/indexedDB
    """
    return False


@CommandProvider
class DeprecatedCommands(MachCommandBase):
    @Command('mochitest-plain', category='testing', conditions=[REMOVED])
    def mochitest_plain(self):
        pass

    @Command('mochitest-chrome', category='testing', conditions=[REMOVED])
    def mochitest_chrome(self):
        pass

    @Command('mochitest-browser', category='testing', conditions=[REMOVED])
    def mochitest_browser(self):
        pass

    @Command('mochitest-devtools', category='testing', conditions=[REMOVED])
    def mochitest_devtools(self):
        pass

    @Command('mochitest-a11y', category='testing', conditions=[REMOVED])
    def mochitest_a11y(self):
        pass

    @Command('jetpack-addon', category='testing', conditions=[REMOVED])
    def jetpack_addon(self):
        pass

    @Command('jetpack-package', category='testing', conditions=[REMOVED])
    def jetpack_package(self):
        pass
