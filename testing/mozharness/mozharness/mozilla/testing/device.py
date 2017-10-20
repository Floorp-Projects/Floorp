#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
'''Interact with a device via ADB

This code is largely from
https://hg.mozilla.org/build/tools/file/default/sut_tools
'''

import datetime
import os
import re
import subprocess
import sys
import time

from mozharness.base.errors import ADBErrorList
from mozharness.base.log import LogMixin, DEBUG
from mozharness.base.script import ScriptMixin


# Device flags
DEVICE_UNREACHABLE = 0x01
DEVICE_NOT_CONNECTED = 0x02
DEVICE_MISSING_SDCARD = 0x03
DEVICE_HOST_ERROR = 0x04
# DEVICE_UNRECOVERABLE_ERROR?
DEVICE_NOT_REBOOTED = 0x05
DEVICE_CANT_REMOVE_DEVROOT = 0x06
DEVICE_CANT_REMOVE_ETC_HOSTS = 0x07
DEVICE_CANT_SET_TIME = 0x08


class DeviceException(Exception):
    pass


# BaseDeviceHandler {{{1
class BaseDeviceHandler(ScriptMixin, LogMixin):
    device_id = None
    device_root = None
    default_port = None
    device_flags = []

    def __init__(self, log_obj=None, config=None, script_obj=None):
        super(BaseDeviceHandler, self).__init__()
        self.config = config
        self.log_obj = log_obj
        self.script_obj = script_obj

    def add_device_flag(self, flag):
        if flag not in self.device_flags:
            self.device_flags.append(flag)

    def query_device_id(self):
        if self.device_id:
            return self.device_id
        c = self.config
        device_id = None
        if c.get('device_id'):
            device_id = c['device_id']
        elif c.get('device_ip'):
            device_id = "%s:%s" % (c['device_ip'],
                                   c.get('device_port', self.default_port))
        self.device_id = device_id
        return self.device_id

    def query_download_filename(self, file_id=None):
        pass

    def ping_device(self):
        pass

    def check_device(self):
        pass

    def cleanup_device(self, reboot=False):
        pass

    def reboot_device(self):
        pass

    def query_device_root(self):
        pass

    def wait_for_device(self, interval=60, max_attempts=20):
        pass

    def install_app(self, file_path):
        pass


