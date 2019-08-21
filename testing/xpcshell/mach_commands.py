# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Integrates the xpcshell test runner with mach.

from __future__ import absolute_import, unicode_literals, print_function

import errno
import os
import sys

from mozlog import structured

from mozbuild.base import (
    MachCommandBase,
    MozbuildObject,
    MachCommandConditions as conditions,
)

from mach.decorators import (
    CommandProvider,
    Command,
)

from multiprocessing import cpu_count
from xpcshellcommandline import parser_desktop, parser_remote

here = os.path.abspath(os.path.dirname(__file__))

if sys.version_info[0] < 3:
    unicode_type = unicode
else:
    unicode_type = str


# This should probably be consolidated with similar classes in other test
# runners.
class InvalidTestPathError(Exception):
    """Exception raised when the test path is not valid."""


class XPCShellRunner(MozbuildObject):
    """Run xpcshell tests."""
    def run_suite(self, **kwargs):
        return self._run_xpcshell_harness(**kwargs)

    def run_test(self, **kwargs):
        """Runs an individual xpcshell test."""

        # TODO Bug 794506 remove once mach integrates with virtualenv.
        build_path = os.path.join(self.topobjdir, 'build')
        if build_path not in sys.path:
            sys.path.append(build_path)

        src_build_path = os.path.join(self.topsrcdir, 'mozilla', 'build')
        if os.path.isdir(src_build_path):
            sys.path.append(src_build_path)

        return self.run_suite(**kwargs)

    def _run_xpcshell_harness(self, **kwargs):
        # Obtain a reference to the xpcshell test runner.
        import runxpcshelltests

        log = kwargs.pop("log")

        xpcshell = runxpcshelltests.XPCShellTests(log=log)
        self.log_manager.enable_unstructured()

        tests_dir = os.path.join(self.topobjdir, '_tests', 'xpcshell')
        # We want output from the test to be written immediately if we are only
        # running a single test.
        single_test = (len(kwargs["testPaths"]) == 1 and
                       os.path.isfile(kwargs["testPaths"][0]) or
                       kwargs["manifest"] and
                       (len(kwargs["manifest"].test_paths()) == 1))

        if single_test:
            kwargs["verbose"] = True

        if kwargs["xpcshell"] is None:
            kwargs["xpcshell"] = self.get_binary_path('xpcshell')

        if kwargs["mozInfo"] is None:
            kwargs["mozInfo"] = os.path.join(self.topobjdir, 'mozinfo.json')

        if kwargs["symbolsPath"] is None:
            kwargs["symbolsPath"] = os.path.join(self.distdir, 'crashreporter-symbols')

        if kwargs["logfiles"] is None:
            kwargs["logfiles"] = False

        if kwargs["profileName"] is None:
            kwargs["profileName"] = "firefox"

        if kwargs["pluginsPath"] is None:
            kwargs['pluginsPath'] = os.path.join(self.distdir, 'plugins')

        if kwargs["testingModulesDir"] is None:
            kwargs["testingModulesDir"] = os.path.join(self.topobjdir, '_tests/modules')

        if kwargs["utility_path"] is None:
            kwargs['utility_path'] = self.bindir

        if kwargs["manifest"] is None:
            kwargs["manifest"] = os.path.join(tests_dir, "xpcshell.ini")

        if kwargs["failure_manifest"] is None:
            kwargs["failure_manifest"] = os.path.join(self.statedir, 'xpcshell.failures.ini')

        # Use the object directory for the temp directory to minimize the chance
        # of file scanning. The overhead from e.g. search indexers and anti-virus
        # scanners like Windows Defender can add tons of overhead to test execution.
        # We encourage people to disable these things in the object directory.
        temp_dir = os.path.join(self.topobjdir, 'temp')
        try:
            os.mkdir(temp_dir)
        except OSError as e:
            if e.errno != errno.EEXIST:
                raise
        kwargs['tempDir'] = temp_dir

        # Python through 2.7.2 has issues with unicode in some of the
        # arguments. Work around that.
        filtered_args = {}
        for k, v in kwargs.iteritems():
            if isinstance(v, unicode_type):
                v = v.encode('utf-8')

            if isinstance(k, unicode_type):
                k = k.encode('utf-8')

            filtered_args[k] = v

        result = xpcshell.runTests(filtered_args)

        self.log_manager.disable_unstructured()

        if not result and not xpcshell.sequential:
            print("Tests were run in parallel. Try running with --sequential "
                  "to make sure the failures were not caused by this.")
        return int(not result)


