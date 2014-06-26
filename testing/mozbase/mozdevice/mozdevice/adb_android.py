# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import time

from adb import ADBDevice, ADBError
from distutils.version import StrictVersion


class ADBAndroidMixin(object):
    """Mixin to extend ADB with Android-specific functionality"""

    # System control methods

    def is_device_ready(self, timeout=None):
        """Checks if a device is ready for testing.

        This method uses the android only package manager to check for
        readiness.

        :param timeout: optional integer specifying the maximum time
            in seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADB constructor is used.
        :raises: * ADBTimeoutError - raised if the command takes longer than
                   timeout seconds.
                 * ADBError - raised if the wait-for-device command fails.

        """
        self.command_output(["wait-for-device"], timeout=timeout)
        pm_error_string = "Error: Could not access the Package Manager"
        pm_list_commands = ["packages", "permission-groups", "permissions",
                            "instrumentation", "features", "libraries"]
        ready_path = os.path.join(self.test_root, "ready")
        for attempt in range(self._device_ready_retry_attempts):
            success = True
            try:
                if self.get_state(timeout=timeout) != 'device':
                    success = False
                else:
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
                            success = False
                            break
            except ADBError, e:
                success = False
                data = e.message

            if not success:
                self._logger.debug('Attempt %s of %s device not ready: %s' % (
                    attempt+1, self._device_ready_retry_attempts,
                    data))
                time.sleep(self._device_ready_retry_wait)

        return success

    def power_on(self, timeout=None):
        """Sets the device's power stayon value.

        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADB constructor is used.
        :raises: * ADBTimeoutError - raised if the command takes longer than
                   timeout seconds.
                 * ADBError - raised if the command exits with a
                   non-zero exit code.

        """
        try:
            self.shell_output('svc power stayon true', timeout=timeout)
        except ADBError, e:
            # Executing this via adb shell errors, but not interactively.
            # Any other exitcode is a real error.
            if 'exitcode: 137' not in e.message:
                raise
            self._logger.warning('Unable to set power stayon true: %s' % e)

    def reboot(self, timeout=None):
        """Reboots the device.

        This method uses the Android only package manager to determine
        if the device is ready after the reboot.

        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADB constructor is used.
        :raises: * ADBTimeoutError - raised if the command takes longer than
                   timeout seconds.
                 * ADBError - raised if the command exits with a
                   non-zero exit code.

        reboot() reboots the device, issues an adb wait-for-device in order to
        wait for the device to complete rebooting, then calls is_device_ready()
        to determine if the device has completed booting.

        """
        self.command_output(["reboot"], timeout=timeout)
        self.command_output(["wait-for-device"], timeout=timeout)
        return self.is_device_ready(timeout=timeout)

    # Application management methods

    def install_app(self, apk_path, timeout=None):
        """Installs an app on the device.

        :param apk_path: string containing the apk file name to be
            installed.
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADB constructor is used.
        :raises: * ADBTimeoutError - raised if the command takes longer than
                   timeout seconds.
                 * ADBError - raised if the command exits with a
                   non-zero exit code.

        """
        data = self.command_output(["install", apk_path], timeout=timeout)
        if data.find('Success') == -1:
            raise ADBError("install failed for %s. Got: %s" %
                           (apk_path, data))

    def is_app_installed(self, app_name, timeout=None):
        """Returns True if an app is installed on the device.

        :param app_name: string containing the name of the app to be
            checked.
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADB constructor is used.
        :raises: * ADBTimeoutError - raised if the command takes longer than
                   timeout seconds.
                 * ADBError - raised if the command exits with a
                   non-zero exit code.

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

        :param app_name: Name of application (e.g. `com.android.chrome`)
        :param activity_name: Name of activity to launch (e.g. `.Main`)
        :param intent: Intent to launch application with
        :param url: URL to open
        :param extras: Dictionary of extra arguments for application.
        :param wait: If True, wait for application to start before
            returning.
        :param fail_if_running: Raise an exception if instance of
            application is already running.
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADB constructor is used.
        :raises: * ADBTimeoutError - raised if the command takes longer than
                   timeout seconds.
                 * ADBError - raised if the command exits with a
                   non-zero exit code.

        """
        # If fail_if_running is True, we throw an exception here. Only one
        # instance of an application can be running at once on Android,
        # starting a new instance may not be what we want depending on what
        # we want to do
        if fail_if_running and self.process_exist(app_name, timeout=timeout):
            raise ADBError("Only one instance of an application may be running "
                           "at once")

        acmd = [ "am", "start" ] + \
            ["-W" if wait else '', "-n", "%s/%s" % (app_name, activity_name)]

        if intent:
            acmd.extend(["-a", intent])

        if extras:
            for (key, val) in extras.iteritems():
                if type(val) is int:
                    extra_type_param = "--ei"
                elif type(val) is bool:
                    extra_type_param = "--ez"
                else:
                    extra_type_param = "--es"
                acmd.extend([extra_type_param, str(key), str(val)])

        if url:
            acmd.extend(["-d", url])

        cmd = self._escape_command_line(acmd)
        self.shell_output(cmd, timeout=timeout)

    def launch_fennec(self, app_name, intent="android.intent.action.VIEW",
                     moz_env=None, extra_args=None, url=None, wait=True,
                     fail_if_running=True, timeout=None):
        """Convenience method to launch Fennec on Android with various
        debugging arguments

        :param app_name: Name of fennec application (e.g.
            `org.mozilla.fennec`)
        :param intent: Intent to launch application.
        :param moz_env: Mozilla specific environment to pass into
            application.
        :param extra_args: Extra arguments to be parsed by fennec.
        :param url: URL to open
        :param wait: If True, wait for application to start before
            returning
        :param fail_if_running: Raise an exception if instance of
            application is already running
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADB constructor is used.
        :raises: * ADBTimeoutError - raised if the command takes longer than
                   timeout seconds.
                 * ADBError - raised if the command exits with a
                   non-zero exit code.

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

        self.launch_application(app_name, ".App", intent, url=url, extras=extras,
                               wait=wait, fail_if_running=fail_if_running,
                               timeout=timeout)

    def stop_application(self, app_name, timeout=None, root=False):
        """Stops the specified application

        For Android 3.0+, we use the "am force-stop" to do this, which
        is reliable and does not require root. For earlier versions of
        Android, we simply try to manually kill the processes started
        by the app repeatedly until none is around any more. This is
        less reliable and does require root.

        :param app_name: Name of application (e.g. `com.android.chrome`)
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADB constructor is used.
        :raises: * ADBTimeoutError - raised if the command takes longer than
                   timeout seconds.
                 * ADBError - raised if the command exits with a
                   non-zero exit code.

        """
        version = self.shell_output("getprop ro.build.version.release",
                                    timeout=timeout, root=root)
        if StrictVersion(version) >= StrictVersion('3.0'):
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

        :param app_name: string containing the name of the app to be
            uninstalled.
        :param reboot: boolean flag indicating that the device should
            be rebooted after the app is uninstalled. No reboot occurs
            if the app is not installed.
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADB constructor is used.
        :raises: * ADBTimeoutError - raised if the command takes longer than
                   timeout seconds.
                 * ADBError - raised if the command exits with a
                   non-zero exit code.

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

        :param apk_path: string containing the apk file name to be
            updated.
        :param timeout: optional integer specifying the maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.
            This timeout is per adb call. The total time spent
            may exceed this value. If it is not specified, the value
            set in the ADB constructor is used.
        :raises: * ADBTimeoutError - raised if the command takes longer than
                   timeout seconds.
                 * ADBError - raised if the command exits with a
                   non-zero exit code.

        """
        output = self.command_output(["install", "-r", apk_path],
                                     timeout=timeout)
        self.reboot(timeout=timeout)
        return output


class ADBAndroid(ADBDevice, ADBAndroidMixin):
    """ADBAndroid provides all of the methods of :class:`mozdevice.ADB` with
    Android specific extensions useful for that platform.

    ::

        from mozdevice import ADBAndroid as ADBDevice

        adb = ADBDevice(...)

        if adb.is_device_ready():
            adb.install_app("/tmp/build.apk")
            adb.launch_fennec("org.mozilla.fennec")

    """
    pass