# ADBDeviceHandler {{{1
class ADBDeviceHandler(BaseDeviceHandler):
    def __init__(self, **kwargs):
        super(ADBDeviceHandler, self).__init__(**kwargs)
        self.default_port = 5555

    def query_device_exe(self, exe_name):
        return self.query_exe(exe_name, exe_dict="device_exes")

    def _query_config_device_id(self):
        return BaseDeviceHandler.query_device_id(self)

    def query_device_id(self, auto_connect=True):
        if self.device_id:
            return self.device_id
        device_id = self._query_config_device_id()
        if device_id:
            if auto_connect:
                self.ping_device(auto_connect=True)
        else:
            self.info("Trying to find device...")
            devices = self._query_attached_devices()
            if not devices:
                self.add_device_flag(DEVICE_NOT_CONNECTED)
                self.fatal("No device connected via adb!\nUse 'adb connect' or specify a device_id or device_ip in config!")
            elif len(devices) > 1:
                self.warning("""More than one device detected; specify 'device_id' or\n'device_ip' to target a specific device!""")
            device_id = devices[0]
            self.info("Found %s." % device_id)
        self.device_id = device_id
        return self.device_id

    # maintenance {{{2
    def ping_device(self, auto_connect=False, silent=False):
        if auto_connect and not self._query_attached_devices():
            self.connect_device()
        if not silent:
            self.info("Determining device connectivity over adb...")
        device_id = self.query_device_id()
        adb = self.query_exe('adb')
        uptime = self.query_device_exe('uptime')
        output = self.get_output_from_command([adb, "-s", device_id,
                                               "shell", uptime],
                                              silent=silent)
        if str(output).startswith("up time:"):
            if not silent:
                self.info("Found %s." % device_id)
            return True
        elif auto_connect:
            # TODO retry?
            self.connect_device()
            return self.ping_device()
        else:
            if not silent:
                self.error("Can't find a device.")
            return False

    def _query_attached_devices(self):
        devices = []
        adb = self.query_exe('adb')
        output = self.get_output_from_command([adb, "devices"])
        starting_list = False
        if output is None:
            self.add_device_flag(DEVICE_HOST_ERROR)
            self.fatal("Can't get output from 'adb devices'; install the Android SDK!")
        for line in output:
            if 'adb: command not found' in line:
                self.add_device_flag(DEVICE_HOST_ERROR)
                self.fatal("Can't find adb; install the Android SDK!")
            if line.startswith("* daemon"):
                continue
            if line.startswith("List of devices"):
                starting_list = True
                continue
            # TODO somehow otherwise determine whether this is an actual
            # device?
            if starting_list:
                devices.append(re.split('\s+', line)[0])
        return devices

    def connect_device(self):
        self.info("Connecting device...")
        adb = self.query_exe('adb')
        cmd = [adb, "connect"]
        device_id = self._query_config_device_id()
        if device_id:
            devices = self._query_attached_devices()
            if device_id in devices:
                # TODO is this the right behavior?
                self.disconnect_device()
            cmd.append(device_id)
        # TODO error check
        self.run_command(cmd, error_list=ADBErrorList)

    def disconnect_device(self):
        self.info("Disconnecting device...")
        device_id = self.query_device_id()
        if device_id:
            adb = self.query_exe('adb')
            # TODO error check
            self.run_command([adb, "-s", device_id,
                              "disconnect"],
                             error_list=ADBErrorList)
        else:
            self.info("No device found.")

    def check_device(self):
        if not self.ping_device(auto_connect=True):
            self.add_device_flag(DEVICE_NOT_CONNECTED)
            self.fatal("Can't find device!")
        if self.query_device_root() is None:
            self.add_device_flag(DEVICE_NOT_CONNECTED)
            self.fatal("Can't connect to device!")

    def reboot_device(self):
        if not self.ping_device(auto_connect=True):
            self.add_device_flag(DEVICE_NOT_REBOOTED)
            self.error("Can't reboot disconnected device!")
            return False
        device_id = self.query_device_id()
        self.info("Rebooting device...")
        adb = self.query_exe('adb')
        cmd = [adb, "-s", device_id, "reboot"]
        self.info("Running command (in the background): %s" % cmd)
        # This won't exit until much later, but we don't need to wait.
        # However, some error checking would be good.
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT)
        time.sleep(10)
        self.disconnect_device()
        status = False
        try:
            self.wait_for_device()
            status = True
        except DeviceException:
            self.error("Can't reconnect to device!")
        if p.poll() is None:
            p.kill()
        p.wait()
        return status

    def cleanup_device(self, reboot=False):
        self.info("Cleaning up device.")
        c = self.config
        device_id = self.query_device_id()
        status = self.remove_device_root()
        if not status:
            self.add_device_flag(DEVICE_CANT_REMOVE_DEVROOT)
            self.fatal("Can't remove device root!")
        if c.get("enable_automation"):
            self.remove_etc_hosts()
        if c.get("device_package_name"):
            adb = self.query_exe('adb')
            killall = self.query_device_exe('killall')
            self.run_command([adb, "-s", device_id, "shell",
                              killall, c["device_package_name"]],
                             error_list=ADBErrorList)
            self.uninstall_app(c['device_package_name'])
        if reboot:
            self.reboot_device()

    # device calls {{{2
    def query_device_root(self, silent=False):
        if self.device_root:
            return self.device_root
        device_root = None
        device_id = self.query_device_id()
        adb = self.query_exe('adb')
        output = self.get_output_from_command("%s -s %s shell df" % (adb, device_id),
                                              silent=silent)
        # TODO this assumes we're connected; error checking?
        if output is None or ' not found' in str(output):
            self.error("Can't get output from 'adb shell df'!\n%s" % output)
            return None
        if "/mnt/sdcard" in output:
            device_root = "/mnt/sdcard/tests"
        else:
            device_root = "/data/local/tmp/tests"
        if not silent:
            self.info("Device root is %s" % str(device_root))
        self.device_root = device_root
        return self.device_root

    def wait_for_device(self, interval=60, max_attempts=20):
        self.info("Waiting for device to come back...")
        time.sleep(interval)
        tries = 0
        while tries <= max_attempts:
            tries += 1
            self.info("Try %d" % tries)
            if self.ping_device(auto_connect=True, silent=True):
                return self.ping_device()
            time.sleep(interval)
        raise DeviceException("Remote Device Error: waiting for device timed out.")

    def query_device_time(self):
        device_id = self.query_device_id()
        adb = self.query_exe('adb')
        # adb shell 'date' will give a date string
        date_string = self.get_output_from_command([adb, "-s", device_id,
                                                    "shell", "date"])
        # TODO what to do when we error?
        return date_string

    def set_device_time(self, device_time=None, error_level='error'):
        # adb shell date -s YYYYMMDD.hhmmss will set date
        device_id = self.query_device_id()
        if device_time is None:
            device_time = time.strftime("%Y%m%d.%H%M%S")
        self.info(self.query_device_time())
        adb = self.query_exe('adb')
        status = self.run_command([adb, "-s", device_id,  "shell", "date", "-s",
                                   str(device_time)],
                                  error_list=ADBErrorList)
        self.info(self.query_device_time())
        return status

    def query_device_file_exists(self, file_name):
        device_id = self.query_device_id()
        adb = self.query_exe('adb')
        output = self.get_output_from_command([adb, "-s", device_id,
                                               "shell", "ls", "-d", file_name])
        if str(output).rstrip() == file_name:
            return True
        return False

    def remove_device_root(self, error_level='error'):
        device_root = self.query_device_root()
        device_id = self.query_device_id()
        if device_root is None:
            self.add_device_flag(DEVICE_UNREACHABLE)
            self.fatal("Can't connect to device!")
        adb = self.query_exe('adb')
        if self.query_device_file_exists(device_root):
            self.info("Removing device root %s." % device_root)
            self.run_command([adb, "-s", device_id, "shell", "rm",
                              "-r", device_root], error_list=ADBErrorList)
            if self.query_device_file_exists(device_root):
                self.add_device_flag(DEVICE_CANT_REMOVE_DEVROOT)
                self.log("Unable to remove device root!", level=error_level)
                return False
        return True

    def install_app(self, file_path):
        c = self.config
        device_id = self.query_device_id()
        adb = self.query_exe('adb')
        if self._log_level_at_least(DEBUG):
            self.run_command([adb, "-s", device_id, "shell", "ps"],
                             error_list=ADBErrorList)
            uptime = self.query_device_exe('uptime')
            self.run_command([adb, "-s", "shell", uptime],
                             error_list=ADBErrorList)
        if not c['enable_automation']:
            # -s to install on sdcard? Needs to be config driven
            self.run_command([adb, "-s", device_id, "install", '-r',
                              file_path],
                             error_list=ADBErrorList)
        else:
            # A slow-booting device may not allow installs, temporarily.
            # Wait up to a few minutes if not immediately successful.
            # Note that "adb install" typically writes status messages
            # to stderr and the adb return code may not differentiate
            # successful installations from failures; instead we check
            # the command output.
            install_complete = False
            retries = 0
            while retries < 6:
                output = self.get_output_from_command([adb, "-s", device_id,
                                                       "install", '-r',
                                                       file_path],
                                                       ignore_errors=True)
                if output and output.lower().find("success") >= 0:
                    install_complete = True
                    break
                self.warning("Failed to install %s" % file_path)
                time.sleep(30)
                retries = retries + 1
            if not install_complete:
                self.fatal("Failed to install %s!" % file_path)

    def uninstall_app(self, package_name, package_root="/data/data",
                      error_level="error"):
        c = self.config
        device_id = self.query_device_id()
        self.info("Uninstalling %s..." % package_name)
        if self.query_device_file_exists('%s/%s' % (package_root, package_name)):
            adb = self.query_exe('adb')
            cmd = [adb, "-s", device_id, "uninstall"]
            if not c.get('enable_automation'):
                cmd.append("-k")
            cmd.append(package_name)
            status = self.run_command(cmd, error_list=ADBErrorList)
            # TODO is this the right error check?
            if status:
                self.log("Failed to uninstall %s!" % package_name,
                         level=error_level)

    # Device-type-specific. {{{2
    def remove_etc_hosts(self, hosts_file="/system/etc/hosts"):
        c = self.config
        if c['device_type'] not in ("tegra250",):
            self.debug("No need to remove /etc/hosts on a non-Tegra250.")
            return
        device_id = self.query_device_id()
        if self.query_device_file_exists(hosts_file):
            self.info("Removing %s file." % hosts_file)
            adb = self.query_exe('adb')
            self.run_command([adb, "-s", device_id, "shell",
                              "mount", "-o", "remount,rw", "-t", "yaffs2",
                              "/dev/block/mtdblock3", "/system"],
                             error_list=ADBErrorList)
            self.run_command([adb, "-s", device_id, "shell", "rm",
                              hosts_file])
            if self.query_device_file_exists(hosts_file):
                self.add_device_flag(DEVICE_CANT_REMOVE_ETC_HOSTS)
                self.fatal("Unable to remove %s!" % hosts_file)
        else:
            self.debug("%s file doesn't exist; skipping." % hosts_file)


