#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import os
sys.path.insert(0, os.path.abspath(os.path.realpath(os.path.dirname(sys.argv[0]))))

import traceback
import remotexpcshelltests
from remotexpcshelltests import RemoteXPCShellTestThread, XPCShellRemote
from mozdevice import devicemanagerADB, DMError
from mozlog import commandline

DEVICE_TEST_ROOT = '/data/local/tests'

from marionette import Marionette
from xpcshellcommandline import parser_b2g

class B2GXPCShellTestThread(RemoteXPCShellTestThread):
    # Overridden
    def launchProcess(self, cmd, stdout, stderr, env, cwd, timeout=None):
        try:
            # This returns 1 even when tests pass - hardcode returncode to 0 (bug 773703)
            outputFile = RemoteXPCShellTestThread.launchProcess(self, cmd, stdout, stderr, env, cwd, timeout=timeout)
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
    def setLD_LIBRARY_PATH(self):
        self.env['LD_LIBRARY_PATH'] = '/system/b2g'
        if not self.options.use_device_libs:
            # overwrite /system/b2g if necessary
            XPCShellRemote.setLD_LIBRARY_PATH(self)

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
            count = XPCShellRemote.pushLibs(self)
            if not count:
                # couldn't find any libs, fallback to device libs
                self.env['LD_LIBRARY_PATH'] = '/system/b2g'
                self.options.use_device_libs = True

def verifyRemoteOptions(parser, options):
    if options.b2g_path is None:
        parser.error("Need to specify a --b2gpath")

    if options.geckoPath and not options.emulator:
        parser.error("You must specify --emulator if you specify --gecko-path")

    if options.logdir and not options.emulator:
        parser.error("You must specify --emulator if you specify --logdir")
    return remotexpcshelltests.verifyRemoteOptions(parser, options)

def run_remote_xpcshell(parser, options, log):
    options = verifyRemoteOptions(parser, options)

    # Create the Marionette instance
    kwargs = {}
    if options.emulator:
        kwargs['emulator'] = options.emulator
        if options.no_window:
            kwargs['noWindow'] = True
        if options.geckoPath:
            kwargs['gecko_path'] = options.geckoPath
        if options.logdir:
            kwargs['logdir'] = options.logdir
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

    if options.emulator:
        dm = marionette.emulator.dm
    else:
        # Create the DeviceManager instance
        kwargs = {'adbPath': options.adb_path}
        if options.deviceIP:
            kwargs['host'] = options.deviceIP
            kwargs['port'] = options.devicePort
        kwargs['deviceRoot'] = options.remoteTestRoot
        dm = devicemanagerADB.DeviceManagerADB(**kwargs)

    if not options.remoteTestRoot:
        options.remoteTestRoot = dm.deviceRoot
    xpcsh = B2GXPCShellRemote(dm, options, log)

    # we don't run concurrent tests on mobile
    options.sequential = True

    if options.xpcshell is None:
        options.xpcshell = "xpcshell"

    try:
        if not xpcsh.runTests(testClass=B2GXPCShellTestThread,
                              mobileArgs=xpcsh.mobileArgs,
                              **vars(options)):
            sys.exit(1)
    except:
        print "Automation Error: Exception caught while running tests"
        traceback.print_exc()
        sys.exit(1)

def main():
    parser = parser_b2g()
    options = parser.parse_args()
    log = commandline.setup_logging("Remote XPCShell",
                                    options,
                                    {"tbpl": sys.stdout})
    run_remote_xpcshell(parser, options, log)

# You usually run this like :
# python runtestsb2g.py --emulator arm --b2gpath $B2GPATH --manifest $MANIFEST [--xre-path $MOZ_HOST_BIN
#                                                                               --adbpath $ADB_PATH
#                                                                               ...]
if __name__ == '__main__':
    main()
