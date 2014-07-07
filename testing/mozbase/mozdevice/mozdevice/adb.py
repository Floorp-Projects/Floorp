# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import logging
import os
import posixpath
import re
import subprocess
import tempfile
import time

class ADBProcess(object):
    """ADBProcess encapsulates the data related to executing the adb process.

    """
    def __init__(self, args):
        #: command argument argument list.
        self.args = args
        #: Temporary file handle to be used for stdout.
        self.stdout_file = tempfile.TemporaryFile()
        #: Temporary file handle to be used for stderr.
        self.stderr_file = tempfile.TemporaryFile()
        #: boolean indicating if the command timed out.
        self.timedout = None
        #: exitcode of the process.
        self.exitcode = None
        #: subprocess Process object used to execute the command.
        self.proc = subprocess.Popen(args,
                                     stdout=self.stdout_file,
                                     stderr=self.stderr_file)

    @property
    def stdout(self):
        """Return the contents of stdout."""
        if not self.stdout_file or self.stdout_file.closed:
            content = ""
        else:
            self.stdout_file.seek(0, os.SEEK_SET)
            content = self.stdout_file.read().rstrip()
        return content

    @property
    def stderr(self):
        """Return the contents of stderr."""
        if not self.stderr_file or self.stderr_file.closed:
            content = ""
        else:
            self.stderr_file.seek(0, os.SEEK_SET)
            content = self.stderr_file.read().rstrip()
        return content

    def __str__(self):
        return ('args: %s, exitcode: %s, stdout: %s, stderr: %s' % (
            ' '.join(self.args), self.exitcode, self.stdout, self.stderr))

# ADBError, ADBRootError, and ADBTimeoutError are treated
# differently in order that unhandled ADBRootErrors and
# ADBTimeoutErrors can be handled distinctly from ADBErrors.

class ADBError(Exception):
    """ADBError is raised in situations where a command executed on a
    device either exited with a non-zero exitcode or when an
    unexpected error condition has occurred. Generally, ADBErrors can
    be handled and the device can continue to be used.

    """
    pass

class ADBRootError(Exception):
    """ADBRootError is raised when a shell command is to be executed as
    root but the device does not support it. This error is fatal since
    there is no recovery possible by the script. You must either root
    your device or change your scripts to not require running as root.

    """
    pass

class ADBTimeoutError(Exception):
    """ADBTimeoutError is raised when either a host command or shell
    command takes longer than the specified timeout to execute. The
    timeout value is set in the ADBCommand constructor and is 300 seconds by
    default. This error is typically fatal since the host is having
    problems communicating with the device. You may be able to recover
    by rebooting, but this is not guaranteed.

    Recovery options are:

    * Killing and restarting the adb server via
      ::

          adb kill-server; adb start-server

    * Rebooting the device manually.
    * Rebooting the host.

    """
    pass


class ADBCommand(object):
    """ADBCommand provides a basic interface to adb commands
    which is used to provide the 'command' methods for the
    classes ADBHost and ADBDevice.

    ADBCommand should only be used as the base class for other
    classes and should not be instantiated directly. To enforce this
    restriction calling ADBCommand's constructor will raise a
    NonImplementedError exception.

    ::

       from mozdevice import ADBCommand

       try:
           adbcommand = ADBCommand(...)
       except NotImplementedError:
           print "ADBCommand can not be instantiated."

    """

    def __init__(self,
                 adb='adb',
                 logger_name='adb',
                 log_level=logging.INFO,
                 timeout=300):
        """Initializes the ADBCommand object.

        :param adb: path to adb executable. Defaults to 'adb'.
        :param logger_name: logging logger name. Defaults to 'adb'.
        :param log_level: logging level. Defaults to logging.INFO.

        :raises: * ADBError
                 * ADBTimeoutError

        """
        if self.__class__ == ADBCommand:
            raise NotImplementedError

        self._logger = logging.getLogger(logger_name)
        self._adb_path = adb
        self._log_level = log_level
        self._timeout = timeout
        self._polling_interval = 0.1

        self._logger.debug("%s: %s" % (self.__class__.__name__,
                                       self.__dict__))

    # Host Command methods

    def command(self, cmds, device_serial=None, timeout=None):
        """Executes an adb command on the host.

        :param cmds: list containing the command and its arguments to be
            executed.
        :param device_serial: optional string specifying the device's
            serial number if the adb command is to be executed against
            a specific device.
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.  This timeout is per adb call. The
            total time spent may exceed this value. If it is not
            specified, the value set in the ADBCommand constructor is used.
        :returns: :class:`mozdevice.ADBProcess`

        command() provides a low level interface for executing
        commands on the host via adb.

        command() executes on the host in such a fashion that stdout
        and stderr of the adb process are file handles on the host and
        the exit code is available as the exit code of the adb
        process.

        The caller provides a list containing commands, as well as a
        timeout period in seconds.

        A subprocess is spawned to execute adb with stdout and stderr
        directed to temporary files. If the process takes longer than
        the specified timeout, the process is terminated.

        It is the caller's responsibilty to clean up by closing
        the stdout and stderr temporary files.

        """
        args = [self._adb_path]
        if device_serial:
            args.extend(['-s', device_serial, 'wait-for-device'])
        args.extend(cmds)

        adb_process = ADBProcess(args)

        if timeout is None:
            timeout = self._timeout

        start_time = time.time()
        adb_process.exitcode = adb_process.proc.poll()
        while ((time.time() - start_time) <= timeout and
               adb_process.exitcode == None):
            time.sleep(self._polling_interval)
            adb_process.exitcode = adb_process.proc.poll()
        if adb_process.exitcode == None:
            adb_process.proc.kill()
            adb_process.timedout = True
            adb_process.exitcode = adb_process.proc.poll()

        adb_process.stdout_file.seek(0, os.SEEK_SET)
        adb_process.stderr_file.seek(0, os.SEEK_SET)

        return adb_process

    def command_output(self, cmds, device_serial=None, timeout=None):
        """Executes an adb command on the host returning stdout.

        :param cmds: list containing the command and its arguments to be
            executed.
        :param device_serial: optional string specifying the device's
            serial number if the adb command is to be executed against
            a specific device.
        :param timeout: optional integer specifying the maximum time in seconds
            for any spawned adb process to complete before throwing
            an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBCommand constructor is used.
        :returns: string - content of stdout.

        :raises: * ADBTimeoutError - raised if the command takes longer than
                   timeout seconds.
                 * ADBError - raised if the command exits with a
                   non-zero exit code.

        """
        adb_process = None
        try:
            # Need to force the use of the ADBCommand class's command
            # since ADBDevice will redefine command and call its
            # own version otherwise.
            adb_process = ADBCommand.command(self, cmds,
                                             device_serial=device_serial,
                                             timeout=timeout)
            if adb_process.timedout:
                raise ADBTimeoutError("%s" % adb_process)
            elif adb_process.exitcode:
                raise ADBError("%s" % adb_process)
            output = adb_process.stdout_file.read().rstrip()
            self._logger.debug('command_output: %s, '
                               'timeout: %s, '
                               'timedout: %s, '
                               'exitcode: %s, output: %s' %
                               (' '.join(adb_process.args),
                                timeout,
                                adb_process.timedout,
                                adb_process.exitcode,
                                output))
            return output
        finally:
            if adb_process and isinstance(adb_process.stdout_file, file):
                adb_process.stdout_file.close()
                adb_process.stderr_file.close()


