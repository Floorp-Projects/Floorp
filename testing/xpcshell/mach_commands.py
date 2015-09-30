# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Integrates the xpcshell test runner with mach.

from __future__ import absolute_import, unicode_literals, print_function

import argparse
import os
import shutil
import sys

from mozlog import structured

from mozbuild.base import (
    MachCommandBase,
    MozbuildObject,
    MachCommandConditions as conditions,
)

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

from xpcshellcommandline import parser_desktop, parser_remote, parser_b2g

ADB_NOT_FOUND = '''
The %s command requires the adb binary to be on your path.

If you have a B2G build, this can be found in
'%s/out/host/<platform>/bin'.
'''.lstrip()

BUSYBOX_URLS = {
    'arm': 'http://www.busybox.net/downloads/binaries/latest/busybox-armv7l',
    'x86': 'http://www.busybox.net/downloads/binaries/latest/busybox-i686'
}

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

        self.run_suite(**kwargs)


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

        # Python through 2.7.2 has issues with unicode in some of the
        # arguments. Work around that.
        filtered_args = {}
        for k, v in kwargs.iteritems():
            if isinstance(v, unicode_type):
                v = v.encode('utf-8')

            if isinstance(k, unicode_type):
                k = k.encode('utf-8')

            filtered_args[k] = v

        result = xpcshell.runTests(**filtered_args)

        self.log_manager.disable_unstructured()

        if not result and not xpcshell.sequential:
            print("Tests were run in parallel. Try running with --sequential "
                  "to make sure the failures were not caused by this.")
        return int(not result)

class AndroidXPCShellRunner(MozbuildObject):
    """Get specified DeviceManager"""
    def get_devicemanager(self, devicemanager, ip, port, remote_test_root):
        import mozdevice
        dm = None
        if devicemanager == "adb":
            if ip:
                dm = mozdevice.DroidADB(ip, port, packageName=None, deviceRoot=remote_test_root)
            else:
                dm = mozdevice.DroidADB(packageName=None, deviceRoot=remote_test_root)
        else:
            if ip:
                dm = mozdevice.DroidSUT(ip, port, deviceRoot=remote_test_root)
            else:
                raise Exception("You must provide a device IP to connect to via the --ip option")
        return dm

    """Run Android xpcshell tests."""
    def run_test(self, **kwargs):
        # TODO Bug 794506 remove once mach integrates with virtualenv.
        build_path = os.path.join(self.topobjdir, 'build')
        if build_path not in sys.path:
            sys.path.append(build_path)

        import remotexpcshelltests

        dm = self.get_devicemanager(kwargs["dm_trans"], kwargs["deviceIP"], kwargs["devicePort"],
                                    kwargs["remoteTestRoot"])

        log = kwargs.pop("log")
        self.log_manager.enable_unstructured()

        if kwargs["xpcshell"] is None:
            kwargs["xpcshell"] = "xpcshell"

        if not kwargs["objdir"]:
            kwargs["objdir"] = self.topobjdir

        if not kwargs["localLib"]:
            kwargs["localLib"] = os.path.join(self.topobjdir, 'dist/fennec')

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
            for file_name in os.listdir(os.path.join(kwargs["objdir"], "dist")):
                if file_name.endswith(".apk") and file_name.startswith("fennec"):
                    kwargs["localAPK"] = os.path.join(kwargs["objdir"], "dist", file_name)
                    print ("using APK: %s" % kwargs["localAPK"])
                    break
            else:
                raise Exception("You must specify an APK")

        options = argparse.Namespace(**kwargs)
        xpcshell = remotexpcshelltests.XPCShellRemote(dm, options, log)

        result = xpcshell.runTests(testClass=remotexpcshelltests.RemoteXPCShellTestThread,
                                   mobileArgs=xpcshell.mobileArgs,
                                   **vars(options))

        self.log_manager.disable_unstructured()

        return int(not result)

