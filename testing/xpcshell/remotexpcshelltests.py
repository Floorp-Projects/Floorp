#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

from argparse import Namespace
import os
import posixpath
import shutil
import subprocess
import sys
import runxpcshelltests as xpcshell
import tempfile
from zipfile import ZipFile

from mozdevice import ADBAndroid, ADBDevice
import mozfile
import mozinfo
from mozlog import commandline

from xpcshellcommandline import parser_remote

here = os.path.dirname(os.path.abspath(__file__))


class RemoteXPCShellTestThread(xpcshell.XPCShellTestThread):
    def __init__(self, *args, **kwargs):
        xpcshell.XPCShellTestThread.__init__(self, *args, **kwargs)

        self.shellReturnCode = None
        # embed the mobile params from the harness into the TestThread
        mobileArgs = kwargs.get('mobileArgs')
        for key in mobileArgs:
            setattr(self, key, mobileArgs[key])

    def initDir(self, path, mask="777", timeout=None, root=True):
        """Initialize a directory by removing it if it exists, creating it
        and changing the permissions."""
        self.device.rm(path, recursive=True, force=True, timeout=timeout, root=root)
        self.device.mkdir(path, parents=True, timeout=timeout, root=root)
        self.device.chmod(path, recursive=True, mask=mask, timeout=timeout, root=root)

    def buildCmdTestFile(self, name):
        remoteDir = self.remoteForLocal(os.path.dirname(name))
        if remoteDir == self.remoteHere:
            remoteName = os.path.basename(name)
        else:
            remoteName = posixpath.join(remoteDir, os.path.basename(name))
        return ['-e', 'const _TEST_FILE = ["%s"];' %
                remoteName.replace('\\', '/')]

    def remoteForLocal(self, local):
        for mapping in self.pathMapping:
            if (os.path.abspath(mapping.local) == os.path.abspath(local)):
                return mapping.remote
        return local

    def setupTempDir(self):
        # make sure the temp dir exists
        self.initDir(self.remoteTmpDir)
        # env var is set in buildEnvironment
        return self.remoteTmpDir

    def setupPluginsDir(self):
        if not os.path.isdir(self.pluginsPath):
            return None

        # making sure tmp dir is set up
        self.setupTempDir()

        pluginsDir = posixpath.join(self.remoteTmpDir, "plugins")
        self.device.push(self.pluginsPath, pluginsDir)
        self.device.chmod(pluginsDir, root=True)
        if self.interactive:
            self.log.info("plugins dir is %s" % pluginsDir)
        return pluginsDir

    def setupProfileDir(self):
        self.initDir(self.profileDir)
        if self.interactive or self.singleFile:
            self.log.info("profile dir is %s" % self.profileDir)
        return self.profileDir

    def setupMozinfoJS(self):
        local = tempfile.mktemp()
        mozinfo.output_to_file(local)
        mozInfoJSPath = posixpath.join(self.profileDir, "mozinfo.json")
        self.device.push(local, mozInfoJSPath)
        self.device.chmod(mozInfoJSPath, root=True)
        os.remove(local)
        return mozInfoJSPath

    def logCommand(self, name, completeCmd, testdir):
        self.log.info("%s | full command: %r" % (name, completeCmd))
        self.log.info("%s | current directory: %r" % (name, self.remoteHere))
        self.log.info("%s | environment: %s" % (name, self.env))

    def getHeadFiles(self, test):
        """Override parent method to find files on remote device.

        Obtains lists of head- files.  Returns a list of head files.
        """
        def sanitize_list(s, kind):
            for f in s.strip().split(' '):
                f = f.strip()
                if len(f) < 1:
                    continue

                path = posixpath.join(self.remoteHere, f)

                # skip check for file existence: the convenience of discovering
                # a missing file does not justify the time cost of the round trip
                # to the device
                yield path

        self.remoteHere = self.remoteForLocal(test['here'])

        headlist = test.get('head', '')
        return list(sanitize_list(headlist, 'head'))

    def buildXpcsCmd(self):
        # change base class' paths to remote paths and use base class to build command
        self.xpcshell = posixpath.join(self.remoteBinDir, "xpcw")
        self.headJSPath = posixpath.join(self.remoteScriptsDir, 'head.js')
        self.httpdJSPath = posixpath.join(self.remoteComponentsDir, 'httpd.js')
        self.httpdManifest = posixpath.join(self.remoteComponentsDir, 'httpd.manifest')
        self.testingModulesDir = self.remoteModulesDir
        self.testharnessdir = self.remoteScriptsDir
        xpcshell.XPCShellTestThread.buildXpcsCmd(self)
        # remove "-g <dir> -a <dir>" and add "--greomni <apk>"
        del(self.xpcsCmd[1:5])
        if self.options['localAPK']:
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
        cmd = ADBDevice._escape_command_line(cmd)
        try:
            # env is ignored here since the environment has already been
            # set for the command via the pushWrapper method.
            adb_process = self.device.shell(cmd, timeout=timeout+10, root=True)
            output_file = adb_process.stdout_file
            self.shellReturnCode = adb_process.exitcode
        except Exception as e:
            if self.timedout:
                # If the test timed out, there is a good chance the shell
                # call also timed out and raised this Exception.
                # Ignore this exception to simplify the error report.
                self.shellReturnCode = None
            else:
                raise e
        # The device manager may have timed out waiting for xpcshell.
        # Guard against an accumulation of hung processes by killing
        # them here. Note also that IPC tests may spawn new instances
        # of xpcshell.
        self.device.pkill("xpcshell")
        return output_file

    def checkForCrashes(self,
                        dump_directory,
                        symbols_path,
                        test_name=None):
        if not self.device.is_dir(self.remoteMinidumpDir, root=True):
            # The minidumps directory is automatically created when Fennec
            # (first) starts, so its lack of presence is a hint that
            # something went wrong.
            print("Automation Error: No crash directory (%s) found on remote device" %
                  self.remoteMinidumpDir)
            # Whilst no crash was found, the run should still display as a failure
            return True
        with mozfile.TemporaryDirectory() as dumpDir:
            self.device.pull(self.remoteMinidumpDir, dumpDir)
            crashed = xpcshell.XPCShellTestThread.checkForCrashes(
                  self, dumpDir, symbols_path, test_name)
            self.initDir(self.remoteMinidumpDir)
        return crashed

    def communicate(self, proc):
        f = proc
        contents = f.read()
        f.close()
        return contents, ""

    def poll(self, proc):
        if not self.device.process_exist("xpcshell"):
            return self.getReturnCode(proc)
        # Process is still running
        return None

    def kill(self, proc):
        return self.device.pkill("xpcshell")

    def getReturnCode(self, proc):
        if self.shellReturnCode is not None:
            return self.shellReturnCode
        else:
            return -1

    def removeDir(self, dirname):
        try:
            self.device.rm(dirname, recursive=True, root=True)
        except Exception as e:
            self.log.warning(str(e))

    # TODO: consider creating a separate log dir.  We don't have the test file structure,
    # so we use filename.log.  Would rather see ./logs/filename.log
    def createLogFile(self, test, stdout):
        try:
            f = None
            filename = test.replace('\\', '/').split('/')[-1] + ".log"
            f = open(filename, "w")
            f.write(stdout)

        finally:
            if f is not None:
                f.close()


