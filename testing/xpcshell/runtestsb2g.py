#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import os
sys.path.insert(0, os.path.abspath(os.path.realpath(os.path.dirname(sys.argv[0]))))

import traceback
from remotexpcshelltests import RemoteXPCShellTestThread, XPCShellRemote, RemoteXPCShellOptions
from mozdevice import devicemanagerADB, DMError

DEVICE_TEST_ROOT = '/data/local/tests'


from marionette import Marionette

class B2GXPCShellTestThread(RemoteXPCShellTestThread):
    # Overridden
    def setLD_LIBRARY_PATH(self, env):
        if self.options.use_device_libs:
            env['LD_LIBRARY_PATH'] = '/system/b2g'
            env['LD_PRELOAD'] = '/system/b2g/libmozglue.so'
        else:
            XPCShellRemote.setLD_LIBRARY_PATH(self, env)

    # Overridden
    def launchProcess(self, cmd, stdout, stderr, env, cwd):
        try:
            # This returns 1 even when tests pass - hardcode returncode to 0 (bug 773703)
            outputFile = RemoteXPCShellTestThread.launchProcess(self, cmd, stdout, stderr, env, cwd)
            self.shellReturnCode = 0
        except DMError:
            self.shellReturnCode = -1
            outputFile = "xpcshelloutput"
            f = open(outputFile, "a")
            f.write("\n%s" % traceback.format_exc())
            f.close()
        return outputFile

class B2GXPCShellRemote(XPCShellRemote):
    # Overridden
    def setupUtilities(self):
        if self.options.clean:
            # Ensure a fresh directory structure for our tests
            self.clean()
            self.device.mkDir(self.options.remoteTestRoot)

        XPCShellRemote.setupUtilities(self)

    def clean(self):
        print >>sys.stderr, "\nCleaning files from previous run.."
        self.device.removeDir(self.options.remoteTestRoot)

    # Overriden
    def setupTestDir(self):
        if self.device._useZip:
            return XPCShellRemote.setupTestDir(self)

        for root, dirs, files in os.walk(self.xpcDir):
            for filename in files:
                rel_path = os.path.relpath(os.path.join(root, filename), self.xpcDir)
                test_file = os.path.join(self.remoteScriptsDir, rel_path)
                print 'pushing %s' % test_file
                self.device.pushFile(os.path.join(root, filename), test_file, retryLimit=10)

    # Overridden
    def pushLibs(self):
        if not self.options.use_device_libs:
            XPCShellRemote.pushLibs(self)

class B2GOptions(RemoteXPCShellOptions):

    def __init__(self):
        RemoteXPCShellOptions.__init__(self)
        defaults = {}

        self.add_option('--b2gpath', action='store',
                        type='string', dest='b2g_path',
                        help="Path to B2G repo or qemu dir")
        defaults['b2g_path'] = None

        self.add_option('--emupath', action='store',
                        type='string', dest='emu_path',
                        help="Path to emulator folder (if different "
                                                      "from b2gpath")

        self.add_option('--no-clean', action='store_false',
                        dest='clean',
                        help="Do not clean TESTROOT. Saves [lots of] time")
        defaults['clean'] = True

        defaults['emu_path'] = None

        self.add_option('--emulator', action='store',
                        type='string', dest='emulator',
                        help="Architecture of emulator to use: x86 or arm")
        defaults['emulator'] = None

        self.add_option('--no-window', action='store_true',
                        dest='no_window',
                        help="Pass --no-window to the emulator")
        defaults['no_window'] = False

        self.add_option('--adbpath', action='store',
                        type='string', dest='adb_path',
                        help="Path to adb")
        defaults['adb_path'] = 'adb'

        self.add_option('--address', action='store',
                        type='string', dest='address',
                        help="host:port of running Gecko instance to connect to")
        defaults['address'] = None

        self.add_option('--use-device-libs', action='store_true',
                        dest='use_device_libs',
                        help="Don't push .so's")
        defaults['use_device_libs'] = False
        self.add_option("--gecko-path", action="store",
                        type="string", dest="geckoPath",
                        help="the path to a gecko distribution that should "
                        "be installed on the emulator prior to test")
        defaults["geckoPath"] = None
        self.add_option("--logcat-dir", action="store",
                        type="string", dest="logcat_dir",
                        help="directory to store logcat dump files")
        defaults["logcat_dir"] = None
        self.add_option('--busybox', action='store',
                        type='string', dest='busybox',
                        help="Path to busybox binary to install on device")
        defaults['busybox'] = None

        defaults["remoteTestRoot"] = DEVICE_TEST_ROOT
        defaults['dm_trans'] = 'adb'
        defaults['debugger'] = None
        defaults['debuggerArgs'] = None

        self.set_defaults(**defaults)

    def verifyRemoteOptions(self, options):
        if options.b2g_path is None:
            self.error("Need to specify a --b2gpath")

        if options.geckoPath and not options.emulator:
            self.error("You must specify --emulator if you specify --gecko-path")

        if options.logcat_dir and not options.emulator:
            self.error("You must specify --emulator if you specify --logcat-dir")
        return RemoteXPCShellOptions.verifyRemoteOptions(self, options)

def main():
    parser = B2GOptions()
    options, args = parser.parse_args()
    options = parser.verifyRemoteOptions(options)

    # Create the Marionette instance
    kwargs = {}
    if options.emulator:
        kwargs['emulator'] = options.emulator
        if options.no_window:
            kwargs['noWindow'] = True
        if options.geckoPath:
            kwargs['gecko_path'] = options.geckoPath
        if options.logcat_dir:
            kwargs['logcat_dir'] = options.logcat_dir
        if options.busybox:
            kwargs['busybox'] = options.busybox
        if options.symbolsPath:
            kwargs['symbols_path'] = options.symbolsPath
    if options.b2g_path:
        kwargs['homedir'] = options.emu_path or options.b2g_path
    if options.address:
        host, port = options.address.split(':')
        kwargs['host'] = host
        kwargs['port'] = int(port)
        kwargs['baseurl'] = 'http://%s:%d/' % (host, int(port))
        if options.emulator:
            kwargs['connectToRunningEmulator'] = True
    marionette = Marionette(**kwargs)

    # Create the DeviceManager instance
    kwargs = {'adbPath': options.adb_path}
    if options.deviceIP:
        kwargs['host'] = options.deviceIP
        kwargs['port'] = options.devicePort
    kwargs['deviceRoot'] = options.remoteTestRoot
    dm = devicemanagerADB.DeviceManagerADB(**kwargs)

    if not options.remoteTestRoot:
        options.remoteTestRoot = dm.getDeviceRoot()
    xpcsh = B2GXPCShellRemote(dm, options, args)

    # we don't run concurrent tests on mobile
    options.sequential = True

    try:
        success = xpcsh.runTests(xpcshell='xpcshell', testdirs=args[0:],
                                 testClass=B2GXPCShellTestThread,
                                 mobileArgs=xpcsh.mobileArgs,
                                 **options.__dict__)
    except:
        print "Automation Error: Exception caught while running tests"
        traceback.print_exc()
        sys.exit(1)

    sys.exit(int(success))


# You usually run this like :
# python runtestsb2g.py --emulator arm --b2gpath $B2GPATH --manifest $MANIFEST [--xre-path $MOZ_HOST_BIN
#                                                                               --adbpath $ADB_PATH
#                                                                               ...]
#
# For xUnit output you should also pass in --tests-root-dir ..objdir-gecko/_tests
#                                          --xunit-file ..objdir_gecko/_tests/results.xml
#                                          --xunit-suite-name xpcshell-tests
if __name__ == '__main__':
    main()
