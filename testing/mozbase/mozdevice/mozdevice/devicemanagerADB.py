# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import logging
import re
import os
import tempfile
import time
import traceback

from distutils import dir_util

from .devicemanager import DeviceManager, DMError
from mozprocess import ProcessHandler
import mozfile
from . import version_codes


class DeviceManagerADB(DeviceManager):
    """
    Implementation of DeviceManager interface that uses the Android "adb"
    utility to communicate with the device. Normally used to communicate
    with a device that is directly connected with the host machine over a USB
    port.
    """

    _haveRootShell = None
    _haveSu = None
    _suModifier = None
    _lsModifier = None
    _useZip = False
    _logcatNeedsRoot = False
    _pollingInterval = 0.01
    _packageName = None
    _tempDir = None
    _adb_version = None
    _sdk_version = None
    _noDevicesOutput = "no devices/emulators found"
    connected = False

    def __init__(self, host=None, port=5555, retryLimit=5, packageName='fennec',
                 adbPath=None, deviceSerial=None, deviceRoot=None,
                 logLevel=logging.ERROR, autoconnect=True, runAdbAsRoot=False,
                 serverHost=None, serverPort=None, **kwargs):
        DeviceManager.__init__(self, logLevel=logLevel,
                               deviceRoot=deviceRoot)
        self.host = host
        self.port = port
        self.retryLimit = retryLimit

        self._serverHost = serverHost
        self._serverPort = serverPort

        # the path to adb, or 'adb' to assume that it's on the PATH
        self._adbPath = adbPath or 'adb'

        # The serial number of the device to use with adb, used in cases
        # where multiple devices are being managed by the same adb instance.
        self._deviceSerial = deviceSerial

        # Some devices do no start adb as root, if allowed you can use
        # this to reboot adbd on the device as root automatically
        self._runAdbAsRoot = runAdbAsRoot

        if packageName == 'fennec':
            if os.getenv('USER'):
                self._packageName = 'org.mozilla.fennec_' + os.getenv('USER')
            else:
                self._packageName = 'org.mozilla.fennec_'
        elif packageName:
            self._packageName = packageName

        # verify that we can run the adb command. can't continue otherwise
        self._verifyADB()

        if autoconnect:
            self.connect()

    def connect(self):
        if not self.connected:
            # try to connect to the device over tcp/ip if we have a hostname
            if self.host:
                self._connectRemoteADB()

            # verify that we can connect to the device. can't continue
            self._verifyDevice()

            # Note SDK version
            try:
                proc = self._runCmd(["shell", "getprop", "ro.build.version.sdk"],
                                    timeout=self.short_timeout)
                self._sdk_version = int(proc.output[0])
            except (OSError, ValueError):
                self._sdk_version = 0
            self._logger.info("Detected Android sdk %d" % self._sdk_version)

            # Some commands require root to work properly, even with ADB (e.g.
            # grabbing APKs out of /data). For these cases, we check whether
            # we're running as root. If that isn't true, check for the
            # existence of an su binary
            self._checkForRoot()

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
        # FIXME: this function buffers all output of the command into memory,
        # always. :(

        # If requested to run as root, check that we can actually do that
        if root:
            if self._haveRootShell is None and self._haveSu is None:
                self._checkForRoot()
            if not self._haveRootShell and not self._haveSu:
                raise DMError(
                    "Shell command '%s' requested to run as root but root "
                    "is not available on this device. Root your device or "
                    "refactor the test/harness to not require root." %
                    self._escapedCommandLine(cmd))

        # Getting the return code is more complex than you'd think because adb
        # doesn't actually return the return code from a process, so we have to
        # capture the output to get it
        if root and self._haveSu:
            cmdline = "su %s \"%s\"" % (self._suModifier,
                                        self._escapedCommandLine(cmd))
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
        args = [self._adbPath]
        if self._serverHost is not None:
            args.extend(['-H', self._serverHost])
        if self._serverPort is not None:
            args.extend(['-P', str(self._serverPort)])
        if self._deviceSerial:
            args.extend(['-s', self._deviceSerial])
        args.extend(["shell", cmdline])

        def _timeout():
            self._logger.error("Timeout exceeded for shell call '%s'" % ' '.join(args))

        self._logger.debug("shell - command: %s" % ' '.join(args))
        proc = ProcessHandler(args, processOutputLine=self._log, onTimeout=_timeout)

        if not timeout:
            # We are asserting that all commands will complete in this time unless
            # otherwise specified
            timeout = self.default_timeout

        timeout = int(timeout)
        proc.run(timeout)
        proc.wait()
        output = proc.output

        if output:
            lastline = output[-1]
            if lastline:
                m = re.search('([0-9]+)', lastline)
                if m:
                    return_code = m.group(1)
                    for line in output:
                        outputfile.write(line + '\n')
                    outputfile.seek(-2, 2)
                    outputfile.truncate()  # truncate off the return code
                    return int(return_code)

        return None

    def forward(self, local, remote):
        """
        Forward socket connections.

        Forward specs are one of:
          tcp:<port>
          localabstract:<unix domain socket name>
          localreserved:<unix domain socket name>
          localfilesystem:<unix domain socket name>
          dev:<character device name>
          jdwp:<process pid> (remote only)
        """
        if not self._checkCmd(['forward', local, remote], timeout=self.short_timeout) == 0:
            raise DMError("Failed to forward socket connection.")

    def remove_forward(self, local=None):
        """
        Turn off forwarding of socket connection.
        """
        cmd = ['forward']
        if local is None:
            cmd.extend(['--remove-all'])
        else:
            cmd.extend(['--remove', local])
        if not self._checkCmd(cmd, timeout=self.short_timeout) == 0:
            raise DMError("Failed to remove connection forwarding.")

    def remount(self):
        "Remounts the /system partition on the device read-write."
        return self._checkCmd(['remount'], timeout=self.short_timeout)

    def devices(self):
        "Return a list of connected devices as (serial, status) tuples."
        proc = self._runCmd(['devices'])
        proc.output.pop(0)  # ignore first line of output
        devices = []
        for line in proc.output:
            result = re.match('(.*?)\t(.*)', line)
            if result:
                devices.append((result.group(1), result.group(2)))
        return devices

    def _connectRemoteADB(self):
        self._checkCmd(["connect", self.host + ":" + str(self.port)])

    def _disconnectRemoteADB(self):
        self._checkCmd(["disconnect", self.host + ":" + str(self.port)])

    def pushFile(self, localname, destname, retryLimit=None, createDir=True):
        # you might expect us to put the file *in* the directory in this case,
        # but that would be inconsistent with historical behavior.
        retryLimit = retryLimit or self.retryLimit
        if self.dirExists(destname):
            raise DMError("Attempted to push a file (%s) to a directory (%s)!" %
                          (localname, destname))
        if not os.access(localname, os.F_OK):
            raise DMError("File not found: %s" % localname)

        proc = self._runCmd(["push", os.path.realpath(localname), destname],
                            retryLimit=retryLimit)
        if proc.returncode != 0:
            raise DMError("Error pushing file %s -> %s; output: %s" %
                          (localname, destname, proc.output))

    def mkDir(self, name):
        result = self._runCmd(["shell", "mkdir", name], timeout=self.short_timeout).output
        if len(result) and 'read-only file system' in result[0].lower():
            raise DMError("Error creating directory: read only file system")

    def pushDir(self, localDir, remoteDir, retryLimit=None, timeout=None):
        # adb "push" accepts a directory as an argument, but if the directory
        # contains symbolic links, the links are pushed, rather than the linked
        # files; we either zip/unzip or re-copy the directory into a temporary
        # one to get around this limitation
        retryLimit = retryLimit or self.retryLimit
        if self._useZip:
            self.removeDir(remoteDir)
            self.mkDirs(remoteDir + "/x")
            try:
                localZip = tempfile.mktemp() + ".zip"
                remoteZip = remoteDir + "/adbdmtmp.zip"
                proc = ProcessHandler(["zip", "-r", localZip, '.'], cwd=localDir,
                                      processOutputLine=self._log)
                proc.run()
                proc.wait()
                self.pushFile(localZip, remoteZip, retryLimit=retryLimit, createDir=False)
                mozfile.remove(localZip)
                data = self._runCmd(["shell", "unzip", "-o", remoteZip,
                                     "-d", remoteDir]).output[0]
                self._checkCmd(["shell", "rm", remoteZip],
                               retryLimit=retryLimit, timeout=self.short_timeout)
                if re.search("unzip: exiting", data) or re.search("Operation not permitted", data):
                    raise Exception("unzip failed, or permissions error")
            except:
                self._logger.warning(traceback.format_exc())
                self._logger.warning("zip/unzip failure: falling back to normal push")
                self._useZip = False
                self.pushDir(localDir, remoteDir, retryLimit=retryLimit, timeout=timeout)
        else:
            localDir = os.path.normpath(localDir)
            remoteDir = os.path.normpath(remoteDir)
            tempParent = tempfile.mkdtemp()
            remoteName = os.path.basename(remoteDir)
            newLocal = os.path.join(tempParent, remoteName)
            dir_util.copy_tree(localDir, newLocal)
            # See do_sync_push in
            # https://android.googlesource.com/platform/system/core/+/master/adb/file_sync_client.cpp
            # Work around change in behavior in adb 1.0.36 where if
            # the remote destination directory exists, adb push will
            # copy the source directory *into* the destination
            # directory otherwise it will copy the source directory
            # *onto* the destination directory.
            if self._adb_version >= '1.0.36':
                remoteDir = '/'.join(remoteDir.rstrip('/').split('/')[:-1])
            try:
                if self._checkCmd(["push", newLocal, remoteDir],
                                  retryLimit=retryLimit, timeout=timeout):
                    raise DMError("failed to push %s (copy of %s) to %s" %
                                  (newLocal, localDir, remoteDir))
            except:
                raise
            finally:
                mozfile.remove(tempParent)

    def dirExists(self, remotePath):
        self._detectLsModifier()
        data = self._runCmd(["shell", "ls", self._lsModifier, remotePath + '/'],
                            timeout=self.short_timeout).output

        if len(data) == 1:
            res = data[0]
            if "Not a directory" in res or "No such file or directory" in res:
                return False
        return True

    def fileExists(self, filepath):
        self._detectLsModifier()
        data = self._runCmd(["shell", "ls", self._lsModifier, filepath],
                            timeout=self.short_timeout).output
        if len(data) == 1:
            foundpath = data[0].decode('utf-8').rstrip()
            if foundpath == filepath:
                return True
        return False

    def removeFile(self, filename):
        if self.fileExists(filename):
            self._checkCmd(["shell", "rm", filename], timeout=self.short_timeout)

    def removeDir(self, remoteDir):
        if self.dirExists(remoteDir):
            self._checkCmd(["shell", "rm", "-r", remoteDir], timeout=self.short_timeout)
        else:
            self.removeFile(remoteDir.strip())

    def moveTree(self, source, destination):
        self._checkCmd(["shell", "mv", source, destination], timeout=self.short_timeout)

    def copyTree(self, source, destination):
        self._checkCmd(["shell", "dd", "if=%s" % source, "of=%s" % destination])

    def listFiles(self, rootdir):
        self._detectLsModifier()
        data = self._runCmd(["shell", "ls", self._lsModifier, rootdir],
                            timeout=self.short_timeout).output
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
            if (data[0].find("Device or resource busy") != -1):
                return []
        return data

    def getProcessList(self):
        ret = []
        p = self._runCmd(["shell", "ps"], timeout=self.short_timeout)
        if not p or not p.output or len(p.output) < 1:
            return ret
        # first line is the headers
        p.output.pop(0)
        for proc in p.output:
            els = proc.split()
            # We need to figure out if this is "user pid name" or
            # "pid user vsz stat command"
            if els[1].isdigit():
                ret.append(list([int(els[1]), els[len(els) - 1], els[0]]))
            else:
                ret.append(list([int(els[0]), els[len(els) - 1], els[1]]))
        return ret

    def fireProcess(self, appname, failIfRunning=False):
        """
        Starts a process

        returns: pid

        DEPRECATED: Use shell() or launchApplication() for new code
        """
        # strip out env vars
        parts = appname.split('"')
        if (len(parts) > 2):
            parts = parts[2:]
        return self.launchProcess(parts, failIfRunning)

    def launchProcess(self, cmd, outputFile="process.txt", cwd='', env='', failIfRunning=False):
        """
        Launches a process, redirecting output to standard out

        WARNING: Does not work how you expect on Android! The application's
        own output will be flushed elsewhere.

        DEPRECATED: Use shell() or launchApplication() for new code
        """
        if cmd[0] == "am":
            self._checkCmd(["shell"] + cmd)
            return outputFile

        acmd = ["-W"]
        cmd = ' '.join(cmd).strip()
        i = cmd.find(" ")
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
        acmd.append(cmd[0:i] + "/org.mozilla.gecko.BrowserApp")
        if args != "":
            acmd.append("--es")
            acmd.append("args")
            acmd.append(args)
        if env != '' and env is not None:
            envCnt = 0
            # env is expected to be a dict of environment variables
            for envkey, envval in env.iteritems():
                acmd.append("--es")
                acmd.append("env" + str(envCnt))
                acmd.append(envkey + "=" + envval)
                envCnt += 1
        if uri != "":
            acmd.append("-d")
            acmd.append(uri)

        acmd = ["shell", ' '.join(map(lambda x: '"' + x + '"', ["am", "start"] + acmd))]
        self._logger.info(acmd)
        self._checkCmd(acmd)
        return outputFile

    def killProcess(self, appname, sig=None, native=False):
        if not native and not sig:
            try:
                self.shellCheckOutput(["am", "force-stop", appname], timeout=self.short_timeout)
            except:
                # no problem - will kill it instead
                self._logger.info("killProcess failed force-stop of %s" % appname)

        shell_args = ["shell"]
        if self._sdk_version >= version_codes.N:
            # Bug 1334613 - force use of root
            if self._haveRootShell is None and self._haveSu is None:
                self._checkForRoot()
            if not self._haveRootShell and not self._haveSu:
                raise DMError(
                    "killProcess '%s' requested to run as root but root "
                    "is not available on this device. Root your device or "
                    "refactor the test/harness to not require root." %
                    appname)
            if not self._haveRootShell:
                shell_args.extend(["su", self._suModifier])

        procs = self.getProcessList()
        for (pid, name, user) in procs:
            if name == appname:
                args = list(shell_args)
                args.append("kill")
                if sig:
                    args.append("-%d" % sig)
                args.append(str(pid))
                p = self._runCmd(args, timeout=self.short_timeout)
                if p.returncode != 0 and len(p.output) > 0 and \
                   'No such process' not in p.output[0]:
                    raise DMError("Error killing process "
                                  "'%s': %s" % (appname, p.output))

    def _runPull(self, remoteFile, localFile):
        """
        Pulls remoteFile from device to host
        """
        try:
            self._runCmd(["pull",  remoteFile, localFile])
        except (OSError, ValueError):
            raise DMError("Error pulling remote file '%s' to '%s'" % (remoteFile, localFile))

    def pullFile(self, remoteFile, offset=None, length=None):
        # TODO: add debug flags and allow for printing stdout
        with mozfile.NamedTemporaryFile() as tf:
            self._runPull(remoteFile, tf.name)
            # we need to reopen the file to get the written contents
            with open(tf.name) as tf2:
                # ADB pull does not support offset and length, but we can
                # instead read only the requested portion of the local file
                if offset is not None and length is not None:
                    tf2.seek(offset)
                    return tf2.read(length)
                elif offset is not None:
                    tf2.seek(offset)
                    return tf2.read()
                else:
                    return tf2.read()

    def getFile(self, remoteFile, localFile):
        self._runPull(remoteFile, localFile)

    def getDirectory(self, remoteDir, localDir, checkDir=True):
        localDir = os.path.normpath(localDir)
        remoteDir = os.path.normpath(remoteDir)
        copyRequired = False
        originalLocal = localDir
        if self._adb_version >= '1.0.36' and \
           os.path.isdir(localDir) and self.dirExists(remoteDir):
            # See do_sync_pull in
            # https://android.googlesource.com/platform/system/core/+/master/adb/file_sync_client.cpp
            # Work around change in behavior in adb 1.0.36 where if
            # the local destination directory exists, adb pull will
            # copy the source directory *into* the destination
            # directory otherwise it will copy the source directory
            # *onto* the destination directory.
            #
            # If the destination directory does exist, pull to its
            # parent directory. If the source and destination leaf
            # directory names are different, pull the source directory
            # into a temporary directory and then copy the temporary
            # directory onto the destination.
            localName = os.path.basename(localDir)
            remoteName = os.path.basename(remoteDir)
            if localName != remoteName:
                copyRequired = True
                tempParent = tempfile.mkdtemp()
                localDir = os.path.join(tempParent, remoteName)
            else:
                localDir = '/'.join(localDir.rstrip('/').split('/')[:-1])
        cmd = ["pull", remoteDir, localDir]
        proc = self._runCmd(cmd)
        if proc.returncode != 0:
            # Raise a DMError when the device is missing, but not when the
            # directory is empty or missing.
            output = ''.join(proc.output)
            if (self._noDevicesOutput in output or
                ("pulled" not in output and
                 "does not exist" not in output)):
                raise DMError("getDirectory() failed to pull %s: %s" %
                              (remoteDir, proc.output))
        if copyRequired:
            dir_util.copy_tree(localDir, originalLocal)
            mozfile.remove(tempParent)

    def validateFile(self, remoteFile, localFile):
        md5Remote = self._getRemoteHash(remoteFile)
        md5Local = self._getLocalHash(localFile)
        if md5Remote is None or md5Local is None:
            return None
        return md5Remote == md5Local

    def _getRemoteHash(self, remoteFile):
        """
        Return the md5 sum of a file on the device
        """
        with tempfile.NamedTemporaryFile() as f:
            self._runPull(remoteFile, f.name)

            return self._getLocalHash(f.name)

    def _setupDeviceRoot(self, deviceRoot):
        # user-specified device root, create it and return it
        if deviceRoot:
            self.mkDir(deviceRoot)
            return deviceRoot

        # we must determine the device root ourselves
        paths = [('/storage/sdcard0', 'tests'),
                 ('/storage/sdcard1', 'tests'),
                 ('/storage/sdcard', 'tests'),
                 ('/mnt/sdcard', 'tests'),
                 ('/sdcard', 'tests'),
                 ('/data/local', 'tests')]
        for (basePath, subPath) in paths:
            if self.dirExists(basePath):
                root = os.path.join(basePath, subPath)
                try:
                    self.mkDir(root)
                    return root
                except:
                    pass

        raise DMError("Unable to set up device root using paths: [%s]"
                      % ", ".join(["'%s'" % os.path.join(b, s) for b, s in paths]))

    def getTempDir(self):
        # Cache result to speed up operations depending
        # on the temporary directory.
        if not self._tempDir:
            self._tempDir = "%s/tmp" % self.deviceRoot
            self.mkDir(self._tempDir)

        return self._tempDir

    def reboot(self, wait=False, **kwargs):
        self._checkCmd(["reboot"])
        if wait:
            self._checkCmd(["wait-for-device"])
            if self._runAdbAsRoot:
                self._adb_root()
            self._checkCmd(["shell", "ls", "/sbin"], timeout=self.short_timeout)

    def updateApp(self, appBundlePath, **kwargs):
        return self._runCmd(["install", "-r", appBundlePath]).output

    def getCurrentTime(self):
        timestr = str(self._runCmd(["shell", "date", "+%s"], timeout=self.short_timeout).output[0])
        if (not timestr or not timestr.isdigit()):
            raise DMError("Unable to get current time using date (got: '%s')" % timestr)
        return int(timestr) * 1000

    def getInfo(self, directive=None):
        directive = directive or "all"
        ret = {}
        if directive == "id" or directive == "all":
            ret["id"] = self._runCmd(["get-serialno"], timeout=self.short_timeout).output[0]
        if directive == "os" or directive == "all":
            ret["os"] = self.shellCheckOutput(
                ["getprop", "ro.build.display.id"], timeout=self.short_timeout)
        if directive == "uptime" or directive == "all":
            uptime = self.shellCheckOutput(["uptime"], timeout=self.short_timeout)
            if not uptime:
                raise DMError("error getting uptime")
            m = re.match("up time: ((\d+) days, )*(\d{2}):(\d{2}):(\d{2})", uptime)
            if m:
                uptime = "%d days %d hours %d minutes %d seconds" % tuple(
                    [int(g or 0) for g in m.groups()[1:]])
            ret["uptime"] = uptime
        if directive == "process" or directive == "all":
            data = self.shellCheckOutput(["ps"], timeout=self.short_timeout)
            ret["process"] = data.split('\n')
        if directive == "systime" or directive == "all":
            ret["systime"] = self.shellCheckOutput(["date"], timeout=self.short_timeout)
        if directive == "memtotal" or directive == "all":
            out = self.shellCheckOutput(["cat", "/proc/meminfo"], timeout=self.short_timeout)
            out = out.replace('\r\n', '\n').splitlines(True)
            for line in out:
                parts = line.split(":")
                if len(parts) == 2 and parts[0] == "MemTotal":
                    ret["memtotal"] = parts[1].strip()
                    break
        if directive == "disk" or directive == "all":
            data = self.shellCheckOutput(
                ["df", "/data", "/system", "/sdcard"], timeout=self.short_timeout)
            ret["disk"] = data.split('\n')
        self._logger.debug("getInfo: %s" % ret)
        return ret

    def uninstallApp(self, appName, installPath=None):
        status = self._runCmd(["uninstall", appName]).output[0].strip()
        if status != 'Success':
            raise DMError("uninstall failed for %s. Got: %s" % (appName, status))

    def uninstallAppAndReboot(self, appName, installPath=None):
        self.uninstallApp(appName)
        self.reboot()

    def _runCmd(self, args, timeout=None, retryLimit=None):
        """
        Runs a command using adb
        If timeout is specified, the process is killed after <timeout> seconds.

        returns: instance of ProcessHandler
        """
        retryLimit = retryLimit or self.retryLimit
        finalArgs = [self._adbPath]
        if self._serverHost is not None:
            finalArgs.extend(['-H', self._serverHost])
        if self._serverPort is not None:
            finalArgs.extend(['-P', str(self._serverPort)])
        if self._deviceSerial:
            finalArgs.extend(['-s', self._deviceSerial])
        finalArgs.extend(args)
        self._logger.debug("_runCmd - command: %s" % ' '.join(finalArgs))
        if not timeout:
            timeout = self.default_timeout

        def _timeout():
            self._logger.error("Timeout exceeded for _runCmd call '%s'" % ' '.join(finalArgs))

        retries = 0
        proc = None
        while retries < retryLimit:
            proc = ProcessHandler(finalArgs, storeOutput=True,
                                  processOutputLine=self._log, onTimeout=_timeout)
            proc.run(timeout=timeout)
            proc.returncode = proc.wait()
            if proc.returncode is not None:
                break
            proc.kill()
            self._logger.warning("_runCmd failed for '%s'" % ' '.join(finalArgs))
            retries += 1
        if retries >= retryLimit:
            self._logger.warning("_runCmd exceeded all retries")
        return proc

    # timeout is specified in seconds, and if no timeout is given,
    # we will run until we hit the default_timeout specified in the __init__
    def _checkCmd(self, args, timeout=None, retryLimit=None):
        """
        Runs a command using adb and waits for the command to finish.
        If timeout is specified, the process is killed after <timeout> seconds.

        returns: returncode from process
        """
        retryLimit = retryLimit or self.retryLimit
        finalArgs = [self._adbPath]
        if self._serverHost is not None:
            finalArgs.extend(['-H', self._serverHost])
        if self._serverPort is not None:
            finalArgs.extend(['-P', str(self._serverPort)])
        if self._deviceSerial:
            finalArgs.extend(['-s', self._deviceSerial])
        finalArgs.extend(args)
        self._logger.debug("_checkCmd - command: %s" % ' '.join(finalArgs))
        if not timeout:
            # We are asserting that all commands will complete in this
            # time unless otherwise specified
            timeout = self.default_timeout

        def _timeout():
            self._logger.error("Timeout exceeded for _checkCmd call '%s'" % ' '.join(finalArgs))

        timeout = int(timeout)
        retries = 0
        while retries < retryLimit:
            proc = ProcessHandler(finalArgs, processOutputLine=self._log, onTimeout=_timeout)
            proc.run(timeout=timeout)
            ret_code = proc.wait()
            if ret_code is None:
                self._logger.error("Failed to launch %s (may retry)" % finalArgs)
                proc.kill()
                retries += 1
            else:
                if ret_code != 0:
                    self._logger.error("Non-zero return code (%d) from %s" % (ret_code, finalArgs))
                    self._logger.error("Output: %s" % proc.output)
                output = ''.join(proc.output)
                if self._noDevicesOutput in output:
                    raise DMError(self._noDevicesOutput)
                return ret_code

        raise DMError("Timeout exceeded for _checkCmd call after %d retries." % retries)

    def chmodDir(self, remoteDir, mask="777"):
        if (self.dirExists(remoteDir)):
            if '/sdcard' in remoteDir:
                self._logger.debug("chmod %s -- skipped (/sdcard)" % remoteDir)
            else:
                files = self.listFiles(remoteDir.strip())
                for f in files:
                    remoteEntry = remoteDir.strip() + "/" + f.strip()
                    if (self.dirExists(remoteEntry)):
                        self.chmodDir(remoteEntry)
                    else:
                        self._checkCmd(["shell", "chmod", mask, remoteEntry],
                                       timeout=self.short_timeout)
                        self._logger.info("chmod %s" % remoteEntry)
                self._checkCmd(["shell", "chmod", mask, remoteDir], timeout=self.short_timeout)
                self._logger.debug("chmod %s" % remoteDir)
        else:
            self._checkCmd(["shell", "chmod", mask, remoteDir.strip()], timeout=self.short_timeout)
            self._logger.debug("chmod %s" % remoteDir.strip())

    def _verifyADB(self):
        """
        Check to see if adb itself can be executed.
        """
        if self._adbPath != 'adb':
            if not os.access(self._adbPath, os.X_OK):
                raise DMError("invalid adb path, or adb not executable: %s" % self._adbPath)

        try:
            re_version = re.compile(r'Android Debug Bridge version (.*)')
            proc = self._runCmd(["version"], timeout=self.short_timeout)
            self._adb_version = re_version.match(proc.output[0]).group(1)
            self._logger.info("Detected adb %s" % self._adb_version)
        except os.error as err:
            raise DMError(
                "unable to execute ADB (%s): ensure Android SDK is installed "
                "and adb is in your $PATH" % err)

    def _verifyDevice(self):
        # If there is a device serial number, see if adb is connected to it
        if self._deviceSerial:
            deviceStatus = None
            for line in self._runCmd(["devices"]).output:
                m = re.match('(.+)?\s+(.+)$', line)
                if m:
                    if self._deviceSerial == m.group(1):
                        deviceStatus = m.group(2)
            if deviceStatus is None:
                raise DMError("device not found: %s" % self._deviceSerial)
            elif deviceStatus != "device":
                raise DMError("bad status for device %s: %s" % (self._deviceSerial, deviceStatus))

        # Check to see if we can connect to device and run a simple command
        if not self._checkCmd(["shell", "echo"], timeout=self.short_timeout) == 0:
            raise DMError("unable to connect to device")

    def _checkForRoot(self):
        self._haveRootShell = False
        self._haveSu = False
        # If requested to attempt to run adbd as root, do so before
        # checking whether adbs is running as root.
        if self._runAdbAsRoot:
            self._adb_root()

        # Check whether we _are_ root by default (some development boards work
        # this way, this is also the result of some relatively rare rooting
        # techniques)
        proc = self._runCmd(["shell", "id"], timeout=self.short_timeout)
        if proc.output and 'uid=0(root)' in proc.output[0]:
            self._haveRootShell = True
            # if this returns true, we don't care about su
            return

        # if root shell is not available, check if 'su' can be used to gain
        # root
        def su_id(su_modifier, timeout):
            proc = self._runCmd(["shell", "su", su_modifier, "id"],
                                timeout=timeout)

            # wait for response for maximum of 15 seconds, in case su
            # prompts for a password or triggers the Android SuperUser
            # prompt
            start_time = time.time()
            retcode = None
            while (time.time() - start_time) <= 15 and retcode is None:
                retcode = proc.poll()
            if retcode is None:  # still not terminated, kill
                proc.kill()

            if proc.output and 'uid=0(root)' in proc.output[0]:
                return True
            return False

        if su_id('0', self.short_timeout):
            self._haveSu = True
            self._suModifier = '0'
        elif su_id('-c', self.short_timeout):
            self._haveSu = True
            self._suModifier = '-c'

    def _isUnzipAvailable(self):
        data = self._runCmd(["shell", "unzip"]).output
        for line in data:
            if (re.search('Usage', line)):
                return True
        return False

    def _isLocalZipAvailable(self):
        def _noOutput(line):
            # suppress output from zip ProcessHandler
            pass
        try:
            proc = ProcessHandler(["zip", "-?"], storeOutput=False, processOutputLine=_noOutput)
            proc.run()
            proc.wait()
        except:
            return False
        return True

    def _verifyZip(self):
        # If "zip" can be run locally, and "unzip" can be run remotely, then pushDir
        # can use these to push just one file per directory -- a significant
        # optimization for large directories.
        self._useZip = False
        if (self._isUnzipAvailable() and self._isLocalZipAvailable()):
            self._logger.info("will use zip to push directories")
            self._useZip = True
        else:
            raise DMError("zip not available")

    def _adb_root(self):
        """ Some devices require us to reboot adbd as root.
            This function takes care of it.
        """
        if self.processInfo("adbd")[2] != "root":
            self._checkCmd(["root"])
            self._checkCmd(["wait-for-device"])
            if self.processInfo("adbd")[2] != "root":
                raise DMError("We tried rebooting adbd as root, however, it failed.")

    def _detectLsModifier(self):
        if self._lsModifier is None:
            # Check if busybox -1A is required in order to get one
            # file per line.
            output = self._runCmd(["shell", "ls", "-1A", "/"],
                                  timeout=self.short_timeout).output
            output = ' '.join(output)
            if 'error: device not found' in output:
                raise DMError(output)
            if "Unknown option '-1'. Aborting." in output:
                self._lsModifier = "-a"
            elif "No such file or directory" in output:
                self._lsModifier = "-a"
            else:
                self._lsModifier = "-1A"
