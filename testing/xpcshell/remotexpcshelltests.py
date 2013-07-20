#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import posixpath
import sys, os
import subprocess
import runxpcshelltests as xpcshell
import tempfile
from automationutils import replaceBackSlashes
import devicemanagerADB, devicemanagerSUT, devicemanager
from zipfile import ZipFile
import shutil

here = os.path.dirname(os.path.abspath(__file__))

def remoteJoin(path1, path2):
    return posixpath.join(path1, path2)

class RemoteXPCShellTestThread(xpcshell.XPCShellTestThread):
    def __init__(self, *args, **kwargs):
        xpcshell.XPCShellTestThread.__init__(self, *args, **kwargs)

        # embed the mobile params from the harness into the TestThread
        mobileArgs = kwargs.get('mobileArgs')
        for key in mobileArgs:
            setattr(self, key, mobileArgs[key])

    def buildCmdTestFile(self, name):
        remoteDir = self.remoteForLocal(os.path.dirname(name))
        if remoteDir == self.remoteHere:
            remoteName = os.path.basename(name)
        else:
            remoteName = remoteJoin(remoteDir, os.path.basename(name))
        return ['-e', 'const _TEST_FILE = ["%s"];' %
                 replaceBackSlashes(remoteName)]

    def remoteForLocal(self, local):
        for mapping in self.pathMapping:
            if (os.path.abspath(mapping.local) == os.path.abspath(local)):
                return mapping.remote
        return local


    def setupTempDir(self):
        # make sure the temp dir exists
        if not self.device.dirExists(self.remoteTmpDir):
            self.device.mkDir(self.remoteTmpDir)
        # env var is set in buildEnvironment
        return self.remoteTmpDir

    def setupPluginsDir(self):
        if not os.path.isdir(self.pluginsPath):
            return None

        # making sure tmp dir is set up
        self.setupTempDir()

        pluginsDir = remoteJoin(self.remoteTmpDir, "plugins")
        self.device.pushDir(self.pluginsPath, pluginsDir)
        if self.interactive:
            self.log.info("TEST-INFO | plugins dir is %s" % pluginsDir)
        return pluginsDir

    def setupProfileDir(self):
        self.device.removeDir(self.profileDir)
        self.device.mkDir(self.profileDir)
        if self.interactive or self.singleFile:
            self.log.info("TEST-INFO | profile dir is %s" % self.profileDir)
        return self.profileDir

    def logCommand(self, name, completeCmd, testdir):
        self.log.info("TEST-INFO | %s | full command: %r" % (name, completeCmd))
        self.log.info("TEST-INFO | %s | current directory: %r" % (name, self.remoteHere))
        self.log.info("TEST-INFO | %s | environment: %s" % (name, self.env))

    def getHeadAndTailFiles(self, test):
        """Override parent method to find files on remote device."""
        def sanitize_list(s, kind):
            for f in s.strip().split(' '):
                f = f.strip()
                if len(f) < 1:
                    continue

                path = remoteJoin(self.remoteHere, f)
                if not self.device.fileExists(path):
                    raise Exception('%s file does not exist: %s' % ( kind,
                        path))

                yield path

        self.remoteHere = self.remoteForLocal(test['here'])

        return (list(sanitize_list(test['head'], 'head')),
                list(sanitize_list(test['tail'], 'tail')))

    def buildXpcsCmd(self, testdir):
        # change base class' paths to remote paths and use base class to build command
        self.xpcshell = remoteJoin(self.remoteBinDir, "xpcw")
        self.headJSPath = remoteJoin(self.remoteScriptsDir, 'head.js')
        self.httpdJSPath = remoteJoin(self.remoteComponentsDir, 'httpd.js')
        self.httpdManifest = remoteJoin(self.remoteComponentsDir, 'httpd.manifest')
        self.testingModulesDir = self.remoteModulesDir
        self.testharnessdir = self.remoteScriptsDir
        xpcshell.XPCShellTestThread.buildXpcsCmd(self, testdir)
        # remove "-g <dir> -a <dir>" and add "--greomni <apk>"
        del(self.xpcsCmd[1:5])
        if self.options.localAPK:
            self.xpcsCmd.insert(3, '--greomni')
            self.xpcsCmd.insert(4, self.remoteAPK)

        if self.remoteDebugger:
            # for example, "/data/local/gdbserver" "localhost:12345"
            self.xpcsCmd = [
              self.remoteDebugger,
              self.remoteDebuggerArgs,
              self.xpcsCmd]

    def launchProcess(self, cmd, stdout, stderr, env, cwd):
        cmd.insert(1, self.remoteHere)
        outputFile = "xpcshelloutput"
        f = open(outputFile, "w+")
        self.shellReturnCode = self.device.shell(cmd, f)
        f.close()
        # The device manager may have timed out waiting for xpcshell.
        # Guard against an accumulation of hung processes by killing
        # them here. Note also that IPC tests may spawn new instances
        # of xpcshell.
        self.device.killProcess(cmd[0])
        self.device.killProcess("xpcshell")
        return outputFile

    def communicate(self, proc):
        f = open(proc, "r")
        contents = f.read()
        f.close()
        os.remove(proc)
        return contents, ""

    def poll(self, proc):
        if self.device.processExist("xpcshell") is None:
            return self.getReturnCode(proc)
        # Process is still running
        return None

    def kill(self, proc):
        return self.device.killProcess("xpcshell", True)

    def getReturnCode(self, proc):
        if self.shellReturnCode is not None:
            return self.shellReturnCode
        else:
            return -1

    def removeDir(self, dirname):
        self.device.removeDir(dirname)

    #TODO: consider creating a separate log dir.  We don't have the test file structure,
    #      so we use filename.log.  Would rather see ./logs/filename.log
    def createLogFile(self, test, stdout):
        try:
            f = None
            filename = test.replace('\\', '/').split('/')[-1] + ".log"
            f = open(filename, "w")
            f.write(stdout)

        finally:
            if f is not None:
                f.close()