class ADBHost(ADBCommand):
    """ADBHost provides a basic interface to adb host commands
    which do not target a specific device.

    ::

       from mozdevice import ADBHost

       adbhost = ADBHost(...)
       adbhost.start_server()

    """
    def __init__(self,
                 adb='adb',
                 logger_name='adb',
                 log_level=logging.INFO,
                 timeout=300):
        """Initializes the ADBHost object.

        :param adb: path to adb executable. Defaults to 'adb'.
        :param logger_name: logging logger name. Defaults to 'adb'.
        :param log_level: logging level. Defaults to logging.INFO.

        :raises: * ADBError
                 * ADBTimeoutError

        """
        ADBCommand.__init__(self, adb=adb, logger_name=logger_name,
                            log_level=log_level, timeout=timeout)

    def command(self, cmds, timeout=None):
        """Executes an adb command on the host.

        :param cmds: list containing the command and its arguments to be
            executed.
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.  This timeout is per adb call. The
            total time spent may exceed this value. If it is not
            specified, the value set in the ADBHost constructor is used.
        :returns: :class:`mozdevice.ADBProcess`

        command() provides a low level interface for executing
        commands on the host via adb.

        command() executes on the host in such a fashion that stdout
        and stderr of the adb process are file handles on the host and
        the exit code is available as the exit code of the adb
        process.

        The caller provides a list containing commands, as well as a
        timeout period in seconds.

        A subprocess is spawned to execute adb with stdout and stderr
        directed to temporary files. If the process takes longer than
        the specified timeout, the process is terminated.

        It is the caller's responsibilty to clean up by closing
        the stdout and stderr temporary files.

        """
        return ADBCommand.command(self, cmds, timeout=timeout)

    def command_output(self, cmds, timeout=None):
        """Executes an adb command on the host returning stdout.

        :param cmds: list containing the command and its arguments to be
            executed.
        :param timeout: optional integer specifying the maximum time in seconds
            for any spawned adb process to complete before throwing
            an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBHost constructor is used.
        :returns: string - content of stdout.

        :raises: * ADBTimeoutError - raised if the command takes longer than
                   timeout seconds.
                 * ADBError - raised if the command exits with a
                   non-zero exit code.

        """
        return ADBCommand.command_output(self, cmds, timeout=timeout)

    def start_server(self, timeout=None):
        """Starts the adb server.

        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.  This timeout is per adb call. The
            total time spent may exceed this value. If it is not
            specified, the value set in the ADBHost constructor is used.
        :raises: * ADBTimeoutError - raised if the command takes longer than
                   timeout seconds.
                 * ADBError - raised if the command exits with a
                   non-zero exit code.

        """
        self.command_output(["start-server"], timeout=timeout)

    def kill_server(self, timeout=None):
        """Kills the adb server.

        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.  This timeout is per adb call. The
            total time spent may exceed this value. If it is not
            specified, the value set in the ADBHost constructor is used.
        :raises: * ADBTimeoutError - raised if the command takes longer than
                   timeout seconds.
                 * ADBError - raised if the command exits with a
                   non-zero exit code.

        """
        self.command_output(["kill-server"], timeout=timeout)

    def devices(self, timeout=None):
        """Executes adb devices -l and returns a list of objects describing attached devices.

        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.  This timeout is per adb call. The
            total time spent may exceed this value. If it is not
            specified, the value set in the ADBHost constructor is used.
        :returns: an object contain
        :raises: * ADBTimeoutError - raised if the command takes longer than
                   timeout seconds.
                 * ADBError - raised if the command exits with a
                   non-zero exit code.

        The output of adb devices -l ::

            $ adb devices -l
            List of devices attached
            b313b945               device usb:1-7 product:d2vzw model:SCH_I535 device:d2vzw

        is parsed and placed into an object as in

        [{'device_serial': 'b313b945', 'state': 'device', 'product': 'd2vzw',
          'usb': '1-7', 'device': 'd2vzw', 'model': 'SCH_I535' }]

        """
        # b313b945               device usb:1-7 product:d2vzw model:SCH_I535 device:d2vzw
        # from Android system/core/adb/transport.c statename()
        re_device_info = re.compile(r'([^\s]+)\s+(offline|bootloader|device|host|recovery|sideload|no permissions|unauthorized|unknown)')
        devices = []
        lines = self.command_output(["devices", "-l"], timeout=timeout).split('\n')
        for line in lines:
            if line == 'List of devices attached ':
                continue
            match = re_device_info.match(line)
            if match:
                device = {
                    'device_serial': match.group(1),
                    'state': match.group(2)
                }
                remainder = line[match.end(2):].strip()
                if remainder:
                    try:
                        device.update(dict([j.split(':')
                                            for j in remainder.split(' ')]))
                    except ValueError:
                        self._logger.warning('devices: Unable to parse '
                                             'remainder for device %s' % line)
                devices.append(device)
        return devices


