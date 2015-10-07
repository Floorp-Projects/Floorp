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
from zipfile import ZipFile
from mozlog import commandline
import shutil
import mozdevice
import mozfile
import mozinfo

from xpcshellcommandline import parser_remote

here = os.path.dirname(os.path.abspath(__file__))

def remoteJoin(path1, path2):
    return posixpath.join(path1, path2)

class RemoteXPCShellTestThread(xpcshell.XPCShellTestThread):
    def __init__(self, *args, **kwargs):
        xpcshell.XPCShellTestThread.__init__(self, *args, **kwargs)

        self.shellReturnCode = None
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
                 remoteName.replace('\\', '/')]

    def remoteForLocal(self, local):
        for mapping in self.pathMapping:
            if (os.path.abspath(mapping.local) == os.path.abspath(local)):
                return mapping.remote
        return local

    def setupTempDir(self):
        # make sure the temp dir exists
        self.clearRemoteDir(self.remoteTmpDir)
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
            self.log.info("plugins dir is %s" % pluginsDir)
        return pluginsDir

    def setupProfileDir(self):
        self.clearRemoteDir(self.profileDir)
        if self.interactive or self.singleFile:
            self.log.info("profile dir is %s" % self.profileDir)
        return self.profileDir

    def setupMozinfoJS(self):
        local = tempfile.mktemp()
        mozinfo.output_to_file(local)
        mozInfoJSPath = remoteJoin(self.profileDir, "mozinfo.json")
        self.device.pushFile(local, mozInfoJSPath)
        os.remove(local)
        return mozInfoJSPath

    def logCommand(self, name, completeCmd, testdir):
        self.log.info("%s | full command: %r" % (name, completeCmd))
        self.log.info("%s | current directory: %r" % (name, self.remoteHere))
        self.log.info("%s | environment: %s" % (name, self.env))

    def getHeadAndTailFiles(self, test):
        """Override parent method to find files on remote device."""
        def sanitize_list(s, kind):
            for f in s.strip().split(' '):
                f = f.strip()
                if len(f) < 1:
                    continue

                path = remoteJoin(self.remoteHere, f)

                # skip check for file existence: the convenience of discovering
                # a missing file does not justify the time cost of the round trip
                # to the device

                yield path

        self.remoteHere = self.remoteForLocal(test['here'])

        return (list(sanitize_list(test['head'], 'head')),
                list(sanitize_list(test['tail'], 'tail')))

    def buildXpcsCmd(self):
        # change base class' paths to remote paths and use base class to build command
        self.xpcshell = remoteJoin(self.remoteBinDir, "xpcw")
        self.headJSPath = remoteJoin(self.remoteScriptsDir, 'head.js')
        self.httpdJSPath = remoteJoin(self.remoteComponentsDir, 'httpd.js')
        self.httpdManifest = remoteJoin(self.remoteComponentsDir, 'httpd.manifest')
        self.testingModulesDir = self.remoteModulesDir
        self.testharnessdir = self.remoteScriptsDir
        xpcshell.XPCShellTestThread.buildXpcsCmd(self)
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

    def killTimeout(self, proc):
        self.kill(proc)

    def launchProcess(self, cmd, stdout, stderr, env, cwd, timeout=None):
        self.timedout = False
        cmd.insert(1, self.remoteHere)
        outputFile = "xpcshelloutput"
        with open(outputFile, 'w+') as f:
            try:
                self.shellReturnCode = self.device.shell(cmd, f, timeout=timeout+10)
            except mozdevice.DMError as e:
                if self.timedout:
                    # If the test timed out, there is a good chance the SUTagent also
                    # timed out and failed to return a return code, generating a
                    # DMError. Ignore the DMError to simplify the error report.
                    self.shellReturnCode = None
                    pass
                else:
                    raise e
        # The device manager may have timed out waiting for xpcshell.
        # Guard against an accumulation of hung processes by killing
        # them here. Note also that IPC tests may spawn new instances
        # of xpcshell.
        self.device.killProcess("xpcshell")
        return outputFile

    def checkForCrashes(self,
                        dump_directory,
                        symbols_path,
                        test_name=None):
        if not self.device.dirExists(self.remoteMinidumpDir):
            # The minidumps directory is automatically created when Fennec
            # (first) starts, so its lack of presence is a hint that
            # something went wrong.
            print "Automation Error: No crash directory (%s) found on remote device" % self.remoteMinidumpDir
            # Whilst no crash was found, the run should still display as a failure
            return True
        with mozfile.TemporaryDirectory() as dumpDir:
            self.device.getDirectory(self.remoteMinidumpDir, dumpDir)
            crashed = xpcshell.XPCShellTestThread.checkForCrashes(self, dumpDir, symbols_path, test_name)
            self.clearRemoteDir(self.remoteMinidumpDir)
        return crashed

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

    def clearRemoteDir(self, remoteDir):
        self.device.shellCheckOutput([self.remoteClearDirScript, remoteDir])

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

    def __init__(self, devmgr, options, log):
        xpcshell.XPCShellTests.__init__(self, log)

        # Add Android version (SDK level) to mozinfo so that manifest entries
        # can be conditional on android_version.
        androidVersion = devmgr.shellCheckOutput(['getprop', 'ro.build.version.sdk'])
        mozinfo.info['android_version'] = androidVersion

        self.localLib = options.localLib
        self.localBin = options.localBin
        self.options = options
        self.device = devmgr
        self.pathMapping = []
        self.remoteTestRoot = "%s/xpcshell" % self.device.deviceRoot
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
        self.remoteMinidumpDir = remoteJoin(self.remoteTestRoot, "minidumps")
        self.remoteClearDirScript = remoteJoin(self.remoteBinDir, "cleardir")
        self.profileDir = remoteJoin(self.remoteTestRoot, "p")
        self.remoteDebugger = options.debugger
        self.remoteDebuggerArgs = options.debuggerArgs
        self.testingModulesDir = options.testingModulesDir

        self.env = {}

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
        self.setupMinidumpDir()
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
            'remoteMinidumpDir': self.remoteMinidumpDir,
            'remoteClearDirScript': self.remoteClearDirScript,
        }
        if self.remoteAPK:
            self.mobileArgs['remoteAPK'] = self.remoteAPK

    def setLD_LIBRARY_PATH(self):
        self.env["LD_LIBRARY_PATH"] = self.remoteBinDir

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
        f.writelines([
            "cd $1\n",
            "echo xpcw: cd $1\n",
            "shift\n",
            "echo xpcw: xpcshell \"$@\"\n",
            "%s/xpcshell \"$@\"\n" % self.remoteBinDir])
        f.close()
        remoteWrapper = remoteJoin(self.remoteBinDir, "xpcw")
        self.device.pushFile(localWrapper, remoteWrapper)
        os.remove(localWrapper)

        # Removing and re-creating a directory is a common operation which
        # can be implemented more efficiently with a shell script.
        localWrapper = tempfile.mktemp()
        f = open(localWrapper, "w")
        # The directory may not exist initially, so rm may fail. 'rm -f' is not
        # supported on some Androids. Similarly, 'test' and 'if [ -d ]' are not
        # universally available, so we just ignore errors from rm.
        f.writelines([
            "#!/system/bin/sh\n",
            "rm -r \"$1\"\n",
            "mkdir \"$1\"\n"])
        f.close()
        self.device.pushFile(localWrapper, self.remoteClearDirScript)
        os.remove(localWrapper)

        self.device.chmodDir(self.remoteBinDir)

    def buildEnvironment(self):
        self.buildCoreEnvironment()
        self.setLD_LIBRARY_PATH()
        self.env["MOZ_LINKER_CACHE"] = self.remoteBinDir
        if self.options.localAPK and self.appRoot:
            self.env["GRE_HOME"] = self.appRoot
        self.env["XPCSHELL_TEST_PROFILE_DIR"] = self.profileDir
        self.env["TMPDIR"] = self.remoteTmpDir
        self.env["HOME"] = self.profileDir
        self.env["XPCSHELL_TEST_TEMP_DIR"] = self.remoteTmpDir
        self.env["XPCSHELL_MINIDUMP_DIR"] = self.remoteMinidumpDir
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
            try:
                self.device.shellCheckOutput(["mkdir", self.remoteBinDir]);
            except mozdevice.DMError:
                # Might get a permission error; try again as root, if available
                self.device.shellCheckOutput(["mkdir", self.remoteBinDir], root=True);
                self.device.shellCheckOutput(["chmod", "777", self.remoteBinDir], root=True);

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

        # The xpcshell binary is required for all tests. Additional binaries
        # are required for some tests. This list should be similar to
        # TEST_HARNESS_BINS in testing/mochitest/Makefile.in.
        binaries = ["xpcshell",
                    "ssltunnel",
                    "certutil",
                    "pk12util",
                    "BadCertServer",
                    "OCSPStaplingServer",
                    "GenerateOCSPResponse"]
        for fname in binaries:
            local = os.path.join(self.localBin, fname)
            if os.path.isfile(local):
                print >> sys.stderr, "Pushing %s.." % fname
                remoteFile = remoteJoin(self.remoteBinDir, fname)
                self.device.pushFile(local, remoteFile)
            else:
                print >> sys.stderr, "*** Expected binary %s not found in %s!" % (fname, self.localBin)

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
        if self.localBin is not None:
            szip = os.path.join(self.localBin, '..', 'host', 'bin', 'szip')
            if not os.path.exists(szip):
                # Tinderbox builds must run szip from the test package
                szip = os.path.join(self.localBin, 'host', 'szip')
            if not os.path.exists(szip):
                # If the test package doesn't contain szip, it means files
                # are not szipped in the test package.
                szip = None
        else:
            szip = None
        pushed_libs_count = 0
        if self.options.localAPK:
            try:
                dir = tempfile.mkdtemp()
                for info in self.localAPKContents.infolist():
                    if info.filename.endswith(".so"):
                        print >> sys.stderr, "Pushing %s.." % info.filename
                        remoteFile = remoteJoin(self.remoteBinDir, os.path.basename(info.filename))
                        self.localAPKContents.extract(info, dir)
                        localFile = os.path.join(dir, info.filename)
                        if szip:
                            try:
                                out = subprocess.check_output([szip, '-d', localFile], stderr=subprocess.STDOUT)
                            except CalledProcessError:
                                print >> sys.stderr, "Error calling %s on %s.." % (szip, localFile)
                                if out:
                                    print >> sys.stderr, out
                        self.device.pushFile(localFile, remoteFile)
                        pushed_libs_count += 1
            finally:
                shutil.rmtree(dir)
            return pushed_libs_count

        for file in os.listdir(self.localLib):
            if (file.endswith(".so")):
                print >> sys.stderr, "Pushing %s.." % file
                if 'libxul' in file:
                    print >> sys.stderr, "This is a big file, it could take a while."
                localFile = os.path.join(self.localLib, file)
                remoteFile = remoteJoin(self.remoteBinDir, file)
                if szip:
                    try:
                        out = subprocess.check_output([szip, '-d', localFile], stderr=subprocess.STDOUT)
                    except CalledProcessError:
                        print >> sys.stderr, "Error calling %s on %s.." % (szip, localFile)
                        if out:
                            print >> sys.stderr, out
                self.device.pushFile(localFile, remoteFile)
                pushed_libs_count += 1

        # Additional libraries may be found in a sub-directory such as "lib/armeabi-v7a"
        localArmLib = os.path.join(self.localLib, "lib")
        if os.path.exists(localArmLib):
            for root, dirs, files in os.walk(localArmLib):
                for file in files:
                    if (file.endswith(".so")):
                        print >> sys.stderr, "Pushing %s.." % file
                        localFile = os.path.join(root, file)
                        remoteFile = remoteJoin(self.remoteBinDir, file)
                        if szip:
                            try:
                                out = subprocess.check_output([szip, '-d', localFile], stderr=subprocess.STDOUT)
                            except CalledProcessError:
                                print >> sys.stderr, "Error calling %s on %s.." % (szip, localFile)
                                if out:
                                    print >> sys.stderr, out
                        self.device.pushFile(localFile, remoteFile)
                        pushed_libs_count += 1

        return pushed_libs_count

    def setupModules(self):
        if self.testingModulesDir:
            self.device.pushDir(self.testingModulesDir, self.remoteModulesDir)

    def setupTestDir(self):
        print 'pushing %s' % self.xpcDir
        try:
            # The tests directory can be quite large: 5000 files and growing!
            # Sometimes - like on a low-end aws instance running an emulator - the push
            # may exceed the default 5 minute timeout, so we increase it here to 10 minutes.
            self.device.pushDir(self.xpcDir, self.remoteScriptsDir, timeout=600, retryLimit=10)
        except TypeError:
            # Foopies have an older mozdevice ver without retryLimit
            self.device.pushDir(self.xpcDir, self.remoteScriptsDir)

    def setupMinidumpDir(self):
        if self.device.dirExists(self.remoteMinidumpDir):
            self.device.removeDir(self.remoteMinidumpDir)
        self.device.mkDir(self.remoteMinidumpDir)

    def buildTestList(self, test_tags=None, test_paths=None):
        xpcshell.XPCShellTests.buildTestList(self, test_tags=test_tags, test_paths=test_paths)
        uniqueTestPaths = set([])
        for test in self.alltests:
            uniqueTestPaths.add(test['here'])
        for testdir in uniqueTestPaths:
            abbrevTestDir = os.path.relpath(testdir, self.xpcDir)
            remoteScriptDir = remoteJoin(self.remoteScriptsDir, abbrevTestDir)
            self.pathMapping.append(PathMapping(testdir, remoteScriptDir))