class B2GXPCShellRunner(MozbuildObject):
    def __init__(self, *args, **kwargs):
        MozbuildObject.__init__(self, *args, **kwargs)

        # TODO Bug 794506 remove once mach integrates with virtualenv.
        build_path = os.path.join(self.topobjdir, 'build')
        if build_path not in sys.path:
            sys.path.append(build_path)

        build_path = os.path.join(self.topsrcdir, 'build')
        if build_path not in sys.path:
            sys.path.append(build_path)

        self.tests_dir = os.path.join(self.topobjdir, '_tests')
        self.xpcshell_dir = os.path.join(self.tests_dir, 'xpcshell')
        self.bin_dir = os.path.join(self.distdir, 'bin')

    def _download_busybox(self, b2g_home, emulator):
        import urllib2

        target_device = 'generic'
        if emulator == 'x86':
            target_device = 'generic_x86'
        system_bin = os.path.join(b2g_home, 'out', 'target', 'product', target_device, 'system', 'bin')
        busybox_path = os.path.join(system_bin, 'busybox')

        if os.path.isfile(busybox_path):
            return busybox_path

        if not os.path.isdir(system_bin):
            os.makedirs(system_bin)

        try:
            data = urllib2.urlopen(BUSYBOX_URLS[emulator])
        except urllib2.URLError:
            print('There was a problem downloading busybox. Proceeding without it,' \
                  'initial setup will be slow.')
            return

        with open(busybox_path, 'wb') as f:
            f.write(data.read())
        return busybox_path

    def run_test(self, **kwargs):
        try:
            import which
            which.which('adb')
        except which.WhichError:
            # TODO Find adb automatically if it isn't on the path
            print(ADB_NOT_FOUND % ('mochitest-remote', kwargs["b2g_home"]))
            sys.exit(1)

        import runtestsb2g

        log = kwargs.pop("log")
        self.log_manager.enable_unstructured()

        if kwargs["xpcshell"] is None:
            kwargs["xpcshell"] = "xpcshell"
        if kwargs["b2g_path"] is None:
            kwargs["b2g_path"] = kwargs["b2g_home"]
        if kwargs["busybox"] is None:
            kwargs["busybox"] = os.environ.get('BUSYBOX')
        if kwargs["busybox"] is None:
            kwargs["busybox"] = self._download_busybox(kwargs["b2g_home"], kwargs["emulator"])

        if kwargs["localLib"] is None:
            kwargs["localLib"] = self.bin_dir
        if kwargs["localBin"] is None:
            kwargs["localBin"] = self.bin_dir
        if kwargs["logdir"] is None:
            kwargs["logdir"] = self.xpcshell_dir
        if kwargs["manifest"] is None:
            kwargs["manifest"] = os.path.join(self.xpcshell_dir, 'xpcshell.ini')
        if kwargs["mozInfo"] is None:
            kwargs["mozInfo"] = os.path.join(self.topobjdir, 'mozinfo.json')
        if kwargs["objdir"] is None:
            kwargs["objdir"] = self.topobjdir
        if kwargs["symbolsPath"] is None:
            kwargs["symbolsPath"] = os.path.join(self.distdir, 'crashreporter-symbols')
        if kwargs["testingModulesDir"] is None:
            kwargs["testingModulesDir"] = os.path.join(self.tests_dir, 'modules')
        if kwargs["use_device_libs"] is None:
            kwargs["use_device_libs"] = True

        if kwargs["device_name"].startswith('emulator') and 'x86' in kwargs["device_name"]:
            kwargs["emulator"] = 'x86'

        parser = parser_b2g()
        options = argparse.Namespace(**kwargs)
        rv = runtestsb2g.run_remote_xpcshell(parser, options, log)

        self.log_manager.disable_unstructured()
        return rv

def get_parser():
    build_obj = MozbuildObject.from_environment(cwd=here)
    if conditions.is_android(build_obj):
        return parser_remote()
    elif conditions.is_b2g(build_obj):
        return parser_b2g()
    else:
        return parser_desktop()

@CommandProvider
class MachCommands(MachCommandBase):
    def __init__(self, context):
        MachCommandBase.__init__(self, context)

        for attr in ('b2g_home', 'device_name'):
            setattr(self, attr, getattr(context, attr, None))

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

        # We should probably have a utility function to ensure the tree is
        # ready to run tests. Until then, we just create the state dir (in
        # case the tree wasn't built with mach).
        self._ensure_state_subdir_exists('.')

        driver = self._spawn(BuildDriver)
        driver.install_tests(remove=False)

        params['log'] = structured.commandline.setup_logging("XPCShellTests",
                                                             params,
                                                             {"mach": sys.stdout},
                                                             {"verbose": True})

        if conditions.is_android(self):
            from mozrunner.devices.android_device import verify_android_device
            verify_android_device(self)
            xpcshell = self._spawn(AndroidXPCShellRunner)
        elif conditions.is_b2g(self):
            xpcshell = self._spawn(B2GXPCShellRunner)
            params['b2g_home'] = self.b2g_home
            params['device_name'] = self.device_name
        else:
            xpcshell = self._spawn(XPCShellRunner)
        xpcshell.cwd = self._mach_context.cwd

        try:
            return xpcshell.run_test(**params)
        except InvalidTestPathError as e:
            print(e.message)
            return 1