# A specialization of XPCShellTests that runs tests on an Android device
# via devicemanager.
class XPCShellRemote(xpcshell.XPCShellTests, object):

    def __init__(self, devmgr, options, args):
        xpcshell.XPCShellTests.__init__(self)
        self.localLib = options.localLib
        self.localBin = options.localBin
        self.options = options
        self.device = devmgr
        self.pathMapping = []
        self.remoteTestRoot = self.device.getTestRoot("xpcshell")
        # remoteBinDir contains xpcshell and its wrapper script, both of which must
        # be executable. Since +x permissions cannot usually be set on /mnt/sdcard,
        # and the test root may be on /mnt/sdcard, remoteBinDir is set to be on
        # /data/local, always.
        self.remoteBinDir = "/data/local/xpcb"
        # Terse directory names are used here ("c" for the components directory)
        # to minimize the length of the command line used to execute
        # xpcshell on the remote device. adb has a limit to the number
        # of characters used in a shell command, and the xpcshell command
        # line can be quite complex.
        self.remoteTmpDir = remoteJoin(self.remoteTestRoot, "tmp")
        self.remoteScriptsDir = self.remoteTestRoot
        self.remoteComponentsDir = remoteJoin(self.remoteTestRoot, "c")
        self.remoteModulesDir = remoteJoin(self.remoteTestRoot, "m")
        self.profileDir = remoteJoin(self.remoteTestRoot, "p")
        self.remoteDebugger = options.debugger
        self.remoteDebuggerArgs = options.debuggerArgs
        self.testingModulesDir = options.testingModulesDir

        if self.options.objdir:
            self.xpcDir = os.path.join(self.options.objdir, "_tests/xpcshell")
        elif os.path.isdir(os.path.join(here, 'tests')):
            self.xpcDir = os.path.join(here, 'tests')
        else:
            print >> sys.stderr, "Couldn't find local xpcshell test directory"
            sys.exit(1)

        if options.localAPK:
            self.localAPKContents = ZipFile(options.localAPK)
        if options.setup:
            self.setupUtilities()
            self.setupModules()
            self.setupTestDir()
        self.remoteAPK = None
        if options.localAPK:
            self.remoteAPK = remoteJoin(self.remoteBinDir, os.path.basename(options.localAPK))
            self.setAppRoot()

        # data that needs to be passed to the RemoteXPCShellTestThread
        self.mobileArgs = {
            'device': self.device,
            'remoteBinDir': self.remoteBinDir,
            'remoteScriptsDir': self.remoteScriptsDir,
            'remoteComponentsDir': self.remoteComponentsDir,
            'remoteModulesDir': self.remoteModulesDir,
            'options': self.options,
            'remoteDebugger': self.remoteDebugger,
            'pathMapping': self.pathMapping,
            'profileDir': self.profileDir,
            'remoteTmpDir': self.remoteTmpDir,
        }
        if self.remoteAPK:
            self.mobileArgs['remoteAPK'] = self.remoteAPK

    def setLD_LIBRARY_PATH(self, env):
        env["LD_LIBRARY_PATH"]=self.remoteBinDir

    def pushWrapper(self):
        # Rather than executing xpcshell directly, this wrapper script is
        # used. By setting environment variables and the cwd in the script,
        # the length of the per-test command line is shortened. This is
        # often important when using ADB, as there is a limit to the length
        # of the ADB command line.
        localWrapper = tempfile.mktemp()
        f = open(localWrapper, "w")
        f.write("#!/system/bin/sh\n")
        for envkey, envval in self.env.iteritems():
            f.write("export %s=%s\n" % (envkey, envval))
        f.write("cd $1\n")
        f.write("echo xpcw: cd $1\n")
        f.write("shift\n")
        f.write("echo xpcw: xpcshell \"$@\"\n")
        f.write("%s/xpcshell \"$@\"\n" % self.remoteBinDir)
        f.close()
        remoteWrapper = remoteJoin(self.remoteBinDir, "xpcw")
        self.device.pushFile(localWrapper, remoteWrapper)
        os.remove(localWrapper)
        self.device.chmodDir(self.remoteBinDir)

    def buildEnvironment(self):
        self.env = {}
        self.buildCoreEnvironment()
        self.setLD_LIBRARY_PATH(self.env)
        self.env["MOZ_LINKER_CACHE"] = self.remoteBinDir
        if self.options.localAPK and self.appRoot:
            self.env["GRE_HOME"] = self.appRoot
        self.env["XPCSHELL_TEST_PROFILE_DIR"] = self.profileDir
        self.env["TMPDIR"] = self.remoteTmpDir
        self.env["HOME"] = self.profileDir
        self.env["XPCSHELL_TEST_TEMP_DIR"] = self.remoteTmpDir
        if self.options.setup:
            self.pushWrapper()

    def setAppRoot(self):
        # Determine the application root directory associated with the package
        # name used by the Fennec APK.
        self.appRoot = None
        packageName = None
        if self.options.localAPK:
            try:
                packageName = self.localAPKContents.read("package-name.txt")
                if packageName:
                    self.appRoot = self.device.getAppRoot(packageName.strip())
            except Exception as detail:
                print "unable to determine app root: " + str(detail)
                pass
        return None

    def setupUtilities(self):
        if (not self.device.dirExists(self.remoteBinDir)):
            # device.mkDir may fail here where shellCheckOutput may succeed -- see bug 817235
            self.device.shellCheckOutput(["mkdir", self.remoteBinDir]);

        remotePrefDir = remoteJoin(self.remoteBinDir, "defaults/pref")
        if (self.device.dirExists(self.remoteTmpDir)):
            self.device.removeDir(self.remoteTmpDir)
        self.device.mkDir(self.remoteTmpDir)
        if (not self.device.dirExists(remotePrefDir)):
            self.device.mkDirs(remoteJoin(remotePrefDir, "extra"))
        if (not self.device.dirExists(self.remoteScriptsDir)):
            self.device.mkDir(self.remoteScriptsDir)
        if (not self.device.dirExists(self.remoteComponentsDir)):
            self.device.mkDir(self.remoteComponentsDir)

        local = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'head.js')
        remoteFile = remoteJoin(self.remoteScriptsDir, "head.js")
        self.device.pushFile(local, remoteFile)

        local = os.path.join(self.localBin, "xpcshell")
        remoteFile = remoteJoin(self.remoteBinDir, "xpcshell")
        self.device.pushFile(local, remoteFile)

        local = os.path.join(self.localBin, "components/httpd.js")
        remoteFile = remoteJoin(self.remoteComponentsDir, "httpd.js")
        self.device.pushFile(local, remoteFile)

        local = os.path.join(self.localBin, "components/httpd.manifest")
        remoteFile = remoteJoin(self.remoteComponentsDir, "httpd.manifest")
        self.device.pushFile(local, remoteFile)

        local = os.path.join(self.localBin, "components/test_necko.xpt")
        remoteFile = remoteJoin(self.remoteComponentsDir, "test_necko.xpt")
        self.device.pushFile(local, remoteFile)

        if self.options.localAPK:
            remoteFile = remoteJoin(self.remoteBinDir, os.path.basename(self.options.localAPK))
            self.device.pushFile(self.options.localAPK, remoteFile)

        self.pushLibs()

    def pushLibs(self):
        if self.options.localAPK:
            try:
                dir = tempfile.mkdtemp()
                szip = os.path.join(self.localBin, '..', 'host', 'bin', 'szip')
                if not os.path.exists(szip):
                    # Tinderbox builds must run szip from the test package
                    szip = os.path.join(self.localBin, 'host', 'szip')
                if not os.path.exists(szip):
                    # If the test package doesn't contain szip, it means files
                    # are not szipped in the test package.
                    szip = None
                for info in self.localAPKContents.infolist():
                    if info.filename.endswith(".so"):
                        print >> sys.stderr, "Pushing %s.." % info.filename
                        remoteFile = remoteJoin(self.remoteBinDir, os.path.basename(info.filename))
                        self.localAPKContents.extract(info, dir)
                        file = os.path.join(dir, info.filename)
                        if szip:
                            out = subprocess.check_output([szip, '-d', file], stderr=subprocess.STDOUT)
                        self.device.pushFile(os.path.join(dir, info.filename), remoteFile)
            finally:
                shutil.rmtree(dir)
            return

        for file in os.listdir(self.localLib):
            if (file.endswith(".so")):
                print >> sys.stderr, "Pushing %s.." % file
                if 'libxul' in file:
                    print >> sys.stderr, "This is a big file, it could take a while."
                remoteFile = remoteJoin(self.remoteBinDir, file)
                self.device.pushFile(os.path.join(self.localLib, file), remoteFile)

        # Additional libraries may be found in a sub-directory such as "lib/armeabi-v7a"
        localArmLib = os.path.join(self.localLib, "lib")
        if os.path.exists(localArmLib):
            for root, dirs, files in os.walk(localArmLib):
                for file in files:
                    if (file.endswith(".so")):
                        print >> sys.stderr, "Pushing %s.." % file
                        remoteFile = remoteJoin(self.remoteBinDir, file)
                        self.device.pushFile(os.path.join(root, file), remoteFile)

    def setupModules(self):
        if self.testingModulesDir:
            self.device.pushDir(self.testingModulesDir, self.remoteModulesDir)

    def setupTestDir(self):
        print 'pushing %s' % self.xpcDir
        try:
            self.device.pushDir(self.xpcDir, self.remoteScriptsDir, retryLimit=10)
        except TypeError:
            # Foopies have an older mozdevice ver without retryLimit
            self.device.pushDir(self.xpcDir, self.remoteScriptsDir)

    def buildTestList(self):
        xpcshell.XPCShellTests.buildTestList(self)
        uniqueTestPaths = set([])
        for test in self.alltests:
            uniqueTestPaths.add(test['here'])
        for testdir in uniqueTestPaths:
            abbrevTestDir = os.path.relpath(testdir, self.xpcDir)
            remoteScriptDir = remoteJoin(self.remoteScriptsDir, abbrevTestDir)
            self.pathMapping.append(PathMapping(testdir, remoteScriptDir))