def verifyRemoteOptions(parser, options):
    if options.localLib is None:
        if options.localAPK and options.objdir:
            for path in ['dist/fennec', 'fennec/lib']:
                options.localLib = os.path.join(options.objdir, path)
                if os.path.isdir(options.localLib):
                    break
            else:
                parser.error("Couldn't find local library dir, specify --local-lib-dir")
        elif options.objdir:
            options.localLib = os.path.join(options.objdir, 'dist/bin')
        elif os.path.isfile(os.path.join(here, '..', 'bin', 'xpcshell')):
            # assume tests are being run from a tests.zip
            options.localLib = os.path.abspath(os.path.join(here, '..', 'bin'))
        else:
            parser.error("Couldn't find local library dir, specify --local-lib-dir")

    if options.localBin is None:
        if options.objdir:
            for path in ['dist/bin', 'bin']:
                options.localBin = os.path.join(options.objdir, path)
                if os.path.isdir(options.localBin):
                    break
            else:
                parser.error("Couldn't find local binary dir, specify --local-bin-dir")
        elif os.path.isfile(os.path.join(here, '..', 'bin', 'xpcshell')):
            # assume tests are being run from a tests.zip
            options.localBin = os.path.abspath(os.path.join(here, '..', 'bin'))
        else:
            parser.error("Couldn't find local binary dir, specify --local-bin-dir")
    return options

