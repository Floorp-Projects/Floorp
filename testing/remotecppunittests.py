#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os, sys
import runcppunittests as cppunittests
import mozcrash, mozlog
import StringIO
import posixpath
from mozdevice import devicemanager, devicemanagerADB, devicemanagerSUT

log = mozlog.getLogger('remotecppunittests')

class RemoteCPPUnitTests(cppunittests.CPPUnitTests):
    def __init__(self, devmgr, options, progs):
        cppunittests.CPPUnitTests.__init__(self)
        self.options = options
        self.device = devmgr
        self.remote_test_root = self.device.getDeviceRoot() + "/cppunittests"
        self.remote_bin_dir = posixpath.join(self.remote_test_root, "b")
        self.remote_tmp_dir = posixpath.join(self.remote_test_root, "tmp")
        self.remote_home_dir = posixpath.join(self.remote_test_root, "h")
        if options.setup:
            self.setup_bin(progs)

    def setup_bin(self, progs):
        if not self.device.dirExists(self.remote_test_root):
            self.device.mkDir(self.remote_test_root)
        if self.device.dirExists(self.remote_tmp_dir):
            self.device.removeDir(self.remote_tmp_dir)
        self.device.mkDir(self.remote_tmp_dir)
        if self.device.dirExists(self.remote_bin_dir):
            self.device.removeDir(self.remote_bin_dir)
        self.device.mkDir(self.remote_bin_dir)
        if self.device.dirExists(self.remote_home_dir):
            self.device.removeDir(self.remote_home_dir)
        self.device.mkDir(self.remote_home_dir)
        self.push_libs()
        self.push_progs(progs)
        self.device.chmodDir(self.remote_bin_dir)

    def push_libs(self):
        for file in os.listdir(self.options.local_lib):
            if file.endswith(".so"):
                print >> sys.stderr, "Pushing %s.." % file
                remote_file = posixpath.join(self.remote_bin_dir, file)
                self.device.pushFile(os.path.join(self.options.local_lib, file), remote_file)
        # Additional libraries may be found in a sub-directory such as "lib/armeabi-v7a"
        local_arm_lib = os.path.join(self.options.local_lib, "lib")
        if os.path.isdir(local_arm_lib):
            for root, dirs, files in os.walk(local_arm_lib):
                for file in files:
                    if (file.endswith(".so")):
                        remote_file = posixpath.join(self.remote_bin_dir, file)
                        self.device.pushFile(os.path.join(root, file), remote_file)

    def push_progs(self, progs):
        for local_file in progs:
            remote_file = posixpath.join(self.remote_bin_dir, os.path.basename(local_file))
            self.device.pushFile(local_file, remote_file)

    def build_environment(self):
        env = self.build_core_environment()
        env['LD_LIBRARY_PATH'] = self.remote_bin_dir
        env["TMPDIR"]=self.remote_tmp_dir
        env["HOME"]=self.remote_home_dir
        env["MOZILLA_FIVE_HOME"] = self.remote_home_dir
        env["MOZ_XRE_DIR"] = self.remote_bin_dir
        if self.options.add_env:
            for envdef in self.options.add_env:
                envdef_parts = envdef.split("=", 1)
                if len(envdef_parts) == 2:
                    env[envdef_parts[0]] = envdef_parts[1]
                elif len(envdef_parts) == 1:
                    env[envdef_parts[0]] = ""
                else:
                    print >> sys.stderr, "warning: invalid --addEnv option skipped: "+envdef

        return env

    def run_one_test(self, prog, env, symbols_path=None):
        """
        Run a single C++ unit test program remotely.

        Arguments:
        * prog: The path to the test program to run.
        * env: The environment to use for running the program.
        * symbols_path: A path to a directory containing Breakpad-formatted
                        symbol files for producing stack traces on crash.

        Return True if the program exits with a zero status, False otherwise.
        """
        basename = os.path.basename(prog)
        remote_bin = posixpath.join(self.remote_bin_dir, basename)
        log.info("Running test %s", basename)
        buf = StringIO.StringIO()
        returncode = self.device.shell([remote_bin], buf, env=env, cwd=self.remote_home_dir,
                                       timeout=cppunittests.CPPUnitTests.TEST_PROC_TIMEOUT)
        print >> sys.stdout, buf.getvalue()
        with cppunittests.TemporaryDirectory() as tempdir:
            self.device.getDirectory(self.remote_home_dir, tempdir)
            if mozcrash.check_for_crashes(tempdir, symbols_path,
                                          test_name=basename):
                log.testFail("%s | test crashed", basename)
                return False
        result = returncode == 0
        if not result:
            log.testFail("%s | test failed with return code %s",
                         basename, returncode)
        return result

