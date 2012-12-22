# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import select
import socket
import SocketServer
import time
import os
import re
import posixpath
import subprocess
from threading import Thread
import StringIO
from devicemanager import DeviceManager, DMError, NetworkTools, _pop_last_line
import errno
from distutils.version import StrictVersion

class DeviceManagerSUT(DeviceManager):
    debug = 2
    _base_prompt = '$>'
    _base_prompt_re = '\$\>'
    _prompt_sep = '\x00'
    _prompt_regex = '.*(' + _base_prompt_re + _prompt_sep + ')'
    _agentErrorRE = re.compile('^##AGENT-WARNING##\ ?(.*)')
    default_timeout = 300

    def __init__(self, host, port = 20701, retryLimit = 5, deviceRoot = None, **kwargs):
        self.host = host
        self.port = port
        self.retryLimit = retryLimit
        self._sock = None
        self._everConnected = False
        self.deviceRoot = deviceRoot

        # Initialize device root
        self.getDeviceRoot()

        # Get version
        verstring = self._runCmds([{ 'cmd': 'ver' }])
        self.agentVersion = re.sub('SUTAgentAndroid Version ', '', verstring)

    def _cmdNeedsResponse(self, cmd):
        """ Not all commands need a response from the agent:
            * rebt obviously doesn't get a response
            * uninstall performs a reboot to ensure starting in a clean state and
              so also doesn't look for a response
        """
        noResponseCmds = [re.compile('^rebt'),
                          re.compile('^uninst .*$'),
                          re.compile('^pull .*$')]

        for c in noResponseCmds:
            if (c.match(cmd)):
                return False

        # If the command is not in our list, then it gets a response
        return True

    def _stripPrompt(self, data):
        """
        take a data blob and strip instances of the prompt '$>\x00'
        """
        promptre = re.compile(self._prompt_regex + '.*')
        retVal = []
        lines = data.split('\n')
        for line in lines:
            foundPrompt = False
            try:
                while (promptre.match(line)):
                    foundPrompt = True
                    pieces = line.split(self._prompt_sep)
                    index = pieces.index('$>')
                    pieces.pop(index)
                    line = self._prompt_sep.join(pieces)
            except(ValueError):
                pass

            # we don't want to append lines that are blank after stripping the
            # prompt (those are basically "prompts")
            if not foundPrompt or line:
                retVal.append(line)

        return '\n'.join(retVal)

    def _shouldCmdCloseSocket(self, cmd):
        """
        Some commands need to close the socket after they are sent:
          * rebt
          * uninst
          * quit
        """
        socketClosingCmds = [re.compile('^quit.*'),
                             re.compile('^rebt.*'),
                             re.compile('^uninst .*$')]

        for c in socketClosingCmds:
            if (c.match(cmd)):
                return True
        return False

    def _sendCmds(self, cmdlist, outputfile, timeout = None, retryLimit = None):
        """
        Wrapper for _doCmds that loops up to retryLimit iterations
        """
        # this allows us to move the retry logic outside of the _doCmds() to make it
        # easier for debugging in the future.
        # note that since cmdlist is a list of commands, they will all be retried if
        # one fails.  this is necessary in particular for pushFile(), where we don't want
        # to accidentally send extra data if a failure occurs during data transmission.

        retryLimit = retryLimit or self.retryLimit
        retries = 0
        while retries < retryLimit:
            try:
                self._doCmds(cmdlist, outputfile, timeout)
                return
            except DMError, err:
                # re-raise error if it's fatal (i.e. the device got the command but
                # couldn't execute it). retry otherwise
                if err.fatal:
                    raise err
                if self.debug >= 4:
                    print err
                retries += 1
                # if we lost the connection or failed to establish one, wait a bit
                if retries < retryLimit and not self._sock:
                    sleep_time = 5 * retries
                    print 'Could not connect; sleeping for %d seconds.' % sleep_time
                    time.sleep(sleep_time)

        raise DMError("Remote Device Error: unable to connect to %s after %s attempts" % (self.host, retryLimit))

    def _runCmds(self, cmdlist, timeout = None, retryLimit = None):
        """
        Similar to _sendCmds, but just returns any output as a string instead of
        writing to a file
        """
        retryLimit = retryLimit or self.retryLimit
        outputfile = StringIO.StringIO()
        self._sendCmds(cmdlist, outputfile, timeout, retryLimit=retryLimit)
        outputfile.seek(0)
        return outputfile.read()

    def _doCmds(self, cmdlist, outputfile, timeout):
        promptre = re.compile(self._prompt_regex + '$')
        shouldCloseSocket = False

        if not timeout:
            # We are asserting that all commands will complete in this time unless otherwise specified
            timeout = self.default_timeout

        if not self._sock:
            try:
                if self.debug >= 1 and self._everConnected:
                    print "reconnecting socket"
                self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            except socket.error, msg:
                self._sock = None
                raise DMError("Automation Error: unable to create socket: "+str(msg))

            try:
                self._sock.connect((self.host, int(self.port)))
                if select.select([self._sock], [], [], timeout)[0]:
                    self._sock.recv(1024)
                else:
                    raise DMError("Remote Device Error: Timeout in connecting", fatal=True)
                    return False
                self._everConnected = True
            except socket.error, msg:
                self._sock.close()
                self._sock = None
                raise DMError("Remote Device Error: Unable to connect socket: "+str(msg))

        for cmd in cmdlist:
            cmdline = '%s\r\n' % cmd['cmd']

            try:
                sent = self._sock.send(cmdline)
                if sent != len(cmdline):
                    raise DMError("Remote Device Error: our cmd was %s bytes and we "
                                  "only sent %s" % (len(cmdline), sent))
                if cmd.get('data'):
                    sent = self._sock.send(cmd['data'])
                    if sent != len(cmd['data']):
                        raise DMError("Remote Device Error: we had %s bytes of data to send, but "
                                      "only sent %s" % (len(cmd['data']), sent))

                if self.debug >= 4:
                    print "sent cmd: " + str(cmd['cmd'])
            except socket.error, msg:
                self._sock.close()
                self._sock = None
                if self.debug >= 1:
                    print "Remote Device Error: Error sending data to socket. cmd="+str(cmd['cmd'])+"; err="+str(msg)
                return False

            # Check if the command should close the socket
            shouldCloseSocket = self._shouldCmdCloseSocket(cmd['cmd'])

            # Handle responses from commands
            if self._cmdNeedsResponse(cmd['cmd']):
                foundPrompt = False
                data = ""
                timer = 0
                select_timeout = 1
                commandFailed = False

                while not foundPrompt:
                    socketClosed = False
                    errStr = ''
                    temp = ''
                    if self.debug >= 4:
                        print "recv'ing..."

                    # Get our response
                    try:
                          # Wait up to a second for socket to become ready for reading...
                        if select.select([self._sock], [], [], select_timeout)[0]:
                            temp = self._sock.recv(1024)
                            if self.debug >= 4:
                                print "response: " + str(temp)
                            timer = 0
                            if not temp:
                                socketClosed = True
                                errStr = 'connection closed'
                        timer += select_timeout
                        if timer > timeout:
                            raise DMError("Automation Error: Timeout in command %s" % cmd['cmd'], fatal=True)
                    except socket.error, err:
                        socketClosed = True
                        errStr = str(err)
                        # This error shows up with we have our tegra rebooted.
                        if err[0] == errno.ECONNRESET:
                            errStr += ' - possible reboot'

                    if socketClosed:
                        self._sock.close()
                        self._sock = None
                        raise DMError("Automation Error: Error receiving data from socket. cmd=%s; err=%s" % (cmd, errStr))

                    data += temp

                    # If something goes wrong in the agent it will send back a string that
                    # starts with '##AGENT-WARNING##'
                    if not commandFailed:
                        errorMatch = self._agentErrorRE.match(data)
                        if errorMatch:
                            # We still need to consume the prompt, so raise an error after
                            # draining the rest of the buffer.
                            commandFailed = True

                    for line in data.splitlines():
                        if promptre.match(line):
                            foundPrompt = True
                            data = self._stripPrompt(data)
                            break

                    # periodically flush data to output file to make sure it doesn't get
                    # too big/unwieldly
                    if len(data) > 1024:
                            outputfile.write(data[0:1024])
                            data = data[1024:]

                if commandFailed:
                    raise DMError("Automation Error: Error processing command '%s'; err='%s'" %
                                  (cmd['cmd'], errorMatch.group(1)), fatal=True)

                # Write any remaining data to outputfile
                outputfile.write(data)

        if shouldCloseSocket:
            try:
                self._sock.close()
                self._sock = None
            except:
                self._sock = None
                raise DMError("Automation Error: Error closing socket")

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
        cmdline = self._escapedCommandLine(cmd)
        if env:
            cmdline = '%s %s' % (self._formatEnvString(env), cmdline)

        haveExecSu = (StrictVersion(self.agentVersion) >= StrictVersion('1.13'))

        # Depending on agent version we send one of the following commands here:
        # * exec (run as normal user)
        # * execsu (run as privileged user)
        # * execcwd (run as normal user from specified directory)
        # * execcwdsu (run as privileged user from specified directory)

        cmd = "exec"
        if cwd:
            cmd += "cwd"
        if root and haveExecSu:
            cmd += "su"

        if cwd:
            self._sendCmds([{ 'cmd': '%s %s %s' % (cmd, cwd, cmdline) }], outputfile, timeout)
        else:
            if (not root) or haveExecSu:
                self._sendCmds([{ 'cmd': '%s %s' % (cmd, cmdline) }], outputfile, timeout)
            else:
                # need to manually inject su -c for backwards compatibility (this may
                # not work on ICS or above!!)
                # (FIXME: this backwards compatibility code is really ugly and should
                # be deprecated at some point in the future)
                self._sendCmds([ { 'cmd': '%s su -c "%s"' % (cmd, cmdline) }], outputfile,
                               timeout)

        # dig through the output to get the return code
        lastline = _pop_last_line(outputfile)
        if lastline:
            m = re.search('return code \[([0-9]+)\]', lastline)
            if m:
                return int(m.group(1))

        # woops, we couldn't find an end of line/return value
        raise DMError("Automation Error: Error finding end of line/return value when running '%s'" % cmdline)

    def pushFile(self, localname, destname, retryLimit = None):
        """
        Copies localname from the host to destname on the device
        """
        retryLimit = retryLimit or self.retryLimit
        self.mkDirs(destname)

        try:
            filesize = os.path.getsize(localname)
            with open(localname, 'rb') as f:
                remoteHash = self._runCmds([{ 'cmd': 'push ' + destname + ' ' + str(filesize),
                                              'data': f.read() }], retryLimit=retryLimit).strip()
        except OSError:
            raise DMError("DeviceManager: Error reading file to push")

        if (self.debug >= 3):
            print "push returned: %s" % hash

        localHash = self._getLocalHash(localname)

        if localHash != remoteHash:
            raise DMError("Automation Error: Push File failed to Validate! (localhash: %s, "
                          "remotehash: %s)" % (localHash, remoteHash))

    def mkDir(self, name):
        """
        Creates a single directory on the device file system
        """
        if not self.dirExists(name):
            self._runCmds([{ 'cmd': 'mkdr ' + name }])

    def pushDir(self, localDir, remoteDir, retryLimit = None):
        """
        Push localDir from host to remoteDir on the device
        """
        retryLimit = retryLimit or self.retryLimit
        if (self.debug >= 2):
            print "pushing directory: %s to %s" % (localDir, remoteDir)

        existentDirectories = []
        for root, dirs, files in os.walk(localDir, followlinks=True):
            parts = root.split(localDir)
            for f in files:
                remoteRoot = remoteDir + '/' + parts[1]
                if (remoteRoot.endswith('/')):
                    remoteName = remoteRoot + f
                else:
                    remoteName = remoteRoot + '/' + f

                if (parts[1] == ""):
                    remoteRoot = remoteDir

                parent = os.path.dirname(remoteName)
                if parent not in existentDirectories:
                    self.mkDirs(remoteName)
                    existentDirectories.append(parent)

                self.pushFile(os.path.join(root, f), remoteName, retryLimit=retryLimit)


    def dirExists(self, remotePath):
        """
        Return True if remotePath is an existing directory on the device.
        """
        ret = self._runCmds([{ 'cmd': 'isdir ' + remotePath }]).strip()
        if not ret:
            raise DMError('Automation Error: DeviceManager isdir returned null')

        return ret == 'TRUE'

    def fileExists(self, filepath):
        """
        Return True if filepath exists and is a file on the device file system
        """
        # Because we always have / style paths we make this a lot easier with some
        # assumptions
        s = filepath.split('/')
        containingpath = '/'.join(s[:-1])
        return s[-1] in self.listFiles(containingpath)

    def listFiles(self, rootdir):
        """
        Lists files on the device rootdir

        returns array of filenames, ['file1', 'file2', ...]
        """
        rootdir = rootdir.rstrip('/')
        if (self.dirExists(rootdir) == False):
            return []
        data = self._runCmds([{ 'cmd': 'cd ' + rootdir }, { 'cmd': 'ls' }])

        files = filter(lambda x: x, data.splitlines())
        if len(files) == 1 and files[0] == '<empty>':
            # special case on the agent: empty directories return just the string "<empty>"
            return []
        return files

    def removeFile(self, filename):
        """
        Removes filename from the device
        """
        if (self.debug>= 2):
            print "removing file: " + filename
        if self.fileExists(filename):
            self._runCmds([{ 'cmd': 'rm ' + filename }])

    def removeDir(self, remoteDir):
        """
        Does a recursive delete of directory on the device: rm -Rf remoteDir
        """
        if self.dirExists(remoteDir):
            self._runCmds([{ 'cmd': 'rmdr ' + remoteDir }])

    def getProcessList(self):
        """
        Lists the running processes on the device

        returns: array of process tuples
        """
        data = self._runCmds([{ 'cmd': 'ps' }])

        processTuples = []
        for line in data.splitlines():
            if line:
                pidproc = line.strip().split()
                try:
                    if (len(pidproc) == 2):
                        processTuples += [[pidproc[0], pidproc[1]]]
                    elif (len(pidproc) == 3):
                        # android returns <userID> <procID> <procName>
                        processTuples += [[int(pidproc[1]), pidproc[2], int(pidproc[0])]]
                    else:
                        # unexpected format
                        raise ValueError
                except ValueError:
                    print "ERROR: Unable to parse process list (bug 805969)"
                    print "Line: %s\nFull output of process list:\n%s" % (line, data)
                    raise DMError("Invalid process line: %s" % line)

        return processTuples

    def fireProcess(self, appname, failIfRunning=False):
        """
        Starts a process

        returns: pid

        DEPRECATED: Use shell() or launchApplication() for new code
        """
        if not appname:
            raise DMError("Automation Error: fireProcess called with no command to run")

        if (self.debug >= 2):
            print "FIRE PROC: '" + appname + "'"

        if (self.processExist(appname) != None):
            print "WARNING: process %s appears to be running already\n" % appname
            if (failIfRunning):
                raise DMError("Automation Error: Process is already running")
        self._runCmds([{ 'cmd': 'exec ' + appname }])

        # The 'exec' command may wait for the process to start and end, so checking
        # for the process here may result in process = None.
        pid = self.processExist(appname)
        if (self.debug >= 4):
            print "got pid: %s for process: %s" % (pid, appname)
        return pid

    def launchProcess(self, cmd, outputFile = "process.txt", cwd = '', env = '', failIfRunning=False):
        """
        Launches a process, redirecting output to standard out

        Returns output filename

        WARNING: Does not work how you expect on Android! The application's
        own output will be flushed elsewhere.

        DEPRECATED: Use shell() or launchApplication() for new code
        """
        if not cmd:
            if (self.debug >= 1):
                print "WARNING: launchProcess called without command to run"
            return None

        cmdline = subprocess.list2cmdline(cmd)
        if (outputFile == "process.txt" or outputFile == None):
            outputFile = self.getDeviceRoot();
            if outputFile is None:
                return None
            outputFile += "/process.txt"
            cmdline += " > " + outputFile

        # Prepend our env to the command
        cmdline = '%s %s' % (self._formatEnvString(env), cmdline)

        # fireProcess may trigger an exception, but we won't handle it
        self.fireProcess(cmdline, failIfRunning)
        return outputFile

    def killProcess(self, appname, forceKill=False):
        """
        Kills the process named appname

        If forceKill is True, process is killed regardless of state
        """
        if forceKill:
            print "WARNING: killProcess(): forceKill parameter unsupported on SUT"
        if self.processExist(appname):
            self._runCmds([{ 'cmd': 'kill ' + appname }])

    def getTempDir(self):
        """
        Return a temporary directory on the device

        Will also ensure that directory exists
        """
        return self._runCmds([{ 'cmd': 'tmpd' }]).strip()

    def catFile(self, remoteFile):
        """
        Returns the contents of remoteFile
        """
        return self._runCmds([{ 'cmd': 'cat ' + remoteFile }])

    def pullFile(self, remoteFile):
        """
        Returns contents of remoteFile using the "pull" command.
        """
        # The "pull" command is different from other commands in that DeviceManager
        # has to read a certain number of bytes instead of just reading to the
        # next prompt.  This is more robust than the "cat" command, which will be
        # confused if the prompt string exists within the file being catted.
        # However it means we can't use the response-handling logic in sendCMD().

        def err(error_msg):
            err_str = 'DeviceManager: pull unsuccessful: %s' % error_msg
            print err_str
            self._sock = None
            raise DMError(err_str)

        # FIXME: We could possibly move these socket-reading functions up to
        # the class level if we wanted to refactor sendCMD().  For now they are
        # only used to pull files.

        def uread(to_recv, error_msg, timeout=None):
            """ unbuffered read """
            timer = 0
            select_timeout = 1
            if not timeout:
                timeout = self.default_timeout

            try:
                if select.select([self._sock], [], [], select_timeout)[0]:
                    data = self._sock.recv(to_recv)
                    timer = 0
                timer += select_timeout
                if timer > timeout:
                    err('timeout in uread while retrieving file')

                if not data:
                    err(error_msg)
                return data
            except:
                err(error_msg)

        def read_until_char(c, buf, error_msg):
            """ read until 'c' is found; buffer rest """
            while not '\n' in buf:
                data = uread(1024, error_msg)
                buf += data
            return buf.partition(c)

        def read_exact(total_to_recv, buf, error_msg):
            """ read exact number of 'total_to_recv' bytes """
            while len(buf) < total_to_recv:
                to_recv = min(total_to_recv - len(buf), 1024)
                data = uread(to_recv, error_msg)
                buf += data
            return buf

        prompt = self._base_prompt + self._prompt_sep
        buf = ''

        # expected return value:
        # <filename>,<filesize>\n<filedata>
        # or, if error,
        # <filename>,-1\n<error message>

        # just send the command first, we read the response inline below
        self._runCmds([{ 'cmd': 'pull ' + remoteFile }])

        # read metadata; buffer the rest
        metadata, sep, buf = read_until_char('\n', buf, 'could not find metadata')
        if not metadata:
            return None
        if self.debug >= 3:
            print 'metadata: %s' % metadata

        filename, sep, filesizestr = metadata.partition(',')
        if sep == '':
            err('could not find file size in returned metadata')
        try:
            filesize = int(filesizestr)
        except ValueError:
            err('invalid file size in returned metadata')

        if filesize == -1:
            # read error message
            error_str, sep, buf = read_until_char('\n', buf, 'could not find error message')
            if not error_str:
                err("blank error message")
            # prompt should follow
            read_exact(len(prompt), buf, 'could not find prompt')
            # failures are expected, so don't use "Remote Device Error" or we'll RETRY
            raise DMError("DeviceManager: pulling file '%s' unsuccessful: %s" % (remoteFile, error_str))

        # read file data
        total_to_recv = filesize + len(prompt)
        buf = read_exact(total_to_recv, buf, 'could not get all file data')
        if buf[-len(prompt):] != prompt:
            err('no prompt found after file data--DeviceManager may be out of sync with agent')
            return buf
        return buf[:-len(prompt)]

    def getFile(self, remoteFile, localFile):
        """
        Copy file from device (remoteFile) to host (localFile)
        """
        data = self.pullFile(remoteFile)

        fhandle = open(localFile, 'wb')
        fhandle.write(data)
        fhandle.close()
        if not self.validateFile(remoteFile, localFile):
            raise DMError("Automation Error: Failed to validate file when downloading %s" %
                          remoteFile)

    def getDirectory(self, remoteDir, localDir, checkDir=True):
        """
        Copy directory structure from device (remoteDir) to host (localDir)
        """
        if (self.debug >= 2):
            print "getting files in '" + remoteDir + "'"
        if checkDir and not self.dirExists(remoteDir):
            raise DMError("Automation Error: Error getting directory: %s not a directory" %
                          remoteDir)

        filelist = self.listFiles(remoteDir)
        if (self.debug >= 3):
            print filelist
        if not os.path.exists(localDir):
            os.makedirs(localDir)

        for f in filelist:
            if f == '.' or f == '..':
                continue
            remotePath = remoteDir + '/' + f
            localPath = os.path.join(localDir, f)
            if self.dirExists(remotePath):
                self.getDirectory(remotePath, localPath, False)
            else:
                self.getFile(remotePath, localPath)

    def validateFile(self, remoteFile, localFile):
        """
        Returns True if remoteFile has the same md5 hash as the localFile
        """
        remoteHash = self._getRemoteHash(remoteFile)
        localHash = self._getLocalHash(localFile)

        if (remoteHash == None):
            return False

        if (remoteHash == localHash):
            return True

        return False

    def _getRemoteHash(self, filename):
        """
        Return the md5 sum of a file on the device
        """
        data = self._runCmds([{ 'cmd': 'hash ' + filename }]).strip()
        if self.debug >= 3:
            print "remote hash returned: '%s'" % data
        return data

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
        if not self.deviceRoot:
            data = self._runCmds([{ 'cmd': 'testroot' }])
            self.deviceRoot = data.strip() + '/tests'

        if not self.dirExists(self.deviceRoot):
            self.mkDir(self.deviceRoot)

        return self.deviceRoot

    def getAppRoot(self, packageName):
        """
        Returns the app root directory

        E.g /tests/fennec or /tests/firefox
        """
        data = self._runCmds([{ 'cmd': 'getapproot ' + packageName }])

        return data.strip()

    def unpackFile(self, file_path, dest_dir=None):
        """
        Unzips a remote bundle to a remote location

        If dest_dir is not specified, the bundle is extracted
        in the same directory
        """
        devroot = self.getDeviceRoot()
        if (devroot == None):
            return None

        # if no dest_dir is passed in just set it to file_path's folder
        if not dest_dir:
            dest_dir = posixpath.dirname(file_path)

        if dest_dir[-1] != '/':
            dest_dir += '/'

        self._runCmds([{ 'cmd': 'unzp %s %s' % (file_path, dest_dir)}])

    def reboot(self, ipAddr=None, port=30000):
        """
        Reboots the device
        """
        cmd = 'rebt'

        if (self.debug > 3):
            print "INFO: sending rebt command"

        if (ipAddr is not None):
        #create update.info file:
            destname = '/data/data/com.mozilla.SUTAgentAndroid/files/update.info'
            data = "%s,%s\rrebooting\r" % (ipAddr, port)
            self._runCmds([{ 'cmd': 'push %s %s' % (destname, len(data)), 'data': data }])

            ip, port = self._getCallbackIpAndPort(ipAddr, port)
            cmd += " %s %s" % (ip, port)
            # Set up our callback server
            callbacksvr = callbackServer(ip, port, self.debug)

        status = self._runCmds([{ 'cmd': cmd }])

        if (ipAddr is not None):
            status = callbacksvr.disconnect()

        if (self.debug > 3):
            print "INFO: rebt- got status back: " + str(status)

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
        data = None
        result = {}
        collapseSpaces = re.compile('  +')

        directives = ['os','id','uptime','uptimemillis','systime','screen',
                                    'rotation','memory','process','disk','power']
        if (directive in directives):
            directives = [directive]

        for d in directives:
            data = self._runCmds([{ 'cmd': 'info ' + d }])

            data = collapseSpaces.sub(' ', data)
            result[d] = data.split('\n')

        # Get rid of any 0 length members of the arrays
        for k, v in result.iteritems():
            result[k] = filter(lambda x: x != '', result[k])

        # Format the process output
        if 'process' in result:
            proclist = []
            for l in result['process']:
                if l:
                    proclist.append(l.split('\t'))
            result['process'] = proclist

        if (self.debug >= 3):
            print "results: " + str(result)
        return result

    def installApp(self, appBundlePath, destPath=None):
        """
        Installs an application onto the device

        appBundlePath - path to the application bundle on the device
        destPath - destination directory of where application should be installed to (optional)
        """
        cmd = 'inst ' + appBundlePath
        if destPath:
            cmd += ' ' + destPath

        data = self._runCmds([{ 'cmd': cmd }])

        f = re.compile('Failure')
        for line in data.split():
            if (f.match(line)):
                raise DMError("Remove Device Error: Error installing app. Error message: %s" % data)

    def uninstallApp(self, appName, installPath=None):
        """
        Uninstalls the named application from device and DOES NOT cause a reboot

        appName - the name of the application (e.g org.mozilla.fennec)
        installPath - the path to where the application was installed (optional)
        """
        cmd = 'uninstall ' + appName
        if installPath:
            cmd += ' ' + installPath
        data = self._runCmds([{ 'cmd': cmd }])

        status = data.split('\n')[0].strip()
        if self.debug > 3:
            print "uninstallApp: '%s'" % status
        if status == 'Success':
            return
        raise DMError("Remote Device Error: uninstall failed for %s" % appName)

    def uninstallAppAndReboot(self, appName, installPath=None):
        """
        Uninstalls the named application from device and causes a reboot

        appName - the name of the application (e.g org.mozilla.fennec)
        installPath - the path to where the application was installed (optional)
        """
        cmd = 'uninst ' + appName
        if installPath:
            cmd += ' ' + installPath
        data = self._runCmds([{ 'cmd': cmd }])

        if (self.debug > 3):
            print "uninstallAppAndReboot: " + str(data)
        return

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
        status = None
        cmd = 'updt '
        if (processName == None):
            # Then we pass '' for processName
            cmd += "'' " + appBundlePath
        else:
            cmd += processName + ' ' + appBundlePath

        if (destPath):
            cmd += " " + destPath

        if (ipAddr is not None):
            ip, port = self._getCallbackIpAndPort(ipAddr, port)
            cmd += " %s %s" % (ip, port)
            # Set up our callback server
            callbacksvr = callbackServer(ip, port, self.debug)

        if (self.debug >= 3):
            print "INFO: updateApp using command: " + str(cmd)

        status = self._runCmds([{ 'cmd': cmd }])

        if ipAddr is not None:
            status = callbacksvr.disconnect()

        if (self.debug >= 3):
            print "INFO: updateApp: got status back: " + str(status)

    def getCurrentTime(self):
        """
        Returns device time in milliseconds since the epoch
        """
        return self._runCmds([{ 'cmd': 'clok' }]).strip()

    def _getCallbackIpAndPort(self, aIp, aPort):
        """
        Connect the ipaddress and port for a callback ping.

        Defaults to current IP address and ports starting at 30000.
        NOTE: the detection for current IP address only works on Linux!
        """
        ip = aIp
        nettools = NetworkTools()
        if (ip == None):
            ip = nettools.getLanIp()
        if (aPort != None):
            port = nettools.findOpenPort(ip, aPort)
        else:
            port = nettools.findOpenPort(ip, 30000)
        return ip, port

    def _formatEnvString(self, env):
        """
        Returns a properly formatted env string for the agent.

        Input - env, which is either None, '', or a dict
        Output - a quoted string of the form: '"envvar1=val1,envvar2=val2..."'
        If env is None or '' return '' (empty quoted string)
        """
        if (env == None or env == ''):
            return ''

        retVal = '"%s"' % ','.join(map(lambda x: '%s=%s' % (x[0], x[1]), env.iteritems()))
        if (retVal == '""'):
            return ''

        return retVal

    def adjustResolution(self, width=1680, height=1050, type='hdmi'):
        """
        adjust the screen resolution on the device, REBOOT REQUIRED

        NOTE: this only works on a tegra ATM

        supported resolutions: 640x480, 800x600, 1024x768, 1152x864, 1200x1024, 1440x900, 1680x1050, 1920x1080
        """
        if self.getInfo('os')['os'][0].split()[0] != 'harmony-eng':
            if (self.debug >= 2):
                print "WARNING: unable to adjust screen resolution on non Tegra device"
            return False

        results = self.getInfo('screen')
        parts = results['screen'][0].split(':')
        if (self.debug >= 3):
            print "INFO: we have a current resolution of %s, %s" % (parts[1].split()[0], parts[2].split()[0])

        #verify screen type is valid, and set it to the proper value (https://bugzilla.mozilla.org/show_bug.cgi?id=632895#c4)
        screentype = -1
        if (type == 'hdmi'):
            screentype = 5
        elif (type == 'vga' or type == 'crt'):
            screentype = 3
        else:
            return False

        #verify we have numbers
        if not (isinstance(width, int) and isinstance(height, int)):
            return False

        if (width < 100 or width > 9999):
            return False

        if (height < 100 or height > 9999):
            return False

        if (self.debug >= 3):
            print "INFO: adjusting screen resolution to %s, %s and rebooting" % (width, height)

        self._runCmds([{ 'cmd': "exec setprop persist.tegra.dpy%s.mode.width %s" % (screentype, width) }])
        self._runCmds([{ 'cmd': "exec setprop persist.tegra.dpy%s.mode.height %s" % (screentype, height) }])

    def chmodDir(self, remoteDir, **kwargs):
        """
        Recursively changes file permissions in a directory
        """
        self._runCmds([{ 'cmd': "chmod "+remoteDir }])