class RemoteXPCShellOptions(xpcshell.XPCShellOptions):

    def __init__(self):
        xpcshell.XPCShellOptions.__init__(self)
        defaults = {}

        self.add_option("--deviceIP", action="store",
                        type = "string", dest = "deviceIP",
                        help = "ip address of remote device to test")
        defaults["deviceIP"] = None

        self.add_option("--devicePort", action="store",
                        type = "string", dest = "devicePort",
                        help = "port of remote device to test")
        defaults["devicePort"] = 20701

        self.add_option("--dm_trans", action="store",
                        type = "string", dest = "dm_trans",
                        help = "the transport to use to communicate with device: [adb|sut]; default=sut")
        defaults["dm_trans"] = "sut"

        self.add_option("--objdir", action="store",
                        type = "string", dest = "objdir",
                        help = "local objdir, containing xpcshell binaries")
        defaults["objdir"] = None

        self.add_option("--apk", action="store",
                        type = "string", dest = "localAPK",
                        help = "local path to Fennec APK")
        defaults["localAPK"] = None

        self.add_option("--noSetup", action="store_false",
                        dest = "setup",
                        help = "do not copy any files to device (to be used only if device is already setup)")
        defaults["setup"] = True

        self.add_option("--local-lib-dir", action="store",
                        type = "string", dest = "localLib",
                        help = "local path to library directory")
        defaults["localLib"] = None

        self.add_option("--local-bin-dir", action="store",
                        type = "string", dest = "localBin",
                        help = "local path to bin directory")
        defaults["localBin"] = None

        self.add_option("--remoteTestRoot", action = "store",
                    type = "string", dest = "remoteTestRoot",
                    help = "remote directory to use as test root (eg. /mnt/sdcard/tests or /data/local/tests)")
        defaults["remoteTestRoot"] = None

        self.set_defaults(**defaults)

    def verifyRemoteOptions(self, options):
        if options.localLib is None:
            if options.localAPK and options.objdir:
                for path in ['dist/fennec', 'fennec/lib']:
                    options.localLib = os.path.join(options.objdir, path)
                    if os.path.isdir(options.localLib):
                        break
                else:
                    self.error("Couldn't find local library dir, specify --local-lib-dir")
            elif options.objdir:
                options.localLib = os.path.join(options.objdir, 'dist/bin')
            elif os.path.isfile(os.path.join(here, '..', 'bin', 'xpcshell')):
                # assume tests are being run from a tests.zip
                options.localLib = os.path.abspath(os.path.join(here, '..', 'bin'))
            else:
                self.error("Couldn't find local library dir, specify --local-lib-dir")

        if options.localBin is None:
            if options.objdir:
                for path in ['dist/bin', 'bin']:
                    options.localBin = os.path.join(options.objdir, path)
                    if os.path.isdir(options.localBin):
                        break
                else:
                    self.error("Couldn't find local binary dir, specify --local-bin-dir")
            elif os.path.isfile(os.path.join(here, '..', 'bin', 'xpcshell')):
                # assume tests are being run from a tests.zip
                options.localBin = os.path.abspath(os.path.join(here, '..', 'bin'))
            else:
                self.error("Couldn't find local binary dir, specify --local-bin-dir")
        return options