class RemoteCPPUnittestOptions(cppunittests.CPPUnittestOptions):
    def __init__(self):
        cppunittests.CPPUnittestOptions.__init__(self)
        defaults = {}

        self.add_option("--deviceIP", action="store",
                        type = "string", dest = "device_ip",
                        help = "ip address of remote device to test")
        defaults["device_ip"] = None

        self.add_option("--devicePort", action="store",
                        type = "string", dest = "device_port",
                        help = "port of remote device to test")
        defaults["device_port"] = 20701

        self.add_option("--dm_trans", action="store",
                        type = "string", dest = "dm_trans",
                        help = "the transport to use to communicate with device: [adb|sut]; default=sut")
        defaults["dm_trans"] = "sut"

        self.add_option("--noSetup", action="store_false",
                        dest = "setup",
                        help = "do not copy any files to device (to be used only if device is already setup)")
        defaults["setup"] = True

        self.add_option("--localLib", action="store",
                        type = "string", dest = "local_lib",
                        help = "location of libraries to push -- preferably stripped")
        defaults["local_lib"] = None

        self.add_option("--remoteTestRoot", action = "store",
                    type = "string", dest = "remote_test_root",
                    help = "remote directory to use as test root (eg. /data/local/tests)")
        # /data/local/tests is used because it is usually not possible to set +x permissions
        # on binaries on /mnt/sdcard
        defaults["remote_test_root"] = "/data/local/tests"

        self.add_option("--addEnv", action = "append",
                    type = "string", dest = "add_env",
                    help = "additional remote environment variable definitions (eg. --addEnv \"somevar=something\")")
        defaults["add_env"] = None

        self.set_defaults(**defaults)

def main():
    parser = RemoteCPPUnittestOptions()
    options, args = parser.parse_args()
    if not args:
        print >>sys.stderr, """Usage: %s <test binary> [<test binary>...]""" % sys.argv[0]
        sys.exit(1)
    if not options.local_lib:
        print >>sys.stderr, """Error: --localLib is required"""
        sys.exit(1)
    if not os.path.isdir(options.local_lib):
        print >>sys.stderr, """Error: --localLib directory %s not found""" % options.local_lib
        sys.exit(1)
    if not options.xre_path:
        print >>sys.stderr, """Error: --xre-path is required"""
        sys.exit(1)
    if options.dm_trans == "adb":
        if options.device_ip:
            dm = devicemanagerADB.DeviceManagerADB(options.device_ip, options.device_port, packageName=None, deviceRoot=options.remote_test_root)
        else:
            dm = devicemanagerADB.DeviceManagerADB(packageName=None, deviceRoot=options.remote_test_root)
    else:
        dm = devicemanagerSUT.DeviceManagerSUT(options.device_ip, options.device_port, deviceRoot=options.remote_test_root)
        if not options.device_ip:
            print "Error: you must provide a device IP to connect to via the --deviceIP option"
            sys.exit(1)
    options.xre_path = os.path.abspath(options.xre_path)
    progs = [os.path.abspath(p) for p in args]
    tester = RemoteCPPUnitTests(dm, options, progs)
    try:
        result = tester.run_tests(progs, options.xre_path, options.symbols_path)
    except Exception, e:
        log.error(str(e))
        result = False
    sys.exit(0 if result else 1)

if __name__ == '__main__':
    main()