class ADBDevice(ADBCommand):
    """ADBDevice provides methods which can be used to interact with
    the associated Android-based device.

    Android specific features such as Application management are not
    included but are provided via the ADBAndroid interface.

    ::

       from mozdevice import ADBDevice

       adbdevice = ADBDevice(...)
       print adbdevice.list_files("/mnt/sdcard")
       if adbdevice.process_exist("org.mozilla.fennec"):
           print "Fennec is running"

    """

    def __init__(self, device_serial,
                 adb='adb',
                 test_root='',
                 logger_name='adb',
                 log_level=logging.INFO,
                 timeout=300,
                 device_ready_retry_wait=20,
                 device_ready_retry_attempts=3):
        """Initializes the ADBDevice object.

        :param device_serial: adb serial number of the device.
        :param adb: path to adb executable. Defaults to 'adb'.
        :param logger_name: logging logger name. Defaults to 'adb'.
        :param log_level: logging level. Defaults to logging.INFO.
        :param device_ready_retry_wait: number of seconds to wait
            between attempts to check if the device is ready after a
            reboot.
        :param device_ready_retry_attempts: number of attempts when
            checking if a device is ready.

        :raises: * ADBError
                 * ADBTimeoutError

        """
        ADBCommand.__init__(self, adb=adb, logger_name=logger_name,
                            log_level=log_level,
                            timeout=timeout)
        self._device_serial = device_serial
        self.test_root = test_root
        self._device_ready_retry_wait = device_ready_retry_wait
        self._device_ready_retry_attempts = device_ready_retry_attempts
        self._have_root_shell = False
        self._have_su = False
        self._have_android_su = False

        uid = 'uid=0'
        cmd_id = 'LD_LIBRARY_PATH=/vendor/lib:/system/lib id'
        # Is shell already running as root?
        # Catch exceptions due to the potential for segfaults
        # calling su when using an improperly rooted device.
        try:
            if self.shell_output("id").find(uid) != -1:
                self._have_root_shell = True
        except ADBError:
            self._logger.exception('ADBDevice.__init__: id')
        # Do we have a 'Superuser' sh like su?
        try:
            if self.shell_output("su -c '%s'" % cmd_id).find(uid) != -1:
                self._have_su = True
        except ADBError:
            self._logger.exception('ADBDevice.__init__: id')
        # Do we have Android's su?
        try:
            if self.shell_output("su 0 id").find(uid) != -1:
                self._have_android_su = True
        except ADBError:
            self._logger.exception('ADBDevice.__init__: id')

        self._mkdir_p = None
        # Force the use of /system/bin/ls or /system/xbin/ls in case
        # there is /sbin/ls which embeds ansi escape codes to colorize
        # the output.  Detect if we are using busybox ls. We want each
        # entry on a single line and we don't want . or ..
        if self.shell_bool("/system/bin/ls /"):
            self._ls = "/system/bin/ls"
        elif self.shell_bool("/system/xbin/ls /"):
            self._ls = "/system/xbin/ls"
        else:
            raise ADBError("ADBDevice.__init__: ls not found")
        try:
            self.shell_output("%s -1A /" % self._ls)
            self._ls += " -1A"
        except ADBError:
            self._ls += " -a"

        self._logger.debug("ADBDevice: %s" % self.__dict__)

        self._setup_test_root()

    @staticmethod
    def _escape_command_line(cmd):
        """Utility function to return escaped and quoted version of command
        line.

        """
        quoted_cmd = []

        for arg in cmd:
            arg.replace('&', '\&')

            needs_quoting = False
            for char in [' ', '(', ')', '"', '&']:
                if arg.find(char) >= 0:
                    needs_quoting = True
                    break
            if needs_quoting:
                arg = "'%s'" % arg

            quoted_cmd.append(arg)

        return " ".join(quoted_cmd)

    @staticmethod
    def _get_exitcode(file_obj):
        """Get the exitcode from the last line of the file_obj for shell
        commands.

        """
        file_obj.seek(0, os.SEEK_END)

        line = ''
        length = file_obj.tell()
        offset = 1
        while length - offset >= 0:
            file_obj.seek(-offset, os.SEEK_END)
            char = file_obj.read(1)
            if not char:
                break
            if char != '\r' and char != '\n':
                line = char + line
            elif line:
                # we have collected everything up to the beginning of the line
                break
            offset += 1

        match = re.match(r'rc=([0-9]+)', line)
        if match:
            exitcode = int(match.group(1))
            file_obj.seek(-1, os.SEEK_CUR)
            file_obj.truncate()
        else:
            exitcode = None

        return exitcode

    def _setup_test_root(self, timeout=None, root=False):
        """setup the device root and cache its value

        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :param root: optional boolean specifying if the command should
            be executed as root.
        :raises: * ADBTimeoutError
                 * ADBRootError - raised if root is requested but the
                   device is not rooted.
                 * ADBError

        """
        # In order to catch situations where the file
        # system is temporily read only, attempt to
        # remove the old test root if it exists, then
        # recreate it.
        if self.test_root:
            for attempt in range(3):
                try:
                    if self.is_dir(self.test_root, timeout=timeout, root=root):
                        self.rm(self.test_root, recursive=True, timeout=timeout, root=root)
                    self.mkdir(self.test_root, timeout=timeout, root=root)
                    return
                except:
                    self._logger.exception("Attempt %d of 3 failed to create device root %s" %
                                           (attempt+1, self.test_root))
                time.sleep(20)
            raise ADBError('Unable to set test root to %s' % self.test_root)

        paths = [('/mnt/sdcard', 'tests'),
                 ('/data/local', 'tests')]
        for (base_path, sub_path) in paths:
            if self.is_dir(base_path, timeout=timeout, root=root):
                test_root = os.path.join(base_path, sub_path)
                for attempt in range(3):
                    try:
                        if self.is_dir(test_root):
                            self.rm(test_root, recursive=True, timeout=timeout, root=root)
                        self.mkdir(test_root, timeout=timeout, root=root)
                        self.test_root = test_root
                        return
                    except:
                        self._logger.exception('_setup_test_root: '
                                               'Attempt %d of 3 failed to set test_root to %s' %
                                               (attempt+1, test_root))
                        time.sleep(20)

        raise ADBError("Unable to set up device root using paths: [%s]"
                       % ", ".join(["'%s'" % os.path.join(b, s)
                                    for b, s in paths]))

    # Host Command methods

    def command(self, cmds, timeout=None):
        """Executes an adb command on the host against the device.

        :param cmds: list containing the command and its arguments to be
            executed.
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.  This timeout is per adb call. The
            total time spent may exceed this value. If it is not
            specified, the value set in the ADBDevice constructor is used.
        :returns: :class:`mozdevice.ADBProcess`

        command() provides a low level interface for executing
        commands for a specific device on the host via adb.

        command() executes on the host in such a fashion that stdout
        and stderr of the adb process are file handles on the host and
        the exit code is available as the exit code of the adb
        process.

        For executing shell commands on the device, use
        ADBDevice.shell().  The caller provides a list containing
        commands, as well as a timeout period in seconds.

        A subprocess is spawned to execute adb for the device with
        stdout and stderr directed to temporary files. If the process
        takes longer than the specified timeout, the process is
        terminated.

        It is the caller's responsibilty to clean up by closing
        the stdout and stderr temporary files.

        """

        return ADBCommand.command(self, cmds,
                                  device_serial=self._device_serial,
                                  timeout=timeout)

    def command_output(self, cmds, timeout=None):
        """Executes an adb command on the host against the device returning
        stdout.

        :param cmds: list containing the command and its arguments to be
            executed.
        :param timeout: optional integer specifying the maximum time in seconds
            for any spawned adb process to complete before throwing
            an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :returns: string - content of stdout.

        :raises: * ADBTimeoutError - raised if the command takes longer than
                   timeout seconds.
                 * ADBError - raised if the command exits with a
                   non-zero exit code.

        """
        return ADBCommand.command_output(self, cmds,
                                         device_serial=self._device_serial,
                                         timeout=timeout)

    # Device Shell methods

    def shell(self, cmd, env=None, cwd=None, timeout=None, root=False):
        """Executes a shell command on the device.

        :param cmd: string containing the command to be executed.
        :param env: optional dictionary of environment variables and
            their values.
        :param cwd: optional string containing the directory from which
            to execute.
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.  This timeout is per adb call. The
            total time spent may exceed this value. If it is not
            specified, the value set in the ADBDevice constructor is used.
        :param root: optional boolean specifying if the command should
            be executed as root.
        :returns: :class:`mozdevice.ADBProcess`
        :raises: ADBRootError - raised if root is requested but the
                 device is not rooted.

        shell() provides a low level interface for executing commands
        on the device via adb shell.

        shell() executes on the host in such as fashion that stdout
        contains the stdout of the host abd process combined with the
        combined stdout/stderr of the shell command on the device
        while stderr is still the stderr of the adb process on the
        host. The exit code of shell() is the exit code of
        the adb command if it was non-zero or the extracted exit code
        from the stdout/stderr of the shell command executed on the
        device.

        The caller provides a flag indicating if the command is to be
        executed as root, a string for any requested working
        directory, a hash defining the environment, a string
        containing shell commands, as well as a timeout period in
        seconds.

        The command line to be executed is created to set the current
        directory, set the required environment variables, optionally
        execute the command using su and to output the return code of
        the command to stdout. The command list is created as a
        command sequence separated by && which will terminate the
        command sequence on the first command which returns a non-zero
        exit code.

        A subprocess is spawned to execute adb shell for the device
        with stdout and stderr directed to temporary files. If the
        process takes longer than the specified timeout, the process
        is terminated. The return code is extracted from the stdout
        and is then removed from the file.

        It is the caller's responsibilty to clean up by closing
        the stdout and stderr temporary files.

        """
        if root:
            ld_library_path='LD_LIBRARY_PATH=/vendor/lib:/system/lib'
            cmd = '%s %s' % (ld_library_path, cmd)
            if self._have_root_shell:
                pass
            elif self._have_su:
                cmd = "su -c \"%s\"" % cmd
            elif self._have_android_su:
                cmd = "su 0 \"%s\"" % cmd
            else:
                raise ADBRootError('Can not run command %s as root!' % cmd)

        # prepend cwd and env to command if necessary
        if cwd:
            cmd = "cd %s && %s" % (cwd, cmd)
        if env:
            envstr = '&& '.join(map(lambda x: 'export %s=%s' %
                                    (x[0], x[1]), env.iteritems()))
            cmd = envstr + "&& " + cmd
        cmd += "; echo rc=$?"

        args = [self._adb_path]
        if self._device_serial:
            args.extend(['-s', self._device_serial])
        args.extend(["wait-for-device", "shell", cmd])

        adb_process = ADBProcess(args)

        if timeout is None:
            timeout = self._timeout

        start_time = time.time()
        exitcode = adb_process.proc.poll()
        while ((time.time() - start_time) <= timeout) and exitcode == None:
            time.sleep(self._polling_interval)
            exitcode = adb_process.proc.poll()
        if exitcode == None:
            adb_process.proc.kill()
            adb_process.timedout = True
            adb_process.exitcode = adb_process.proc.poll()
        elif exitcode == 0:
            adb_process.exitcode = self._get_exitcode(adb_process.stdout_file)
        else:
            adb_process.exitcode = exitcode

        adb_process.stdout_file.seek(0, os.SEEK_SET)
        adb_process.stderr_file.seek(0, os.SEEK_SET)

        return adb_process

    def shell_bool(self, cmd, env=None, cwd=None, timeout=None, root=False):
        """Executes a shell command on the device returning True on success
        and False on failure.

        :param cmd: string containing the command to be executed.
        :param env: optional dictionary of environment variables and
            their values.
        :param cwd: optional string containing the directory from which
            to execute.
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :param root: optional boolean specifying if the command should
            be executed as root.
        :returns: boolean

        :raises: * ADBTimeoutError  - raised if the command takes longer than
                   timeout seconds.
                 * ADBRootError - raised if root is requested but the
                   device is not rooted.

        """
        adb_process = None
        try:
            adb_process = self.shell(cmd, env=env, cwd=cwd,
                                     timeout=timeout, root=root)
            if adb_process.timedout:
                raise ADBTimeoutError("%s" % adb_process)
            return adb_process.exitcode == 0
        finally:
            if adb_process:
                adb_process.stdout_file.close()
                adb_process.stderr_file.close()

    def shell_output(self, cmd, env=None, cwd=None, timeout=None, root=False):
        """Executes an adb shell on the device returning stdout.

        :param cmd: string containing the command to be executed.
        :param env: optional dictionary of environment variables and
            their values.
        :param cwd: optional string containing the directory from which
            to execute.
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.  This timeout is per
            adb call. The total time spent may exceed this
            value. If it is not specified, the value set
            in the ADBDevice constructor is used.  :param root:
            optional boolean specifying if the command
            should be executed as root.
        :returns: string - content of stdout.
        :raises: * ADBTimeoutError - raised if the command takes longer than
                   timeout seconds.
                 * ADBRootError - raised if root is requested but the
                   device is not rooted.
                 * ADBError - raised if the command exits with a
                   non-zero exit code.

        """
        adb_process = None
        try:
            adb_process = self.shell(cmd, env=env, cwd=cwd,
                                     timeout=timeout, root=root)
            if adb_process.timedout:
                raise ADBTimeoutError("%s" % adb_process)
            elif adb_process.exitcode:
                raise ADBError("%s" % adb_process)
            output = adb_process.stdout_file.read().rstrip()
            self._logger.debug('shell_output: %s, '
                               'timeout: %s, '
                               'root: %s, '
                               'timedout: %s, '
                               'exitcode: %s, '
                               'output: %s' %
                               (' '.join(adb_process.args),
                                timeout,
                                root,
                                adb_process.timedout,
                                adb_process.exitcode,
                                output))
            return output
        finally:
            if adb_process and isinstance(adb_process.stdout_file, file):
                adb_process.stdout_file.close()
                adb_process.stderr_file.close()

    # Informational methods

    def clear_logcat(self, timeout=None):
        """Clears logcat via adb logcat -c.

        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.  This timeout is per
            adb call. The total time spent may exceed this
            value. If it is not specified, the value set
            in the ADBDevice constructor is used.
        :raises: * ADBTimeoutError - raised if adb logcat takes longer than
                   timeout seconds.
                 * ADBError - raised if adb logcat exits with a non-zero
                   exit code.

        """
        self.command_output(["logcat", "-c"], timeout=timeout)

    def get_logcat(self,
                  filter_specs=[
                      "dalvikvm:I",
                      "ConnectivityService:S",
                      "WifiMonitor:S",
                      "WifiStateTracker:S",
                      "wpa_supplicant:S",
                      "NetworkStateTracker:S"],
                  format="time",
                  filter_out_regexps=[],
                  timeout=None):
        """Returns the contents of the logcat file as a list of strings.

        :param filter_specs: optional list containing logcat messages to
            be included.
        :param format: optional logcat format.
        :param filterOutRexps: optional list of logcat messages to be
            excluded.
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :returns: list of lines logcat output.
        :raises: * ADBTimeoutError - raised if adb logcat takes longer than
                   timeout seconds.
                 * ADBError - raised if adb logcat exits with a non-zero
                   exit code.

        """
        cmds = ["logcat", "-v", format, "-d"] + filter_specs
        lines = self.command_output(cmds, timeout=timeout).split('\r')

        for regex in filter_out_regexps:
            lines = [line for line in lines if not re.search(regex, line)]

        return lines

    def get_prop(self, prop, timeout=None):
        """Gets value of a property from the device via adb shell getprop.

        :param prop: string containing the propery name.
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :returns: string value of property.
        :raises: * ADBTimeoutError - raised if the command takes longer than
                   timeout seconds.
                 * ADBError - raised if adb shell getprop exits with a
                   non-zero exit code.

        """
        output = self.shell_output('getprop %s' % prop, timeout=timeout)
        return output

    def get_state(self, timeout=None):
        """Returns the device's state via adb get-state.

        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before throwing
            an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :returns: string value of adb get-state.
        :raises: * ADBTimeoutError - raised if adb get-state takes longer
                   than timeout seconds.
                 * ADBError - raised if adb get-state exits with a
                   non-zero exit code.

        """
        output = self.command_output(["get-state"], timeout=timeout).strip()
        return output

    # File management methods

    def chmod(self, path, recursive=False, mask="777", timeout=None, root=False):
        """Recursively changes the permissions of a directory on the
        device.

        :param path: string containing the directory name on the device.
        :param recursive: boolean specifying if the command should be
            executed recursively.
        :param mask: optional string containing the octal permissions.
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before throwing
            an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :param root: optional boolean specifying if the command should
            be executed as root.
        :raises: * ADBTimeoutError - raised if any of the adb commands takes
                   longer than timeout seconds.
                 * ADBRootError - raised if root is requested but the
                   device is not rooted.
                 * ADBError  - raised if any of the adb commands raises
                   an uncaught ADBError.

        """
        path = posixpath.normpath(path.strip())
        self._logger.debug('chmod: path=%s, recursive=%s, mask=%s, root=%s' %
                           (path, recursive, mask, root))
        self.shell_output("chmod %s %s" % (mask, path),
                          timeout=timeout, root=root)
        if recursive and self.is_dir(path, timeout=timeout, root=root):
            files = self.list_files(path, timeout=timeout, root=root)
            for f in files:
                entry = path + "/" + f
                self._logger.debug('chmod: entry=%s' % entry)
                if self.is_dir(entry, timeout=timeout, root=root):
                    self._logger.debug('chmod: recursion entry=%s' % entry)
                    self.chmod(entry, recursive=recursive, mask=mask,
                               timeout=timeout, root=root)
                elif self.is_file(entry, timeout=timeout, root=root):
                    try:
                        self.shell_output("chmod %s %s" % (mask, entry),
                                          timeout=timeout, root=root)
                        self._logger.debug('chmod: file entry=%s' % entry)
                    except ADBError, e:
                        if e.message.find('No such file or directory'):
                            # some kind of race condition is causing files
                            # to disappear. Catch and report the error here.
                            self._logger.warning('chmod: File %s vanished!: %s' %
                                                 (entry, e))
                else:
                    self._logger.warning('chmod: entry %s does not exist' %
                                         entry)

    def exists(self, path, timeout=None, root=False):
        """Returns True if the path exists on the device.

        :param path: string containing the directory name on the device.
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :param root: optional boolean specifying if the command should be
            executed as root.
        :returns: boolean - True if path exists.
        :raises: * ADBTimeoutError - raised if the command takes longer than
                   timeout seconds.
                 * ADBRootError - raised if root is requested but the
                   device is not rooted.

        """
        path = posixpath.normpath(path)
        return self.shell_bool('ls -a %s' % path, timeout=timeout, root=root)

    def is_dir(self, path, timeout=None, root=False):
        """Returns True if path is an existing directory on the device.

        :param path: string containing the path on the device.
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :param root: optional boolean specifying if the command should
            be executed as root.
        :returns: boolean - True if path exists on the device and is a
            directory.
        :raises: * ADBTimeoutError - raised if the command takes longer than
                   timeout seconds.
                 * ADBRootError - raised if root is requested but the
                   device is not rooted.

        """
        path = posixpath.normpath(path)
        return self.shell_bool('ls -a %s/' % path, timeout=timeout, root=root)

    def is_file(self, path, timeout=None, root=False):
        """Returns True if path is an existing file on the device.

        :param path: string containing the file name on the device.
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :param root: optional boolean specifying if the command should
            be executed as root.
        :returns: boolean - True if path exists on the device and is a
            file.
        :raises: * ADBTimeoutError - raised if the command takes longer than
                   timeout seconds.
                 * ADBRootError - raised if root is requested but the
                   device is not rooted.

        """
        path = posixpath.normpath(path)
        return (
            self.exists(path, timeout=timeout, root=root) and
            not self.is_dir(path, timeout=timeout, root=root))

    def list_files(self, path, timeout=None, root=False):
        """Return a list of files/directories contained in a directory
        on the device.

        :param path: string containing the directory name on the device.
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :param root: optional boolean specifying if the command should
            be executed as root.
        :returns: list of files/directories contained in the directory.
        :raises: * ADBTimeoutError - raised if the command takes longer than
                   timeout seconds.
                 * ADBRootError - raised if root is requested but the
                   device is not rooted.

        """
        path = posixpath.normpath(path.strip())
        data = []
        if self.is_dir(path, timeout=timeout, root=root):
            try:
                data = self.shell_output("%s %s" % (self._ls, path),
                                         timeout=timeout,
                                         root=root).split('\r\n')
                self._logger.debug('list_files: data: %s' % data)
            except ADBError:
                self._logger.exception('Ignoring exception in ADBDevice.list_files')
                pass
        data[:] = [item for item in data if item]
        self._logger.debug('list_files: %s' % data)
        return data

    def mkdir(self, path, parents=False, timeout=None, root=False):
        """Create a directory on the device.

        :param path: string containing the directory name on the device
            to be created.
        :param parents: boolean indicating if the parent directories are
            also to be created. Think mkdir -p path.
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :param root: optional boolean specifying if the command should
            be executed as root.
        :raises: * ADBTimeoutError - raised if any adb command takes longer
                   than timeout seconds.
                 * ADBRootError - raised if root is requested but the
                   device is not rooted.
                 * ADBError - raised if adb shell mkdir exits with a
                   non-zero exit code or if the directory is not
                   created.

        """
        path = posixpath.normpath(path)
        if parents:
            if self._mkdir_p is None or self._mkdir_p:
                # Use shell_bool to catch the possible
                # non-zero exitcode if -p is not supported.
                if self.shell_bool('mkdir -p %s' % path, timeout=timeout):
                    self._mkdir_p = True
                    return
            # mkdir -p is not supported. create the parent
            # directories individually.
            if not self.is_dir(posixpath.dirname(path)):
                parts = path.split('/')
                name = "/"
                for part in parts[:-1]:
                    if part != "":
                        name = posixpath.join(name, part)
                        if not self.is_dir(name):
                            # Use shell_output to allow any non-zero
                            # exitcode to raise an ADBError.
                            self.shell_output('mkdir %s' % name,
                                              timeout=timeout, root=root)
        self.shell_output('mkdir %s' % path, timeout=timeout, root=root)
        if not self.is_dir(path, timeout=timeout, root=root):
            raise ADBError('mkdir %s Failed' % path)

    def push(self, local, remote, timeout=None):
        """Pushes a file or directory to the device.

        :param local: string containing the name of the local file or
            directory name.
        :param remote: string containing the name of the remote file or
            directory name.
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :raises: * ADBTimeoutError - raised if the adb push takes longer than
                   timeout seconds.
                 * ADBError - raised if the adb push exits with a
                   non-zero exit code.

        """
        self.command_output(["push", os.path.realpath(local), remote],
                            timeout=timeout)

    def rm(self, path, recursive=False, force=False, timeout=None, root=False):
        """Delete files or directories on the device.

        :param path: string containing the file name on the device.
        :param recursive: optional boolean specifying if the command is
            to be applied recursively to the target. Default is False.
        :param force: optional boolean which if True will not raise an
            error when attempting to delete a non-existent file. Default
            is False.
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :param root: optional boolean specifying if the command should
            be executed as root.
        :raises: * ADBTimeoutError - raised if any of the adb commands takes
                   longer than timeout seconds.
                 * ADBRootError - raised if root is requested but the
                   device is not rooted.
                 * ADBError - raised if the adb shell rm command exits
                   with a non-zero exit code or if the file is not
                   removed, or if force was not specified and the
                   file did not exist.

        """
        cmd = "rm"
        if recursive:
            cmd += " -r"
        try:
            self.shell_output("%s %s" % (cmd, path), timeout=timeout, root=root)
            if self.is_file(path, timeout=timeout, root=root):
                raise ADBError('rm("%s") failed to remove file.' % path)
        except ADBError, e:
            if not force and 'No such file or directory' in e.message:
                raise

    def rmdir(self, path, timeout=None, root=False):
        """Delete empty directory on the device.

        :param path: string containing the directory name on the device.
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :param root: optional boolean specifying if the command should
            be executed as root.
        :raises: * ADBTimeoutError - raised if the command takes longer than
                   timeout seconds.
                 * ADBRootError - raised if root is requested but the
                   device is not rooted.
                 * ADBError - raised if the adb shell rmdir command
                   exits with a non-zero exit code or if the
                   directory was not removed..

        """
        self.shell_output("rmdir %s" % path, timeout=timeout, root=root)
        if self.is_dir(path, timeout=timeout, root=root):
            raise ADBError('rmdir("%s") failed to remove directory.' % path)

    # Process management methods

    def get_process_list(self, timeout=None):
        """Returns list of tuples (pid, name, user) for running
        processes on device.

        :param timeout: optional integer specifying the maximum time
            in seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified,
            the value set in the ADBDevice constructor is used.
        :returns: list of (pid, name, user) tuples for running processes
            on the device.
        :raises: * ADBTimeoutError - raised if the adb shell ps command
                   takes longer than timeout seconds.
                 * ADBError - raised if the adb shell ps command exits
                   with a non-zero exit code or if the ps output
                   is not in the expected format.

        """
        adb_process = None
        try:
            adb_process = self.shell("ps", timeout=timeout)
            if adb_process.timedout:
                raise ADBTimeoutError("%s" % adb_process)
            elif adb_process.exitcode:
                raise ADBError("%s" % adb_process)
            # first line is the headers
            header = adb_process.stdout_file.readline()
            pid_i = -1
            user_i = -1
            els = header.split()
            for i in range(len(els)):
                item = els[i].lower()
                if item == 'user':
                    user_i = i
                elif item == 'pid':
                    pid_i = i
            if user_i == -1 or pid_i == -1:
                self._logger.error('get_process_list: %s' % header)
                raise ADBError('get_process_list: Unknown format: %s: %s' % (
                    header, adb_process))
            ret = []
            line = adb_process.stdout_file.readline()
            while line:
                els = line.split()
                try:
                    ret.append([int(els[pid_i]), els[-1], els[user_i]])
                except ValueError:
                    self._logger.exception('get_process_list: %s %s' % (
                        header, line))
                    raise ADBError('get_process_list: %s: %s: %s' % (
                        header, line, adb_process))
                line = adb_process.stdout_file.readline()
            self._logger.debug('get_process_list: %s' % ret)
            return ret
        finally:
            if adb_process and isinstance(adb_process.stdout_file, file):
                adb_process.stdout_file.close()
                adb_process.stderr_file.close()

    def kill(self, pids, sig=None,  attempts=3, wait=5,
             timeout=None, root=False):
        """Kills processes on the device given a list of process ids.

        :param pids: list of process ids to be killed.
        :param sig: optional signal to be sent to the process.
        :param attempts: number of attempts to try to kill the processes.
        :param wait: number of seconds to wait after each attempt.
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :param root: optional boolean specifying if the command should
            be executed as root.
        :raises: * ADBTimeoutError - raised if adb shell kill takes longer
                   than timeout seconds.
                 * ADBRootError - raised if root is requested but the
                   device is not rooted.
                 * ADBError - raised if adb shell kill exits with a
                   non-zero exit code or not all of the processes have
                   been killed.

        """
        pid_list = [str(pid) for pid in pids]
        for attempt in range(attempts):
            args = ["kill"]
            if sig:
                args.append("-%d" % sig)
            args.extend(pid_list)
            try:
                self.shell_output(' '.join(args), timeout=timeout, root=root)
            except ADBError, e:
                if 'No such process' not in e.message:
                    raise
            pid_set = set(pid_list)
            current_pid_set = set([str(proc[0]) for proc in
                                   self.get_process_list(timeout=timeout)])
            pid_list = list(pid_set.intersection(current_pid_set))
            if not pid_list:
                break
            self._logger.debug("Attempt %d of %d to kill processes %s failed" %
                               (attempt+1, attempts, pid_list))
            time.sleep(wait)

        if pid_list:
            raise ADBError('kill: processes %s not killed' % pid_list)

    def pkill(self, appname, sig=None, attempts=3, wait=5,
              timeout=None, root=False):
        """Kills a processes on the device matching a name.

        :param appname: string containing the app name of the process to
            be killed. Note that only the first 75 characters of the
            process name are significant.
        :param sig: optional signal to be sent to the process.
        :param attempts: number of attempts to try to kill the processes.
        :param wait: number of seconds to wait after each attempt.
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :param root: optional boolean specifying if the command should
            be executed as root.

        :raises: * ADBTimeoutError - raised if any of the adb commands takes
                   longer than timeout seconds.
                 * ADBRootError - raised if root is requested but the
                   device is not rooted.
                 * ADBError - raised if any of the adb commands raises
                   ADBError or if the process is not killed.

        """
        procs = self.get_process_list(timeout=timeout)
        # limit the comparion to the first 75 characters due to a
        # limitation in processname length in android.
        pids = [proc[0] for proc in procs if proc[1] == appname[:75]]
        if not pids:
            return

        try:
            self.kill(pids, sig, attempts=attempts, wait=wait,
                      timeout=timeout, root=root)
        except ADBError, e:
            if self.process_exist(appname, timeout=timeout):
                raise e

    def process_exist(self, process_name, timeout=None):
        """Returns True if process with name process_name is running on
        device.

        :param process_name: string containing the name of the process
            to check. Note that only the first 75 characters of the
            process name are significant.
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :returns: boolean - True if process exists.

        :raises: * ADBTimeoutError - raised if any of the adb commands takes
                   longer than timeout seconds.
                 * ADBError - raised if the adb shell ps command exits
                   with a non-zero exit code or if the ps output is
                   not in the expected format.

        """
        if not isinstance(process_name, basestring):
            raise ADBError("Process name %s is not a string" % process_name)

        pid = None

        # Filter out extra spaces.
        parts = [x for x in process_name.split(' ') if x != '']
        process_name = ' '.join(parts)

        # Filter out the quoted env string if it exists
        # ex: '"name=value;name2=value2;etc=..." process args' -> 'process args'
        parts = process_name.split('"')
        if len(parts) > 2:
            process_name = ' '.join(parts[2:]).strip()

        pieces = process_name.split(' ')
        parts = pieces[0].split('/')
        app = parts[-1]

        proc_list = self.get_process_list(timeout=timeout)
        if not proc_list:
            return False

        for proc in proc_list:
            proc_name = proc[1].split('/')[-1]
            # limit the comparion to the first 75 characters due to a
            # limitation in processname length in android.
            if proc_name == app[:75]:
                return True
        return False