gCallbackData = ''

class myServer(SocketServer.TCPServer):
    allow_reuse_address = True

class callbackServer():
    def __init__(self, ip, port, debuglevel):
        global gCallbackData
        if (debuglevel >= 1):
            print "DEBUG: gCallbackData is: %s on port: %s" % (gCallbackData, port)
        gCallbackData = ''
        self.ip = ip
        self.port = port
        self.connected = False
        self.debug = debuglevel
        if (self.debug >= 3):
            print "Creating server with " + str(ip) + ":" + str(port)
        self.server = myServer((ip, port), self.myhandler)
        self.server_thread = Thread(target=self.server.serve_forever)
        self.server_thread.setDaemon(True)
        self.server_thread.start()

    def disconnect(self, step = 60, timeout = 600):
        t = 0
        if (self.debug >= 3):
            print "Calling disconnect on callback server"
        while t < timeout:
            if (gCallbackData):
                # Got the data back
                if (self.debug >= 3):
                    print "Got data back from agent: " + str(gCallbackData)
                break
            else:
                if (self.debug >= 0):
                    print '.',
            time.sleep(step)
            t += step

        try:
            if (self.debug >= 3):
                print "Shutting down server now"
            self.server.shutdown()
        except:
            if (self.debug >= 1):
                print "Automation Error: Unable to shutdown callback server - check for a connection on port: " + str(self.port)

        #sleep 1 additional step to ensure not only we are online, but all our services are online
        time.sleep(step)
        return gCallbackData

    class myhandler(SocketServer.BaseRequestHandler):
        def handle(self):
            global gCallbackData
            gCallbackData = self.request.recv(1024)
            #print "Callback Handler got data: " + str(gCallbackData)
            self.request.send("OK")
