# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import hashlib
import mozlog
import os
import posixpath
import re
import struct
import StringIO
import zlib

from functools import wraps

class DMError(Exception):
    "generic devicemanager exception."

    def __init__(self, msg= '', fatal = False):
        self.msg = msg
        self.fatal = fatal

    def __str__(self):
        return self.msg

def abstractmethod(method):
    line = method.func_code.co_firstlineno
    filename = method.func_code.co_filename
    @wraps(method)
    def not_implemented(*args, **kwargs):
        raise NotImplementedError('Abstract method %s at File "%s", line %s '
                                   'should be implemented by a concrete class' %
                                   (repr(method), filename, line))
    return not_implemented

class DeviceManager(object):
    """
    Represents a connection to a device. Once an implementation of this class
    is successfully instantiated, you may do things like list/copy files to
    the device, launch processes on the device, and install or remove
    applications from the device.

    Never instantiate this class directly! Instead, instantiate an
    implementation of it like DeviceManagerADB or DeviceManagerSUT.
    """

    _logcatNeedsRoot = True

    def __init__(self, logLevel=None, deviceRoot=None):
        try:
            self._logger = mozlog.structured.structuredlog.get_default_logger(component="mozdevice")
            if not self._logger: # no global structured logger, fall back to reg logging
                self._logger = mozlog.getLogger("mozdevice")
                if logLevel is not None:
                    self._logger.setLevel(logLevel)
        except AttributeError:
            # Structured logging doesn't work on Python 2.6
            self._logger = None
        self._logLevel = logLevel
        self._remoteIsWin = None
        self._isDeviceRootSetup = False
        self._deviceRoot = deviceRoot

    def _log(self, data):
        """
        This helper function is called by ProcessHandler to log
        the output produced by processes
        """
        self._logger.debug(data)

    @property
    def remoteIsWin(self):
        if self._remoteIsWin is None:
            self._remoteIsWin = self.getInfo("os")["os"][0] == "windows"
        return self._remoteIsWin

    @property
    def logLevel(self):
        return self._logLevel

    @logLevel.setter
    def logLevel_setter(self, newLogLevel):
        self._logLevel = newLogLevel
        self._logger.setLevel(self._logLevel)

    @property
    def debug(self):
        self._logger.warning("dm.debug is deprecated. Use logLevel.")
        levels = {mozlog.DEBUG: 5, mozlog.INFO: 3, mozlog.WARNING: 2,
                  mozlog.ERROR: 1, mozlog.CRITICAL: 0}
        return levels[self.logLevel]

    @debug.setter
    def debug_setter(self, newDebug):
        self._logger.warning("dm.debug is deprecated. Use logLevel.")
        newDebug = 5 if newDebug > 5 else newDebug # truncate >=5 to 5
        levels = {5: mozlog.DEBUG, 3: mozlog.INFO, 2: mozlog.WARNING,
                  1: mozlog.ERROR, 0: mozlog.CRITICAL}
        self.logLevel = levels[newDebug]

    @abstractmethod
    def getInfo(self, directive=None):
        """
        Returns a dictionary of information strings about the device.

        :param directive: information you want to get. Options are:

          - `os` - name of the os
          - `id` - unique id of the device
          - `uptime` - uptime of the device
          - `uptimemillis` - uptime of the device in milliseconds (NOT supported on all implementations)
          - `systime` - system time of the device
          - `screen` - screen resolution
          - `memory` - memory stats
          - `memtotal` - total memory available on the device, for example 927208 kB
          - `process` - list of running processes (same as ps)
          - `disk` - total, free, available bytes on disk
          - `power` - power status (charge, battery temp)
          - `temperature` - device temperature

         If `directive` is `None`, will return all available information
        """

    @abstractmethod
    def getCurrentTime(self):
        """
        Returns device time in milliseconds since the epoch.
        """

    def getIP(self, interfaces=['eth0', 'wlan0']):
        """
        Returns the IP of the device, or None if no connection exists.
        """
        for interface in interfaces:
            match = re.match(r"%s: ip (\S+)" % interface,
                             self.shellCheckOutput(['ifconfig', interface]))
            if match:
                return match.group(1)

    def recordLogcat(self):
        """
        Clears the logcat file making it easier to view specific events.
        """
        #TODO: spawn this off in a separate thread/process so we can collect all the logcat information

        # Right now this is just clearing the logcat so we can only see what happens after this call.
        self.shellCheckOutput(['/system/bin/logcat', '-c'], root=self._logcatNeedsRoot)

    def getLogcat(self, filterSpecs=["dalvikvm:I", "ConnectivityService:S",
                                      "WifiMonitor:S", "WifiStateTracker:S",
                                      "wpa_supplicant:S", "NetworkStateTracker:S"],
                  format="time",
                  filterOutRegexps=[]):
        """
        Returns the contents of the logcat file as a list of strings
        """
        cmdline = ["/system/bin/logcat", "-v", format, "-d"] + filterSpecs
        lines = self.shellCheckOutput(cmdline,
                                      root=self._logcatNeedsRoot).split('\r')

        for regex in filterOutRegexps:
            lines = [line for line in lines if not re.search(regex, line)]

        return lines

    def saveScreenshot(self, filename):
        """
        Takes a screenshot of what's being display on the device. Uses
        "screencap" on newer (Android 3.0+) devices (and some older ones with
        the functionality backported). This function also works on B2G.

        Throws an exception on failure. This will always fail on devices
        without the screencap utility.
        """
        screencap = '/system/bin/screencap'
        if not self.fileExists(screencap):
            raise DMError("Unable to capture screenshot on device: no screencap utility")

        with open(filename, 'w') as pngfile:
            # newer versions of screencap can write directly to a png, but some
            # older versions can't
            tempScreenshotFile = self.deviceRoot + "/ss-dm.tmp"
            self.shellCheckOutput(["sh", "-c", "%s > %s" %
                                   (screencap, tempScreenshotFile)],
                                  root=True)
            buf = self.pullFile(tempScreenshotFile)
            width = int(struct.unpack("I", buf[0:4])[0])
            height = int(struct.unpack("I", buf[4:8])[0])
            with open(filename, 'w') as pngfile:
                pngfile.write(self._writePNG(buf[12:], width, height))
            self.removeFile(tempScreenshotFile)

    @abstractmethod
    def pushFile(self, localFilename, remoteFilename, retryLimit=1, createDir=True):
        """
        Copies localname from the host to destname on the device.
        """

    @abstractmethod
    def pushDir(self, localDirname, remoteDirname, retryLimit=1, timeout=None):
        """
        Push local directory from host to remote directory on the device,
        """

    @abstractmethod
    def pullFile(self, remoteFilename, offset=None, length=None):
        """
        Returns contents of remoteFile using the "pull" command.

        :param remoteFilename: Path to file to pull from remote device.
        :param offset: Offset in bytes from which to begin reading (optional)
        :param length: Number of bytes to read (optional)
        """

    @abstractmethod
    def getFile(self, remoteFilename, localFilename):
        """
        Copy file from remote device to local file on host.
        """

    @abstractmethod
    def getDirectory(self, remoteDirname, localDirname, checkDir=True):
        """
        Copy directory structure from device (remoteDirname) to host (localDirname).
        """

    @abstractmethod
    def validateFile(self, remoteFilename, localFilename):
        """
        Returns True if a file on the remote device has the same md5 hash as a local one.
        """

    def validateDir(self, localDirname, remoteDirname):
        """
        Returns True if remoteDirname on device is same as localDirname on host.
        """

        self._logger.info("validating directory: %s to %s" % (localDirname, remoteDirname))
        for root, dirs, files in os.walk(localDirname):
            parts = root.split(localDirname)
            for f in files:
                remoteRoot = remoteDirname + '/' + parts[1]
                remoteRoot = remoteRoot.replace('/', '/')
                if (parts[1] == ""):
                    remoteRoot = remoteDirname
                remoteName = remoteRoot + '/' + f
                if (self.validateFile(remoteName, os.path.join(root, f)) <> True):
                        return False
        return True

    @abstractmethod
    def mkDir(self, remoteDirname):
        """
        Creates a single directory on the device file system.
        """

    def mkDirs(self, filename):
        """
        Make directory structure on the device.

        WARNING: does not create last part of the path. For example, if asked to
        create `/mnt/sdcard/foo/bar/baz`, it will only create `/mnt/sdcard/foo/bar`
        """
        filename = posixpath.normpath(filename)
        containing = posixpath.dirname(filename)
        if not self.dirExists(containing):
            parts = filename.split('/')
            name = "/" if not self.remoteIsWin else parts.pop(0)
            for part in parts[:-1]:
                if part != "":
                    name = posixpath.join(name, part)
                    self.mkDir(name) # mkDir will check previous existence

    @abstractmethod
    def dirExists(self, dirpath):
        """
        Returns whether dirpath exists and is a directory on the device file system.
        """

    @abstractmethod
    def fileExists(self, filepath):
        """
        Return whether filepath exists on the device file system,
        regardless of file type.
        """

    @abstractmethod
    def listFiles(self, rootdir):
        """
        Lists files on the device rootdir.

        Returns array of filenames, ['file1', 'file2', ...]
        """

    @abstractmethod
    def removeFile(self, filename):
        """
        Removes filename from the device.
        """

    @abstractmethod
    def removeDir(self, remoteDirname):
        """
        Does a recursive delete of directory on the device: rm -Rf remoteDirname.
        """

    @abstractmethod
    def moveTree(self, source, destination):
         """
         Does a move of the file or directory on the device.

        :param source: Path to the original file or directory
        :param destination: Path to the destination file or directory
         """

    @abstractmethod
    def copyTree(self, source, destination):
         """
         Does a copy of the file or directory on the device.

        :param source: Path to the original file or directory
        :param destination: Path to the destination file or directory
         """

    @abstractmethod
    def chmodDir(self, remoteDirname, mask="777"):
        """
        Recursively changes file permissions in a directory.
        """

    @property
    def deviceRoot(self):
        """
        The device root on the device filesystem for putting temporary
        testing files.
        """
        # derive deviceroot value if not set
        if not self._deviceRoot or not self._isDeviceRootSetup:
            self._deviceRoot = self._setupDeviceRoot(self._deviceRoot)
            self._isDeviceRootSetup = True

        return self._deviceRoot

    @abstractmethod
    def _setupDeviceRoot(self):
        """
        Sets up and returns a device root location that can be written to by tests.
        """

    def getDeviceRoot(self):
        """
        Get the device root on the device filesystem for putting temporary
        testing files.

        .. deprecated:: 0.38
          Use the :py:attr:`deviceRoot` property instead.
        """
        return self.deviceRoot

    @abstractmethod
    def getTempDir(self):
        """
        Returns a temporary directory we can use on this device, ensuring
        also that it exists.
        """

    @abstractmethod
    def shell(self, cmd, outputfile, env=None, cwd=None, timeout=None, root=False):
        """
        Executes shell command on device and returns exit code.

        :param cmd: Commandline list to execute
        :param outputfile: File to store output
        :param env: Environment to pass to exec command
        :param cwd: Directory to execute command from
        :param timeout: specified in seconds, defaults to 'default_timeout'
        :param root: Specifies whether command requires root privileges
        """

    def shellCheckOutput(self, cmd, env=None, cwd=None, timeout=None, root=False):
        """
        Executes shell command on device and returns output as a string. Raises if
        the return code is non-zero.

        :param cmd: Commandline list to execute
        :param env: Environment to pass to exec command
        :param cwd: Directory to execute command from
        :param timeout: specified in seconds, defaults to 'default_timeout'
        :param root: Specifies whether command requires root privileges
        :raises: DMError
        """
        buf = StringIO.StringIO()
        retval = self.shell(cmd, buf, env=env, cwd=cwd, timeout=timeout, root=root)
        output = str(buf.getvalue()[0:-1]).rstrip()
        buf.close()
        if retval != 0:
            raise DMError("Non-zero return code for command: %s (output: '%s', retval: '%s')" % (cmd, output, retval))
        return output

    @abstractmethod
    def getProcessList(self):
        """
        Returns array of tuples representing running processes on the device.

        Format of tuples is (processId, processName, userId)
        """

    def processInfo(self, processName):
        """
        Returns information on the process with processName.
        Information on process is in tuple format: (pid, process path, user)
        If a process with the specified name does not exist this function will return None.
        """
        if not isinstance(processName, basestring):
            raise TypeError("Process name %s is not a string" % processName)

        processInfo = None

        #filter out extra spaces
        parts = filter(lambda x: x != '', processName.split(' '))
        processName = ' '.join(parts)

        #filter out the quoted env string if it exists
        #ex: '"name=value;name2=value2;etc=..." process args' -> 'process args'
        parts = processName.split('"')
        if (len(parts) > 2):
            processName = ' '.join(parts[2:]).strip()

        pieces = processName.split(' ')
        parts = pieces[0].split('/')
        app = parts[-1]

        procList = self.getProcessList()
        if (procList == []):
            return None

        for proc in procList:
            procName = proc[1].split('/')[-1]
            if (procName == app):
                processInfo = proc
                break
        return processInfo

    def processExist(self, processName):
        """
        Returns True if process with name processName is running on device.
        """
        processInfo = self.processInfo(processName)
        if processInfo:
            return processInfo[0]

    @abstractmethod
    def killProcess(self, processName, sig=None):
        """
        Kills the process named processName. If sig is not None, process is
        killed with the specified signal.

        :param processName: path or name of the process to kill
        :param sig: signal to pass into the kill command (optional)
        """

    @abstractmethod
    def reboot(self, wait=False, ipAddr=None):
        """
        Reboots the device.

        :param wait: block on device to come back up before returning
        :param ipAddr: if specified, try to make the device connect to this
                       specific IP address after rebooting (only works with
                       SUT; if None, we try to determine a reasonable address
                       ourselves)
        """

    @abstractmethod
    def installApp(self, appBundlePath, destPath=None):
        """
        Installs an application onto the device.

        :param appBundlePath: path to the application bundle on the device
        :param destPath: destination directory of where application should be installed to (optional)
        """

    @abstractmethod
    def uninstallApp(self, appName, installPath=None):
        """
        Uninstalls the named application from device and DOES NOT cause a reboot.

        :param appName: the name of the application (e.g org.mozilla.fennec)
        :param installPath: the path to where the application was installed (optional)
        """

    @abstractmethod
    def uninstallAppAndReboot(self, appName, installPath=None):
        """
        Uninstalls the named application from device and causes a reboot.

        :param appName: the name of the application (e.g org.mozilla.fennec)
        :param installPath: the path to where the application was installed (optional)
        """

    @abstractmethod
    def updateApp(self, appBundlePath, processName=None, destPath=None,
                  wait=False, ipAddr=None):
        """
        Updates the application on the device and reboots.

        :param appBundlePath: path to the application bundle on the device
        :param processName: used to end the process if the applicaiton is
                            currently running (optional)
        :param destPath: Destination directory to where the application should
                         be installed (optional)
        :param wait: block on device to come back up before returning
        :param ipAddr: if specified, try to make the device connect to this
                       specific IP address after rebooting (only works with
                       SUT; if None and wait is True, we try to determine a
                       reasonable address ourselves)
        """

    @staticmethod
    def _writePNG(buf, width, height):
        """
        Method for writing a PNG from a buffer, used by getScreenshot on older devices,
        """
        # Based on: http://code.activestate.com/recipes/577443-write-a-png-image-in-native-python/
        width_byte_4 = width * 4
        raw_data = b"".join(b'\x00' + buf[span:span + width_byte_4] for span in range(0, (height - 1) * width * 4, width_byte_4))
        def png_pack(png_tag, data):
            chunk_head = png_tag + data
            return struct.pack("!I", len(data)) + chunk_head + struct.pack("!I", 0xFFFFFFFF & zlib.crc32(chunk_head))
        return b"".join([
                b'\x89PNG\r\n\x1a\n',
                png_pack(b'IHDR', struct.pack("!2I5B", width, height, 8, 6, 0, 0, 0)),
                png_pack(b'IDAT', zlib.compress(raw_data, 9)),
                png_pack(b'IEND', b'')])

    @abstractmethod
    def _getRemoteHash(self, filename):
        """
        Return the md5 sum of a file on the device.
        """

    @staticmethod
    def _getLocalHash(filename):
        """
        Return the MD5 sum of a file on the host.
        """
        f = open(filename, 'rb')
        if (f == None):
            return None

        try:
            mdsum = hashlib.md5()
        except:
            return None

        while 1:
            data = f.read(1024)
            if not data:
                break
            mdsum.update(data)

        f.close()
        hexval = mdsum.hexdigest()
        return hexval

    @staticmethod
    def _escapedCommandLine(cmd):
        """
        Utility function to return escaped and quoted version of command line.
        """
        quotedCmd = []

        for arg in cmd:
            arg.replace('&', '\&')

            needsQuoting = False
            for char in [ ' ', '(', ')', '"', '&' ]:
                if arg.find(char) >= 0:
                    needsQuoting = True
                    break
            if needsQuoting:
                arg = '\'%s\'' % arg

            quotedCmd.append(arg)

        return " ".join(quotedCmd)