class PathMapping:

    def __init__(self, localDir, remoteDir):
        self.local = localDir
        self.remote = remoteDir

def main():
    if sys.version_info < (2,7):
        print >>sys.stderr, "Error: You must use python version 2.7 or newer but less than 3.0"
        sys.exit(1)

    parser = parser_remote()
    options = parser.parse_args()
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

    options = verifyRemoteOptions(parser, options)
    log = commandline.setup_logging("Remote XPCShell",
                                    options,
                                    {"tbpl": sys.stdout})

    if options.dm_trans == "adb":
        if options.deviceIP:
            dm = mozdevice.DroidADB(options.deviceIP, options.devicePort, packageName=None, deviceRoot=options.remoteTestRoot)
        else:
            dm = mozdevice.DroidADB(packageName=None, deviceRoot=options.remoteTestRoot)
    else:
        if not options.deviceIP:
            print "Error: you must provide a device IP to connect to via the --device option"
            sys.exit(1)
        dm = mozdevice.DroidSUT(options.deviceIP, options.devicePort, deviceRoot=options.remoteTestRoot)

    if options.interactive and not options.testPath:
        print >>sys.stderr, "Error: You must specify a test filename in interactive mode!"
        sys.exit(1)

    if options.xpcshell is None:
        options.xpcshell = "xpcshell"

    xpcsh = XPCShellRemote(dm, options, log)

    # we don't run concurrent tests on mobile
    options.sequential = True

    if not xpcsh.runTests(testClass=RemoteXPCShellTestThread,
                          mobileArgs=xpcsh.mobileArgs,
                          **vars(options)):
        sys.exit(1)


if __name__ == '__main__':
    main()
