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
from devicemanager import DeviceManager, FileError, DMError, NetworkTools, _pop_last_line
import errno
from distutils.version import StrictVersion

class AgentError(Exception):
    "SUTAgent-specific exception."

    def __init__(self, msg= '', fatal = False):
        self.msg = msg
        self.fatal = fatal

    def __str__(self):
        return self.msg

class DeviceManagerSUT(DeviceManager):
    debug = 2
    tempRoot = os.getcwd()
    base_prompt = '$>'
    base_prompt_re = '\$\>'
    prompt_sep = '\x00'
    prompt_regex = '.*(' + base_prompt_re + prompt_sep + ')'
    agentErrorRE = re.compile('^##AGENT-WARNING##\ ?(.*)')
    default_timeout = 300

    # TODO: member variable to indicate error conditions.
    # This should be set to a standard error from the errno module.
    # So, for example, when an error occurs because of a missing file/directory,
    # before returning, the function would do something like 'self.error = errno.ENOENT'.
    # The error would be set where appropriate--so sendCMD() could set socket errors,
    # pushFile() and other file-related commands could set filesystem errors, etc.

    def __init__(self, host, port = 20701, retrylimit = 5, deviceRoot = None):
        self.host = host
        self.port = port
        self.retrylimit = retrylimit
        self._sock = None
        self.deviceRoot = deviceRoot
        if self.getDeviceRoot() == None:
            raise BaseException("Failed to connect to SUT Agent and retrieve the device root.")
        try:
            verstring = self._runCmds([{ 'cmd': 'ver' }])
            self.agentVersion = re.sub('SUTAgentAndroid Version ', '', verstring)
        except AgentError, err:
            raise BaseException("Failed to get SUTAgent version")

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
        internal function
        take a data blob and strip instances of the prompt '$>\x00'
        """
        promptre = re.compile(self.prompt_regex + '.*')
        retVal = []
        lines = data.split('\n')
        for line in lines:
            foundPrompt = False
            try:
                while (promptre.match(line)):
                    foundPrompt = True
                    pieces = line.split(self.prompt_sep)
                    index = pieces.index('$>')
                    pieces.pop(index)
                    line = self.prompt_sep.join(pieces)
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

    def _sendCmds(self, cmdlist, outputfile, timeout = None):
        """
        Wrapper for _doCmds that loops up to self.retrylimit iterations
        """
        # this allows us to move the retry logic outside of the _doCmds() to make it
        # easier for debugging in the future.
        # note that since cmdlist is a list of commands, they will all be retried if
        # one fails.  this is necessary in particular for pushFile(), where we don't want
        # to accidentally send extra data if a failure occurs during data transmission.

        retries = 0
        while retries < self.retrylimit:
            try:
                self._doCmds(cmdlist, outputfile, timeout)
                return
            except AgentError, err:
                # re-raise error if it's fatal (i.e. the device got the command but
                # couldn't execute it). retry otherwise
                if err.fatal:
                    raise err
                if self.debug >= 2:
                    print err
                retries += 1
                # if we lost the connection or failed to establish one, wait a bit
                if retries < self.retrylimit and not self._sock:
                    sleep_time = 5 * retries
                    print 'Could not connect; sleeping for %d seconds.' % sleep_time
                    time.sleep(sleep_time)

        raise AgentError("Remote Device Error: unable to connect to %s after %s attempts" % (self.host, self.retrylimit))

    def _runCmds(self, cmdlist, timeout = None):
        """
        Similar to _sendCmds, but just returns any output as a string instead of
        writing to a file
        """
        outputfile = StringIO.StringIO()
        self._sendCmds(cmdlist, outputfile, timeout)
        outputfile.seek(0)
        return outputfile.read()

    def _doCmds(self, cmdlist, outputfile, timeout):
        promptre = re.compile(self.prompt_regex + '$')
        shouldCloseSocket = False

        if not timeout:
            # We are asserting that all commands will complete in this time unless otherwise specified
            timeout = self.default_timeout

        if not self._sock:
            try:
                if self.debug >= 1:
                    print "reconnecting socket"
                self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            except socket.error, msg:
                self._sock = None
                raise AgentError("Automation Error: unable to create socket: "+str(msg))

            try:
                self._sock.connect((self.host, int(self.port)))
                if select.select([self._sock], [], [], timeout)[0]:
                    self._sock.recv(1024)
                else:
                    raise AgentError("Remote Device Error: Timeout in connecting", fatal=True)
                    return False
            except socket.error, msg:
                self._sock.close()
                self._sock = None
                raise AgentError("Remote Device Error: unable to connect socket: "+str(msg))

        for cmd in cmdlist:
            cmdline = '%s\r\n' % cmd['cmd']

            try:
                sent = self._sock.send(cmdline)
                if sent != len(cmdline):
                    raise AgentError("ERROR: our cmd was %s bytes and we "
                                                      "only sent %s" % (len(cmdline), sent))
                if cmd.get('data'):
                    sent = self._sock.send(cmd['data'])
                    if sent != len(cmd['data']):
                            raise AgentError("ERROR: we had %s bytes of data to send, but "
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
                            raise AgentError("Automation Error: Timeout in command %s" % cmd['cmd'], fatal=True)
                    except socket.error, err:
                        socketClosed = True
                        errStr = str(err)
                        # This error shows up with we have our tegra rebooted.
                        if err[0] == errno.ECONNRESET:
                            errStr += ' - possible reboot'

                    if socketClosed:
                        self._sock.close()
                        self._sock = None
                        raise AgentError("Automation Error: Error receiving data from socket. cmd=%s; err=%s" % (cmd, errStr))

                    data += temp

                    # If something goes wrong in the agent it will send back a string that
                    # starts with '##AGENT-WARNING##'
                    if not commandFailed:
                        errorMatch = self.agentErrorRE.match(data)
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
                    raise AgentError("Automation Error: Agent Error processing command '%s'; err='%s'" %
                                                      (cmd['cmd'], errorMatch.group(1)), fatal=True)

                # Write any remaining data to outputfile
                outputfile.write(data)

        if shouldCloseSocket:
            try:
                self._sock.close()
                self._sock = None
            except:
                self._sock = None
                raise AgentError("Automation Error: Error closing socket")

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

        try:
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
        except AgentError:
            return None

        # dig through the output to get the return code
        lastline = _pop_last_line(outputfile)
        if lastline:
            m = re.search('return code \[([0-9]+)\]', lastline)
            if m:
                return int(m.group(1))

        # woops, we couldn't find an end of line/return value
        return None

    def pushFile(self, localname, destname):
        """
        Copies localname from the host to destname on the device

        returns:
          success: True
          failure: False
        """
        if (os.name == "nt"):
            destname = destname.replace('\\', '/')

        if (self.debug >= 3):
            print "in push file with: " + localname + ", and: " + destname
        if (self.dirExists(destname)):
            if (not destname.endswith('/')):
                destname = destname + '/'
            destname = destname + os.path.basename(localname)
        if (self.validateFile(destname, localname) == True):
            if (self.debug >= 3):
                print "files are validated"
            return True

        if self.mkDirs(destname) == None:
            print "Automation Error: unable to make dirs: " + destname
            return False

        if (self.debug >= 3):
            print "sending: push " + destname

        filesize = os.path.getsize(localname)
        f = open(localname, 'rb')
        data = f.read()
        f.close()

        try:
            retVal = self._runCmds([{ 'cmd': 'push ' + destname + ' ' + str(filesize),
                                                              'data': data }])
        except AgentError, e:
            print "Automation Error: error pushing file: %s" % e.msg
            return False

        if (self.debug >= 3):
            print "push returned: " + str(retVal)

        validated = False
        if (retVal):
            retline = retVal.strip()
            if (retline == None):
                # Then we failed to get back a hash from agent, try manual validation
                validated = self.validateFile(destname, localname)
            else:
                # Then we obtained a hash from push
                localHash = self._getLocalHash(localname)
                if (str(localHash) == str(retline)):
                    validated = True
        else:
            # We got nothing back from sendCMD, try manual validation
            validated = self.validateFile(destname, localname)

        if (validated):
            if (self.debug >= 3):
                print "Push File Validated!"
            return True
        else:
            if (self.debug >= 2):
                print "Automation Error: Push File Failed to Validate!"
            return False

    def mkDir(self, name):
        """
        Creates a single directory on the device file system

        returns:
          success: directory name
          failure: None
        """
        if (self.dirExists(name)):
            return name
        else:
            try:
                retVal = self._runCmds([{ 'cmd': 'mkdr ' + name }])
            except AgentError:
                retVal = None
            return retVal

    def pushDir(self, localDir, remoteDir):
        """
        Push localDir from host to remoteDir on the device

        returns:
          success: remoteDir
          failure: None
        """
        if (self.debug >= 2):
            print "pushing directory: %s to %s" % (localDir, remoteDir)
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
                if (self.pushFile(os.path.join(root, f), remoteName) == False):
                    # retry once
                    self.removeFile(remoteName)
                    if (self.pushFile(os.path.join(root, f), remoteName) == False):
                        return None
        return remoteDir

    def dirExists(self, dirname):
        """
        Checks if dirname exists and is a directory
        on the device file system

        returns:
          success: True
          failure: False
        """
        match = ".*" + dirname.replace('^', '\^') + "$"
        dirre = re.compile(match)
        try:
            data = self._runCmds([ { 'cmd': 'cd ' + dirname }, { 'cmd': 'cwd' }])
        except AgentError:
            return False

        found = False
        for d in data.splitlines():
            if (dirre.match(d)):
                found = True

        return found

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
        s = filepath.split('/')
        containingpath = '/'.join(s[:-1])
        listfiles = self.listFiles(containingpath)
        for f in listfiles:
            if (f == s[-1]):
                return True
        return False

    def listFiles(self, rootdir):
        """
        Lists files on the device rootdir

        returns:
          success: array of filenames, ['file1', 'file2', ...]
          failure: None
        """
        rootdir = rootdir.rstrip('/')
        if (self.dirExists(rootdir) == False):
            return []
        try:
            data = self._runCmds([{ 'cmd': 'cd ' + rootdir }, { 'cmd': 'ls' }])
        except AgentError:
            return []

        files = filter(lambda x: x, data.splitlines())
        if len(files) == 1 and files[0] == '<empty>':
            # special case on the agent: empty directories return just the string "<empty>"
            return []
        return files

    def removeFile(self, filename):
        """
        Removes filename from the device

        returns:
          success: output of telnet
          failure: None
        """
        if (self.debug>= 2):
            print "removing file: " + filename
        try:
            retVal = self._runCmds([{ 'cmd': 'rm ' + filename }])
        except AgentError:
            return None

        return retVal

    def removeDir(self, remoteDir):
        """
        Does a recursive delete of directory on the device: rm -Rf remoteDir

        returns:
          success: output of telnet
          failure: None
        """
        try:
            retVal = self._runCmds([{ 'cmd': 'rmdr ' + remoteDir }])
        except AgentError:
            return None

        return retVal

    def getProcessList(self):
        """
        Lists the running processes on the device

        returns:
          success: array of process tuples
          failure: []
        """
        try:
            data = self._runCmds([{ 'cmd': 'ps' }])
        except AgentError:
            return []

        files = []
        for line in data.splitlines():
            if line:
                pidproc = line.strip().split()
                if (len(pidproc) == 2):
                    files += [[pidproc[0], pidproc[1]]]
                elif (len(pidproc) == 3):
                    #android returns <userID> <procID> <procName>
                    files += [[pidproc[1], pidproc[2], pidproc[0]]]
        return files

    def fireProcess(self, appname, failIfRunning=False):
        """
        DEPRECATED: Use shell() or launchApplication() for new code

        returns:
          success: pid
          failure: None
        """
        if (not appname):
            if (self.debug >= 1):
                print "WARNING: fireProcess called with no command to run"
            return None

        if (self.debug >= 2):
            print "FIRE PROC: '" + appname + "'"

        if (self.processExist(appname) != None):
            print "WARNING: process %s appears to be running already\n" % appname
            if (failIfRunning):
                return None

        try:
            self._runCmds([{ 'cmd': 'exec ' + appname }])
        except AgentError:
            return None

        # The 'exec' command may wait for the process to start and end, so checking
        # for the process here may result in process = None.
        process = self.processExist(appname)
        if (self.debug >= 4):
            print "got pid: %s for process: %s" % (process, appname)

        return process

    def launchProcess(self, cmd, outputFile = "process.txt", cwd = '', env = '', failIfRunning=False):
        """
        DEPRECATED: Use shell() or launchApplication() for new code

        returns:
          success: output filename
          failure: None
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

        if self.fireProcess(cmdline, failIfRunning) is None:
            return None
        return outputFile

    def killProcess(self, appname, forceKill=False):
        """
        Kills the process named appname.
        If forceKill is True, process is killed regardless of state

        returns:
          success: True
          failure: False
        """
        if forceKill:
            print "WARNING: killProcess(): forceKill parameter unsupported on SUT"
        try:
            self._runCmds([{ 'cmd': 'kill ' + appname }])
        except AgentError:
            return False

        return True

    def getTempDir(self):
        """
        Gets the temporary directory we are using on this device
        base on our device root, ensuring also that it exists.

        returns:
          success: path for temporary directory
          failure: None
        """
        try:
            data = self._runCmds([{ 'cmd': 'tmpd' }])
        except AgentError:
            return None

        return data.strip()

    def catFile(self, remoteFile):
        """
        Returns the contents of remoteFile

        returns:
          success: filecontents, string
          failure: None
        """
        try:
            data = self._runCmds([{ 'cmd': 'cat ' + remoteFile }])
        except AgentError:
            return None

        return data

    def pullFile(self, remoteFile):
        """
        Returns contents of remoteFile using the "pull" command.

        returns:
          success: output of pullfile, string
          failure: None
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
            raise FileError(err_str)

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
                    return None

                if not data:
                    err(error_msg)
                    return None
                return data
            except:
                err(error_msg)
                return None

        def read_until_char(c, buf, error_msg):
            """ read until 'c' is found; buffer rest """
            while not '\n' in buf:
                data = uread(1024, error_msg)
                if data == None:
                    err(error_msg)
                    return ('', '', '')
                buf += data
            return buf.partition(c)

        def read_exact(total_to_recv, buf, error_msg):
            """ read exact number of 'total_to_recv' bytes """
            while len(buf) < total_to_recv:
                to_recv = min(total_to_recv - len(buf), 1024)
                data = uread(to_recv, error_msg)
                if data == None:
                    return None
                buf += data
            return buf

        prompt = self.base_prompt + self.prompt_sep
        buf = ''

        # expected return value:
        # <filename>,<filesize>\n<filedata>
        # or, if error,
        # <filename>,-1\n<error message>
        try:
            # just send the command first, we read the response inline below
            self._runCmds([{ 'cmd': 'pull ' + remoteFile }])
        except AgentError:
            return None

        # read metadata; buffer the rest
        metadata, sep, buf = read_until_char('\n', buf, 'could not find metadata')
        if not metadata:
            return None
        if self.debug >= 3:
            print 'metadata: %s' % metadata

        filename, sep, filesizestr = metadata.partition(',')
        if sep == '':
            err('could not find file size in returned metadata')
            return None
        try:
            filesize = int(filesizestr)
        except ValueError:
            err('invalid file size in returned metadata')
            return None

        if filesize == -1:
            # read error message
            error_str, sep, buf = read_until_char('\n', buf, 'could not find error message')
            if not error_str:
                return None
            # prompt should follow
            read_exact(len(prompt), buf, 'could not find prompt')
            # failures are expected, so don't use "Remote Device Error" or we'll RETRY
            print "DeviceManager: pulling file '%s' unsuccessful: %s" % (remoteFile, error_str)
            return None

        # read file data
        total_to_recv = filesize + len(prompt)
        buf = read_exact(total_to_recv, buf, 'could not get all file data')
        if buf == None:
            return None
        if buf[-len(prompt):] != prompt:
            err('no prompt found after file data--DeviceManager may be out of sync with agent')
            return buf
        return buf[:-len(prompt)]

    def getFile(self, remoteFile, localFile = ''):
        """
        Copy file from device (remoteFile) to host (localFile)

        returns:
          success: contents of file, string
          failure: None
        """
        if localFile == '':
            localFile = os.path.join(self.tempRoot, "temp.txt")

        try:
            retVal = self.pullFile(remoteFile)
        except:
            return None

        if (retVal is None):
            return None

        fhandle = open(localFile, 'wb')
        fhandle.write(retVal)
        fhandle.close()
        if not self.validateFile(remoteFile, localFile):
            print 'DeviceManager: failed to validate file when downloading %s' % remoteFile
            return None
        return retVal

    def getDirectory(self, remoteDir, localDir, checkDir=True):
        """
        Copy directory structure from device (remoteDir) to host (localDir)

        returns:
          success: list of files, string
          failure: None
        """
        if (self.debug >= 2):
            print "getting files in '" + remoteDir + "'"
        if checkDir:
            try:
                is_dir = self.isDir(remoteDir)
            except FileError:
                return None
            if not is_dir:
                return None

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
            try:
                is_dir = self.isDir(remotePath)
            except FileError:
                print 'isdir failed on file "%s"; continuing anyway...' % remotePath
                continue
            if is_dir:
                if (self.getDirectory(remotePath, localPath, False) == None):
                    print 'Remote Device Error: failed to get directory "%s"' % remotePath
                    return None
            else:
                # It's sometimes acceptable to have getFile() return None, such as
                # when the agent encounters broken symlinks.
                # FIXME: This should be improved so we know when a file transfer really
                # failed.
                if self.getFile(remotePath, localPath) == None:
                    print 'failed to get file "%s"; continuing anyway...' % remotePath
        return filelist

    def isDir(self, remotePath):
        """
        Checks if remotePath is a directory on the device

        returns:
          success: True
          failure: False
        """
        try:
            data = self._runCmds([{ 'cmd': 'isdir ' + remotePath }])
        except AgentError:
            # normally there should be no error here; a nonexistent file/directory will
            # return the string "<filename>: No such file or directory".
            # However, I've seen AGENT-WARNING returned before.
            return False

        retVal = data.strip()
        if not retVal:
            raise FileError('isdir returned null')
        return retVal == 'TRUE'

    def validateFile(self, remoteFile, localFile):
        """
        Checks if the remoteFile has the same md5 hash as the localFile

        returns:
          success: True
          failure: False
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

        returns:
          success: MD5 hash for given filename
          failure: None
        """
        try:
            data = self._runCmds([{ 'cmd': 'hash ' + filename }])
        except AgentError:
            return None

        retVal = None
        if data:
            retVal = data.strip()
        if self.debug >= 3:
            print "remote hash returned: '%s'" % retVal
        return retVal

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
        if self.deviceRoot:
            deviceRoot = self.deviceRoot
        else:
            try:
                data = self._runCmds([{ 'cmd': 'testroot' }])
            except:
                return None

            deviceRoot = data.strip() + '/tests'

        if (not self.dirExists(deviceRoot)):
            if (self.mkDir(deviceRoot) == None):
                return None

        self.deviceRoot = deviceRoot
        return self.deviceRoot

    def getAppRoot(self, packageName):
        """
        Returns the app root directory
        E.g /tests/fennec or /tests/firefox

        returns:
          success: path for app root
          failure: None
        """
        try:
            data = self._runCmds([{ 'cmd': 'getapproot ' + packageName }])
        except:
            return None

        return data.strip()

    def unpackFile(self, file_path, dest_dir=None):
        """
        Unzips a remote bundle to a remote location
        If dest_dir is not specified, the bundle is extracted
        in the same directory

        returns:
          success: output of unzip command
          failure: None
        """
        devroot = self.getDeviceRoot()
        if (devroot == None):
            return None

        # if no dest_dir is passed in just set it to file_path's folder
        if not dest_dir:
            dest_dir = posixpath.dirname(file_path)

        if dest_dir[-1] != '/':
            dest_dir += '/'

        try:
            data = self._runCmds([{ 'cmd': 'unzp %s %s' % (file_path, dest_dir)}])
        except AgentError:
            return None

        return data

    def reboot(self, ipAddr=None, port=30000):
        """
        Reboots the device

        returns:
          success: status from test agent
          failure: None
        """
        cmd = 'rebt'

        if (self.debug > 3):
            print "INFO: sending rebt command"

        if (ipAddr is not None):
        #create update.info file:
            try:
                destname = '/data/data/com.mozilla.SUTAgentAndroid/files/update.info'
                data = "%s,%s\rrebooting\r" % (ipAddr, port)
                self._runCmds([{ 'cmd': 'push %s %s' % (destname, len(data)), 'data': data }])
            except AgentError:
                return None

            ip, port = self._getCallbackIpAndPort(ipAddr, port)
            cmd += " %s %s" % (ip, port)
            # Set up our callback server
            callbacksvr = callbackServer(ip, port, self.debug)

        try:
            status = self._runCmds([{ 'cmd': cmd }])
        except AgentError:
            return None

        if (ipAddr is not None):
            status = callbacksvr.disconnect()

        if (self.debug > 3):
            print "INFO: rebt- got status back: " + str(status)
        return status

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

        returns:
          success: dict of info strings by directive name
          failure: None
        """
        data = None
        result = {}
        collapseSpaces = re.compile('  +')

        directives = ['os','id','uptime','uptimemillis','systime','screen',
                                    'rotation','memory','process','disk','power']
        if (directive in directives):
            directives = [directive]

        for d in directives:
            try:
                data = self._runCmds([{ 'cmd': 'info ' + d }])
            except AgentError:
                return result

            if (data is None):
                continue
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

        returns:
          success: None
          failure: error string
        """
        cmd = 'inst ' + appBundlePath
        if destPath:
            cmd += ' ' + destPath

        try:
            data = self._runCmds([{ 'cmd': cmd }])
        except AgentError, err:
            print "Remote Device Error: Error installing app: %s" % err
            return "%s" % err

        f = re.compile('Failure')
        for line in data.split():
            if (f.match(line)):
                return line
        return None

    def uninstallApp(self, appName, installPath=None):
        """
        Uninstalls the named application from device and DOES NOT cause a reboot
        appName - the name of the application (e.g org.mozilla.fennec)
        installPath - the path to where the application was installed (optional)

        returns:
          success: None
          failure: DMError exception thrown
        """
        cmd = 'uninstall ' + appName
        if installPath:
            cmd += ' ' + installPath
        try:
            data = self._runCmds([{ 'cmd': cmd }])
        except AgentError, err:
            raise DMError("Remote Device Error: Error uninstalling all %s" % appName)

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

        returns:
          success: None
          failure: DMError exception thrown
        """
        cmd = 'uninst ' + appName
        if installPath:
            cmd += ' ' + installPath
        try:
            data = self._runCmds([{ 'cmd': cmd }])
        except AgentError:
            raise DMError("Remote Device Error: uninstall failed for %s" % appName)

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

        returns:
          success: text status from command or callback server
          failure: None
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

        try:
            status = self._runCmds([{ 'cmd': cmd }])
        except AgentError:
            return None

        if ipAddr is not None:
            status = callbacksvr.disconnect()

        if (self.debug >= 3):
            print "INFO: updateApp: got status back: " + str(status)

        return status

    def getCurrentTime(self):
        """
        Returns device time in milliseconds since the epoch

        returns:
          success: time in ms
          failure: None
        """
        try:
            data = self._runCmds([{ 'cmd': 'clok' }])
        except AgentError:
            return None

        return data.strip()

    def _getCallbackIpAndPort(self, aIp, aPort):
        """
        Connect the ipaddress and port for a callback ping.  Defaults to current IP address
        And ports starting at 30000.
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
        return:
          success: True
          failure: False

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
        try:
            self._runCmds([{ 'cmd': "exec setprop persist.tegra.dpy%s.mode.width %s" % (screentype, width) }])
            self._runCmds([{ 'cmd': "exec setprop persist.tegra.dpy%s.mode.height %s" % (screentype, height) }])
        except AgentError:
            return False

        return True

    def chmodDir(self, remoteDir, **kwargs):
        """
        Recursively changes file permissions in a directory

        returns:
          success: True
          failure: False
        """
        try:
            self._runCmds([{ 'cmd': "chmod "+remoteDir }])
        except AgentError:
            return False
        return True

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
