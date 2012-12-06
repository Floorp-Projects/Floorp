# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import hashlib
import socket
import os
import re
import struct
import StringIO
import zlib

from Zeroconf import Zeroconf, ServiceBrowser

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
    def not_implemented(*args, **kwargs):
        raise NotImplementedError('Abstract method %s at File "%s", line %s '
                                   'should be implemented by a concrete class' %
                                   (repr(method), filename, line))
    return not_implemented

class DeviceManager:

    _logcatNeedsRoot = True

    @abstractmethod
    def shell(self, cmd, outputfile, env=None, cwd=None, timeout=None, root=False):
        """
        Executes shell command on device and returns exit code

        cmd - Command string to execute
        outputfile - File to store output
        env - Environment to pass to exec command
        cwd - Directory to execute command from
        timeout - specified in seconds, defaults to 'default_timeout'
        root - Specifies whether command requires root privileges
        """

    def shellCheckOutput(self, cmd, env=None, cwd=None, timeout=None, root=False):
        """
        executes shell command on device and returns the the output

        env - Environment to pass to exec command
        cwd - Directory to execute command from
        timeout - specified in seconds, defaults to 'default_timeout'
        root - Specifies whether command requires root privileges
        """
        buf = StringIO.StringIO()
        retval = self.shell(cmd, buf, env=env, cwd=cwd, timeout=timeout, root=root)
        output = str(buf.getvalue()[0:-1]).rstrip()
        buf.close()
        if retval != 0:
            raise DMError("Non-zero return code for command: %s (output: '%s', retval: '%s')" % (cmd, output, retval))
        return output

    @abstractmethod
    def pushFile(self, localname, destname):
        """
        Copies localname from the host to destname on the device
        """

    @abstractmethod
    def mkDir(self, name):
        """
        Creates a single directory on the device file system
        """

    def mkDirs(self, filename):
        """
        Make directory structure on the device
        WARNING: does not create last part of the path
        """
        dirParts = filename.rsplit('/', 1)
        if not self.dirExists(dirParts[0]):
            parts = filename.split('/')
            name = ""
            for part in parts:
                if part == parts[-1]:
                    break
                if part != "":
                    name += '/' + part
                    self.mkDir(name) # mkDir will check previous existence

    @abstractmethod
    def pushDir(self, localDir, remoteDir):
        """
        Push localDir from host to remoteDir on the device
        """

    @abstractmethod
    def fileExists(self, filepath):
        """
        Checks if filepath exists and is a file on the device file system

        returns:
          success: True
          failure: False
        """

    @abstractmethod
    def listFiles(self, rootdir):
        """
        Lists files on the device rootdir

        returns:
          success: array of filenames, ['file1', 'file2', ...]
          failure: None
        """

    @abstractmethod
    def removeFile(self, filename):
        """
        Removes filename from the device

        returns:
          success: output of telnet
          failure: None
        """

    @abstractmethod
    def removeDir(self, remoteDir):
        """
        Does a recursive delete of directory on the device: rm -Rf remoteDir

        returns:
          success: output of telnet
          failure: None
        """

    @abstractmethod
    def getProcessList(self):
        """
        Lists the running processes on the device

        returns:
          success: array of process tuples
          failure: []
        """

    def processExist(self, appname):
        """
        Iterates process list and checks if pid exists

        returns:
          success: pid
          failure: None
        """
        if not isinstance(appname, basestring):
            raise TypeError("appname %s is not a string" % appname)

        pid = None

        #filter out extra spaces
        parts = filter(lambda x: x != '', appname.split(' '))
        appname = ' '.join(parts)

        #filter out the quoted env string if it exists
        #ex: '"name=value;name2=value2;etc=..." process args' -> 'process args'
        parts = appname.split('"')
        if (len(parts) > 2):
            appname = ' '.join(parts[2:]).strip()

        pieces = appname.split(' ')
        parts = pieces[0].split('/')
        app = parts[-1]

        procList = self.getProcessList()
        if (procList == []):
            return None

        for proc in procList:
            procName = proc[1].split('/')[-1]
            if (procName == app):
                pid = proc[0]
                break
        return pid


    @abstractmethod
    def killProcess(self, appname, forceKill=False):
        """
        Kills the process named appname.
        If forceKill is True, process is killed regardless of state

        returns:
          success: True
          failure: False
        """

    @abstractmethod
    def catFile(self, remoteFile):
        """
        Returns the contents of remoteFile

        returns:
          success: filecontents, string
          failure: None
        """

    @abstractmethod
    def pullFile(self, remoteFile):
        """
        Returns contents of remoteFile using the "pull" command.

        returns:
          success: output of pullfile, string
          failure: None
        """

    @abstractmethod
    def getFile(self, remoteFile, localFile):
        """
        Copy file from device (remoteFile) to host (localFile)
        """

    @abstractmethod
    def getDirectory(self, remoteDir, localDir, checkDir=True):
        """
        Copy directory structure from device (remoteDir) to host (localDir)

        returns:
          success: list of files, string
          failure: None
        """

    @abstractmethod
    def validateFile(self, remoteFile, localFile):
        """
        Checks if the remoteFile has the same md5 hash as the localFile

        returns:
          success: True
          failure: False
        """

    @abstractmethod
    def _getRemoteHash(self, filename):
        """
        Return the md5 sum of a file on the device

        returns:
          success: MD5 hash for given filename
          failure: None
        """

    @staticmethod
    def _getLocalHash(filename):
        """
        Return the MD5 sum of a file on the host

        returns:
          success: MD5 hash for given filename
          failure: None
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

    @abstractmethod
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

    @abstractmethod
    def getAppRoot(self, packageName=None):
        """
        Returns the app root directory
        E.g /tests/fennec or /tests/firefox

        returns:
          success: path for app root
          failure: None
        """
        # TODO Support org.mozilla.firefox and B2G

    def getTestRoot(self, harness):
        """
        Gets the directory location on the device for a specific test type
        Harness is one of: xpcshell|reftest|mochitest

        returns:
          success: path for test root
          failure: None
        """

        devroot = self.getDeviceRoot()
        if (devroot == None):
            return None

        if (re.search('xpcshell', harness, re.I)):
            self.testRoot = devroot + '/xpcshell'
        elif (re.search('?(i)reftest', harness)):
            self.testRoot = devroot + '/reftest'
        elif (re.search('?(i)mochitest', harness)):
            self.testRoot = devroot + '/mochitest'
        return self.testRoot

    @abstractmethod
    def getTempDir(self):
        """
        Gets the temporary directory we are using on this device
        base on our device root, ensuring also that it exists.

        returns:
          success: path for temporary directory
          failure: None
        """

    def signal(self, processID, signalType, signalAction):
        """
        Sends a specific process ID a signal code and action.
        For Example: SIGINT and SIGDFL to process x
        """
        #currently not implemented in device agent - todo
        pass

    def getReturnCode(self, processID):
        """Get a return code from process ending -- needs support on device-agent"""
        # TODO: make this real

        return 0

    def getIP(self, interfaces=['eth0', 'wlan0']):
        """
        Gets the IP of the device, or None if no connection exists.
        """
        for interface in interfaces:
            match = re.match(r"%s: ip (\S+)" % interface,
                             self.shellCheckOutput(['ifconfig', interface]))
            if match:
                return match.group(1)

    @abstractmethod
    def unpackFile(self, file_path, dest_dir=None):
        """
        Unzips a remote bundle to a remote location
        If dest_dir is not specified, the bundle is extracted
        in the same directory

        returns:
          success: output of unzip command
          failure: None
        """

    @abstractmethod
    def reboot(self, ipAddr=None, port=30000):
        """
        Reboots the device

        returns:
          success: status from test agent
          failure: None
        """

    def validateDir(self, localDir, remoteDir):
        """
        Validate localDir from host to remoteDir on the device

        returns:
          success: True
          failure: False
        """

        if (self.debug >= 2):
            print "validating directory: " + localDir + " to " + remoteDir
        for root, dirs, files in os.walk(localDir):
            parts = root.split(localDir)
            for f in files:
                remoteRoot = remoteDir + '/' + parts[1]
                remoteRoot = remoteRoot.replace('/', '/')
                if (parts[1] == ""):
                    remoteRoot = remoteDir
                remoteName = remoteRoot + '/' + f
                if (self.validateFile(remoteName, os.path.join(root, f)) <> True):
                        return False
        return True

    @abstractmethod
    def getInfo(self, directive=None):
        """
        Returns information about the device:
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

        returns: dict of info strings by directive name
        """

    @abstractmethod
    def installApp(self, appBundlePath, destPath=None):
        """
        Installs an application onto the device
        appBundlePath - path to the application bundle on the device
        destPath - destination directory of where application should be installed to (optional)
        """

    @abstractmethod
    def uninstallApp(self, appName, installPath=None):
        """
        Uninstalls the named application from device and DOES NOT cause a reboot
        appName - the name of the application (e.g org.mozilla.fennec)
        installPath - the path to where the application was installed (optional)
        """

    @abstractmethod
    def uninstallAppAndReboot(self, appName, installPath=None):
        """
        Uninstalls the named application from device and causes a reboot
        appName - the name of the application (e.g org.mozilla.fennec)
        installPath - the path to where the application was installed (optional)
        """

    @abstractmethod
    def updateApp(self, appBundlePath, processName=None, destPath=None, ipAddr=None, port=30000):
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

    @abstractmethod
    def getCurrentTime(self):
        """
        Returns device time in milliseconds since the epoch

        returns:
          success: time in ms
          failure: None
        """

    def recordLogcat(self):
        """
        Clears the logcat file making it easier to view specific events
        """
        #TODO: spawn this off in a separate thread/process so we can collect all the logcat information

        # Right now this is just clearing the logcat so we can only see what happens after this call.
        self.shellCheckOutput(['/system/bin/logcat', '-c'], root=self._logcatNeedsRoot)

    def getLogcat(self, filterSpecs=["dalvikvm:S", "ConnectivityService:S",
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

    @staticmethod
    def _writePNG(buf, width, height):
        """
        Method for writing a PNG from a buffer, used by getScreenshot on older devices
        Based on: http://code.activestate.com/recipes/577443-write-a-png-image-in-native-python/
        """
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
            tempScreenshotFile = self.getDeviceRoot() + "/ss-dm.tmp"
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
    def chmodDir(self, remoteDir, mask="777"):
        """
        Recursively changes file permissions in a directory

        returns:
          success: True
          failure: False
        """

    @staticmethod
    def _escapedCommandLine(cmd):
        """ Utility function to return escaped and quoted version of command line """
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


class NetworkTools:
    def __init__(self):
        pass

    # Utilities to get the local ip address
    def getInterfaceIp(self, ifname):
        if os.name != "nt":
            import fcntl
            import struct
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            return socket.inet_ntoa(fcntl.ioctl(
                                    s.fileno(),
                                    0x8915,  # SIOCGIFADDR
                                    struct.pack('256s', ifname[:15])
                                    )[20:24])
        else:
            return None

    def getLanIp(self):
        try:
            ip = socket.gethostbyname(socket.gethostname())
        except socket.gaierror:
            ip = socket.gethostbyname(socket.gethostname() + ".local") # for Mac OS X
        if (ip is None or ip.startswith("127.")) and os.name != "nt":
            interfaces = ["eth0","eth1","eth2","wlan0","wlan1","wifi0","ath0","ath1","ppp0"]
            for ifname in interfaces:
                try:
                    ip = self.getInterfaceIp(ifname)
                    break
                except IOError:
                    pass
        return ip

    # Gets an open port starting with the seed by incrementing by 1 each time
    def findOpenPort(self, ip, seed):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            connected = False
            if isinstance(seed, basestring):
                seed = int(seed)
            maxportnum = seed + 5000 # We will try at most 5000 ports to find an open one
            while not connected:
                try:
                    s.bind((ip, seed))
                    connected = True
                    s.close()
                    break
                except:
                    if seed > maxportnum:
                        print "Automation Error: Could not find open port after checking 5000 ports"
                    raise
                seed += 1
        except:
            print "Automation Error: Socket error trying to find open port"

        return seed

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