# A specialization of XPCShellTests that runs tests on an Android device.
class XPCShellRemote(xpcshell.XPCShellTests, object):

    def __init__(self, options, log):
        xpcshell.XPCShellTests.__init__(self, log)

        self.options = options
        verbose = False
        if options['log_tbpl_level'] == 'debug' or options['log_mach_level'] == 'debug':
            verbose = True
        self.device = ADBAndroid(adb=options['adbPath'] or 'adb',
                                 device=options['deviceSerial'],
                                 test_root=options['remoteTestRoot'],
                                 verbose=verbose)
        self.remoteTestRoot = posixpath.join(self.device.test_root, "xpc")
        # Add Android version (SDK level) to mozinfo so that manifest entries
        # can be conditional on android_version.
        mozinfo.info['android_version'] = self.device.version

        self.localLib = options['localLib']
        self.localBin = options['localBin']
        self.pathMapping = []
        # remoteBinDir contains xpcshell and its wrapper script, both of which must
        # be executable. Since +x permissions cannot usually be set on /mnt/sdcard,
        # and the test root may be on /mnt/sdcard, remoteBinDir is set to be on
        # /data/local, always.
        self.remoteBinDir = posixpath.join("/data", "local", "xpcb")
        # Terse directory names are used here ("c" for the components directory)
        # to minimize the length of the command line used to execute
        # xpcshell on the remote device. adb has a limit to the number
        # of characters used in a shell command, and the xpcshell command
        # line can be quite complex.
        self.remoteTmpDir = posixpath.join(self.remoteTestRoot, "tmp")
        self.remoteScriptsDir = self.remoteTestRoot
        self.remoteComponentsDir = posixpath.join(self.remoteTestRoot, "c")
        self.remoteModulesDir = posixpath.join(self.remoteTestRoot, "m")
        self.remoteMinidumpDir = posixpath.join(self.remoteTestRoot, "minidumps")
        self.profileDir = posixpath.join(self.remoteTestRoot, "p")
        self.remoteDebugger = options['debugger']
        self.remoteDebuggerArgs = options['debuggerArgs']
        self.testingModulesDir = options['testingModulesDir']

        self.env = {}

        if options['objdir']:
            self.xpcDir = os.path.join(options['objdir'], "_tests/xpcshell")
        elif os.path.isdir(os.path.join(here, 'tests')):
            self.xpcDir = os.path.join(here, 'tests')
        else:
            print("Couldn't find local xpcshell test directory", file=sys.stderr)
            sys.exit(1)

        if options['localAPK']:
            self.localAPKContents = ZipFile(options['localAPK'])
        if options['setup']:
            self.setupTestDir()
            self.setupUtilities()
            self.setupModules()
        self.initDir(self.remoteMinidumpDir)
        self.remoteAPK = None
        if options['localAPK']:
            self.remoteAPK = posixpath.join(self.remoteBinDir,
                                            os.path.basename(options['localAPK']))
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
        }
        if self.remoteAPK:
            self.mobileArgs['remoteAPK'] = self.remoteAPK

    def initDir(self, path, mask="777", timeout=None, root=True):
        """Initialize a directory by removing it if it exists, creating it
        and changing the permissions."""
        self.device.rm(path, recursive=True, force=True, timeout=timeout, root=root)
        self.device.mkdir(path, parents=True, timeout=timeout, root=root)
        self.device.chmod(path, recursive=True, mask=mask, timeout=timeout, root=root)

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
        remoteWrapper = posixpath.join(self.remoteBinDir, "xpcw")
        self.device.push(localWrapper, remoteWrapper)
        self.device.chmod(remoteWrapper, root=True)
        os.remove(localWrapper)

    def buildEnvironment(self):
        self.buildCoreEnvironment()
        self.setLD_LIBRARY_PATH()
        self.env["MOZ_LINKER_CACHE"] = self.remoteBinDir
        if self.options['localAPK'] and self.appRoot:
            self.env["GRE_HOME"] = self.appRoot
        self.env["XPCSHELL_TEST_PROFILE_DIR"] = self.profileDir
        self.env["TMPDIR"] = self.remoteTmpDir
        self.env["HOME"] = self.profileDir
        self.env["XPCSHELL_TEST_TEMP_DIR"] = self.remoteTmpDir
        self.env["XPCSHELL_MINIDUMP_DIR"] = self.remoteMinidumpDir
        if self.options['setup']:
            self.pushWrapper()

    def setAppRoot(self):
        # Determine the application root directory associated with the package
        # name used by the Fennec APK.
        self.appRoot = None
        packageName = None
        if self.options['localAPK']:
            try:
                packageName = self.localAPKContents.read("package-name.txt")
                if packageName:
                    self.appRoot = posixpath.join("/data", "data", packageName.strip())
            except Exception as detail:
                print("unable to determine app root: " + str(detail))
                pass
        return None

    def setupUtilities(self):
        self.initDir(self.remoteTmpDir)
        self.initDir(self.remoteBinDir)
        remotePrefDir = posixpath.join(self.remoteBinDir, "defaults", "pref")
        self.initDir(posixpath.join(remotePrefDir, "extra"))
        self.initDir(self.remoteComponentsDir)

        local = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'head.js')
        remoteFile = posixpath.join(self.remoteScriptsDir, "head.js")
        self.device.push(local, remoteFile)
        self.device.chmod(remoteFile, root=True)

        # The xpcshell binary is required for all tests. Additional binaries
        # are required for some tests. This list should be similar to
        # TEST_HARNESS_BINS in testing/mochitest/Makefile.in.
        binaries = ["xpcshell",
                    "ssltunnel",
                    "certutil",
                    "pk12util",
                    "BadCertServer",
                    "OCSPStaplingServer",
                    "GenerateOCSPResponse",
                    "SymantecSanctionsServer"]
        for fname in binaries:
            local = os.path.join(self.localBin, fname)
            if os.path.isfile(local):
                print("Pushing %s.." % fname, file=sys.stderr)
                remoteFile = posixpath.join(self.remoteBinDir, fname)
                self.device.push(local, remoteFile)
                self.device.chmod(remoteFile, root=True)
            else:
                print("*** Expected binary %s not found in %s!" %
                      (fname, self.localBin), file=sys.stderr)

        local = os.path.join(self.localBin, "components/httpd.js")
        remoteFile = posixpath.join(self.remoteComponentsDir, "httpd.js")
        self.device.push(local, remoteFile)
        self.device.chmod(remoteFile, root=True)

        local = os.path.join(self.localBin, "components/httpd.manifest")
        remoteFile = posixpath.join(self.remoteComponentsDir, "httpd.manifest")
        self.device.push(local, remoteFile)
        self.device.chmod(remoteFile, root=True)

        if self.options['localAPK']:
            remoteFile = posixpath.join(self.remoteBinDir,
                                        os.path.basename(self.options['localAPK']))
            self.device.push(self.options['localAPK'], remoteFile)
            self.device.chmod(remoteFile, root=True)

        self.pushLibs()

    def pushLibs(self):
        elfhack = None
        xrePath = self.options.get('xrePath')
        if xrePath:
            elfhack = os.path.join(xrePath, 'elfhack')
            if not os.path.exists(elfhack):
                elfhack = None
        pushed_libs_count = 0
        if self.options['localAPK']:
            try:
                dir = tempfile.mkdtemp()
                for info in self.localAPKContents.infolist():
                    if info.filename.endswith(".so"):
                        print("Pushing %s.." % info.filename, file=sys.stderr)
                        remoteFile = posixpath.join(self.remoteBinDir,
                                                    os.path.basename(info.filename))
                        self.localAPKContents.extract(info, dir)
                        localFile = os.path.join(dir, info.filename)
                        with open(localFile) as f:
                            # Decompress xz-compressed file.
                            if f.read(5)[1:] == '7zXZ':
                                cmd = ['xz', '-df', '--suffix', '.so', localFile]
                                subprocess.check_output(cmd)
                                # xz strips the ".so" file suffix.
                                os.rename(localFile[:-3], localFile)
                                # elfhack -r should provide better crash reports
                                if elfhack:
                                    cmd = [elfhack, '-r', localFile]
                                    subprocess.check_output(cmd)
                        self.device.push(localFile, remoteFile)
                        pushed_libs_count += 1
                        self.device.chmod(remoteFile, root=True)
            finally:
                shutil.rmtree(dir)
            return pushed_libs_count

        for file in os.listdir(self.localLib):
            if (file.endswith(".so")):
                print("Pushing %s.." % file, file=sys.stderr)
                if 'libxul' in file:
                    print("This is a big file, it could take a while.", file=sys.stderr)
                localFile = os.path.join(self.localLib, file)
                remoteFile = posixpath.join(self.remoteBinDir, file)
                self.device.push(localFile, remoteFile)
                pushed_libs_count += 1
                self.device.chmod(remoteFile, root=True)

        # Additional libraries may be found in a sub-directory such as "lib/armeabi-v7a"
        localArmLib = os.path.join(self.localLib, "lib")
        if os.path.exists(localArmLib):
            for root, dirs, files in os.walk(localArmLib):
                for file in files:
                    if (file.endswith(".so")):
                        print("Pushing %s.." % file, file=sys.stderr)
                        localFile = os.path.join(root, file)
                        remoteFile = posixpath.join(self.remoteBinDir, file)
                        self.device.push(localFile, remoteFile)
                        pushed_libs_count += 1
                        self.device.chmod(remoteFile, root=True)

        return pushed_libs_count

    def setupModules(self):
        if self.testingModulesDir:
            self.device.push(self.testingModulesDir, self.remoteModulesDir)
            self.device.chmod(self.remoteModulesDir, root=True)

    def setupTestDir(self):
        print('pushing %s' % self.xpcDir)
        # The tests directory can be quite large: 5000 files and growing!
        # Sometimes - like on a low-end aws instance running an emulator - the push
        # may exceed the default 5 minute timeout, so we increase it here to 10 minutes.
        self.initDir(self.remoteScriptsDir)
        self.device.push(self.xpcDir, self.remoteScriptsDir, timeout=600)
        self.device.chmod(self.remoteScriptsDir, recursive=True, root=True)

    def setupMinidumpDir(self):
        self.initDir(self.remoteMinidumpDir)

    def buildTestList(self, test_tags=None, test_paths=None, verify=False):
        xpcshell.XPCShellTests.buildTestList(
            self, test_tags=test_tags, test_paths=test_paths, verify=verify)
        uniqueTestPaths = set([])
        for test in self.alltests:
            uniqueTestPaths.add(test['here'])
        for testdir in uniqueTestPaths:
            abbrevTestDir = os.path.relpath(testdir, self.xpcDir)
            remoteScriptDir = posixpath.join(self.remoteScriptsDir, abbrevTestDir)
            self.pathMapping.append(PathMapping(testdir, remoteScriptDir))


