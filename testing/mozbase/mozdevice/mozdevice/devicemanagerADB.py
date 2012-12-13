# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import subprocess
from devicemanager import DeviceManager, DMError, _pop_last_line
import re
import os
import shutil
import tempfile
import time

class DeviceManagerADB(DeviceManager):

    _haveRootShell = False
    _haveSu = False
    _useRunAs = False
    _useDDCopy = False
    _useZip = False
    _logcatNeedsRoot = False
    _pollingInterval = 0.01
    _packageName = None
    _tempDir = None
    default_timeout = 300

    def __init__(self, host=None, port=20701, retrylimit=5, packageName='fennec',
                 adbPath='adb', deviceSerial=None, deviceRoot=None, **kwargs):
        self.host = host
        self.port = port
        self.retrylimit = retrylimit
        self.deviceRoot = deviceRoot

        # the path to adb, or 'adb' to assume that it's on the PATH
        self._adbPath = adbPath

        # The serial number of the device to use with adb, used in cases
        # where multiple devices are being managed by the same adb instance.
        self._deviceSerial = deviceSerial

        if packageName == 'fennec':
            if os.getenv('USER'):
                self._packageName = 'org.mozilla.fennec_' + os.getenv('USER')
            else:
                self._packageName = 'org.mozilla.fennec_'
        elif packageName:
            self._packageName = packageName

        # verify that we can run the adb command. can't continue otherwise
        self._verifyADB()

        # try to connect to the device over tcp/ip if we have a hostname
        if self.host:
            self._connectRemoteADB()

        # verify that we can connect to the device. can't continue
        self._verifyDevice()

        # set up device root
        self._setupDeviceRoot()

        # Some commands require root to work properly, even with ADB (e.g.
        # grabbing APKs out of /data). For these cases, we check whether
        # we're running as root. If that isn't true, check for the
        # existence of an su binary
        self._checkForRoot()

        # Can we use run-as? (not required)
        try:
            self._verifyRunAs()
        except DMError:
            pass

        # can we use zip to speed up some file operations? (currently not
        # required)
        try:
            self._verifyZip()
        except DMError:
            pass

    def __del__(self):
        if self.host:
            self._disconnectRemoteADB()

    def shell(self, cmd, outputfile, env=None, cwd=None, timeout=None, root=False):
        """
        Executes shell command on device. Returns exit code.

        cmd - Command string to execute
        outputfile - File to store output
        env - Environment to pass to exec command
        cwd - Directory to execute command from
        timeout - specified in seconds, defaults to 'default_timeout'
        root - Specifies whether command requires root privileges
        """
        # FIXME: this function buffers all output of the command into memory,
        # always. :(

        # If requested to run as root, check that we can actually do that
        if root and not self._haveRootShell and not self._haveSu:
            raise DMError("Shell command '%s' requested to run as root but root "
                          "is not available on this device. Root your device or "
                          "refactor the test/harness to not require root." %
                          self._escapedCommandLine(cmd))

        # Getting the return code is more complex than you'd think because adb
        # doesn't actually return the return code from a process, so we have to
        # capture the output to get it
        if root and not self._haveRootShell:
            cmdline = "su -c \"%s\"" % self._escapedCommandLine(cmd)
        else:
            cmdline = self._escapedCommandLine(cmd)
        cmdline += "; echo $?"

        # prepend cwd and env to command if necessary
        if cwd:
            cmdline = "cd %s; %s" % (cwd, cmdline)
        if env:
            envstr = '; '.join(map(lambda x: 'export %s=%s' % (x[0], x[1]), env.iteritems()))
            cmdline = envstr + "; " + cmdline

        # all output should be in stdout
        args=[self._adbPath]
        if self._deviceSerial:
            args.extend(['-s', self._deviceSerial])
        args.extend(["shell", cmdline])

        procOut = tempfile.SpooledTemporaryFile()
        procErr = tempfile.SpooledTemporaryFile()
        proc = subprocess.Popen(args, stdout=procOut, stderr=procErr)

        if not timeout:
            # We are asserting that all commands will complete in this time unless otherwise specified
            timeout = self.default_timeout

        timeout = int(timeout)
        start_time = time.time()
        ret_code = proc.poll()
        while ((time.time() - start_time) <= timeout) and ret_code == None:
            time.sleep(self._pollingInterval)
            ret_code = proc.poll()
        if ret_code == None:
            proc.kill()
            raise DMError("Timeout exceeded for shell call")

        procOut.seek(0)
        outputfile.write(procOut.read().rstrip('\n'))
        procOut.close()
        procErr.close()

        lastline = _pop_last_line(outputfile)
        if lastline:
            m = re.search('([0-9]+)', lastline)
            if m:
                return_code = m.group(1)
                outputfile.seek(-2, 2)
                outputfile.truncate() # truncate off the return code
                return int(return_code)

        return None

    def _connectRemoteADB(self):
        self._checkCmd(["connect", self.host + ":" + str(self.port)])

    def _disconnectRemoteADB(self):
        self._checkCmd(["disconnect", self.host + ":" + str(self.port)])

    def pushFile(self, localname, destname):
        """
        Copies localname from the host to destname on the device
        """
        # you might expect us to put the file *in* the directory in this case,
        # but that would be different behaviour from devicemanagerSUT. Throw
        # an exception so we have the same behaviour between the two
        # implementations
        if self.dirExists(destname):
            raise DMError("Attempted to push a file (%s) to a directory (%s)!" %
                          (localname, destname))

        if self._useRunAs:
            remoteTmpFile = self.getTempDir() + "/" + os.path.basename(localname)
            self._checkCmd(["push", os.path.realpath(localname), remoteTmpFile])
            if self._useDDCopy:
                self.shellCheckOutput(["dd", "if=" + remoteTmpFile, "of=" + destname])
            else:
                self.shellCheckOutput(["cp", remoteTmpFile, destname])
            self.shellCheckOutput(["rm", remoteTmpFile])
        else:
            self._checkCmd(["push", os.path.realpath(localname), destname])

    def mkDir(self, name):
        """
        Creates a single directory on the device file system
        """
        result = self._runCmdAs(["shell", "mkdir", name]).stdout.read()
        if 'read-only file system' in result.lower():
            raise DMError("Error creating directory: read only file system")

    def pushDir(self, localDir, remoteDir):
        """
        Push localDir from host to remoteDir on the device
        """
        # adb "push" accepts a directory as an argument, but if the directory
        # contains symbolic links, the links are pushed, rather than the linked
        # files; we either zip/unzip or re-copy the directory into a temporary
        # one to get around this limitation
        if not self.dirExists(remoteDir):
            self.mkDirs(remoteDir+"/x")
        if self._useZip:
            try:
                localZip = tempfile.mktemp() + ".zip"
                remoteZip = remoteDir + "/adbdmtmp.zip"
                subprocess.Popen(["zip", "-r", localZip, '.'], cwd=localDir,
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE).communicate()
                self.pushFile(localZip, remoteZip)
                os.remove(localZip)
                data = self._runCmdAs(["shell", "unzip", "-o", remoteZip, "-d", remoteDir]).stdout.read()
                self._checkCmdAs(["shell", "rm", remoteZip])
                if re.search("unzip: exiting", data) or re.search("Operation not permitted", data):
                    raise Exception("unzip failed, or permissions error")
            except:
                print "zip/unzip failure: falling back to normal push"
                self._useZip = False
                self.pushDir(localDir, remoteDir)
        else:
            tmpDir = tempfile.mkdtemp()
            # copytree's target dir must not already exist, so create a subdir
            tmpDirTarget = os.path.join(tmpDir, "tmp")
            shutil.copytree(localDir, tmpDirTarget)
            self._checkCmd(["push", tmpDirTarget, remoteDir])
            shutil.rmtree(tmpDir)

    def dirExists(self, remotePath):
        """
        Return True if remotePath is an existing directory on the device.
        """
        p = self._runCmd(["shell", "ls", "-a", remotePath + '/'])

        data = p.stdout.readlines()
        if len(data) == 1:
            res = data[0]
            if "Not a directory" in res or "No such file or directory" in res:
                return False
        return True

    def fileExists(self, filepath):
        """
        Return True if filepath exists and is a file on the device file system
        """
        p = self._runCmd(["shell", "ls", "-a", filepath])
        data = p.stdout.readlines()
        if (len(data) == 1):
            if (data[0].rstrip() == filepath):
                return True
        return False

    def removeFile(self, filename):
        """
        Removes filename from the device
        """
        if self.fileExists(filename):
            self._runCmd(["shell", "rm", filename])

    def _removeSingleDir(self, remoteDir):
        """
        Deletes a single empty directory
        """
        return self._runCmd(["shell", "rmdir", remoteDir]).stdout.read()

    def removeDir(self, remoteDir):
        """
        Does a recursive delete of directory on the device: rm -Rf remoteDir
        """
        if (self.dirExists(remoteDir)):
            self._runCmd(["shell", "rm", "-r", remoteDir])
        else:
            self.removeFile(remoteDir.strip())

    def listFiles(self, rootdir):
        """
        Lists files on the device rootdir

        returns array of filenames, ['file1', 'file2', ...]
        """
        p = self._runCmd(["shell", "ls", "-a", rootdir])
        data = p.stdout.readlines()
        data[:] = [item.rstrip('\r\n') for item in data]
        if (len(data) == 1):
            if (data[0] == rootdir):
                return []
            if (data[0].find("No such file or directory") != -1):
                return []
            if (data[0].find("Not a directory") != -1):
                return []
            if (data[0].find("Permission denied") != -1):
                return []
            if (data[0].find("opendir failed") != -1):
                return []
        return data

    def getProcessList(self):
        """
        Lists the running processes on the device

        returns:
          success: array of process tuples
          failure: []
        """
        p = self._runCmd(["shell", "ps"])
            # first line is the headers
        p.stdout.readline()
        proc = p.stdout.readline()
        ret = []
        while (proc):
            els = proc.split()
            ret.append(list([int(els[1]), els[len(els) - 1], els[0]]))
            proc =  p.stdout.readline()
        return ret

    def fireProcess(self, appname, failIfRunning=False):
        """
        Starts a process

        returns: pid

        DEPRECATED: Use shell() or launchApplication() for new code
        """
        #strip out env vars
        parts = appname.split('"');
        if (len(parts) > 2):
            parts = parts[2:]
        return self.launchProcess(parts, failIfRunning)

    def launchProcess(self, cmd, outputFile = "process.txt", cwd = '', env = '', failIfRunning=False):
        """
        Launches a process, redirecting output to standard out

        WARNING: Does not work how you expect on Android! The application's
        own output will be flushed elsewhere.

        DEPRECATED: Use shell() or launchApplication() for new code
        """
        if cmd[0] == "am":
            self._checkCmd(["shell"] + cmd)
            return outputFile

        acmd = ["shell", "am", "start", "-W"]
        cmd = ' '.join(cmd).strip()
        i = cmd.find(" ")
        # SUT identifies the URL by looking for :\\ -- another strategy to consider
        re_url = re.compile('^[http|file|chrome|about].*')
        last = cmd.rfind(" ")
        uri = ""
        args = ""
        if re_url.match(cmd[last:].strip()):
            args = cmd[i:last].strip()
            uri = cmd[last:].strip()
        else:
            args = cmd[i:].strip()
        acmd.append("-n")
        acmd.append(cmd[0:i] + "/.App")
        if args != "":
            acmd.append("--es")
            acmd.append("args")
            acmd.append(args)
        if env != '' and env != None:
            envCnt = 0
            # env is expected to be a dict of environment variables
            for envkey, envval in env.iteritems():
                acmd.append("--es")
                acmd.append("env" + str(envCnt))
                acmd.append(envkey + "=" + envval);
                envCnt += 1
        if uri != "":
            acmd.append("-d")
            acmd.append(''.join(['\'',uri, '\'']));
        print acmd
        self._checkCmd(acmd)
        return outputFile

    def killProcess(self, appname, forceKill=False):
        """
        Kills the process named appname.

        If forceKill is True, process is killed regardless of state
        """
        procs = self.getProcessList()
        for (pid, name, user) in procs:
            if name == appname:
                args = ["shell", "kill"]
                if forceKill:
                    args.append("-9")
                args.append(str(pid))
                p = self._runCmdAs(args)
                p.communicate()
                if p.returncode != 0:
                    raise DMError("Error killing process "
                                  "'%s': %s" % (appname, p.stdout.read()))

    def catFile(self, remoteFile):
        """
        Returns the contents of remoteFile
        """
        return self.pullFile(remoteFile)

    def _runPull(self, remoteFile, localFile):
        """
        Pulls remoteFile from device to host
        """
        try:
            # First attempt to pull file regularly
            outerr = self._runCmd(["pull",  remoteFile, localFile]).communicate()

            # Now check stderr for errors
            if outerr[1]:
                errl = outerr[1].splitlines()
                if (len(errl) == 1):
                    if (((errl[0].find("Permission denied") != -1)
                        or (errl[0].find("does not exist") != -1))
                        and self._useRunAs):
                        # If we lack permissions to read but have run-as, then we should try
                        # to copy the file to a world-readable location first before attempting
                        # to pull it again.
                        remoteTmpFile = self.getTempDir() + "/" + os.path.basename(remoteFile)
                        self._checkCmdAs(["shell", "dd", "if=" + remoteFile, "of=" + remoteTmpFile])
                        self._checkCmdAs(["shell", "chmod", "777", remoteTmpFile])
                        self._runCmd(["pull",  remoteTmpFile, localFile]).stdout.read()
                        # Clean up temporary file
                        self._checkCmdAs(["shell", "rm", remoteTmpFile])
        except (OSError, ValueError):
            raise DMError("Error pulling remote file '%s' to '%s'" % (remoteFile, localFile))

    def pullFile(self, remoteFile):
        """
        Returns contents of remoteFile using the "pull" command.
        """
        # TODO: add debug flags and allow for printing stdout
        localFile = tempfile.mkstemp()[1]
        self._runPull(remoteFile, localFile)

        f = open(localFile, 'r')
        ret = f.read()
        f.close()
        os.remove(localFile)
        return ret

    def getFile(self, remoteFile, localFile):
        """
        Copy file from device (remoteFile) to host (localFile).
        """
        self._runPull(remoteFile, localFile)

    def getDirectory(self, remoteDir, localDir, checkDir=True):
        """
        Copy directory structure from device (remoteDir) to host (localDir)
        """
        self._runCmd(["pull", remoteDir, localDir])

    def validateFile(self, remoteFile, localFile):
        """
        Returns True if remoteFile has the same md5 hash as the localFile
        """
        md5Remote = self._getRemoteHash(remoteFile)
        md5Local = self._getLocalHash(localFile)
        if md5Remote is None or md5Local is None:
            return None
        return md5Remote == md5Local

    def _getRemoteHash(self, remoteFile):
        """
        Return the md5 sum of a file on the device
        """
        localFile = tempfile.mkstemp()[1]
        localFile = self._runPull(remoteFile, localFile)

        if localFile is None:
            return None

        md5 = self._getLocalHash(localFile)
        os.remove(localFile)

        return md5

    def _setupDeviceRoot(self):
        """
        setup the device root and cache its value
        """
        # if self.deviceRoot is already set, create it if necessary, and use it
        if self.deviceRoot:
            if not self.dirExists(self.deviceRoot):
                try:
                    self.mkDir(self.deviceRoot)
                except:
                    print "Unable to create device root %s" % self.deviceRoot
                    raise
            return

        # /mnt/sdcard/tests is preferred to /data/local/tests, but this can be
        # over-ridden by creating /data/local/tests
        testRoot = "/data/local/tests"
        if (self.dirExists(testRoot)):
            self.deviceRoot = testRoot
            return

        paths = [('/mnt/sdcard', 'tests'),
                 ('/data/local', 'tests')]
        for (basePath, subPath) in paths:
            if self.dirExists(basePath):
                testRoot = os.path.join(basePath, subPath)
                try:
                    self.mkDir(testRoot)
                    self.deviceRoot = testRoot
                    return
                except:
                    pass

        raise DMError("Unable to set up device root using paths: [%s]"
                        % ", ".join(["'%s'" % os.path.join(b, s) for b, s in paths]))

    def getDeviceRoot(self):
        """
        Gets the device root for the testing area on the device

        For all devices we will use / type slashes and depend on the device-agent
        to sort those out.  The agent will return us the device location where we
        should store things, we will then create our /tests structure relative to
        that returned path.
        Structure on the device is as follows:
        /tests
            /<fennec>|<firefox>  --> approot
            /profile
            /xpcshell
            /reftest
            /mochitest
        """
        return self.deviceRoot

    def getTempDir(self):
        """
        Return a temporary directory on the device

        Will also ensure that directory exists
        """
        # Cache result to speed up operations depending
        # on the temporary directory.
        if not self._tempDir:
            self._tempDir = self.getDeviceRoot() + "/tmp"
            self.mkDir(self._tempDir)

        return self._tempDir

    def getAppRoot(self, packageName):
        """
        Returns the app root directory

        E.g /tests/fennec or /tests/firefox
        """
        devroot = self.getDeviceRoot()
        if (devroot == None):
            return None

        if (packageName and self.dirExists('/data/data/' + packageName)):
            self._packageName = packageName
            return '/data/data/' + packageName
        elif (self._packageName and self.dirExists('/data/data/' + self._packageName)):
            return '/data/data/' + self._packageName

        # Failure (either not installed or not a recognized platform)
        raise DMError("Failed to get application root for: %s" % packageName)

    def reboot(self, wait = False, **kwargs):
        """
        Reboots the device
        """
        self._runCmd(["reboot"])
        if (not wait):
            return
        countdown = 40
        while (countdown > 0):
            self._checkCmd(["wait-for-device", "shell", "ls", "/sbin"])

    def updateApp(self, appBundlePath, **kwargs):
        """
        Updates the application on the device.

        appBundlePath - path to the application bundle on the device
        processName - used to end the process if the applicaiton is currently running (optional)
        destPath - Destination directory to where the application should be installed (optional)
        ipAddr - IP address to await a callback ping to let us know that the device has updated
                 properly - defaults to current IP.
        port - port to await a callback ping to let us know that the device has updated properly
               defaults to 30000, and counts up from there if it finds a conflict
        """
        return self._runCmd(["install", "-r", appBundlePath]).stdout.read()

    def getCurrentTime(self):
        """
        Returns device time in milliseconds since the epoch
        """
        timestr = self._runCmd(["shell", "date", "+%s"]).stdout.read().strip()
        if (not timestr or not timestr.isdigit()):
            raise DMError("Unable to get current time using date (got: '%s')" % timestr)
        return str(int(timestr)*1000)

    def getInfo(self, directive=None):
        """
        Returns information about the device

        Directive indicates the information you want to get, your choices are:
          os - name of the os
          id - unique id of the device
          uptime - uptime of the device
          uptimemillis - uptime of the device in milliseconds (NOT supported on all implementations)
          systime - system time of the device
          screen - screen resolution
          memory - memory stats
          process - list of running processes (same as ps)
          disk - total, free, available bytes on disk
          power - power status (charge, battery temp)
          all - all of them - or call it with no parameters to get all the information

        returns: dictionary of info strings by directive name
        """
        ret = {}
        if (directive == "id" or directive == "all"):
            ret["id"] = self._runCmd(["get-serialno"]).stdout.read()
        if (directive == "os" or directive == "all"):
            ret["os"] = self._runCmd(["shell", "getprop", "ro.build.display.id"]).stdout.read()
        if (directive == "uptime" or directive == "all"):
            utime = self._runCmd(["shell", "uptime"]).stdout.read()
            if (not utime):
                raise DMError("error getting uptime")
            utime = utime[9:]
            hours = utime[0:utime.find(":")]
            utime = utime[utime[1:].find(":") + 2:]
            minutes = utime[0:utime.find(":")]
            utime = utime[utime[1:].find(":") +  2:]
            seconds = utime[0:utime.find(",")]
            ret["uptime"] = ["0 days " + hours + " hours " + minutes + " minutes " + seconds + " seconds"]
        if (directive == "process" or directive == "all"):
            ret["process"] = self._runCmd(["shell", "ps"]).stdout.read()
        if (directive == "systime" or directive == "all"):
            ret["systime"] = self._runCmd(["shell", "date"]).stdout.read()
        print ret
        return ret

    def uninstallApp(self, appName, installPath=None):
        """
        Uninstalls the named application from device and DOES NOT cause a reboot

        appName - the name of the application (e.g org.mozilla.fennec)
        installPath - the path to where the application was installed (optional)
        """
        data = self._runCmd(["uninstall", appName]).stdout.read().strip()
        status = data.split('\n')[0].strip()
        if status != 'Success':
            raise DMError("uninstall failed for %s. Got: %s" % (appName, status))

    def uninstallAppAndReboot(self, appName, installPath=None):
        """
        Uninstalls the named application from device and causes a reboot

        appName - the name of the application (e.g org.mozilla.fennec)
        installPath - the path to where the application was installed (optional)
        """
        self.uninstallApp(appName)
        self.reboot()
        return

    def _runCmd(self, args):
        """
        Runs a command using adb

        returns: returncode from subprocess.Popen
        """
        finalArgs = [self._adbPath]
        if self._deviceSerial:
            finalArgs.extend(['-s', self._deviceSerial])
        # use run-as to execute commands as the package we're testing if
        # possible
        if not self._haveRootShell and self._useRunAs and \
                args[0] == "shell" and args[1] not in [ "run-as", "am" ]:
            args.insert(1, "run-as")
            args.insert(2, self._packageName)
        finalArgs.extend(args)
        return subprocess.Popen(finalArgs, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    def _runCmdAs(self, args):
        """
        Runs a command using adb
        If self._useRunAs is True, the command is run-as user specified in self._packageName

        returns: returncode from subprocess.Popen
        """
        if self._useRunAs:
            args.insert(1, "run-as")
            args.insert(2, self._packageName)
        return self._runCmd(args)

    # timeout is specified in seconds, and if no timeout is given,
    # we will run until we hit the default_timeout specified in the __init__
    def _checkCmd(self, args, timeout=None):
        """
        Runs a command using adb and waits for the command to finish.
        If timeout is specified, the process is killed after <timeout> seconds.

        returns: returncode from subprocess.Popen
        """
        # use run-as to execute commands as the package we're testing if
        # possible
        finalArgs = [self._adbPath]
        if self._deviceSerial:
            finalArgs.extend(['-s', self._deviceSerial])
        if not self._haveRootShell and self._useRunAs and \
                args[0] == "shell" and args[1] not in [ "run-as", "am" ]:
            args.insert(1, "run-as")
            args.insert(2, self._packageName)
        finalArgs.extend(args)
        if not timeout:
            # We are asserting that all commands will complete in this time unless otherwise specified
            timeout = self.default_timeout

        timeout = int(timeout)
        proc = subprocess.Popen(finalArgs)
        start_time = time.time()
        ret_code = proc.poll()
        while ((time.time() - start_time) <= timeout) and ret_code == None:
            time.sleep(self._pollingInterval)
            ret_code = proc.poll()
        if ret_code == None:
            proc.kill()
            raise DMError("Timeout exceeded for _checkCmd call")
        return ret_code

    def _checkCmdAs(self, args, timeout=None):
        """
        Runs a command using adb and waits for command to finish
        If self._useRunAs is True, the command is run-as user specified in self._packageName
        If timeout is specified, the process is killed after <timeout> seconds

        returns: returncode from subprocess.Popen
        """
        if (self._useRunAs):
            args.insert(1, "run-as")
            args.insert(2, self._packageName)
        return self._checkCmd(args, timeout)

    def chmodDir(self, remoteDir, mask="777"):
        """
        Recursively changes file permissions in a directory
        """
        if (self.dirExists(remoteDir)):
            files = self.listFiles(remoteDir.strip())
            for f in files:
                remoteEntry = remoteDir.strip() + "/" + f.strip()
                if (self.dirExists(remoteEntry)):
                    self.chmodDir(remoteEntry)
                else:
                    self._checkCmdAs(["shell", "chmod", mask, remoteEntry])
                    print "chmod " + remoteEntry
            self._checkCmdAs(["shell", "chmod", mask, remoteDir])
            print "chmod " + remoteDir
        else:
            self._checkCmdAs(["shell", "chmod", mask, remoteDir.strip()])
            print "chmod " + remoteDir.strip()

    def _verifyADB(self):
        """
        Check to see if adb itself can be executed.
        """
        if self._adbPath != 'adb':
            if not os.access(self._adbPath, os.X_OK):
                raise DMError("invalid adb path, or adb not executable: %s", self._adbPath)

        try:
            self._checkCmd(["version"])
        except os.error, err:
            raise DMError("unable to execute ADB (%s): ensure Android SDK is installed and adb is in your $PATH" % err)
        except subprocess.CalledProcessError:
            raise DMError("unable to execute ADB: ensure Android SDK is installed and adb is in your $PATH")

    def _verifyDevice(self):
        # If there is a device serial number, see if adb is connected to it
        if self._deviceSerial:
            deviceStatus = None
            proc = subprocess.Popen([self._adbPath, "devices"],
                                    stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT)
            for line in proc.stdout:
                m = re.match('(.+)?\s+(.+)$', line)
                if m:
                    if self._deviceSerial == m.group(1):
                        deviceStatus = m.group(2)
            if deviceStatus == None:
                raise DMError("device not found: %s" % self._deviceSerial)
            elif deviceStatus != "device":
                raise DMError("bad status for device %s: %s" % (self._deviceSerial, deviceStatus))

        # Check to see if we can connect to device and run a simple command
        try:
            self._checkCmd(["shell", "echo"])
        except subprocess.CalledProcessError:
            raise DMError("unable to connect to device: is it plugged in?")

    def _isCpAvailable(self):
        """
        Checks to see if cp command is installed
        """
        # Some Android systems may not have a cp command installed,
        # or it may not be executable by the user.
        data = self._runCmd(["shell", "cp"]).stdout.read()
        if (re.search('Usage', data)):
            return True
        else:
            data = self._runCmd(["shell", "dd", "-"]).stdout.read()
            if (re.search('unknown operand', data)):
                print "'cp' not found, but 'dd' was found as a replacement"
                self._useDDCopy = True
                return True
            print "unable to execute 'cp' on device; consider installing busybox from Android Market"
            return False

    def _verifyRunAs(self):
        # If a valid package name is available, and certain other
        # conditions are met, devicemanagerADB can execute file operations
        # via the "run-as" command, so that pushed files and directories
        # are created by the uid associated with the package, more closely
        # echoing conditions encountered by Fennec at run time.
        # Check to see if run-as can be used here, by verifying a
        # file copy via run-as.
        self._useRunAs = False
        devroot = self.getDeviceRoot()
        if (self._packageName and self._isCpAvailable() and devroot):
            tmpDir = self.getTempDir()

            # The problem here is that run-as doesn't cause a non-zero exit code
            # when failing because of a non-existent or non-debuggable package :(
            runAsOut = self._runCmd(["shell", "run-as", self._packageName, "mkdir", devroot + "/sanity"]).communicate()[0]
            if runAsOut.startswith("run-as:") and ("not debuggable" in runAsOut or "is unknown" in runAsOut):
                raise DMError("run-as failed sanity check")

            tmpfile = tempfile.NamedTemporaryFile()
            self._checkCmd(["push", tmpfile.name, tmpDir + "/tmpfile"])
            if self._useDDCopy:
                self._checkCmd(["shell", "run-as", self._packageName, "dd", "if=" + tmpDir + "/tmpfile", "of=" + devroot + "/sanity/tmpfile"])
            else:
                self._checkCmd(["shell", "run-as", self._packageName, "cp", tmpDir + "/tmpfile", devroot + "/sanity"])
            if (self.fileExists(devroot + "/sanity/tmpfile")):
                print "will execute commands via run-as " + self._packageName
                self._useRunAs = True
            self._checkCmd(["shell", "rm", devroot + "/tmp/tmpfile"])
            self._checkCmd(["shell", "run-as", self._packageName, "rm", "-r", devroot + "/sanity"])

    def _checkForRoot(self):
        # Check whether we _are_ root by default (some development boards work
        # this way, this is also the result of some relatively rare rooting
        # techniques)
        proc = self._runCmd(["shell", "id"])
        data = proc.stdout.read()
        if data.find('uid=0(root)') >= 0:
            self._haveRootShell = True
            # if this returns true, we don't care about su
            return

        # if root shell is not available, check if 'su' can be used to gain
        # root
        proc = self._runCmd(["shell", "su", "-c", "id"])

        # wait for response for maximum of 15 seconds, in case su prompts for a
        # password or triggers the Android SuperUser prompt
        start_time = time.time()
        retcode = None
        while (time.time() - start_time) <= 15 and retcode is None:
            retcode = proc.poll()
        if retcode is None: # still not terminated, kill
            proc.kill()

        data = proc.stdout.read()
        if data.find('uid=0(root)') >= 0:
            self._haveSu = True

    def _isUnzipAvailable(self):
        data = self._runCmdAs(["shell", "unzip"]).stdout.read()
        if (re.search('Usage', data)):
            return True
        else:
            return False

    def _isLocalZipAvailable(self):
        try:
            subprocess.check_call(["zip", "-?"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        except:
            return False
        return True

    def _verifyZip(self):
        # If "zip" can be run locally, and "unzip" can be run remotely, then pushDir
        # can use these to push just one file per directory -- a significant
        # optimization for large directories.
        self._useZip = False
        if (self._isUnzipAvailable() and self._isLocalZipAvailable()):
            print "will use zip to push directories"
            self._useZip = True
        else:
            raise DMError("zip not available")
