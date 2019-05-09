# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import os
import posixpath
import re
import shutil
import signal
import subprocess
import tempfile
import time
import traceback

from distutils import dir_util
from . import version_codes


class ADBProcess(object):
    """ADBProcess encapsulates the data related to executing the adb process."""

    def __init__(self, args):
        #: command argument argument list.
        self.args = args
        #: Temporary file handle to be used for stdout.
        self.stdout_file = tempfile.NamedTemporaryFile(mode='w+b')
        #: boolean indicating if the command timed out.
        self.timedout = None
        #: exitcode of the process.
        self.exitcode = None
        #: subprocess Process object used to execute the command.
        self.proc = subprocess.Popen(args,
                                     stdout=self.stdout_file,
                                     stderr=subprocess.STDOUT)

    @property
    def stdout(self):
        """Return the contents of stdout."""
        if not self.stdout_file or self.stdout_file.closed:
            content = ""
        else:
            self.stdout_file.seek(0, os.SEEK_SET)
            content = self.stdout_file.read().rstrip()
        return content

    def __str__(self):
        # Remove -s <serialno> from the error message to allow bug suggestions
        # to be independent of the individual failing device.
        arg_string = ' '.join(self.args)
        arg_string = re.sub(' -s [\w-]+', '', arg_string)
        return ('args: %s, exitcode: %s, stdout: %s' % (
            arg_string, self.exitcode, self.stdout))

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


class ADBProcessError(ADBError):
    """ADBProcessError is raised when an associated ADBProcess is
    available and relevant.
    """
    def __init__(self, adb_process):
        ADBError.__init__(self, str(adb_process))
        self.adb_process = adb_process


class ADBListDevicesError(ADBError):
    """ADBListDevicesError is raised when errors are found listing the
    devices, typically not any permissions.

    The devices information is stocked with the *devices* member.
    """

    def __init__(self, msg, devices):
        ADBError.__init__(self, msg)
        self.devices = devices


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
           adbcommand = ADBCommand()
       except NotImplementedError:
           print "ADBCommand can not be instantiated."
    """

    def __init__(self,
                 adb='adb',
                 adb_host=None,
                 adb_port=None,
                 logger_name='adb',
                 timeout=300,
                 verbose=False,
                 require_root=True):
        """Initializes the ADBCommand object.

        :param str adb: path to adb executable. Defaults to 'adb'.
        :param adb_host: host of the adb server.
        :type adb_host: str or None
        :param adb_port: port of the adb server.
        :type adb_port: integer or None
        :param str logger_name: logging logger name. Defaults to 'adb'.
        :param timeout: The default maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.  This timeout is per adb call. The
            total time spent may exceed this value. If it is not
            specified, the value defaults to 300.
        :type timeout: integer or None
        :param bool verbose: provide verbose output
        :param bool require_root: check that we have root permissions on device

        :raises: * ADBError
                 * ADBTimeoutError
        """
        if self.__class__ == ADBCommand:
            raise NotImplementedError

        self._logger = self._get_logger(logger_name)
        self._verbose = verbose
        self._require_root = require_root
        self._adb_path = adb
        self._adb_host = adb_host
        self._adb_port = adb_port
        self._timeout = timeout
        self._polling_interval = 0.1
        self._adb_version = ''

        self._logger.debug("%s: %s" % (self.__class__.__name__,
                                       self.__dict__))

        # catch early a missing or non executable adb command
        # and get the adb version while we are at it.
        try:
            output = subprocess.Popen([adb, 'version'],
                                      stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE).communicate()
            re_version = re.compile(r'Android Debug Bridge version (.*)')
            self._adb_version = re_version.match(output[0]).group(1)

        except Exception as exc:
            raise ADBError('%s: %s is not executable.' % (exc, adb))

    def _get_logger(self, logger_name):
        logger = None
        try:
            import mozlog
            logger = mozlog.get_default_logger(logger_name)
        except ImportError:
            pass

        if logger is None:
            import logging
            logger = logging.getLogger(logger_name)
        return logger

    # Host Command methods

    def command(self, cmds, device_serial=None, timeout=None):
        """Executes an adb command on the host.

        :param list cmds: The command and its arguments to be
            executed.
        :param device_serial: The device's
            serial number if the adb command is to be executed against
            a specific device.
        :type device_serial: str or None
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.  This timeout is per adb call. The
            total time spent may exceed this value. If it is not
            specified, the value set in the ADBCommand constructor is used.
        :type timeout: integer or None
        :returns: :class:`mozdevice.ADBProcess`

        command() provides a low level interface for executing
        commands on the host via adb.

        command() executes on the host in such a fashion that stdout
        of the adb process is a file handle on the host and
        the exit code is available as the exit code of the adb
        process.

        The caller provides a list containing commands, as well as a
        timeout period in seconds.

        A subprocess is spawned to execute adb with stdout and stderr
        directed to a temporary file. If the process takes longer than
        the specified timeout, the process is terminated.

        It is the caller's responsibilty to clean up by closing
        the stdout temporary file.
        """
        args = [self._adb_path]
        if self._adb_host:
            args.extend(['-H', self._adb_host])
        if self._adb_port:
            args.extend(['-P', str(self._adb_port)])
        if device_serial:
            args.extend(['-s', device_serial, 'wait-for-device'])
        args.extend(cmds)

        adb_process = ADBProcess(args)

        if timeout is None:
            timeout = self._timeout

        start_time = time.time()
        adb_process.exitcode = adb_process.proc.poll()
        while ((time.time() - start_time) <= float(timeout) and
               adb_process.exitcode is None):
            time.sleep(self._polling_interval)
            adb_process.exitcode = adb_process.proc.poll()
        if adb_process.exitcode is None:
            adb_process.proc.kill()
            adb_process.timedout = True
            adb_process.exitcode = adb_process.proc.poll()

        adb_process.stdout_file.seek(0, os.SEEK_SET)

        return adb_process

    def command_output(self, cmds, device_serial=None, timeout=None):
        """Executes an adb command on the host returning stdout.

        :param list cmds: The command and its arguments to be
            executed.
        :param device_serial: The device's
            serial number if the adb command is to be executed against
            a specific device.
        :type device_serial: str or None
        :param timeout: The maximum time in seconds
            for any spawned adb process to complete before throwing
            an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBCommand constructor is used.
        :type timeout: integer or None
        :returns: string - content of stdout.

        :raises: * ADBTimeoutError
                 * ADBError
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
                raise ADBProcessError(adb_process)
            output = adb_process.stdout_file.read().rstrip()
            if self._verbose:
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