def verifyRemoteOptions(parser, options):
    if isinstance(options, Namespace):
        options = vars(options)

    if options['localLib'] is None:
        if options['localAPK'] and options['objdir']:
            for path in ['dist/fennec', 'fennec/lib']:
                options['localLib'] = os.path.join(options['objdir'], path)
                if os.path.isdir(options['localLib']):
                    break
            else:
                parser.error("Couldn't find local library dir, specify --local-lib-dir")
        elif options['objdir']:
            options['localLib'] = os.path.join(options['objdir'], 'dist/bin')
        elif os.path.isfile(os.path.join(here, '..', 'bin', 'xpcshell')):
            # assume tests are being run from a tests.zip
            options['localLib'] = os.path.abspath(os.path.join(here, '..', 'bin'))
        else:
            parser.error("Couldn't find local library dir, specify --local-lib-dir")

    if options['localBin'] is None:
        if options['objdir']:
            for path in ['dist/bin', 'bin']:
                options['localBin'] = os.path.join(options['objdir'], path)
                if os.path.isdir(options['localBin']):
                    break
            else:
                parser.error("Couldn't find local binary dir, specify --local-bin-dir")
        elif os.path.isfile(os.path.join(here, '..', 'bin', 'xpcshell')):
            # assume tests are being run from a tests.zip
            options['localBin'] = os.path.abspath(os.path.join(here, '..', 'bin'))
        else:
            parser.error("Couldn't find local binary dir, specify --local-bin-dir")
    return options


