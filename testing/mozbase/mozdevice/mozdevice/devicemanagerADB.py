# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import subprocess
from devicemanager import DeviceManager, DMError, _pop_last_line
import re
import os
import tempfile
import time

class DeviceManagerADB(DeviceManager):

    def __init__(self, host=None, port=20701, retrylimit=5, packageName='fennec',
                 adbPath='adb', deviceSerial=None, deviceRoot=None):
        self.host = host
        self.port = port
        self.retrylimit = retrylimit
        self.retries = 0
        self._sock = None
        self.haveRootShell = False
        self.haveSu = False
        self.useRunAs = False
        self.useDDCopy = False
        self.useZip = False
        self.packageName = None
        self.tempDir = None
        self.deviceRoot = deviceRoot
        self.default_timeout = 300

        # the path to adb, or 'adb' to assume that it's on the PATH
        self.adbPath = adbPath

        # The serial number of the device to use with adb, used in cases
        # where multiple devices are being managed by the same adb instance.
        self.deviceSerial = deviceSerial

        if packageName == 'fennec':
            if os.getenv('USER'):
                self.packageName = 'org.mozilla.fennec_' + os.getenv('USER')
            else:
                self.packageName = 'org.mozilla.fennec_'
        elif packageName:
            self.packageName = packageName

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
        Executes shell command on device.

        cmd - Command string to execute
        outputfile - File to store output
        env - Environment to pass to exec command
        cwd - Directory to execute command from
        timeout - specified in seconds, defaults to 'default_timeout'
        root - Specifies whether command requires root privileges

        returns:
          success: Return code from command
          failure: None
        """
        # FIXME: this function buffers all output of the command into memory,
        # always. :(

        # If requested to run as root, check that we can actually do that
        if root and not self.haveRootShell and not self.haveSu:
            raise DMError("Shell command '%s' requested to run as root but root "
                          "is not available on this device. Root your device or "
                          "refactor the test/harness to not require root." %
                          self._escapedCommandLine(cmd))

        # Getting the return code is more complex than you'd think because adb
        # doesn't actually return the return code from a process, so we have to
        # capture the output to get it
        if root and not self.haveRootShell:
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
        args=[self.adbPath]
        if self.deviceSerial:
            args.extend(['-s', self.deviceSerial])
        args.extend(["shell", cmdline])
        proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        if not timeout:
            # We are asserting that all commands will complete in this time unless otherwise specified
            timeout = self.default_timeout

        timeout = int(timeout)
        start_time = time.time()
        ret_code = proc.poll()
        while ((time.time() - start_time) <= timeout) and ret_code == None:
            time.sleep(1)
            ret_code = proc.poll()
        if ret_code == None:
            proc.kill()
            raise DMError("Timeout exceeded for shell call")
        (stdout, stderr) = proc.communicate()
        outputfile.write(stdout.rstrip('\n'))

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

        returns:
          success: True
          failure: False
        """
        try:
            if (os.name == "nt"):
                destname = destname.replace('\\', '/')
            if (self.useRunAs):
                remoteTmpFile = self.getTempDir() + "/" + os.path.basename(localname)
                self._checkCmd(["push", os.path.realpath(localname), remoteTmpFile])
                if self.useDDCopy:
                    self._checkCmdAs(["shell", "dd", "if=" + remoteTmpFile, "of=" + destname])
                else:
                    self._checkCmdAs(["shell", "cp", remoteTmpFile, destname])
                self._checkCmd(["shell", "rm", remoteTmpFile])
            else:
                self._checkCmd(["push", os.path.realpath(localname), destname])
            if (self.isDir(destname)):
                destname = destname + "/" + os.path.basename(localname)
            return True
        except:
            return False

    def mkDir(self, name):
        """
        Creates a single directory on the device file system

        returns:
          success: directory name
          failure: None
        """
        try:
            result = self._runCmdAs(["shell", "mkdir", name]).stdout.read()
            if 'read-only file system' in result.lower():
                return None
            if 'file exists' in result.lower():
                return name
            return name
        except:
            return None

    def pushDir(self, localDir, remoteDir):
        """
        Push localDir from host to remoteDir on the device

        returns:
          success: remoteDir
          failure: None
        """
        # adb "push" accepts a directory as an argument, but if the directory
        # contains symbolic links, the links are pushed, rather than the linked
        # files; we either zip/unzip or push file-by-file to get around this
        # limitation
        try:
            if (not self.dirExists(remoteDir)):
                self.mkDirs(remoteDir+"/x")
            if (self.useZip):
                try:
                    localZip = tempfile.mktemp()+".zip"
                    remoteZip = remoteDir + "/adbdmtmp.zip"
                    subprocess.check_output(["zip", "-r", localZip, '.'], cwd=localDir)
                    self.pushFile(localZip, remoteZip)
                    os.remove(localZip)
                    data = self._runCmdAs(["shell", "unzip", "-o", remoteZip, "-d", remoteDir]).stdout.read()
                    self._checkCmdAs(["shell", "rm", remoteZip])
                    if (re.search("unzip: exiting", data) or re.search("Operation not permitted", data)):
                        raise Exception("unzip failed, or permissions error")
                except:
                    print "zip/unzip failure: falling back to normal push"
                    self.useZip = False
                    self.pushDir(localDir, remoteDir)
            else:
                for root, dirs, files in os.walk(localDir, followlinks=True):
                    relRoot = os.path.relpath(root, localDir)
                    for f in files:
                        localFile = os.path.join(root, f)
                        remoteFile = remoteDir + "/"
                        if (relRoot!="."):
                            remoteFile = remoteFile + relRoot + "/"
                        remoteFile = remoteFile + f
                        self.pushFile(localFile, remoteFile)
                    for d in dirs:
                        targetDir = remoteDir + "/"
                        if (relRoot!="."):
                            targetDir = targetDir + relRoot + "/"
                        targetDir = targetDir + d
                        if (not self.dirExists(targetDir)):
                            self.mkDir(targetDir)
            return remoteDir
        except:
            print "pushing " + localDir + " to " + remoteDir + " failed"
            return None

    def dirExists(self, dirname):
        """
        Checks if dirname exists and is a directory
        on the device file system

        returns:
          success: True
          failure: False
        """
        return self.isDir(dirname)

    # Because we always have / style paths we make this a lot easier with some
    # assumptions
    def fileExists(self, filepath):
        """
        Checks if filepath exists and is a file on
        the device file system

        returns:
          success: True
          failure: False
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

        returns:
          success: output of telnet
          failure: None
        """
        return self._runCmd(["shell", "rm", filename]).stdout.read()

    def _removeSingleDir(self, remoteDir):
        """
        Deletes a single empty directory

        returns:
          success: output of telnet
          failure: None
        """
        return self._runCmd(["shell", "rmdir", remoteDir]).stdout.read()

    def removeDir(self, remoteDir):
        """
        Does a recursive delete of directory on the device: rm -Rf remoteDir

        returns:
          success: output of telnet
          failure: None
        """
        out = ""
        if (self.isDir(remoteDir)):
            files = self.listFiles(remoteDir.strip())
            for f in files:
                if (self.isDir(remoteDir.strip() + "/" + f.strip())):
                    out += self.removeDir(remoteDir.strip() + "/" + f.strip())
                else:
                    out += self.removeFile(remoteDir.strip() + "/" + f.strip())
            out += self._removeSingleDir(remoteDir.strip())
        else:
            out += self.removeFile(remoteDir.strip())
        return out

    def isDir(self, remotePath):
        """
        Checks if remotePath is a directory on the device

        returns:
          success: True
          failure: False
        """
        p = self._runCmd(["shell", "ls", "-a", remotePath + '/'])

        data = p.stdout.readlines()
        if len(data) == 1:
            res = data[0]
            if "Not a directory" in res or "No such file or directory" in res:
                return False
        return True

    def listFiles(self, rootdir):
        """
        Lists files on the device rootdir, requires cd to directory first

        returns:
          success: array of filenames, ['file1', 'file2', ...]
          failure: None
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
            ret.append(list([els[1], els[len(els) - 1], els[0]]))
            proc =  p.stdout.readline()
        return ret

    def fireProcess(self, appname, failIfRunning=False):
        """
        DEPRECATED: Use shell() or launchApplication() for new code

        returns:
          success: pid
          failure: None
        """
        #strip out env vars
        parts = appname.split('"');
        if (len(parts) > 2):
            parts = parts[2:]
        return self.launchProcess(parts, failIfRunning)

    def launchProcess(self, cmd, outputFile = "process.txt", cwd = '', env = '', failIfRunning=False):
        """
        DEPRECATED: Use shell() or launchApplication() for new code

        returns:
          success: output filename
          failure: None
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

        external function
        returns:
          success: True
          failure: False
        """
        procs = self.getProcessList()
        didKillProcess = False
        for (pid, name, user) in procs:
            if name == appname:
                args = ["shell", "kill"]
                if forceKill:
                    args.append("-9")
                args.append(pid)
                p = self._runCmdAs(args)
                p.communicate()
                if p.returncode == 0:
                    didKillProcess = True

        return didKillProcess

    def catFile(self, remoteFile):
        """
        Returns the contents of remoteFile

        returns:
          success: filecontents, string
          failure: None
        """
        return self.pullFile(remoteFile)

    def _runPull(self, remoteFile, localFile):
        """
        Pulls remoteFile from device to host

        returns:
          success: path to localFile
          failure: None
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
                        and self.useRunAs):
                        # If we lack permissions to read but have run-as, then we should try
                        # to copy the file to a world-readable location first before attempting
                        # to pull it again.
                        remoteTmpFile = self.getTempDir() + "/" + os.path.basename(remoteFile)
                        self._checkCmdAs(["shell", "dd", "if=" + remoteFile, "of=" + remoteTmpFile])
                        self._checkCmdAs(["shell", "chmod", "777", remoteTmpFile])
                        self._runCmd(["pull",  remoteTmpFile, localFile]).stdout.read()
                        # Clean up temporary file
                        self._checkCmdAs(["shell", "rm", remoteTmpFile])
            return localFile
        except (OSError, ValueError):
            return None

    def pullFile(self, remoteFile):
        """
        Returns contents of remoteFile using the "pull" command.

        returns:
          success: output of pullfile, string
          failure: None
        """
        # TODO: add debug flags and allow for printing stdout
        localFile = tempfile.mkstemp()[1]
        localFile = self._runPull(remoteFile, localFile)

        if localFile is None:
            print 'Automation Error: failed to pull file %s!' % remoteFile
            return None

        f = open(localFile, 'r')
        ret = f.read()
        f.close()
        os.remove(localFile)
        return ret

    def getFile(self, remoteFile, localFile = 'temp.txt'):
        """
        Copy file from device (remoteFile) to host (localFile)

        returns:
          success: contents of file, string
          failure: None
        """
        try:
            contents = self.pullFile(remoteFile)
        except:
            return None

        if contents is None:
            return None

        fhandle = open(localFile, 'wb')
        fhandle.write(contents)
        fhandle.close()
        return contents


    def getDirectory(self, remoteDir, localDir, checkDir=True):
        """
        Copy directory structure from device (remoteDir) to host (localDir)

        returns:
          success: list of files, string
          failure: None
        """
        # checkDir has no affect in devicemanagerADB
        ret = []
        p = self._runCmd(["pull", remoteDir, localDir])
        p.stdout.readline()
        line = p.stdout.readline()
        while (line):
            els = line.split()
            f = els[len(els) - 1]
            i = f.find(localDir)
            if (i != -1):
                if (localDir[len(localDir) - 1] != '/'):
                    i = i + 1
                f = f[i + len(localDir):]
            i = f.find("/")
            if (i > 0):
                f = f[0:i]
            ret.append(f)
            line =  p.stdout.readline()
        #the last line is a summary
        if (len(ret) > 0):
            ret.pop()
        return ret



    def validateFile(self, remoteFile, localFile):
        """
        Checks if the remoteFile has the same md5 hash as the localFile

        returns:
          success: True/False
          failure: None
        """
        md5Remote = self._getRemoteHash(remoteFile)
        md5Local = self._getLocalHash(localFile)
        if md5Remote is None or md5Local is None:
            return None
        return md5Remote == md5Local

    def _getRemoteHash(self, remoteFile):
        """
        Return the md5 sum of a file on the device

        returns:
          success: MD5 hash for given filename
          failure: None
        """
        localFile = tempfile.mkstemp()[1]
        localFile = self._runPull(remoteFile, localFile)

        if localFile is None:
            return None

        md5 = self._getLocalHash(localFile)
        os.remove(localFile)

        return md5

    # setup the device root and cache its value
    def _setupDeviceRoot(self):
        # if self.deviceRoot is already set, create it if necessary, and use it
        if self.deviceRoot:
            if not self.dirExists(self.deviceRoot):
                if not self.mkDir(self.deviceRoot):
                    raise DMError("Unable to create device root %s" % self.deviceRoot)
            return

        # /mnt/sdcard/tests is preferred to /data/local/tests, but this can be
        # over-ridden by creating /data/local/tests
        testRoot = "/data/local/tests"
        if (self.dirExists(testRoot)):
            self.deviceRoot = testRoot
            return

        for (basePath, subPath) in [('/mnt/sdcard', 'tests'),
                                                                ('/data/local', 'tests')]:
            if self.dirExists(basePath):
                testRoot = os.path.join(basePath, subPath)
                if self.mkDir(testRoot):
                    self.deviceRoot = testRoot
                    return

        raise DMError("Unable to set up device root as /mnt/sdcard/tests "
                                    "or /data/local/tests")

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

        returns:
          success: path for device root
          failure: None
        """
        return self.deviceRoot

    def getTempDir(self):
        """
        Gets the temporary directory we are using on this device
        base on our device root, ensuring also that it exists.

        returns:
          success: path for temporary directory
          failure: None
        """
        # Cache result to speed up operations depending
        # on the temporary directory.
        if self.tempDir == None:
            self.tempDir = self.getDeviceRoot() + "/tmp"
            if (not self.dirExists(self.tempDir)):
                return self.mkDir(self.tempDir)

        return self.tempDir

    def getAppRoot(self, packageName):
        """
        Returns the app root directory
        E.g /tests/fennec or /tests/firefox

        returns:
          success: path for app root
          failure: None
        """
        devroot = self.getDeviceRoot()
        if (devroot == None):
            return None

        if (packageName and self.dirExists('/data/data/' + packageName)):
            self.packageName = packageName
            return '/data/data/' + packageName
        elif (self.packageName and self.dirExists('/data/data/' + self.packageName)):
            return '/data/data/' + self.packageName

        # Failure (either not installed or not a recognized platform)
        print "devicemanagerADB: getAppRoot failed"
        return None

    def reboot(self, wait = False, **kwargs):
        """
        Reboots the device

        returns:
          success: status from test agent
          failure: None
        """
        ret = self._runCmd(["reboot"]).stdout.read()
        if (not wait):
            return "Success"
        countdown = 40
        while (countdown > 0):
            countdown
            try:
                self._checkCmd(["wait-for-device", "shell", "ls", "/sbin"])
                return ret
            except:
                try:
                    self._checkCmd(["root"])
                except:
                    time.sleep(1)
                    print "couldn't get root"
        return "Success"

    def updateApp(self, appBundlePath, **kwargs):
        """
        Updates the application on the device.
        appBundlePath - path to the application bundle on the device

        returns:
          success: text status from command or callback server
          failure: None
        """
        return self._runCmd(["install", "-r", appBundlePath]).stdout.read()

    def getCurrentTime(self):
        """
        Returns device time in milliseconds since the epoch

        returns:
          success: time in ms
          failure: None
        """
        timestr = self._runCmd(["shell", "date", "+%s"]).stdout.read().strip()
        if (not timestr or not timestr.isdigit()):
            return None
        return str(int(timestr)*1000)

    def getInfo(self, directive=None):
        """
        Returns information about the device:
        Directive indicates the information you want to get, your choices are:
          os - name of the os
          id - unique id of the device
          uptime - uptime of the device
          systime - system time of the device
          screen - screen resolution
          memory - memory stats
          process - list of running processes (same as ps)
          disk - total, free, available bytes on disk
          power - power status (charge, battery temp)
          all - all of them - or call it with no parameters to get all the information
        ### Note that uptimemillis is NOT supported, as there is no way to get this
        ### data from the shell.

        returns:
           success: dict of info strings by directive name
           failure: {}
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
        installPath - ignored, but used for compatibility with SUTAgent

        returns:
          success: None
          failure: DMError exception thrown
        """
        data = self._runCmd(["uninstall", appName]).stdout.read().strip()
        status = data.split('\n')[0].strip()
        if status == 'Success':
            return
        raise DMError("uninstall failed for %s" % appName)

    def uninstallAppAndReboot(self, appName, installPath=None):
        """
        Uninstalls the named application from device and causes a reboot
        appName - the name of the application (e.g org.mozilla.fennec)
        installPath - ignored, but used for compatibility with SUTAgent

        returns:
          success: None
          failure: DMError exception thrown
        """
        results = self.uninstallApp(appName)
        self.reboot()
        return

    def _runCmd(self, args):
        """
        Runs a command using adb

        returns:
          returncode from subprocess.Popen
        """
        finalArgs = [self.adbPath]
        if self.deviceSerial:
            finalArgs.extend(['-s', self.deviceSerial])
        # use run-as to execute commands as the package we're testing if
        # possible
        if not self.haveRootShell and self.useRunAs and args[0] == "shell" and args[1] != "run-as":
            args.insert(1, "run-as")
            args.insert(2, self.packageName)
        finalArgs.extend(args)
        return subprocess.Popen(finalArgs, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    def _runCmdAs(self, args):
        """
        Runs a command using adb
        If self.useRunAs is True, the command is run-as user specified in self.packageName

        returns:
          returncode from subprocess.Popen
        """
        if self.useRunAs:
            args.insert(1, "run-as")
            args.insert(2, self.packageName)
        return self._runCmd(args)

    # timeout is specified in seconds, and if no timeout is given,
    # we will run until we hit the default_timeout specified in the __init__
    def _checkCmd(self, args, timeout=None):
        """
        Runs a command using adb and waits for the command to finish.
        If timeout is specified, the process is killed after <timeout> seconds.

        returns:
          returncode from subprocess.Popen
        """
        # use run-as to execute commands as the package we're testing if
        # possible
        finalArgs = [self.adbPath]
        if self.deviceSerial:
            finalArgs.extend(['-s', self.deviceSerial])
        if not self.haveRootShell and self.useRunAs and args[0] == "shell" and args[1] != "run-as":
            args.insert(1, "run-as")
            args.insert(2, self.packageName)
        finalArgs.extend(args)
        if not timeout:
            # We are asserting that all commands will complete in this time unless otherwise specified
            timeout = self.default_timeout

        timeout = int(timeout)
        proc = subprocess.Popen(finalArgs)
        start_time = time.time()
        ret_code = proc.poll()
        while ((time.time() - start_time) <= timeout) and ret_code == None:
            time.sleep(1)
            ret_code = proc.poll()
        if ret_code == None:
            proc.kill()
            raise DMError("Timeout exceeded for _checkCmd call")
        return ret_code

    def _checkCmdAs(self, args, timeout=None):
        """
        Runs a command using adb and waits for command to finish
        If self.useRunAs is True, the command is run-as user specified in self.packageName
        If timeout is specified, the process is killed after <timeout> seconds

        returns:
          returncode from subprocess.Popen
        """
        if (self.useRunAs):
            args.insert(1, "run-as")
            args.insert(2, self.packageName)
        return self._checkCmd(args, timeout)

    def chmodDir(self, remoteDir, mask="777"):
        """
        Recursively changes file permissions in a directory

        returns:
          success: True
          failure: False
        """
        if (self.isDir(remoteDir)):
            files = self.listFiles(remoteDir.strip())
            for f in files:
                remoteEntry = remoteDir.strip() + "/" + f.strip()
                if (self.isDir(remoteEntry)):
                    self.chmodDir(remoteEntry)
                else:
                    self._checkCmdAs(["shell", "chmod", mask, remoteEntry])
                    print "chmod " + remoteEntry
            self._checkCmdAs(["shell", "chmod", mask, remoteDir])
            print "chmod " + remoteDir
        else:
            self._checkCmdAs(["shell", "chmod", mask, remoteDir.strip()])
            print "chmod " + remoteDir.strip()
        return True

    def _verifyADB(self):
        """
        Check to see if adb itself can be executed.
        """
        if self.adbPath != 'adb':
            if not os.access(self.adbPath, os.X_OK):
                raise DMError("invalid adb path, or adb not executable: %s", self.adbPath)

        try:
            self._checkCmd(["version"])
        except os.error, err:
            raise DMError("unable to execute ADB (%s): ensure Android SDK is installed and adb is in your $PATH" % err)
        except subprocess.CalledProcessError:
            raise DMError("unable to execute ADB: ensure Android SDK is installed and adb is in your $PATH")

    def _verifyDevice(self):
        # If there is a device serial number, see if adb is connected to it
        if self.deviceSerial:
            deviceStatus = None
            proc = subprocess.Popen([self.adbPath, "devices"],
                                    stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT)
            for line in proc.stdout:
                m = re.match('(.+)?\s+(.+)$', line)
                if m:
                    if self.deviceSerial == m.group(1):
                        deviceStatus = m.group(2)
            if deviceStatus == None:
                raise DMError("device not found: %s" % self.deviceSerial)
            elif deviceStatus != "device":
                raise DMError("bad status for device %s: %s" % (self.deviceSerial, deviceStatus))

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
                self.useDDCopy = True
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
        self.useRunAs = False
        devroot = self.getDeviceRoot()
        if (self.packageName and self._isCpAvailable() and devroot):
            tmpDir = self.getTempDir()

            # The problem here is that run-as doesn't cause a non-zero exit code
            # when failing because of a non-existent or non-debuggable package :(
            runAsOut = self._runCmd(["shell", "run-as", self.packageName, "mkdir", devroot + "/sanity"]).communicate()[0]
            if runAsOut.startswith("run-as:") and ("not debuggable" in runAsOut or "is unknown" in runAsOut):
                raise DMError("run-as failed sanity check")

            tmpfile = tempfile.NamedTemporaryFile()
            self._checkCmd(["push", tmpfile.name, tmpDir + "/tmpfile"])
            if self.useDDCopy:
                self._checkCmd(["shell", "run-as", self.packageName, "dd", "if=" + tmpDir + "/tmpfile", "of=" + devroot + "/sanity/tmpfile"])
            else:
                self._checkCmd(["shell", "run-as", self.packageName, "cp", tmpDir + "/tmpfile", devroot + "/sanity"])
            if (self.fileExists(devroot + "/sanity/tmpfile")):
                print "will execute commands via run-as " + self.packageName
                self.useRunAs = True
            self._checkCmd(["shell", "rm", devroot + "/tmp/tmpfile"])
            self._checkCmd(["shell", "run-as", self.packageName, "rm", "-r", devroot + "/sanity"])

    def _checkForRoot(self):
        # Check whether we _are_ root by default (some development boards work
        # this way, this is also the result of some relatively rare rooting
        # techniques)
        proc = self._runCmd(["shell", "id"])
        data = proc.stdout.read()
        if data.find('uid=0(root)') >= 0:
            self.haveRootShell = True
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
            self.haveSu = True

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
        self.useZip = False
        if (self._isUnzipAvailable() and self._isLocalZipAvailable()):
            print "will use zip to push directories"
            self.useZip = True
        else:
            raise DMError("zip not available")