class PathMapping:

    def __init__(self, localDir, remoteDir):
        self.local = localDir
        self.remote = remoteDir

def main():

    if sys.version_info < (2,7):
        print >>sys.stderr, "Error: You must use python version 2.7 or newer but less than 3.0"
        sys.exit(1)

    parser = RemoteXPCShellOptions()
    options, args = parser.parse_args()
    if not options.localAPK:
        for file in os.listdir(os.path.join(options.objdir, "dist")):
            if (file.endswith(".apk") and file.startswith("fennec")):
                options.localAPK = os.path.join(options.objdir, "dist")
                options.localAPK = os.path.join(options.localAPK, file)
                print >>sys.stderr, "using APK: " + options.localAPK
                break
        else:
            print >>sys.stderr, "Error: please specify an APK"
            sys.exit(1)

    options = parser.verifyRemoteOptions(options)

    if len(args) < 1 and options.manifest is None:
        print >>sys.stderr, """Usage: %s <test dirs>
             or: %s --manifest=test.manifest """ % (sys.argv[0], sys.argv[0])
        sys.exit(1)

    if (options.dm_trans == "adb"):
        if (options.deviceIP):
            dm = devicemanagerADB.DeviceManagerADB(options.deviceIP, options.devicePort, packageName=None, deviceRoot=options.remoteTestRoot)
        else:
            dm = devicemanagerADB.DeviceManagerADB(packageName=None, deviceRoot=options.remoteTestRoot)
    else:
        dm = devicemanagerSUT.DeviceManagerSUT(options.deviceIP, options.devicePort, deviceRoot=options.remoteTestRoot)
        if (options.deviceIP == None):
            print "Error: you must provide a device IP to connect to via the --device option"
            sys.exit(1)

    if options.interactive and not options.testPath:
        print >>sys.stderr, "Error: You must specify a test filename in interactive mode!"
        sys.exit(1)

    xpcsh = XPCShellRemote(dm, options, args)

    # we don't run concurrent tests on mobile
    options.sequential = True

    if not xpcsh.runTests(xpcshell='xpcshell',
                          testClass=RemoteXPCShellTestThread,
                          testdirs=args[0:],
                          mobileArgs=xpcsh.mobileArgs,
                          **options.__dict__):
        sys.exit(1)


if __name__ == '__main__':
    main()