class ADBHost(ADBCommand):
    """ADBHost provides a basic interface to adb host commands
    which do not target a specific device.

    ::

       from mozdevice import ADBHost

       adbhost = ADBHost()
       adbhost.start_server()
    """

    def __init__(self,
                 adb='adb',
                 adb_host=None,
                 adb_port=None,
                 logger_name='adb',
                 timeout=300,
                 verbose=False):
        """Initializes the ADBHost object.

        :param str adb: path to adb executable. Defaults to 'adb'.
        :param adb_host: host of the adb server.
        :type adb_host: str or None
        :param adb_port: port of the adb server.
        :type adb_port: integer or None
        :param str logger_name: logging logger name. Defaults to 'adb'.
        :param timeout: The default maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.  This timeout is per adb call. The
            total time spent may exceed this value. If it is not
            specified, the value defaults to 300.
        :type timeout: integer or None
        :param bool verbose: provide verbose output

        :raises: * ADBError
                 * ADBTimeoutError
        """
        ADBCommand.__init__(self, adb=adb, adb_host=adb_host,
                            adb_port=adb_port, logger_name=logger_name,
                            timeout=timeout, verbose=verbose, require_root=False)

    def command(self, cmds, timeout=None):
        """Executes an adb command on the host.

        :param list cmds: The command and its arguments to be
            executed.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.  This timeout is per adb call. The
            total time spent may exceed this value. If it is not
            specified, the value set in the ADBHost constructor is used.
        :type timeout: integer or None
        :returns: :class:`mozdevice.ADBProcess`

        command() provides a low level interface for executing
        commands on the host via adb.

        command() executes on the host in such a fashion that stdout
        of the adb process is a file handle on the host and
        the exit code is available as the exit code of the adb
        process.

        The caller provides a list containing commands, as well as a
        timeout period in seconds.

        A subprocess is spawned to execute adb with stdout and stderr
        directed to a temporary file. If the process takes longer than
        the specified timeout, the process is terminated.

        It is the caller's responsibilty to clean up by closing
        the stdout temporary file.
        """
        return ADBCommand.command(self, cmds, timeout=timeout)

    def command_output(self, cmds, timeout=None):
        """Executes an adb command on the host returning stdout.

        :param list cmds: The command and its arguments to be
            executed.
        :param timeout: The maximum time in seconds
            for any spawned adb process to complete before throwing
            an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBHost constructor is used.
        :type timeout: integer or None
        :returns: string - content of stdout.

        :raises: * ADBTimeoutError
                 * ADBError
        """
        return ADBCommand.command_output(self, cmds, timeout=timeout)

    def start_server(self, timeout=None):
        """Starts the adb server.

        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.  This timeout is per adb call. The
            total time spent may exceed this value. If it is not
            specified, the value set in the ADBHost constructor is used.
        :type timeout: integer or None
        :raises: * ADBTimeoutError
                 * ADBError

        Attempting to use start_server with any adb_host value other than None
        will fail with an ADBError exception.

        You will need to start the server on the remote host via the command:

        .. code-block:: shell

            adb -a fork-server server

        If you wish the remote adb server to restart automatically, you can
        enclose the command in a loop as in:

        .. code-block:: shell

            while true; do
              adb -a fork-server server
            done
        """
        self.command_output(["start-server"], timeout=timeout)

    def kill_server(self, timeout=None):
        """Kills the adb server.

        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.  This timeout is per adb call. The
            total time spent may exceed this value. If it is not
            specified, the value set in the ADBHost constructor is used.
        :type timeout: integer or None
        :raises: * ADBTimeoutError
                 * ADBError
        """
        self.command_output(["kill-server"], timeout=timeout)

    def devices(self, timeout=None):
        """Executes adb devices -l and returns a list of objects describing attached devices.

        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.  This timeout is per adb call. The
            total time spent may exceed this value. If it is not
            specified, the value set in the ADBHost constructor is used.
        :type timeout: integer or None
        :returns: an object contain
        :raises: * ADBTimeoutError
                 * ADBListDevicesError
                 * ADBError

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
        re_device_info = re.compile(
            r"([^\s]+)\s+(offline|bootloader|device|host|recovery|sideload|"
            "no permissions|unauthorized|unknown)")
        devices = []
        lines = self.command_output(["devices", "-l"], timeout=timeout).splitlines()
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
        for device in devices:
            if device['state'] == 'no permissions':
                raise ADBListDevicesError(
                    "No permissions to detect devices. You should restart the"
                    " adb server as root:\n"
                    "\n# adb kill-server\n# adb start-server\n"
                    "\nor maybe configure your udev rules.",
                    devices)
        return devices


class ADBDevice(ADBCommand):
    """ADBDevice provides methods which can be used to interact with the
    associated Android-based device.

    ::

       from mozdevice import ADBDevice

       adbdevice = ADBDevice()
       print(adbdevice.list_files("/mnt/sdcard"))
       if adbdevice.process_exist("org.mozilla.fennec"):
           print("Fennec is running")
    """
    SOCKET_DIRECTON_REVERSE = "reverse"
    SOCKET_DIRECTON_FORWARD = "forward"

    def __init__(self,
                 device=None,
                 adb='adb',
                 adb_host=None,
                 adb_port=None,
                 test_root='',
                 logger_name='adb',
                 timeout=300,
                 verbose=False,
                 device_ready_retry_wait=20,
                 device_ready_retry_attempts=3,
                 require_root=True):
        """Initializes the ADBDevice object.

        :param device: When a string is passed, it is interpreted as the
            device serial number. This form is not compatible with
            devices containing a ":" in the serial; in this case
            ValueError will be raised.
            When a dictionary is passed it must have one or both of
            the keys "device_serial" and "usb". This is compatible
            with the dictionaries in the list returned by
            ADBHost.devices(). If the value of device_serial is a
            valid serial not containing a ":" it will be used to
            identify the device, otherwise the value of the usb key,
            prefixed with "usb:" is used.
            If None is passed and there is exactly one device attached
            to the host, that device is used. If there is more than one
            device attached, ValueError is raised. If no device is
            attached the constructor will block until a device is
            attached or the timeout is reached.
        :type device: dict, str or None
        :param adb_host: host of the adb server to connect to.
        :type adb_host: str or None
        :param adb_port: port of the adb server to connect to.
        :type adb_port: integer or None
        :param str logger_name: logging logger name. Defaults to 'adb'.
        :param timeout: The default maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.  This timeout is per adb call. The
            total time spent may exceed this value. If it is not
            specified, the value defaults to 300.
        :type timeout: integer or None
        :param bool verbose: provide verbose output
        :param integer device_ready_retry_wait: number of seconds to wait
            between attempts to check if the device is ready after a
            reboot.
        :param integer device_ready_retry_attempts: number of attempts when
            checking if a device is ready.
        :param bool require_root: check that we have root permissions on device

        :raises: * ADBError
                 * ADBTimeoutError
                 * ValueError
        """
        ADBCommand.__init__(self, adb=adb, adb_host=adb_host,
                            adb_port=adb_port, logger_name=logger_name,
                            timeout=timeout, verbose=verbose,
                            require_root=require_root)
        self._logger.info('Using adb %s' % self._adb_version)
        self._device_serial = self._get_device_serial(device)
        self._initial_test_root = test_root
        self._test_root = None
        self._device_ready_retry_wait = device_ready_retry_wait
        self._device_ready_retry_attempts = device_ready_retry_attempts
        self._have_root_shell = False
        self._have_su = False
        self._have_android_su = False
        self._re_internal_storage = None

        # Catch exceptions due to the potential for segfaults
        # calling su when using an improperly rooted device.

        # Note this check to see if adbd is running is performed on
        # the device in the state it exists in when the ADBDevice is
        # initialized. It may be the case that it has been manipulated
        # since its last boot and that its current state does not
        # match the state the device will have immediately after a
        # reboot. For example, if adb root was called manually prior
        # to ADBDevice being initialized, then self._have_root_shell
        # will not reflect the state of the device after it has been
        # rebooted again. Therefore this check will need to be
        # performed again after a reboot.

        self._check_adb_root(timeout=timeout)

        # To work around bug 1525401 where su -c id will return an
        # exitcode of 1 if selinux permissive is not already in effect,
        # we need su to turn off selinux prior to checking for su.
        # We can use shell() directly to prevent the non-zero exitcode
        # from raising an ADBError.
        adb_process = self.shell("su -c setenforce 0")
        self._logger.info("setenforce 0 exitcode %s, stdout: %s" % (
            adb_process.proc.poll(),
            adb_process.proc.stdout))

        uid = 'uid=0'
        # Do we have a 'Superuser' sh like su?
        try:
            if (self._require_root and
                self.shell_output("su -c id", timeout=timeout).find(uid) != -1):
                self._have_su = True
                self._logger.info("su -c supported")
        except ADBError as e:
            self._logger.debug("Check for su -c failed: {}".format(e))

        # Check if Android's su 0 command works.
        # su 0 id will hang on Pixel 2 8.1.0/OPM2.171019.029.B1/4720900
        # rooted via magisk. If we already have detected su -c support,
        # we can skip this check.
        try:
            if (self._require_root and
                not self._have_su and
                self.shell_output("su 0 id", timeout=timeout).find(uid) != -1):
                self._have_android_su = True
                self._logger.info("su 0 supported")
        except ADBError as e:
            self._logger.debug("Check for su 0 failed: {}".format(e))

        self._mkdir_p = None
        # Force the use of /system/bin/ls or /system/xbin/ls in case
        # there is /sbin/ls which embeds ansi escape codes to colorize
        # the output.  Detect if we are using busybox ls. We want each
        # entry on a single line and we don't want . or ..
        ls_dir = "/sdcard"

        if self.is_file("/system/bin/ls", timeout=timeout):
            self._ls = "/system/bin/ls"
        elif self.is_file("/system/xbin/ls", timeout=timeout):
            self._ls = "/system/xbin/ls"
        else:
            raise ADBError("ADBDevice.__init__: ls could not be found")

        # A race condition can occur especially with emulators where
        # the device appears to be available but it has not completed
        # mounting the sdcard. We can work around this by checking if
        # the sdcard is missing when we attempt to ls it and retrying
        # if it is not yet available.
        start_time = time.time()
        boot_completed = False
        while not boot_completed and (time.time() - start_time) <= float(timeout):
            try:
                self.shell_output("%s -1A {}".format(ls_dir) % self._ls, timeout=timeout)
                boot_completed = True
                self._ls += " -1A"
            except ADBError as e:
                if 'No such file or directory' not in e.message:
                    boot_completed = True
                    self._ls += " -a"
            if not boot_completed:
                time.sleep(2)
        if not boot_completed:
            raise ADBTimeoutError("ADBDevice: /sdcard not found.")

        self._logger.info("%s supported" % self._ls)

        # Do we have cp?
        self._have_cp = self.shell_bool("type cp", timeout=timeout)
        self._logger.info("Native cp support: %s" % self._have_cp)

        # Do we have chmod -R?
        try:
            self._chmod_R = False
            re_recurse = re.compile(r'[-]R')
            chmod_output = self.shell_output("chmod --help", timeout=timeout)
            match = re_recurse.search(chmod_output)
            if match:
                self._chmod_R = True
        except ADBError as e:
            self._logger.debug("Check chmod -R: {}".format(e))
            match = re_recurse.search(e.message)
            if match:
                self._chmod_R = True
        self._logger.info("Native chmod -R support: {}".format(self._chmod_R))

        # Do we have chown -R?
        try:
            self._chown_R = False
            chown_output = self.shell_output("chown --help", timeout=timeout)
            match = re_recurse.search(chown_output)
            if match:
                self._chown_R = True
        except ADBError as e:
            self._logger.debug("Check chown -R: {}".format(e))
        self._logger.info("Native chown -R support: {}".format(self._chown_R))

        try:
            cleared = self.shell_bool('logcat -P ""', timeout=timeout)
        except ADBError:
            cleared = False
        if not cleared:
            self._logger.info("Unable to turn off logcat chatty")

        self._selinux = None
        self.enforcing = 'Permissive'

        self.version = 0
        while self.version < 1 and (time.time() - start_time) <= float(timeout):
            try:
                version = self.get_prop("ro.build.version.sdk",
                                        timeout=timeout)
                self.version = int(version)
            except ValueError:
                self._logger.info("unexpected ro.build.version.sdk: '%s'" % version)
                time.sleep(2)
        if self.version < 1:
            # note slightly different meaning to the ADBTimeoutError here (and above):
            # failed to get valid (numeric) version string in all attempts in allowed time
            raise ADBTimeoutError("ADBDevice: unable to determine ro.build.version.sdk.")

        # Do we have pidof?
        if self.version >= version_codes.N:
            self._have_pidof = self.shell_bool("type pidof", timeout=timeout)
        else:
            # unexpected pidof behavior observed on Android 6 in bug 1514363
            self._have_pidof = False
        self._logger.info("Native pidof support: {}".format(self._have_pidof))

        # Bug 1529960 observed pidof intermittently returning no results for a
        # running process on the 7.0 x86_64 emulator.
        characteristics = self.get_prop("ro.build.characteristics", timeout=timeout)
        abi = self.get_prop("ro.product.cpu.abi", timeout=timeout)
        self._have_flaky_pidof = (self.version == version_codes.N and
                                  abi == 'x86_64' and
                                  'emulator' in characteristics)

        # Beginning in Android 8.1 /data/anr/traces.txt no longer contains
        # a single file traces.txt but instead will contain individual files
        # for each stack.
        # See https://github.com/aosp-mirror/platform_build/commit/
        # fbba7fe06312241c7eb8c592ec2ac630e4316d55
        stack_trace_dir = self.shell_output("getprop dalvik.vm.stack-trace-dir",
                                            timeout=timeout)
        if not stack_trace_dir:
            stack_trace_file = self.shell_output("getprop dalvik.vm.stack-trace-file",
                                                 timeout=timeout)
            if stack_trace_file:
                stack_trace_dir = posixpath.dirname(stack_trace_file)
            else:
                stack_trace_dir = '/data/anr'
        self.stack_trace_dir = stack_trace_dir

        self._logger.debug("ADBDevice: %s" % self.__dict__)

    def _get_device_serial(self, device):
        if device is None:
            devices = ADBHost(adb=self._adb_path, adb_host=self._adb_host,
                              adb_port=self._adb_port).devices()
            if len(devices) > 1:
                raise ValueError("ADBDevice called with multiple devices "
                                 "attached and no device specified")
            elif len(devices) == 0:
                # We could error here, but this way we'll wait-for-device before we next
                # run a command, which seems more friendly
                return
            device = devices[0]

        # Allow : in device serial if it matches a tcpip device serial.
        re_device_serial_tcpip = re.compile(r'[^:]+:[0-9]+$')

        def is_valid_serial(serial):
            return serial.startswith("usb:") or \
                re_device_serial_tcpip.match(serial) is not None or \
                ":" not in serial

        if isinstance(device, (str, unicode)):
            # Treat this as a device serial
            if not is_valid_serial(device):
                raise ValueError("Device serials containing ':' characters are "
                                 "invalid. Pass the output from "
                                 "ADBHost.devices() for the device instead")
            return device

        serial = device.get("device_serial")
        if serial is not None and is_valid_serial(serial):
            return serial
        usb = device.get("usb")
        if usb is not None:
            return "usb:%s" % usb

        raise ValueError("Unable to get device serial")

    def _check_root_user(self, timeout=None):
        uid = 'uid=0'
        # Is shell already running as root?
        try:
            if self.shell_output("id", timeout=timeout).find(uid) != -1:
                self._logger.info("adbd running as root")
                return True
        except ADBError:
            self._logger.debug("Check for root user failed")
        return False

    def _check_adb_root(self, timeout=None):
        self._have_root_shell = self._check_root_user(timeout=timeout)

        # Do we need to run adb root to get a root shell?
        if not self._have_root_shell:
            try:
                self.command_output(["root"], timeout=timeout)
                self._have_root_shell = self._check_root_user(timeout=timeout)
                if self._have_root_shell:
                    self._logger.info("adbd restarted as root")
                else:
                    self._logger.info("adbd not restarted as root")
            except ADBError:
                self._logger.debug("Check for root adbd failed")

    def _pidof(self, appname, timeout=None):
        if self._have_pidof:
            try:
                pid_output = self.shell_output('pidof %s' % appname, timeout=timeout)
                re_pids = re.compile(r'[0-9]+')
                pids = re_pids.findall(pid_output)
                if self._have_flaky_pidof and not pids:
                    time.sleep(0.1)
                    pid_output = self.shell_output('pidof %s' % appname, timeout=timeout)
                    pids = re_pids.findall(pid_output)
            except ADBError:
                pids = []
        else:
            procs = self.get_process_list(timeout=timeout)
            # limit the comparion to the first 75 characters due to a
            # limitation in processname length in android.
            pids = [proc[0] for proc in procs if proc[1] == appname[:75]]
        return pids

    @staticmethod
    def _escape_command_line(cmd):
        """Utility function to return escaped and quoted version of command
        line.
        """
        re_quotable_chars = re.compile(r"[ ()\"&'\]]")

        def is_quoted(s, delim):
            if not s:
                return False
            return s[0] == delim and s[-1] == delim

        quoted_cmd = []

        for arg in cmd:
            if not is_quoted(arg, "'") and not is_quoted(arg, '"') and \
               re_quotable_chars.search(arg):
                arg = '"%s"' % arg.replace(r'"', r'\"')

            quoted_cmd.append(arg)

        return " ".join(quoted_cmd)

    @staticmethod
    def _get_exitcode(file_obj):
        """Get the exitcode from the last line of the file_obj for shell
        commands.
        """
        re_returncode = re.compile(r'adb_returncode=([0-9]+)')
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

        match = re_returncode.match(line)
        if match:
            exitcode = int(match.group(1))
            # Set the position in the file to the position of the
            # adb_returncode and truncate it from the output.
            file_obj.seek(-1, os.SEEK_CUR)
            file_obj.truncate()
        else:
            exitcode = None
            # We may have a situation where the adb_returncode= is not
            # at the end of the output. This happens at least in the
            # failure jit-tests on arm. To work around this
            # possibility, we can search the entire output for the
            # appropriate match.
            file_obj.seek(0, os.SEEK_SET)
            for line in file_obj:
                match = re_returncode.search(line)
                if match:
                    exitcode = int(match.group(1))
                    break
            # Reset the position in the file to the end.
            file_obj.seek(0, os.SEEK_END)

        return exitcode

    def is_path_internal_storage(self, path, timeout=None):
        """
        Return True if the path matches an internal storage path
        as defined by either '/sdcard', '/mnt/sdcard', or any of the
        .*_STORAGE environment variables on the device otherwise False.
        :param str path: The path to test.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.  This timeout is per adb call. The
            total time spent may exceed this value. If it is not
            specified, the value set in the ADBDevice constructor is used.
        :returns: boolean

        :raises: * ADBTimeoutError
                 * ADBError
        """
        if not self._re_internal_storage:
            storage_dirs = set(['/mnt/sdcard', '/sdcard'])
            re_STORAGE = re.compile('([^=]+STORAGE)=(.*)')
            lines = self.shell_output('set', timeout=timeout).split()
            for line in lines:
                m = re_STORAGE.match(line.strip())
                if m and m.group(2):
                    storage_dirs.add(m.group(2))
            self._re_internal_storage = re.compile('/|'.join(list(storage_dirs)) + '/')
        return self._re_internal_storage.match(path) is not None

    @property
    def test_root(self):
        """
        The test_root property returns the directory on the device where
        temporary test files are stored.

        The first time test_root it is called it determines and caches a value
        for the test root on the device. It determines the appropriate test
        root by attempting to create a 'dummy' directory on each of a list of
        directories and returning the first successful directory as the
        test_root value.

        The default list of directories checked by test_root are:

        - /sdcard/tests
        - /mnt/sdcard/tests
        - /data/local/tests

        You may override the default list by providing a test_root argument to
        the :class:`ADBDevice` constructor which will then be used when
        attempting to create the 'dummy' directory.

        :raises: * ADBTimeoutError
                 * ADBRootError
                 * ADBError
        """
        if self._test_root is not None:
            return self._test_root

        if self._initial_test_root:
            paths = [self._initial_test_root]
        else:
            paths = ['/sdcard/tests',
                     '/mnt/sdcard/tests',
                     '/data/local/tests']

        max_attempts = 3
        for attempt in range(1, max_attempts + 1):
            for test_root in paths:
                self._logger.debug("Setting test root to %s attempt %d of %d" %
                                   (test_root, attempt, max_attempts))

                if self._try_test_root(test_root):
                    self._test_root = test_root
                    return self._test_root

                self._logger.debug('_setup_test_root: '
                                   'Attempt %d of %d failed to set test_root to %s' %
                                   (attempt, max_attempts, test_root))

            if attempt != max_attempts:
                time.sleep(20)

        raise ADBError("Unable to set up test root using paths: [%s]"
                       % ", ".join(paths))

    def _try_test_root(self, test_root):
        base_path, sub_path = posixpath.split(test_root)
        if not self.is_dir(base_path, root=self._require_root):
            return False

        try:
            dummy_dir = posixpath.join(test_root, 'dummy')
            if self.is_dir(dummy_dir, root=self._require_root):
                self.rm(dummy_dir, recursive=True, root=self._require_root)
            self.mkdir(dummy_dir, parents=True, root=self._require_root)
            self.chmod(test_root, recursive=True, root=self._require_root)
        except ADBError:
            self._logger.debug("%s is not writable" % test_root)
            return False

        return True

    # Host Command methods

    def command(self, cmds, timeout=None):
        """Executes an adb command on the host against the device.

        :param list cmds: The command and its arguments to be
            executed.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.  This timeout is per adb call. The
            total time spent may exceed this value. If it is not
            specified, the value set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :returns: :class:`mozdevice.ADBProcess`

        command() provides a low level interface for executing
        commands for a specific device on the host via adb.

        command() executes on the host in such a fashion that stdout
        of the adb process are file handles on the host and
        the exit code is available as the exit code of the adb
        process.

        For executing shell commands on the device, use
        ADBDevice.shell().  The caller provides a list containing
        commands, as well as a timeout period in seconds.

        A subprocess is spawned to execute adb for the device with
        stdout and stderr directed to a temporary file. If the process
        takes longer than the specified timeout, the process is
        terminated.

        It is the caller's responsibilty to clean up by closing
        the stdout temporary file.
        """

        return ADBCommand.command(self, cmds,
                                  device_serial=self._device_serial,
                                  timeout=timeout)

    def command_output(self, cmds, timeout=None):
        """Executes an adb command on the host against the device returning
        stdout.

        :param list cmds: The command and its arguments to be executed.
        :param timeout: The maximum time in seconds
            for any spawned adb process to complete before throwing
            an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :returns: string - content of stdout.

        :raises: * ADBTimeoutError
                 * ADBError
        """
        return ADBCommand.command_output(self, cmds,
                                         device_serial=self._device_serial,
                                         timeout=timeout)

    # Networking methods

    def _validate_port(self, port, is_local=True):
        """Validate a port forwarding specifier. Raises ValueError on failure.

        :param str port: The port specifier to validate
        :param bool is_local: Flag indicating whether the port represents a local port.
        """
        prefixes = ["tcp", "localabstract", "localreserved", "localfilesystem", "dev"]

        if not is_local:
            prefixes += ["jdwp"]

        parts = port.split(":", 1)
        if len(parts) != 2 or parts[0] not in prefixes:
            raise ValueError("Invalid port specifier %s" % port)

    def _validate_direction(self, direction):
        """Validate direction of the socket connection. Raises ValueError on failure.

        :param str direction: The socket direction specifier to validate
        :raises: * ValueError
        """
        if direction not in [self.SOCKET_DIRECTON_FORWARD, self.SOCKET_DIRECTON_REVERSE]:
            raise ValueError('Invalid direction specifier {}'.format(direction))

    def create_socket_connection(self, direction, local, remote, allow_rebind=True, timeout=None):
        """Sets up a socket connection in the specified direction.

        :param str direction: Direction of the socket connection
        :param str local: Local port
        :param str remote: Remote port
        :param bool allow_rebind: Do not fail if port is already bound
        :param timeout: The maximum time in seconds
            for any spawned adb process to complete before throwing
            an ADBTimeoutError. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :raises: * ValueError
                 * ADBTimeoutError
                 * ADBError
        """
        # validate socket direction, and local and remote port formatting.
        self._validate_direction(direction)
        for port, is_local in [(local, True), (remote, False)]:
            self._validate_port(port, is_local=is_local)

        cmd = [direction, local, remote]

        if not allow_rebind:
            cmd.insert(1, "--no-rebind")

        # execute commands to establish socket connection.
        self.command_output(cmd, timeout=timeout)

    def list_socket_connections(self, direction, timeout=None):
        """Return a list of tuples specifying active socket connectionss.

        Return values are of the form (device, local, remote).

        :param str direction: 'forward' to list forward socket connections
                              'reverse' to list reverse socket connections
        :param timeout: The maximum time in seconds
            for any spawned adb process to complete before throwing
            an ADBTimeoutError. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :raises: * ValueError
                 * ADBTimeoutError
                 * ADBError
        """
        self._validate_direction(direction)

        cmd = [direction, "--list"]
        output = self.command_output(cmd, timeout=timeout)
        return [tuple(line.split(" ")) for line in output.splitlines() if line.strip()]

    def remove_socket_connections(self, direction, local=None, timeout=None):
        """Remove existing socket connections for a given direction.

        :param str direction: 'forward' to remove forward socket connection
                              'reverse' to remove reverse socket connection
        :param local: local port specifier as for ADBDevice.forward. If local
            is not specified removes all forwards.
        :type local: str or None
        :param timeout: The maximum time in seconds
            for any spawned adb process to complete before throwing
            an ADBTimeoutError. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :raises: * ValueError
                 * ADBTimeoutError
                 * ADBError
        """
        self._validate_direction(direction)

        cmd = [direction]

        if local is None:
            cmd.extend(["--remove-all"])
        else:
            self._validate_port(local, is_local=True)
            cmd.extend(["--remove", local])

        self.command_output(cmd, timeout=timeout)

    # Legacy port forward methods

    def forward(self, local, remote, allow_rebind=True, timeout=None):
        """Forward a local port to a specific port on the device.

        See `ADBDevice.create_socket_connection`.
        """
        self.create_socket_connection(self.SOCKET_DIRECTON_FORWARD,
                                      local, remote, allow_rebind, timeout)

    def list_forwards(self, timeout=None):
        """Return a list of tuples specifying active forwards.

        See `ADBDevice.list_socket_connection`.
        """
        return self.list_socket_connections(self.SOCKET_DIRECTON_FORWARD, timeout)

    def remove_forwards(self, local=None, timeout=None):
        """Remove existing port forwards.

        See `ADBDevice.remove_socket_connection`.
        """
        self.remove_socket_connections(self.SOCKET_DIRECTON_FORWARD, local, timeout)

    # Legacy port reverse methods

    def reverse(self, local, remote, allow_rebind=True, timeout=None):
        """Sets up a reverse socket connection from device to host.

        See `ADBDevice.create_socket_connection`.
        """
        self.create_socket_connection(self.SOCKET_DIRECTON_REVERSE,
                                      local, remote, allow_rebind, timeout)

    def list_reverses(self, timeout=None):
        """Returns a list of tuples showing active reverse socket connections.

        See `ADBDevice.list_socket_connection`.
        """
        return self.list_socket_connections(self.SOCKET_DIRECTON_REVERSE, timeout)

    def remove_reverses(self, local=None, timeout=None):
        """Remove existing reverse socket connections.

        See `ADBDevice.remove_socket_connection`.
        """
        self.remove_socket_connections(self.SOCKET_DIRECTON_REVERSE,
                                       local, timeout)

    # Device Shell methods

    def shell(self, cmd, env=None, cwd=None, timeout=None, root=False,
              stdout_callback=None):
        """Executes a shell command on the device.

        :param str cmd: The command to be executed.
        :param env: Contains the environment variables and
            their values.
        :type env: dict or None
        :param cwd: The directory from which to execute.
        :type cwd: str or None
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.  This timeout is per adb call. The
            total time spent may exceed this value. If it is not
            specified, the value set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :param bool root: Flag specifying if the command should
            be executed as root.
        :param stdout_callback: Function called for each line of output.
        :returns: :class:`mozdevice.ADBProcess`
        :raises: ADBRootError

        shell() provides a low level interface for executing commands
        on the device via adb shell.

        shell() executes on the host in such as fashion that stdout
        contains the stdout and stderr of the host abd process
        combined with the stdout and stderr of the shell command
        on the device. The exit code of shell() is the exit code of
        the adb command if it was non-zero or the extracted exit code
        from the output of the shell command executed on the
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
        with stdout and stderr directed to a temporary file. If the
        process takes longer than the specified timeout, the process
        is terminated. The return code is extracted from the stdout
        and is then removed from the file.

        It is the caller's responsibilty to clean up by closing
        the stdout temporary files.

        """
        def _timed_read_line_handler(signum, frame):
            raise IOError('ReadLineTimeout')

        def _timed_read_line(filehandle, timeout=None):
            """
            Attempt to readline from filehandle. If readline does not return
            within timeout seconds, raise IOError('ReadLineTimeout').
            On Windows, required signal facilities are usually not available;
            as a result, the timeout is not respected and some reads may
            block on Windows.
            """
            if not hasattr(signal, 'SIGALRM'):
                return filehandle.readline()
            if timeout is None:
                timeout = 5
            line = ''
            default_alarm_handler = signal.getsignal(signal.SIGALRM)
            signal.signal(signal.SIGALRM, _timed_read_line_handler)
            signal.alarm(int(timeout))
            try:
                line = filehandle.readline()
            finally:
                signal.alarm(0)
                signal.signal(signal.SIGALRM, default_alarm_handler)
            return line

        if root and not self._have_root_shell:
            # If root was requested and we do not already have a root
            # shell, then use the appropriate version of su to invoke
            # the shell cmd. Prefer Android's su version since it may
            # falsely report support for su -c.
            if self._have_android_su:
                cmd = "su 0 %s" % cmd
            elif self._have_su:
                cmd = "su -c \"%s\"" % cmd
            else:
                raise ADBRootError('Can not run command %s as root!' % cmd)

        # prepend cwd and env to command if necessary
        if cwd:
            cmd = "cd %s && %s" % (cwd, cmd)
        if env:
            envstr = '&& '.join(map(lambda x: 'export %s=%s' %
                                    (x[0], x[1]), env.iteritems()))
            cmd = envstr + "&& " + cmd
        cmd += "; echo adb_returncode=$?"

        args = [self._adb_path]
        if self._adb_host:
            args.extend(['-H', self._adb_host])
        if self._adb_port:
            args.extend(['-P', str(self._adb_port)])
        if self._device_serial:
            args.extend(['-s', self._device_serial])
        args.extend(["wait-for-device", "shell", cmd])
        adb_process = ADBProcess(args)

        if timeout is None:
            timeout = self._timeout

        start_time = time.time()
        exitcode = adb_process.proc.poll()
        if not stdout_callback:
            while ((time.time() - start_time) <= float(timeout)) and exitcode is None:
                time.sleep(self._polling_interval)
                exitcode = adb_process.proc.poll()
        else:
            stdout2 = open(adb_process.stdout_file.name, 'rb')
            while ((time.time() - start_time) <= float(timeout)) and exitcode is None:
                try:
                    line = _timed_read_line(stdout2)
                    if line and len(line) > 0:
                        stdout_callback(line.rstrip())
                    else:
                        # no new output, so sleep and poll
                        time.sleep(self._polling_interval)
                except IOError:
                    pass
                exitcode = adb_process.proc.poll()
        if exitcode is None:
            adb_process.proc.kill()
            adb_process.timedout = True
            adb_process.exitcode = adb_process.proc.poll()
        elif exitcode == 0:
            adb_process.exitcode = self._get_exitcode(adb_process.stdout_file)
        else:
            adb_process.exitcode = exitcode

        if stdout_callback:
            line = stdout2.readline()
            while line:
                stdout_callback(line.rstrip())
                line = stdout2.readline()
            stdout2.close()

        adb_process.stdout_file.seek(0, os.SEEK_SET)

        return adb_process

    def shell_bool(self, cmd, env=None, cwd=None, timeout=None, root=False):
        """Executes a shell command on the device returning True on success
        and False on failure.

        :param str cmd: The command to be executed.
        :param env: Contains the environment variables and
            their values.
        :type env: dict or None
        :param cwd: The directory from which to execute.
        :type cwd: str or None
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :param bool root: Flag specifying if the command should
            be executed as root.
        :returns: boolean

        :raises: * ADBTimeoutError
                 * ADBRootError
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

    def shell_output(self, cmd, env=None, cwd=None, timeout=None, root=False):
        """Executes an adb shell on the device returning stdout.

        :param str cmd: The command to be executed.
        :param env: Contains the environment variables and their values.
        :type env: dict or None
        :param cwd: The directory from which to execute.
        :type cwd: str or None
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.  This timeout is per
            adb call. The total time spent may exceed this
            value. If it is not specified, the value set
            in the ADBDevice constructor is used.
        :type timeout: integer or None
        :param bool root: Flag specifying if the command
            should be executed as root.
        :returns: string - content of stdout.
        :raises: * ADBTimeoutError
                 * ADBRootError
                 * ADBError
        """
        adb_process = None
        try:
            adb_process = self.shell(cmd, env=env, cwd=cwd,
                                     timeout=timeout, root=root)
            if adb_process.timedout:
                raise ADBTimeoutError("%s" % adb_process)
            elif adb_process.exitcode:
                raise ADBProcessError(adb_process)
            output = adb_process.stdout_file.read().rstrip()
            if self._verbose:
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

    # Informational methods

    def _get_logcat_buffer_args(self, buffers):
        valid_buffers = set(['radio', 'main', 'events'])
        invalid_buffers = set(buffers).difference(valid_buffers)
        if invalid_buffers:
            raise ADBError('Invalid logcat buffers %s not in %s ' % (
                list(invalid_buffers), list(valid_buffers)))
        args = []
        for b in buffers:
            args.extend(['-b', b])
        return args

    def clear_logcat(self, timeout=None, buffers=[]):
        """Clears logcat via adb logcat -c.

        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.  This timeout is per
            adb call. The total time spent may exceed this
            value. If it is not specified, the value set
            in the ADBDevice constructor is used.
        :type timeout: integer or None
        :param list buffers: Log buffers to clear. Valid buffers are
            "radio", "events", and "main". Defaults to "main".
        :raises: * ADBTimeoutError
                 * ADBError
        """
        buffers = self._get_logcat_buffer_args(buffers)
        cmds = ["logcat", "-c"] + buffers
        self.command_output(cmds, timeout=timeout)
        self.shell_output("log logcat cleared", timeout=timeout)

    def get_logcat(self,
                   filter_specs=[
                       "dalvikvm:I",
                       "ConnectivityService:S",
                       "WifiMonitor:S",
                       "WifiStateTracker:S",
                       "wpa_supplicant:S",
                       "NetworkStateTracker:S",
                       "EmulatedCamera_Camera:S",
                       "EmulatedCamera_Device:S",
                       "EmulatedCamera_FakeCamera:S",
                       "EmulatedCamera_FakeDevice:S",
                       "EmulatedCamera_CallbackNotifier:S",
                       "GnssLocationProvider:S",
                       "Hyphenator:S",
                       "BatteryStats:S"],
                   format="time",
                   filter_out_regexps=[],
                   timeout=None,
                   buffers=[]):
        """Returns the contents of the logcat file as a list of strings.

        :param list filter_specs: Optional logcat messages to
            be included.
        :param str format: Optional logcat format.
        :param list filterOutRexps: Optional logcat messages to be
            excluded.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :param list buffers: Log buffers to retrieve. Valid buffers are
            "radio", "events", and "main". Defaults to "main".
        :returns: list of lines logcat output.
        :raises: * ADBTimeoutError
                 * ADBError
        """
        buffers = self._get_logcat_buffer_args(buffers)
        cmds = ["logcat", "-v", format, "-d"] + buffers + filter_specs
        lines = self.command_output(cmds, timeout=timeout).splitlines()

        for regex in filter_out_regexps:
            lines = [line for line in lines if not re.search(regex, line)]

        return lines

    def get_prop(self, prop, timeout=None):
        """Gets value of a property from the device via adb shell getprop.

        :param str prop: The propery name.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :returns: string value of property.
        :raises: * ADBTimeoutError
                 * ADBError
        """
        output = self.shell_output('getprop %s' % prop, timeout=timeout)
        return output

    def get_state(self, timeout=None):
        """Returns the device's state via adb get-state.

        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before throwing
            an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :returns: string value of adb get-state.
        :raises: * ADBTimeoutError
                 * ADBError
        """
        output = self.command_output(["get-state"], timeout=timeout).strip()
        return output

    def get_ip_address(self, interfaces=None, timeout=None):
        """Returns the device's ip address, or None if it doesn't have one

        :param interfaces: Interfaces to allow, or None to allow any
            non-loopback interface.
        :type interfaces: list or None
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before throwing
            an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :returns: string ip address of the device or None if it could not
            be found.
        :raises: * ADBTimeoutError
                 * ADBError
        """
        if not interfaces:
            interfaces = ["wlan0", "eth0"]
            wifi_interface = self.get_prop('wifi.interface', timeout=timeout)
            self._logger.debug('get_ip_address: wifi_interface: %s' % wifi_interface)
            if wifi_interface and wifi_interface not in interfaces:
                interfaces = interfaces.append(wifi_interface)

        # ifconfig interface
        # can return two different formats:
        # eth0: ip 192.168.1.139 mask 255.255.255.0 flags [up broadcast running multicast]
        # or
        # wlan0     Link encap:Ethernet  HWaddr 00:9A:CD:B8:39:65
        # inet addr:192.168.1.38  Bcast:192.168.1.255  Mask:255.255.255.0
        # inet6 addr: fe80::29a:cdff:feb8:3965/64 Scope: Link
        # UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1
        # RX packets:180 errors:0 dropped:0 overruns:0 frame:0
        # TX packets:218 errors:0 dropped:0 overruns:0 carrier:0
        # collisions:0 txqueuelen:1000
        # RX bytes:84577 TX bytes:31202

        re1_ip = re.compile(r'(\w+): ip ([0-9.]+) mask.*')
        # re1_ip will match output of the first format
        # with group 1 returning the interface and group 2 returing the ip address.

        # re2_interface will match the interface line in the second format
        # while re2_ip will match the inet addr line of the second format.
        re2_interface = re.compile(r'(\w+)\s+Link')
        re2_ip = re.compile(r'\s+inet addr:([0-9.]+)')

        matched_interface = None
        matched_ip = None
        re_bad_addr = re.compile(r'127.0.0.1|0.0.0.0')

        self._logger.debug('get_ip_address: ifconfig')
        for interface in interfaces:
            try:
                output = self.shell_output('ifconfig %s' % interface,
                                           timeout=timeout)
            except ADBError:
                output = ''

            for line in output.splitlines():
                if not matched_interface:
                    match = re1_ip.match(line)
                    if match:
                        matched_interface, matched_ip = match.groups()
                    else:
                        match = re2_interface.match(line)
                        if match:
                            matched_interface = match.group(1)
                else:
                    match = re2_ip.match(line)
                    if match:
                        matched_ip = match.group(1)

                if matched_ip:
                    if not re_bad_addr.match(matched_ip):
                        self._logger.debug('get_ip_address: found: %s %s' %
                                           (matched_interface, matched_ip))
                        return matched_ip
                    matched_interface = None
                    matched_ip = None

        self._logger.debug('get_ip_address: netcfg')
        # Fall back on netcfg if ifconfig does not work.
        # $ adb shell netcfg
        # lo       UP   127.0.0.1/8       0x00000049 00:00:00:00:00:00
        # dummy0   DOWN   0.0.0.0/0       0x00000082 8e:cd:67:48:b7:c2
        # rmnet0   DOWN   0.0.0.0/0       0x00000000 00:00:00:00:00:00
        # rmnet1   DOWN   0.0.0.0/0       0x00000000 00:00:00:00:00:00
        # rmnet2   DOWN   0.0.0.0/0       0x00000000 00:00:00:00:00:00
        # rmnet3   DOWN   0.0.0.0/0       0x00000000 00:00:00:00:00:00
        # rmnet4   DOWN   0.0.0.0/0       0x00000000 00:00:00:00:00:00
        # rmnet5   DOWN   0.0.0.0/0       0x00000000 00:00:00:00:00:00
        # rmnet6   DOWN   0.0.0.0/0       0x00000000 00:00:00:00:00:00
        # rmnet7   DOWN   0.0.0.0/0       0x00000000 00:00:00:00:00:00
        # sit0     DOWN   0.0.0.0/0       0x00000080 00:00:00:00:00:00
        # vip0     DOWN   0.0.0.0/0       0x00001012 00:01:00:00:00:01
        # wlan0    UP   192.168.1.157/24  0x00001043 38:aa:3c:1c:f6:94

        re3_netcfg = re.compile(r'(\w+)\s+UP\s+([1-9]\d{0,2}\.\d{1,3}\.\d{1,3}\.\d{1,3})')
        try:
            output = self.shell_output('netcfg', timeout=timeout)
        except ADBError:
            output = ''
        for line in output.splitlines():
            match = re3_netcfg.search(line)
            if match:
                matched_interface, matched_ip = match.groups()
                if matched_interface == "lo" or re_bad_addr.match(matched_ip):
                    matched_interface = None
                    matched_ip = None
                elif matched_ip and matched_interface in interfaces:
                    self._logger.debug('get_ip_address: found: %s %s' %
                                       (matched_interface, matched_ip))
                    return matched_ip
        self._logger.debug('get_ip_address: not found')
        return matched_ip

    # File management methods

    def remount(self, timeout=None):
        """Remount /system/ in read/write mode

        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before throwing
            an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :raises: * ADBTimeoutError
                 * ADBError"""

        rv = self.command_output(["remount"], timeout=timeout)
        if not rv.startswith("remount succeeded"):
            raise ADBError("Unable to remount device")

    def batch_execute(self, commands, timeout=None, root=False):
        """Writes commands to a temporary file then executes on the device.

        :param list commands_list: List of commands to be run by the shell.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before throwing
            an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :param bool root: Flag specifying if the command should
            be executed as root.
        :raises: * ADBTimeoutError
                 * ADBRootError
                 * ADBError
        """
        try:
            tmpf = tempfile.NamedTemporaryFile(delete=False)
            tmpf.write('\n'.join(commands))
            tmpf.close()
            script = '/sdcard/{}'.format(os.path.basename(tmpf.name))
            self.push(tmpf.name, script)
            self.shell_output('sh {}'.format(script), timeout=timeout,
                              root=root)
        finally:
            if tmpf:
                os.unlink(tmpf.name)
            if script:
                self.rm(script, timeout=timeout, root=root)

    def chmod(self, path, recursive=False, mask="777", timeout=None, root=False):
        """Recursively changes the permissions of a directory on the
        device.

        :param str path: The directory name on the device.
        :param bool recursive: Flag specifying if the command should be
            executed recursively.
        :param str mask: The octal permissions.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before throwing
            an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :param bool root: Flag specifying if the command should
            be executed as root.
        :raises: * ADBTimeoutError
                 * ADBRootError
                 * ADBError
        """
        # Note that on some tests such as webappstartup, an error
        # occurs during recursive calls to chmod where a "No such file
        # or directory" error will occur for the
        # /data/data/org.mozilla.fennec/files/mozilla/*.webapp0/lock
        # which is a symbolic link to a socket: lock ->
        # 127.0.0.1:+<port>.  On Linux, chmod -R ignores symbolic
        # links but it appear Android's version does not. We ignore
        # this type of error, but pass on any other errors that are
        # detected.
        path = posixpath.normpath(path.strip())
        self._logger.debug('chmod: path=%s, recursive=%s, mask=%s, root=%s' %
                           (path, recursive, mask, root))
        if self.is_path_internal_storage(path, timeout=timeout):
            # External storage on Android is case-insensitive and permissionless
            # therefore even with the proper privileges it is not possible
            # to change modes.
            self._logger.debug('Ignoring attempt to chmod external storage')
            return

        # build up the command to be run based on capabilities.
        command = ['chmod']

        if recursive and self._chmod_R:
            command.append('-R')

        command.append(mask)

        if recursive and not self._chmod_R:
            paths = self.ls(path, recursive=True, timeout=timeout, root=root)
            base = ' '.join(command)
            commands = [' '.join([base, entry]) for entry in paths]
            self.batch_execute(commands, timeout, root)
        else:
            command.append(path)
            self.shell_output(cmd=' '.join(command), timeout=timeout, root=root)

    def chown(self, path, owner, group=None, recursive=False, timeout=None, root=False):
        """Run the chown command on the provided path.

        :param str path: path name on the device.
        :param str owner: new owner of the path.
        :param group: optional parameter specifying the new group the path
                      should belong to.
        :type group: str or None
        :param bool recursive: optional value specifying whether the command should
                    operate on files and directories recursively.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before throwing
            an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :param bool root: Flag specifying if the command should
            be executed as root.
        :raises: * ADBTimeoutError
                 * ADBRootError
                 * ADBError
        """
        path = posixpath.normpath(path.strip())
        if self.is_path_internal_storage(path, timeout=timeout):
            self._logger.warning('Ignoring attempt to chown external storage')
            return

        # build up the command to be run based on capabilities.
        command = ['chown']

        if recursive and self._chown_R:
            command.append('-R')

        if group:
            # officially supported notation is : but . has been checked with
            # sdk 17 and it works.
            command.append('{owner}.{group}'.format(owner=owner, group=group))
        else:
            command.append(owner)

        if recursive and not self._chown_R:
            # recursive desired, but chown -R is not supported natively.
            # like with chmod, get the list of subpaths, put them into a script
            # then run it with adb with one call.
            paths = self.ls(path, recursive=True, timeout=timeout, root=root)
            base = ' '.join(command)
            commands = [' '.join([base, entry]) for entry in paths]

            self.batch_execute(commands, timeout, root)
        else:
            # recursive or not, and chown -R is supported natively.
            # command can simply be run as provided by the user.
            command.append(path)
            self.shell_output(cmd=' '.join(command), timeout=timeout, root=root)

    def _test_path(self, argument, path, timeout=None, root=False):
        """Performs path and file type checking.

        :param str argument: Command line argument to the test command.
        :param str path: The path or filename on the device.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :param bool root: Flag specifying if the command should be
            executed as root.
        :returns: boolean - True if path or filename fulfills the
            condition of the test.
        :raises: * ADBTimeoutError
                 * ADBRootError
        """
        return self.shell_bool('test -{arg} {path}'.format(arg=argument,
                                                           path=path),
                               timeout=timeout, root=root)

    def exists(self, path, timeout=None, root=False):
        """Returns True if the path exists on the device.

        :param str path: The path name on the device.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :param bool root: Flag specifying if the command should be
            executed as root.
        :returns: boolean - True if path exists.
        :raises: * ADBTimeoutError
                 * ADBRootError
        """
        path = posixpath.normpath(path)
        return self._test_path('e', path, timeout, root)

    def is_dir(self, path, timeout=None, root=False):
        """Returns True if path is an existing directory on the device.

        :param str path: The directory on the device.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :param bool root: Flag specifying if the command should
            be executed as root.
        :returns: boolean - True if path exists on the device and is a
            directory.
        :raises: * ADBTimeoutError
                 * ADBRootError
        """
        path = posixpath.normpath(path)
        return self._test_path('d', path, timeout, root)

    def is_file(self, path, timeout=None, root=False):
        """Returns True if path is an existing file on the device.

        :param str path: The file name on the device.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :param bool root: Flag specifying if the command should
            be executed as root.
        :returns: boolean - True if path exists on the device and is a
            file.
        :raises: * ADBTimeoutError
                 * ADBRootError
        """
        path = posixpath.normpath(path)
        return self._test_path('f', path, timeout, root)

    def list_files(self, path, timeout=None, root=False):
        """Return a list of files/directories contained in a directory
        on the device.

        :param str path: The directory name on the device.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :param bool root: Flag specifying if the command should
            be executed as root.
        :returns: list of files/directories contained in the directory.
        :raises: * ADBTimeoutError
                 * ADBRootError
        """
        path = posixpath.normpath(path.strip())
        data = []
        if self.is_dir(path, timeout=timeout, root=root):
            try:
                data = self.shell_output("%s %s" % (self._ls, path),
                                         timeout=timeout,
                                         root=root).splitlines()
                self._logger.debug('list_files: data: %s' % data)
            except ADBError:
                self._logger.error('Ignoring exception in ADBDevice.list_files\n%s' %
                                   traceback.format_exc())
        data[:] = [item for item in data if item]
        self._logger.debug('list_files: %s' % data)
        return data

    def ls(self, path, recursive=False, timeout=None, root=False):
        """Return a list of matching files/directories on the device.

        The ls method emulates the behavior of the ls shell command.
        It differs from the list_files method by supporting wild cards
        and returning matches even if the path is not a directory and
        by allowing a recursive listing.

        ls /sdcard always returns /sdcard and not the contents of the
        sdcard path. The ls method makes the behavior consistent with
        others paths by adjusting /sdcard to /sdcard/. Note this is
        also the case of other sdcard related paths such as
        /storage/emulated/legacy but no adjustment is made in those
        cases.

        The ls method works around a Nexus 4 bug which prevents
        recursive listing of directories on the sdcard unless the path
        ends with "/*" by adjusting sdcard paths ending in "/" to end
        with "/*". This adjustment is only made on official Nexus 4
        builds with property ro.product.model "Nexus 4". Note that
        this will fail to return any "hidden" files or directories
        which begin with ".".

        :param str path: The directory name on the device.
        :param bool recursive: Flag specifying if a recursive listing
            is to be returned. If recursive is False, the returned
            matches will be relative to the path. If recursive is True,
            the returned matches will be absolute paths.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :param bool root: Flag specifying if the command should
            be executed as root.
        :returns: list of files/directories contained in the directory.
        :raises: * ADBTimeoutError
                 * ADBRootError
        """
        path = posixpath.normpath(path.strip())
        parent = ''
        entries = {}

        if path == '/sdcard':
            path += '/'

        # Android 2.3 and later all appear to support ls -R however
        # Nexus 4 does not perform a recursive search on the sdcard
        # unless the path is a directory with * wild card.
        if not recursive:
            recursive_flag = ''
        else:
            recursive_flag = '-R'
            if path.startswith('/sdcard') and path.endswith('/'):
                model = self.get_prop('ro.product.model',
                                      timeout=timeout,
                                      root=root)
                if model == 'Nexus 4':
                    path += '*'
        lines = self.shell_output('%s %s %s' % (self._ls, recursive_flag, path),
                                  timeout=timeout,
                                  root=root).splitlines()
        for line in lines:
            line = line.strip()
            if not line:
                parent = ''
                continue
            if line.endswith(':'):  # This is a directory
                parent = line.replace(':', '/')
                entry = parent
                # Remove earlier entry which is marked as a file.
                if parent[:-1] in entries:
                    del entries[parent[:-1]]
            elif parent:
                entry = "%s%s" % (parent, line)
            else:
                entry = line
            entries[entry] = 1
        entry_list = entries.keys()
        entry_list.sort()
        return entry_list

    def mkdir(self, path, parents=False, timeout=None, root=False):
        """Create a directory on the device.

        :param str path: The directory name on the device
            to be created.
        :param bool parents: Flag indicating if the parent directories are
            also to be created. Think mkdir -p path.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :param bool root: Flag specifying if the command should
            be executed as root.
        :raises: * ADBTimeoutError
                 * ADBRootError
                 * ADBError
        """
        path = posixpath.normpath(path)
        if parents:
            if self._mkdir_p is None or self._mkdir_p:
                # Use shell_bool to catch the possible
                # non-zero exitcode if -p is not supported.
                if self.shell_bool('mkdir -p %s' % path, timeout=timeout,
                                   root=root):
                    self._mkdir_p = True
                    return
            # mkdir -p is not supported. create the parent
            # directories individually.
            if not self.is_dir(posixpath.dirname(path), root=root):
                parts = path.split('/')
                name = "/"
                for part in parts[:-1]:
                    if part != "":
                        name = posixpath.join(name, part)
                        if not self.is_dir(name, root=root):
                            # Use shell_output to allow any non-zero
                            # exitcode to raise an ADBError.
                            self.shell_output('mkdir %s' % name,
                                              timeout=timeout, root=root)

        # If parents is True and the directory does exist, we don't
        # need to do anything. Otherwise we call mkdir. If the
        # directory already exists or if it is a file instead of a
        # directory, mkdir will fail and we will raise an ADBError.
        if not parents or not self.is_dir(path, root=root):
            self.shell_output('mkdir %s' % path, timeout=timeout, root=root)
        if not self.is_dir(path, timeout=timeout, root=root):
            raise ADBError('mkdir %s Failed' % path)

    def push(self, local, remote, timeout=None):
        """Pushes a file or directory to the device.

        :param str local: The name of the local file or
            directory name.
        :param str remote: The name of the remote file or
            directory name.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :raises: * ADBTimeoutError
                 * ADBError
        """
        # remove trailing /
        local = os.path.normpath(local)
        remote = posixpath.normpath(remote)
        copy_required = False
        if os.path.isdir(local):
            copy_required = True
            temp_parent = tempfile.mkdtemp()
            remote_name = os.path.basename(remote)
            new_local = os.path.join(temp_parent, remote_name)
            dir_util.copy_tree(local, new_local)
            local = new_local
            # See do_sync_push in
            # https://android.googlesource.com/platform/system/core/+/master/adb/file_sync_client.cpp
            # Work around change in behavior in adb 1.0.36 where if
            # the remote destination directory exists, adb push will
            # copy the source directory *into* the destination
            # directory otherwise it will copy the source directory
            # *onto* the destination directory.
            if self._adb_version >= '1.0.36' and self.is_dir(remote):
                remote = '/'.join(remote.rstrip('/').split('/')[:-1])
        try:
            self.command_output(["push", local, remote], timeout=timeout)
        except BaseException:
            raise
        finally:
            if copy_required:
                shutil.rmtree(temp_parent)

    def pull(self, remote, local, timeout=None):
        """Pulls a file or directory from the device.

        :param str remote: The path of the remote file or
            directory.
        :param str local: The path of the local file or
            directory name.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :raises: * ADBTimeoutError
                 * ADBError
        """
        # remove trailing /
        local = os.path.normpath(local)
        remote = posixpath.normpath(remote)
        copy_required = False
        original_local = local
        if self._adb_version >= '1.0.36' and \
           os.path.isdir(local) and self.is_dir(remote):
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
            local_name = os.path.basename(local)
            remote_name = os.path.basename(remote)
            if local_name != remote_name:
                copy_required = True
                temp_parent = tempfile.mkdtemp()
                local = os.path.join(temp_parent, remote_name)
            else:
                local = '/'.join(local.rstrip('/').split('/')[:-1])
        try:
            self.command_output(["pull", remote, local], timeout=timeout)
        except BaseException:
            raise
        finally:
            if copy_required:
                dir_util.copy_tree(local, original_local)
                shutil.rmtree(temp_parent)

    def get_file(self, remote, offset=None, length=None, timeout=None):
        """Pull file from device and return the file's content

        :param str remote: The path of the remote file.
        :param offset: If specified, return only content beyond this offset.
        :param length: If specified, limit content length accordingly.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :raises: * ADBTimeoutError
                 * ADBError
        """
        with tempfile.NamedTemporaryFile() as tf:
            self.pull(remote, tf.name, timeout=timeout)
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

    def rm(self, path, recursive=False, force=False, timeout=None, root=False):
        """Delete files or directories on the device.

        :param str path: The path of the remote file or directory.
        :param bool recursive: Flag specifying if the command is
            to be applied recursively to the target. Default is False.
        :param bool force: Flag which if True will not raise an
            error when attempting to delete a non-existent file. Default
            is False.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :param bool root: Flag specifying if the command should
            be executed as root.
        :raises: * ADBTimeoutError
                 * ADBRootError
                 * ADBError
        """
        cmd = "rm"
        if recursive:
            cmd += " -r"
        try:
            self.shell_output("%s %s" % (cmd, path), timeout=timeout, root=root)
            if self.is_file(path, timeout=timeout, root=root):
                raise ADBError('rm("%s") failed to remove file.' % path)
        except ADBError as e:
            if not force and 'No such file or directory' in e.message:
                raise

    def rmdir(self, path, timeout=None, root=False):
        """Delete empty directory on the device.

        :param str path: The directory name on the device.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :param bool root: Flag specifying if the command should
            be executed as root.
        :raises: * ADBTimeoutError
                 * ADBRootError
                 * ADBError
        """
        self.shell_output("rmdir %s" % path, timeout=timeout, root=root)
        if self.is_dir(path, timeout=timeout, root=root):
            raise ADBError('rmdir("%s") failed to remove directory.' % path)

    # Process management methods

    def get_process_list(self, timeout=None):
        """Returns list of tuples (pid, name, user) for running
        processes on device.

        :param timeout: The maximum time
            in seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified,
            the value set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :returns: list of (pid, name, user) tuples for running processes
            on the device.
        :raises: * ADBTimeoutError
                 * ADBError
        """
        adb_process = None
        max_attempts = 2
        try:
            for attempt in range(1, max_attempts + 1):
                adb_process = self.shell("ps", timeout=timeout)
                if adb_process.timedout:
                    raise ADBTimeoutError("%s" % adb_process)
                elif adb_process.exitcode:
                    raise ADBProcessError(adb_process)
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
                if user_i != -1 and pid_i != -1:
                    break
                # if this isn't the final attempt, don't print this as an error
                if attempt < max_attempts:
                    self._logger.info('get_process_list: attempt: %d %s' % (attempt, header))
                else:
                    raise ADBError('get_process_list: Unknown format: %s: %s' % (
                        header, adb_process))
            ret = []
            line = adb_process.stdout_file.readline()
            while line:
                els = line.split()
                try:
                    ret.append([int(els[pid_i]), els[-1], els[user_i]])
                except ValueError:
                    self._logger.error('get_process_list: %s %s\n%s' % (
                        header, line, traceback.format_exc()))
                    raise ADBError('get_process_list: %s: %s: %s' % (
                        header, line, adb_process))
                line = adb_process.stdout_file.readline()
            self._logger.debug('get_process_list: %s' % ret)
            return ret
        finally:
            if adb_process and isinstance(adb_process.stdout_file, file):
                adb_process.stdout_file.close()

    def kill(self, pids, sig=None, attempts=3, wait=5,
             timeout=None, root=False):
        """Kills processes on the device given a list of process ids.

        :param list pids: process ids to be killed.
        :param sig: signal to be sent to the process.
        :type sig: integer or None
        :param integer attempts: number of attempts to try to
            kill the processes.
        :param integer wait: number of seconds to wait after each attempt.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :param bool root: Flag specifying if the command should
            be executed as root.
        :raises: * ADBTimeoutError
                 * ADBRootError
                 * ADBError
        """
        pid_list = [str(pid) for pid in pids]
        for attempt in range(attempts):
            args = ["kill"]
            if sig:
                args.append("-%d" % sig)
            args.extend(pid_list)
            try:
                self.shell_output(' '.join(args), timeout=timeout, root=root)
            except ADBError as e:
                if 'No such process' not in e.message:
                    raise
            pid_set = set(pid_list)
            current_pid_set = set([str(proc[0]) for proc in
                                   self.get_process_list(timeout=timeout)])
            pid_list = list(pid_set.intersection(current_pid_set))
            if not pid_list:
                break
            self._logger.debug("Attempt %d of %d to kill processes %s failed" %
                               (attempt + 1, attempts, pid_list))
            time.sleep(wait)

        if pid_list:
            raise ADBError('kill: processes %s not killed' % pid_list)

    def pkill(self, appname, sig=None, attempts=3, wait=5,
              timeout=None, root=False):
        """Kills a processes on the device matching a name.

        :param str appname: The app name of the process to
            be killed. Note that only the first 75 characters of the
            process name are significant.
        :param sig: optional signal to be sent to the process.
        :type sig: integer or None
        :param integer attempts: number of attempts to try to
            kill the processes.
        :param integer wait: number of seconds to wait after each attempt.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :param bool root: Flag specifying if the command should
            be executed as root.

        :raises: * ADBTimeoutError
                 * ADBRootError
                 * ADBError
        """
        pids = self._pidof(appname, timeout=timeout)

        if not pids:
            return

        try:
            self.kill(pids, sig, attempts=attempts, wait=wait,
                      timeout=timeout, root=root)
        except ADBError as e:
            if self.process_exist(appname, timeout=timeout):
                raise e

    def process_exist(self, process_name, timeout=None):
        """Returns True if process with name process_name is running on
        device.

        :param str process_name: The name of the process
            to check. Note that only the first 75 characters of the
            process name are significant.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :returns: boolean - True if process exists.

        :raises: * ADBTimeoutError
                 * ADBError
        """
        if not isinstance(process_name, basestring):
            raise ADBError("Process name %s is not a string" % process_name)

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

        if self._pidof(app, timeout=timeout):
            return True
        return False

    def cp(self, source, destination, recursive=False, timeout=None,
           root=False):
        """Copies a file or directory on the device.

        :param source: string containing the path of the source file or
            directory.
        :param destination: string containing the path of the destination file
            or directory.
        :param recursive: optional boolean indicating if a recursive copy is to
            be performed. Required if the source is a directory. Defaults to
            False. Think cp -R source destination.
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :raises: * ADBTimeoutError
                 * ADBRootError
                 * ADBError
        """
        source = posixpath.normpath(source)
        destination = posixpath.normpath(destination)
        if self._have_cp:
            r = '-R' if recursive else ''
            self.shell_output('cp %s %s %s' % (r, source, destination),
                              timeout=timeout, root=root)
            return

        # Emulate cp behavior depending on if source and destination
        # already exists and whether they are a directory or file.
        if not self.exists(source, timeout=timeout, root=root):
            raise ADBError("cp: can't stat '%s': No such file or directory" %
                           source)

        if self.is_file(source, timeout=timeout, root=root):
            if self.is_dir(destination, timeout=timeout, root=root):
                # Copy the source file into the destination directory
                destination = posixpath.join(destination,
                                             posixpath.basename(source))
            self.shell_output('dd if=%s of=%s' % (source, destination),
                              timeout=timeout, root=root)
            return

        if self.is_file(destination, timeout=timeout, root=root):
            raise ADBError('cp: %s: Not a directory' % destination)

        if not recursive:
            raise ADBError("cp: omitting directory '%s'" % source)

        if self.is_dir(destination, timeout=timeout, root=root):
            # Copy the source directory into the destination directory.
            destination_dir = posixpath.join(destination,
                                             posixpath.basename(source))
        else:
            # Copy the contents of the source directory into the
            # destination directory.
            destination_dir = destination

        try:
            # Do not create parent directories since cp does not.
            self.mkdir(destination_dir, timeout=timeout, root=root)
        except ADBError as e:
            if 'File exists' not in e.message:
                raise

        for i in self.list_files(source, timeout=timeout, root=root):
            self.cp(posixpath.join(source, i),
                    posixpath.join(destination_dir, i),
                    recursive=recursive,
                    timeout=timeout, root=root)

    def mv(self, source, destination, timeout=None, root=False):
        """Moves a file or directory on the device.

        :param source: string containing the path of the source file or
            directory.
        :param destination: string containing the path of the destination file
            or directory.
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :raises: * ADBTimeoutError
                 * ADBRootError
                 * ADBError
        """
        source = posixpath.normpath(source)
        destination = posixpath.normpath(destination)
        self.shell_output('mv %s %s' % (source, destination), timeout=timeout,
                          root=root)

    def reboot(self, timeout=None):
        """Reboots the device.

        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADB constructor is used.
        :raises: * ADBTimeoutError
                 * ADBError

        reboot() reboots the device, issues an adb wait-for-device in order to
        wait for the device to complete rebooting, then calls is_device_ready()
        to determine if the device has completed booting.

        If the device supports running adbd as root, adbd will be
        restarted running as root. Then, if the device supports
        SELinux, setenforce Permissive will be called to change
        SELinux to permissive. This must be done after adbd is
        restarted in order for the SELinux Permissive setting to
        persist.

        """
        self.command_output(["reboot"], timeout=timeout)
        # command_output automatically inserts a 'wait-for-device'
        # argument to adb. Issuing an empty command is the same as adb
        # -s <device> wait-for-device. We don't send an explicit
        # 'wait-for-device' since that would add duplicate
        # 'wait-for-device' arguments which is an error in newer
        # versions of adb.
        self.command_output([], timeout=timeout)
        self._check_adb_root(timeout=timeout)
        return self.is_device_ready(timeout=timeout)

    def get_info(self, directive=None, timeout=None):
        """
        Returns a dictionary of information strings about the device.

        :param directive: information you want to get. Options are:
             - `battery` - battery charge as a percentage
             - `disk` - total, free, available bytes on disk
             - `id` - unique id of the device
             - `os` - name of the os
             - `process` - list of running processes (same as ps)
             - `systime` - system time of the device
             - `uptime` - uptime of the device

            If `directive` is `None`, will return all available information
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADB constructor is used.
        :raises: * ADBTimeoutError
                 * ADBError
        """
        directives = ['battery', 'disk', 'id', 'os', 'process', 'systime',
                      'uptime']

        if directive in directives:
            directives = [directive]

        info = {}
        if 'battery' in directives:
            info['battery'] = self.get_battery_percentage(timeout=timeout)
        if 'disk' in directives:
            info['disk'] = self.shell_output('df /data /system /sdcard',
                                             timeout=timeout).splitlines()
        if 'id' in directives:
            info['id'] = self.command_output(['get-serialno'], timeout=timeout)
        if 'os' in directives:
            info['os'] = self.get_prop('ro.build.display.id',
                                       timeout=timeout)
        if 'process' in directives:
            ps = self.shell_output('ps', timeout=timeout)
            info['process'] = ps.splitlines()
        if 'systime' in directives:
            info['systime'] = self.shell_output('date', timeout=timeout)
        if 'uptime' in directives:
            uptime = self.shell_output('uptime', timeout=timeout)
            if uptime:
                m = re.match(r'up time: ((\d+) days, )*(\d{2}):(\d{2}):(\d{2})',
                             uptime)
                if m:
                    uptime = '%d days %d hours %d minutes %d seconds' % tuple(
                        [int(g or 0) for g in m.groups()[1:]])
                info['uptime'] = uptime
        return info

    # Properties to manage SELinux on the device:
    # https://source.android.com/devices/tech/security/selinux/index.html
    # setenforce [ Enforcing | Permissive | 1 | 0 ]
    # getenforce returns either Enforcing or Permissive

    @property
    def selinux(self):
        """Returns True if SELinux is supported, False otherwise.
        """
        if self._selinux is None:
            self._selinux = (self.enforcing != '')
        return self._selinux

    @property
    def enforcing(self):
        try:
            enforce = self.shell_output('getenforce')
        except ADBError as e:
            enforce = ''
            self._logger.warning('Unable to get SELinux enforcing due to %s.' % e)
        return enforce

    @enforcing.setter
    def enforcing(self, value):
        """Set SELinux mode.
        :param str value: The new SELinux mode. Should be one of
            Permissive, 0, Enforcing, 1 but it is not validated.

        We do not attempt to set SELinux when _require_root is False.  This
        allows experimentation with running unrooted devices.
        """
        try:
            if not self._require_root:
                self._logger.info('Ignoring attempt to set SELinux %s.' % value)
            else:
                self._logger.info('Setting SELinux %s' % value)
                self.shell_output("setenforce %s" % value, root=True)
        except (ADBError, ADBRootError) as e:
            self._logger.warning('Unable to set SELinux Permissive due to %s.' % e)

    # Informational methods

    def get_battery_percentage(self, timeout=None):
        """Returns the battery charge as a percentage.

        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :returns: battery charge as a percentage.
        :raises: * ADBTimeoutError
                 * ADBError
        """
        level = None
        scale = None
        percentage = 0
        cmd = "dumpsys battery"
        re_parameter = re.compile(r'\s+(\w+):\s+(\d+)')
        lines = self.shell_output(cmd, timeout=timeout).splitlines()
        for line in lines:
            match = re_parameter.match(line)
            if match:
                parameter = match.group(1)
                value = match.group(2)
                if parameter == 'level':
                    level = float(value)
                elif parameter == 'scale':
                    scale = float(value)
                if parameter is not None and scale is not None:
                    percentage = 100.0 * level / scale
                    break
        return percentage

    def get_top_activity(self, timeout=None):
        """Returns the name of the top activity (focused app) reported by dumpsys

        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADBDevice constructor is used.
        :type timeout: integer or None
        :returns: package name of top activity or None (cannot be determined)
        :raises: * ADBTimeoutError
                 * ADBError
        """
        package = None
        data = None
        cmd = "dumpsys window windows"
        try:
            data = self.shell_output(cmd, timeout=timeout)
        except Exception:
            # dumpsys intermittently fails on some platforms (4.3 arm emulator)
            return package
        m = re.search('mFocusedApp(.+)/', data)
        if not m:
            # alternative format seen on newer versions of Android
            m = re.search('FocusedApplication(.+)/', data)
        if m:
            line = m.group(0)
            # Extract package name: string of non-whitespace ending in forward slash
            m = re.search('(\S+)/$', line)
            if m:
                package = m.group(1)
        if self._verbose:
            self._logger.debug('get_top_activity: %s' % str(package))
        return package

    # System control methods

    def is_device_ready(self, timeout=None):
        """Checks if a device is ready for testing.

        This method uses the android only package manager to check for
        readiness.

        :param timeout: The maximum time
            in seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADB constructor is used.
        :type timeout: integer or None
        :raises: * ADBTimeoutError
                 * ADBError
        """
        # command_output automatically inserts a 'wait-for-device'
        # argument to adb. Issuing an empty command is the same as adb
        # -s <device> wait-for-device. We don't send an explicit
        # 'wait-for-device' since that would add duplicate
        # 'wait-for-device' arguments which is an error in newer
        # versions of adb.
        self.command_output([], timeout=timeout)
        pm_error_string = "Error: Could not access the Package Manager"
        pm_list_commands = ["packages", "permission-groups", "permissions",
                            "instrumentation", "features", "libraries"]
        ready_path = os.path.join(self.test_root, "ready")
        for attempt in range(self._device_ready_retry_attempts):
            failure = 'Unknown failure'
            success = True
            try:
                state = self.get_state(timeout=timeout)
                if state != 'device':
                    failure = "Device state: %s" % state
                    success = False
                else:
                    if self.enforcing != 'Permissive':
                        self.enforcing = 'Permissive'
                    if self.is_dir(ready_path, timeout=timeout):
                        self.rmdir(ready_path, timeout=timeout)
                    self.mkdir(ready_path, timeout=timeout)
                    self.rmdir(ready_path, timeout=timeout)
                    # Invoke the pm list commands to see if it is up and
                    # running.
                    for pm_list_cmd in pm_list_commands:
                        data = self.shell_output("pm list %s" % pm_list_cmd,
                                                 timeout=timeout)
                        if pm_error_string in data:
                            failure = data
                            success = False
                            break
            except ADBError as e:
                success = False
                failure = e.message

            if not success:
                self._logger.debug('Attempt %s of %s device not ready: %s' % (
                    attempt + 1, self._device_ready_retry_attempts,
                    failure))
                time.sleep(self._device_ready_retry_wait)

        return success

    def power_on(self, timeout=None):
        """Sets the device's power stayon value.

        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADB constructor is used.
        :type timeout: integer or None
        :raises: * ADBTimeoutError
                 * ADBError
        """
        try:
            self.shell_output('svc power stayon true',
                              timeout=timeout,
                              root=True)
        except ADBError as e:
            # Executing this via adb shell errors, but not interactively.
            # Any other exitcode is a real error.
            if 'exitcode: 137' not in e.message:
                raise
            self._logger.warning('Unable to set power stayon true: %s' % e)

    # Application management methods

    def grant_runtime_permissions(self, app_name, timeout=None):
        """
        Grant required runtime permissions to the specified app
        (typically org.mozilla.fennec_$USER).

        :param str: app_name: Name of application (e.g. `org.mozilla.fennec`)
        """
        try:
            if self.version >= version_codes.M:
                permissions = [
                    'android.permission.WRITE_EXTERNAL_STORAGE',
                    'android.permission.READ_EXTERNAL_STORAGE',
                    'android.permission.ACCESS_COARSE_LOCATION',
                    'android.permission.ACCESS_FINE_LOCATION',
                    'android.permission.CAMERA',
                    'android.permission.RECORD_AUDIO',
                ]
                self._logger.info("Granting important runtime permissions to %s" % app_name)
                for permission in permissions:
                    self.shell_output('pm grant %s %s' % (app_name, permission))
        except ADBError as e:
            self._logger.warning("Unable to grant runtime permissions to %s due to %s" %
                                 (app_name, e))

    def install_app(self, apk_path, replace=False, timeout=None):
        """Installs an app on the device.

        :param str apk_path: The apk file name to be installed.
        :param bool replace: If True, replace existing application.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADB constructor is used.
        :type timeout: integer or None
        :raises: * ADBTimeoutError
                 * ADBError
        """
        cmd = ["install"]
        if replace:
            cmd.append("-r")
        cmd.append(apk_path)
        data = self.command_output(cmd, timeout=timeout)
        if data.find('Success') == -1:
            raise ADBError("install failed for %s. Got: %s" %
                           (apk_path, data))

    def is_app_installed(self, app_name, timeout=None):
        """Returns True if an app is installed on the device.

        :param str app_name: name of the app to be checked.
        :param timeout: maximum time in seconds for any spawned
            adb process to complete before throwing an ADBTimeoutError.
            This timeout is per adb call. If it is not specified,
            the value set in the ADB constructor is used.
        :type timeout: integer or None

        :raises: * ADBTimeoutError
                 * ADBError
        """
        pm_error_string = 'Error: Could not access the Package Manager'
        data = self.shell_output("pm list package %s" % app_name, timeout=timeout)
        if pm_error_string in data:
            raise ADBError(pm_error_string)
        output = [line for line in data.splitlines() if line.strip()]
        return any(['package:{}'.format(app_name) == out for out in output])

    def launch_application(self, app_name, activity_name, intent, url=None,
                           extras=None, wait=True, fail_if_running=True,
                           grant_runtime_permissions=True,
                           timeout=None):
        """Launches an Android application

        :param str app_name: Name of application (e.g. `com.android.chrome`)
        :param str activity_name: Name of activity to launch (e.g. `.Main`)
        :param str intent: Intent to launch application with
        :param url: URL to open
        :type url: str or None
        :param extras: Extra arguments for application.
        :type extras: dict or None
        :param bool wait: If True, wait for application to start before
            returning.
        :param bool fail_if_running: Raise an exception if instance of
            application is already running.
        :param bool grant_runtime_permissions: Grant special runtime
            permissions.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADB constructor is used.
        :type timeout: integer or None
        :raises: * ADBTimeoutError
                 * ADBError
        """
        # If fail_if_running is True, we throw an exception here. Only one
        # instance of an application can be running at once on Android,
        # starting a new instance may not be what we want depending on what
        # we want to do
        if fail_if_running and self.process_exist(app_name, timeout=timeout):
            raise ADBError("Only one instance of an application may be running "
                           "at once")

        if grant_runtime_permissions:
            self.grant_runtime_permissions(app_name)

        acmd = ["am", "start"] + \
            ["-W" if wait else '', "-n", "%s/%s" % (app_name, activity_name)]

        if intent:
            acmd.extend(["-a", intent])

        # Note that isinstance(True, int) and isinstance(False, int)
        # is True. This means we must test the type of the value
        # against bool prior to testing it against int in order to
        # prevent falsely identifying a bool value as an int.
        if extras:
            for (key, val) in extras.iteritems():
                if isinstance(val, bool):
                    extra_type_param = "--ez"
                elif isinstance(val, int):
                    extra_type_param = "--ei"
                else:
                    extra_type_param = "--es"
                acmd.extend([extra_type_param, str(key), str(val)])

        if url:
            acmd.extend(["-d", url])

        cmd = self._escape_command_line(acmd)
        self._logger.info('launch_application: %s' % cmd)
        cmd_output = self.shell_output(cmd, timeout=timeout)
        if 'Error:' in cmd_output:
            for line in cmd_output.split('\n'):
                self._logger.info(line)
            raise ADBError('launch_activity %s/%s failed' % (app_name, activity_name))

    def launch_fennec(self, app_name, intent="android.intent.action.VIEW",
                      moz_env=None, extra_args=None, url=None, wait=True,
                      fail_if_running=True, timeout=None):
        """Convenience method to launch Fennec on Android with various
        debugging arguments

        :param str app_name: Name of fennec application (e.g.
            `org.mozilla.fennec`)
        :param str intent: Intent to launch application.
        :param moz_env: Mozilla specific environment to pass into
            application.
        :type moz_env: str or None
        :param extra_args: Extra arguments to be parsed by fennec.
        :type extra_args: str or None
        :param url: URL to open
        :type url: str or None
        :param bool wait: If True, wait for application to start before
            returning.
        :param bool fail_if_running: Raise an exception if instance of
            application is already running.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADB constructor is used.
        :type timeout: integer or None
        :raises: * ADBTimeoutError
                 * ADBError
        """
        extras = {}

        if moz_env:
            # moz_env is expected to be a dictionary of environment variables:
            # Fennec itself will set them when launched
            for (env_count, (env_key, env_val)) in enumerate(moz_env.iteritems()):
                extras["env" + str(env_count)] = env_key + "=" + env_val

        # Additional command line arguments that fennec will read and use (e.g.
        # with a custom profile)
        if extra_args:
            extras['args'] = " ".join(extra_args)

        self.launch_application(app_name, "org.mozilla.gecko.BrowserApp",
                                intent, url=url, extras=extras,
                                wait=wait, fail_if_running=fail_if_running,
                                timeout=timeout)

    def launch_activity(self, app_name, activity_name=None,
                        intent="android.intent.action.MAIN",
                        moz_env=None, extra_args=None, url=None, e10s=False,
                        wait=True, fail_if_running=True, timeout=None):
        """Convenience method to launch an application on Android with various
        debugging arguments; convenient for geckoview apps.

        :param str app_name: Name of application (e.g.
            `org.mozilla.geckoview_example` or `org.mozilla.geckoview.test`)
        :param str activity_name: Activity name, like `GeckoViewActivity`, or
            `TestRunnerActivity`.
        :param str intent: Intent to launch application.
        :type moz_env: str or None
        :param extra_args: Extra arguments to be parsed by the app.
        :type extra_args: str or None
        :param url: URL to open
        :type url: str or None
        :param bool e10s: If True, run in multiprocess mode.
        :param bool wait: If True, wait for application to start before
            returning.
        :param bool fail_if_running: Raise an exception if instance of
            application is already running.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADB constructor is used.
        :type timeout: integer or None
        :raises: * ADBTimeoutError
                 * ADBError
        """
        extras = {}

        if moz_env:
            # moz_env is expected to be a dictionary of environment variables:
            # geckoview_example itself will set them when launched
            for (env_count, (env_key, env_val)) in enumerate(moz_env.iteritems()):
                extras["env" + str(env_count)] = env_key + "=" + env_val

        # Additional command line arguments that the app will read and use (e.g.
        # with a custom profile)
        if extra_args:
            extras['args'] = " ".join(extra_args)
        extras['use_multiprocess'] = e10s
        self.launch_application(app_name,
                                "%s.%s" % (app_name, activity_name),
                                intent, url=url, extras=extras,
                                wait=wait, fail_if_running=fail_if_running,
                                timeout=timeout)

    def stop_application(self, app_name, timeout=None, root=False):
        """Stops the specified application

        For Android 3.0+, we use the "am force-stop" to do this, which
        is reliable and does not require root. For earlier versions of
        Android, we simply try to manually kill the processes started
        by the app repeatedly until none is around any more. This is
        less reliable and does require root.

        :param str app_name: Name of application (e.g. `com.android.chrome`)
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADB constructor is used.
        :type timeout: integer or None
        :param bool root: Flag specifying if the command should be
            executed as root.
        :raises: * ADBTimeoutError
                 * ADBError
        """
        if self.version >= version_codes.HONEYCOMB:
            self.shell_output("am force-stop %s" % app_name,
                              timeout=timeout, root=root)
        else:
            num_tries = 0
            max_tries = 5
            while self.process_exist(app_name, timeout=timeout):
                if num_tries > max_tries:
                    raise ADBError("Couldn't successfully kill %s after %s "
                                   "tries" % (app_name, max_tries))
                self.pkill(app_name, timeout=timeout, root=root)
                num_tries += 1

                # sleep for a short duration to make sure there are no
                # additional processes in the process of being launched
                # (this is not 100% guaranteed to work since it is inherently
                # racey, but it's the best we can do)
                time.sleep(1)

    def uninstall_app(self, app_name, reboot=False, timeout=None):
        """Uninstalls an app on the device.

        :param str app_name: The name of the app to be
            uninstalled.
        :param bool reboot: Flag indicating that the device should
            be rebooted after the app is uninstalled. No reboot occurs
            if the app is not installed.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADB constructor is used.
        :type timeout: integer or None
        :raises: * ADBTimeoutError
                 * ADBError
        """
        if self.is_app_installed(app_name, timeout=timeout):
            data = self.command_output(["uninstall", app_name], timeout=timeout)
            if data.find('Success') == -1:
                self._logger.debug('uninstall_app failed: %s' % data)
                raise ADBError("uninstall failed for %s. Got: %s" % (app_name, data))
            if reboot:
                self.reboot(timeout=timeout)

    def update_app(self, apk_path, timeout=None):
        """Updates an app on the device and reboots.

        :param str apk_path: The apk file name to be
            updated.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADB constructor is used.
        :type timeout: integer or None
        :raises: * ADBTimeoutError
                 * ADBError
        """
        cmd = ["install", "-r"]
        if self.version >= version_codes.M:
            cmd.append("-g")
        cmd.append(apk_path)
        output = self.command_output(cmd, timeout=timeout)
        self.reboot(timeout=timeout)
        return output
