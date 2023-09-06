# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import datetime
import os
import posixpath
import shutil
import tempfile
import time

from mozdevice import ADBError, ADBHost
from six.moves.configparser import ConfigParser, RawConfigParser


class Device(object):
    connected = False

    def __init__(self, app_ctx, logdir=None, serial=None, restore=True):
        self.app_ctx = app_ctx
        self.device = self.app_ctx.device
        self.restore = restore
        self.serial = serial
        self.logdir = os.path.abspath(os.path.expanduser(logdir))
        self.added_files = set()
        self.backup_files = set()

    @property
    def remote_profiles(self):
        """
        A list of remote profiles on the device.
        """
        remote_ini = self.app_ctx.remote_profiles_ini
        if not self.device.is_file(remote_ini):
            raise IOError("Remote file '%s' not found" % remote_ini)

        local_ini = tempfile.NamedTemporaryFile()
        self.device.pull(remote_ini, local_ini.name)
        cfg = ConfigParser()
        cfg.read(local_ini.name)

        profiles = []
        for section in cfg.sections():
            if cfg.has_option(section, "Path"):
                if cfg.has_option(section, "IsRelative") and cfg.getint(
                    section, "IsRelative"
                ):
                    profiles.append(
                        posixpath.join(
                            posixpath.dirname(remote_ini), cfg.get(section, "Path")
                        )
                    )
                else:
                    profiles.append(cfg.get(section, "Path"))
        return profiles

    def pull_minidumps(self):
        """
        Saves any minidumps found in the remote profile on the local filesystem.

        :returns: Path to directory containing the dumps.
        """
        remote_dump_dir = posixpath.join(self.app_ctx.remote_profile, "minidumps")
        local_dump_dir = tempfile.mkdtemp()
        try:
            self.device.pull(remote_dump_dir, local_dump_dir)
        except ADBError as e:
            # OK if directory not present -- sometimes called before browser start
            if "does not exist" not in str(e):
                try:
                    shutil.rmtree(local_dump_dir)
                except Exception:
                    pass
                finally:
                    raise e
            else:
                print("WARNING: {}".format(e))
        if os.listdir(local_dump_dir):
            self.device.rm(remote_dump_dir, recursive=True)
            self.device.mkdir(remote_dump_dir, parents=True)
        return local_dump_dir

    def setup_profile(self, profile):
        """
        Copy profile to the device and update the remote profiles.ini
        to point to the new profile.

        :param profile: mozprofile object to copy over.
        """
        if self.device.is_dir(self.app_ctx.remote_profile):
            self.device.rm(self.app_ctx.remote_profile, recursive=True)

        self.device.push(profile.profile, self.app_ctx.remote_profile)

        timeout = 5  # seconds
        starttime = datetime.datetime.now()
        while datetime.datetime.now() - starttime < datetime.timedelta(seconds=timeout):
            if self.device.is_file(self.app_ctx.remote_profiles_ini):
                break
            time.sleep(1)
        local_profiles_ini = tempfile.NamedTemporaryFile()
        if not self.device.is_file(self.app_ctx.remote_profiles_ini):
            # Unless fennec is already running, and/or remote_profiles_ini is
            # not inside the remote_profile (deleted above), this is entirely
            # normal.
            print("timed out waiting for profiles.ini")
        else:
            self.device.pull(self.app_ctx.remote_profiles_ini, local_profiles_ini.name)

        config = ProfileConfigParser()
        config.read(local_profiles_ini.name)
        for section in config.sections():
            if "Profile" in section:
                config.set(section, "IsRelative", 0)
                config.set(section, "Path", self.app_ctx.remote_profile)

        new_profiles_ini = tempfile.NamedTemporaryFile()
        config.write(open(new_profiles_ini.name, "w"))

        self.backup_file(self.app_ctx.remote_profiles_ini)
        self.device.push(new_profiles_ini.name, self.app_ctx.remote_profiles_ini)

        # Ideally all applications would read the profile the same way, but in practice
        # this isn't true. Perform application specific profile-related setup if necessary.
        if hasattr(self.app_ctx, "setup_profile"):
            for remote_path in self.app_ctx.remote_backup_files:
                self.backup_file(remote_path)
            self.app_ctx.setup_profile(profile)

    def _get_online_devices(self):
        adbhost = ADBHost(adb=self.app_ctx.adb)
        devices = adbhost.devices()
        return [
            d["device_serial"]
            for d in devices
            if d["state"] != "offline"
            if not d["device_serial"].startswith("emulator")
        ]

    def connect(self):
        """
        Connects to a running device. If no serial was specified in the
        constructor, defaults to the first entry in `adb devices`.
        """
        if self.connected:
            return

        online_devices = self._get_online_devices()
        if not online_devices:
            raise IOError(
                "No devices connected. Ensure the device is on and "
                "remote debugging via adb is enabled in the settings."
            )
        self.serial = online_devices[0]

        self.connected = True

    def reboot(self):
        """
        Reboots the device via adb.
        """
        self.device.reboot()

    def wait_for_net(self):
        active = False
        time_out = 0
        while not active and time_out < 40:
            if self.device.get_ip_address() is not None:
                active = True
            time_out += 1
            time.sleep(1)
        return active

    def backup_file(self, remote_path):
        if not self.restore:
            return

        if self.device.exists(remote_path):
            self.device.cp(remote_path, "%s.orig" % remote_path, recursive=True)
            self.backup_files.add(remote_path)
        else:
            self.added_files.add(remote_path)

    def cleanup(self):
        """
        Cleanup the device.
        """
        if not self.restore:
            return

        try:
            self.device.remount()
            # Restore the original profile
            for added_file in self.added_files:
                self.device.rm(added_file)

            for backup_file in self.backup_files:
                if self.device.exists("%s.orig" % backup_file):
                    self.device.mv("%s.orig" % backup_file, backup_file)

            # Perform application specific profile cleanup if necessary
            if hasattr(self.app_ctx, "cleanup_profile"):
                self.app_ctx.cleanup_profile()

            # Remove the test profile
            self.device.rm(self.app_ctx.remote_profile, force=True, recursive=True)
        except Exception as e:
            print("cleanup aborted: %s" % str(e))

    def _rotate_log(self, srclog, index=1):
        """
        Rotate a logfile, by recursively rotating logs further in the sequence,
        deleting the last file if necessary.
        """
        basename = os.path.basename(srclog)
        basename = basename[: -len(".log")]
        if index > 1:
            basename = basename[: -len(".1")]
        basename = "%s.%d.log" % (basename, index)

        destlog = os.path.join(self.logdir, basename)
        if os.path.isfile(destlog):
            if index == 3:
                os.remove(destlog)
            else:
                self._rotate_log(destlog, index + 1)
        shutil.move(srclog, destlog)


class ProfileConfigParser(RawConfigParser):
    """
    Class to create profiles.ini config files

    Subclass of RawConfigParser that outputs .ini files in the exact
    format expected for profiles.ini, which is slightly different
    than the default format.
    """

    def optionxform(self, optionstr):
        return optionstr

    def write(self, fp):
        if self._defaults:
            fp.write("[%s]\n" % ConfigParser.DEFAULTSECT)
            for key, value in self._defaults.items():
                fp.write("%s=%s\n" % (key, str(value).replace("\n", "\n\t")))
            fp.write("\n")
        for section in self._sections:
            fp.write("[%s]\n" % section)
            for key, value in self._sections[section].items():
                if key == "__name__":
                    continue
                if (value is not None) or (self._optcre == self.OPTCRE):
                    key = "=".join((key, str(value).replace("\n", "\n\t")))
                fp.write("%s\n" % (key))
            fp.write("\n")