class PathMapping:

    def __init__(self, localDir, remoteDir):
        self.local = localDir
        self.remote = remoteDir


def main():
    if sys.version_info < (2, 7):
        print("Error: You must use python version 2.7 or newer but less than 3.0", file=sys.stderr)
        sys.exit(1)

    parser = parser_remote()
    options = parser.parse_args()
    if not options.localAPK:
        for file in os.listdir(os.path.join(options.objdir, "dist")):
            if (file.endswith(".apk") and file.startswith("fennec")):
                options.localAPK = os.path.join(options.objdir, "dist")
                options.localAPK = os.path.join(options.localAPK, file)
                print("using APK: " + options.localAPK, file=sys.stderr)
                break
        else:
            print("Error: please specify an APK", file=sys.stderr)
            sys.exit(1)

    options = verifyRemoteOptions(parser, options)
    log = commandline.setup_logging("Remote XPCShell",
                                    options,
                                    {"tbpl": sys.stdout})

    if options['interactive'] and not options['testPath']:
        print("Error: You must specify a test filename in interactive mode!", file=sys.stderr)
        sys.exit(1)

    if options['xpcshell'] is None:
        options['xpcshell'] = "xpcshell"

    xpcsh = XPCShellRemote(options, log)

    # we don't run concurrent tests on mobile
    options['sequential'] = True

    if not xpcsh.runTests(options,
                          testClass=RemoteXPCShellTestThread,
                          mobileArgs=xpcsh.mobileArgs):
        sys.exit(1)


if __name__ == '__main__':
    main()
