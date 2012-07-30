#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import os
import traceback
from remotexpcshelltests import XPCShellRemote, RemoteXPCShellOptions
from automationutils import *
import devicemanagerADB

DEVICE_TEST_ROOT = '/data/local/tests'

sys.path.insert(0, os.path.abspath(os.path.realpath(os.path.dirname(sys.argv[0]))))

from marionette import Marionette


class B2GXPCShellRemote(XPCShellRemote):

    # Overridden
    def setupUtilities(self):
        # Ensure a fresh directory structure for our tests
        self.clean()
        self.device.mkDir(DEVICE_TEST_ROOT)

        XPCShellRemote.setupUtilities(self)

    def clean(self):
        print >>sys.stderr, "\nCleaning files from previous run.."
        self.device.removeDir(DEVICE_TEST_ROOT)

    # Overridden
    def pushLibs(self):
        if not self.options.use_device_libs:
            XPCShellRemote.pushLibs(self)

    # Overridden
    def setLD_LIBRARY_PATH(self, env):
        if self.options.use_device_libs:
            env['LD_LIBRARY_PATH'] = '/system/b2g'
        else:
            XPCShellRemote.setLD_LIBRARY_PATH(self, env)

    # Overridden
    # This returns 1 even when tests pass - this is why it's switched to 0
    # https://bugzilla.mozilla.org/show_bug.cgi?id=773703
    def getReturnCode(self, proc):
#        if self.shellReturnCode is not None:
#            return self.shellReturnCode
#        return -1
        return 0


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

        defaults['dm_trans'] = 'adb'
        defaults['debugger'] = None
        defaults['debuggerArgs'] = None

        self.set_defaults(**defaults)


def main():
    parser = B2GOptions()
    options, args = parser.parse_args()

    if options.objdir is None:
        try:
            options.objdir = os.path.join(options.b2g_path, 'objdir-gecko')
        except:
            print >> sys.stderr, "Need to specify a --b2gpath"
            sys.exit(1)

    # Create the Marionette instance
    kwargs = {}
    if options.emulator:
        kwargs['emulator'] = options.emulator
        if options.no_window:
            kwargs['noWindow'] = True
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
    kwargs['deviceRoot'] = DEVICE_TEST_ROOT
    dm = devicemanagerADB.DeviceManagerADB(**kwargs)

    options.remoteTestRoot = dm.getDeviceRoot()

    xpcsh = B2GXPCShellRemote(dm, options, args)

    try:
        success = xpcsh.runTests(xpcshell='xpcshell', testdirs=args[0:], **options.__dict__)
    except:
        print "TEST-UNEXPECTED-FAIL | %s | Exception caught while running tests." % sys.exc_info()[1]
        traceback.print_exc()
        sys.exit(1)

    sys.exit(int(success))


# You usually run this like :
# python runtestsb2g.py --emulator arm --b2gpath $B2GPATH --manifest $MANIFEST [--objdir $OBJDIR
#                                                                               --adbpath $ADBPATH
#                                                                               ...]
#
# For xUnit output you should also pass in --tests-root-dir ..objdir-gecko/_tests
#                                          --xunit-file ..objdir_gecko/_tests/results.xml
#                                          --xunit-suite-name xpcshell-tests
if __name__ == '__main__':
    main()