def _pop_last_line(file_obj):
    """
    Utility function to get the last line from a file (shared between ADB and
    SUT device managers). Function also removes it from the file. Intended to
    strip off the return code from a shell command.
    """
    bytes_from_end = 1
    file_obj.seek(0, 2)
    length = file_obj.tell() + 1
    while bytes_from_end < length:
        file_obj.seek((-1)*bytes_from_end, 2)
        data = file_obj.read()

        if bytes_from_end == length-1 and len(data) == 0: # no data, return None
            return None

        if data[0] == '\n' or bytes_from_end == length-1:
            # found the last line, which should have the return value
            if data[0] == '\n':
                data = data[1:]

            # truncate off the return code line
            file_obj.truncate(length - bytes_from_end)
            file_obj.seek(0,2)
            file_obj.write('\0')

            return data

        bytes_from_end += 1

    return None

class ZeroconfListener(object):
    def __init__(self, hwid, evt):
        self.hwid = hwid
        self.evt = evt

    # Format is 'SUTAgent [hwid:015d2bc2825ff206] [ip:10_242_29_221]._sutagent._tcp.local.'
    def addService(self, zeroconf, type, name):
        #print "Found _sutagent service broadcast:", name
        if not name.startswith("SUTAgent"):
            return

        sutname = name.split('.')[0]
        m = re.search('\[hwid:([^\]]*)\]', sutname)
        if m is None:
            return

        hwid = m.group(1)

        m = re.search('\[ip:([0-9_]*)\]', sutname)
        if m is None:
            return

        ip = m.group(1).replace("_", ".")

        if self.hwid == hwid:
            self.ip = ip
            self.evt.set()

    def removeService(self, zeroconf, type, name):
        pass