class AndroidXPCShellRunner(MozbuildObject):
    """Run Android xpcshell tests."""
    def run_test(self, **kwargs):
        # TODO Bug 794506 remove once mach integrates with virtualenv.
        build_path = os.path.join(self.topobjdir, 'build')
        if build_path not in sys.path:
            sys.path.append(build_path)

        import remotexpcshelltests

        log = kwargs.pop("log")
        self.log_manager.enable_unstructured()

        if kwargs["xpcshell"] is None:
            kwargs["xpcshell"] = "xpcshell"

        if not kwargs["objdir"]:
            kwargs["objdir"] = self.topobjdir

        if not kwargs["localBin"]:
            kwargs["localBin"] = os.path.join(self.topobjdir, 'dist/bin')

        if not kwargs["testingModulesDir"]:
            kwargs["testingModulesDir"] = os.path.join(self.topobjdir, '_tests/modules')

        if not kwargs["mozInfo"]:
            kwargs["mozInfo"] = os.path.join(self.topobjdir, 'mozinfo.json')

        if not kwargs["manifest"]:
            kwargs["manifest"] = os.path.join(self.topobjdir, '_tests/xpcshell/xpcshell.ini')

        if not kwargs["symbolsPath"]:
            kwargs["symbolsPath"] = os.path.join(self.distdir, 'crashreporter-symbols')

        if not kwargs["localAPK"]:
            for root, _, paths in os.walk(os.path.join(kwargs["objdir"], "gradle")):
                for file_name in paths:
                    if (file_name.endswith(".apk") and
                        file_name.startswith("geckoview-withGeckoBinaries")):
                        kwargs["localAPK"] = os.path.join(root, file_name)
                        print("using APK: %s" % kwargs["localAPK"])
                        break
                if kwargs["localAPK"]:
                    break
            else:
                raise Exception("APK not found in objdir. You must specify an APK.")

        if not kwargs["sequential"]:
            kwargs["sequential"] = True

        xpcshell = remotexpcshelltests.XPCShellRemote(kwargs, log)

        result = xpcshell.runTests(kwargs, testClass=remotexpcshelltests.RemoteXPCShellTestThread,
                                   mobileArgs=xpcshell.mobileArgs)

        self.log_manager.disable_unstructured()

        return int(not result)


def get_parser():
    build_obj = MozbuildObject.from_environment(cwd=here)
    if conditions.is_android(build_obj):
        return parser_remote()
    else:
        return parser_desktop()


@CommandProvider
class MachCommands(MachCommandBase):
    @Command('xpcshell-test', category='testing',
             description='Run XPCOM Shell tests (API direct unit testing)',
             conditions=[lambda *args: True],
             parser=get_parser)
    def run_xpcshell_test(self, test_objects=None, **params):
        from mozbuild.controller.building import BuildDriver

        if test_objects is not None:
            from manifestparser import TestManifest
            m = TestManifest()
            m.tests.extend(test_objects)
            params['manifest'] = m

        driver = self._spawn(BuildDriver)
        driver.install_tests(test_objects)

        # We should probably have a utility function to ensure the tree is
        # ready to run tests. Until then, we just create the state dir (in
        # case the tree wasn't built with mach).
        self._ensure_state_subdir_exists('.')

        if not params.get('log'):
            log_defaults = {self._mach_context.settings['test']['format']: sys.stdout}
            fmt_defaults = {
                "level": self._mach_context.settings['test']['level'],
                "verbose": True
            }
            params['log'] = structured.commandline.setup_logging(
                "XPCShellTests", params, log_defaults, fmt_defaults)

        if not params['threadCount']:
            params['threadCount'] = int((cpu_count() * 3) / 2)

        if conditions.is_android(self):
            from mozrunner.devices.android_device import verify_android_device, get_adb_path
            device_serial = params.get('deviceSerial')
            verify_android_device(self, network=True, device_serial=device_serial)
            if not params['adbPath']:
                params['adbPath'] = get_adb_path(self)
            xpcshell = self._spawn(AndroidXPCShellRunner)
        else:
            xpcshell = self._spawn(XPCShellRunner)
        xpcshell.cwd = self._mach_context.cwd

        try:
            return xpcshell.run_test(**params)
        except InvalidTestPathError as e:
            print(e.message)
            return 1