# DeviceMixin {{{1
DEVICE_PROTOCOL_DICT = {
    'adb': ADBDeviceHandler,
}

device_config_options = [[
    ["--device-ip"],
    {"action": "store",
     "dest": "device_ip",
     "help": "Specify the IP address of the device."
     }
], [
    ["--device-port"],
    {"action": "store",
     "dest": "device_port",
     "help": "Specify the IP port of the device."
     }
], [
    ["--device-heartbeat-port"],
    {"action": "store",
     "dest": "device_heartbeat_port",
     "help": "Specify the heartbeat port of the device."
     }
], [
    ["--device-protocol"],
    {"action": "store",
     "dest": "device_protocol",
     "choices": DEVICE_PROTOCOL_DICT.keys(),
     "help": "Specify the device communication protocol."
     }
], [
    ["--device-type"],
    # A bit useless atm, but we can add new device types as we add support
    # for them.
    {"action": "store",
     "choices": ["non-tegra", "tegra250"],
     "default": "non-tegra",
     "dest": "device_type",
     "help": "Specify the device type."
     }
], [
    ["--devicemanager-path"],
    {"action": "store",
     "dest": "devicemanager_path",
     "help": "Specify the parent dir of devicemanager.py."
     }
]]


class DeviceMixin(object):
    '''BaseScript mixin, designed to interface with the device.

    '''
    device_handler = None
    device_root = None

    def query_device_handler(self):
        if self.device_handler:
            return self.device_handler
        c = self.config
        device_protocol = c.get('device_protocol')
        device_class = DEVICE_PROTOCOL_DICT.get(device_protocol)
        if not device_class:
            self.fatal("Unknown device_protocol %s; set via --device-protocol!" % str(device_protocol))
        self.device_handler = device_class(
            log_obj=self.log_obj,
            config=self.config,
            script_obj=self,
        )
        return self.device_handler

    def check_device(self):
        dh = self.query_device_handler()
        return dh.check_device()

    def cleanup_device(self, **kwargs):
        dh = self.query_device_handler()
        return dh.cleanup_device(**kwargs)

    def install_app(self):
        dh = self.query_device_handler()
        return dh.install_app(file_path=self.installer_path)

    def reboot_device(self):
        dh = self.query_device_handler()
        return dh.reboot_device()
