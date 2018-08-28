# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import os
import posixpath
import re
import time

from abc import ABCMeta

from . import version_codes

from .adb import ADBDevice, ADBError, ADBRootError


class ADBAndroid(ADBDevice):
    """ADBAndroid implements :class:`ADBDevice` providing Android-specific
    functionality.

    ::

       from mozdevice import ADBAndroid

       adbdevice = ADBAndroid()
       print(adbdevice.list_files("/mnt/sdcard"))
       if adbdevice.process_exist("org.mozilla.fennec"):
           print("Fennec is running")
    """
    __metaclass__ = ABCMeta

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
        """Initializes the ADBAndroid object.

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
        ADBDevice.__init__(self, device=device, adb=adb,
                           adb_host=adb_host, adb_port=adb_port,
                           test_root=test_root,
                           logger_name=logger_name,
                           timeout=timeout,
                           verbose=verbose,
                           device_ready_retry_wait=device_ready_retry_wait,
                           device_ready_retry_attempts=device_ready_retry_attempts,
                           require_root=require_root)
        self._selinux = None
        self.enforcing = 'Permissive'
        self.version = int(self.shell_output("getprop ro.build.version.sdk",
                                             timeout=timeout))
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
        ready = ADBDevice.reboot(self, timeout=timeout)
        self._check_adb_root(timeout=timeout)
        return ready

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
        if self.version >= version_codes.M:
            cmd.append("-g")
        if replace:
            cmd.append("-r")
        cmd.append(apk_path)
        data = self.command_output(cmd, timeout=timeout)
        if data.find('Success') == -1:
            raise ADBError("install failed for %s. Got: %s" %
                           (apk_path, data))

    def is_app_installed(self, app_name, timeout=None):
        """Returns True if an app is installed on the device.

        :param str app_name: The name of the app to be checked.
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
        pm_error_string = 'Error: Could not access the Package Manager'
        data = self.shell_output("pm list package %s" % app_name, timeout=timeout)
        if pm_error_string in data:
            raise ADBError(pm_error_string)
        if app_name not in data:
            return False
        return True

    def launch_application(self, app_name, activity_name, intent, url=None,
                           extras=None, wait=True, fail_if_running=True,
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
        self.shell_output(cmd, timeout=timeout)

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
                        intent="android.intent.action.Main",
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
