#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import datetime
import glob
import os
import subprocess
import tempfile
import time
from mozharness.mozilla.automation import TBPL_RETRY, EXIT_STATUS_DICT


class AndroidMixin:
    """
       Mixin class used by Android test scripts.
    """

    def init(self):
        self.logcat_proc = None
        self.logcat_file = None

        self.adb_path = self.query_exe('adb')
        self.sdk_level = None
        self.device_name = os.environ['DEVICE_NAME']
        self.device_serial = os.environ['DEVICE_SERIAL']
        self.device_ip = os.environ['DEVICE_IP']

    def _retry(self, max_attempts, interval, func, description, max_time=0):
        '''
        Execute func until it returns True, up to max_attempts times, waiting for
        interval seconds between each attempt. description is logged on each attempt.
        If max_time is specified, no further attempts will be made once max_time
        seconds have elapsed; this provides some protection for the case where
        the run-time for func is long or highly variable.
        '''
        status = False
        attempts = 0
        if max_time > 0:
            end_time = datetime.datetime.now() + datetime.timedelta(seconds=max_time)
        else:
            end_time = None
        while attempts < max_attempts and not status:
            if (end_time is not None) and (datetime.datetime.now() > end_time):
                self.info("Maximum retry run-time of %d seconds exceeded; "
                          "remaining attempts abandoned" % max_time)
                break
            if attempts != 0:
                self.info("Sleeping %d seconds" % interval)
                time.sleep(interval)
            attempts += 1
            self.info(">> %s: Attempt #%d of %d" % (description, attempts, max_attempts))
            status = func()
        return status

    def _run_proc(self, cmd, quiet=False):
        self.info('Running %s' % subprocess.list2cmdline(cmd))
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0)
        out, err = p.communicate()
        if out and not quiet:
            self.info('%s' % str(out.strip()))
        if err and not quiet:
            self.info('stderr: %s' % str(err.strip()))
        return out, err

    def _run_with_timeout(self, timeout, cmd, quiet=False):
        timeout_cmd = ['timeout', '%s' % timeout] + cmd
        return self._run_proc(timeout_cmd, quiet=quiet)

    def _run_adb_with_timeout(self, timeout, cmd, quiet=False):
        cmd = [self.adb_path, '-s', self.device_serial] + cmd
        return self._run_with_timeout(timeout, cmd, quiet)

    def _verify_adb(self):
        self.info('Verifying adb connectivity')
        self._run_with_timeout(180, [self.adb_path,
                                     '-s',
                                     self.device_serial,
                                     'wait-for-device'])
        return True

    def _verify_adb_device(self):
        out, _ = self._run_with_timeout(30, [self.adb_path, 'devices'])
        if (self.device_serial in out) and ("device" in out):
            return True
        return False

    def _is_boot_completed(self):
        boot_cmd = ['shell', 'getprop', 'sys.boot_completed']
        out, _ = self._run_adb_with_timeout(30, boot_cmd)
        if out.strip() == '1':
            return True
        return False

    def _install_apk(self):
        install_ok = False
        if int(self.sdk_level) >= 23:
            cmd = ['install', '-r', '-g', self.installer_path]
        else:
            cmd = ['install', '-r', self.installer_path]
            self.warning("Installing apk with default run-time permissions (sdk %s)" %
                         str(self.sdk_level))
        out, err = self._run_adb_with_timeout(300, cmd, True)
        if 'Success' in out or 'Success' in err:
            install_ok = True
        return install_ok

    def _get_repo_url(self, path):
        """
           Return a url for a file (typically a tooltool manifest) in this hg repo
           and using this revision (or mozilla-central/default if repo/rev cannot
           be determined).

           :param path specifies the directory path to the file of interest.
        """
        if 'GECKO_HEAD_REPOSITORY' in os.environ and 'GECKO_HEAD_REV' in os.environ:
            # probably taskcluster
            repo = os.environ['GECKO_HEAD_REPOSITORY']
            revision = os.environ['GECKO_HEAD_REV']
        else:
            # something unexpected!
            repo = 'https://hg.mozilla.org/mozilla-central'
            revision = 'default'
            self.warning('Unable to find repo/revision for manifest; '
                         'using mozilla-central/default')
        url = '%s/raw-file/%s/%s' % (
            repo,
            revision,
            path)
        return url

    def _tooltool_fetch(self, url, dir):
        c = self.config
        manifest_path = self.download_file(
            url,
            file_name='releng.manifest',
            parent_dir=dir
        )
        if not os.path.exists(manifest_path):
            self.fatal("Could not retrieve manifest needed to retrieve "
                       "artifacts from %s" % manifest_path)
        # from TooltoolMixin, included in TestingMixin
        self.tooltool_fetch(manifest_path,
                            output_dir=dir,
                            cache=c.get("tooltool_cache", None))

    def logcat_start(self):
        """
           Start recording logcat. Writes logcat to the upload directory.
        """
        self.mkdir_p(self.query_abs_dirs()['abs_blob_upload_dir'])
        # Start logcat for the device. The adb process runs until the
        # corresponding device is stopped. Output is written directly to
        # the blobber upload directory so that it is uploaded automatically
        # at the end of the job.
        logcat_filename = 'logcat-%s.log' % self.device_serial
        logcat_path = os.path.join(self.abs_dirs['abs_blob_upload_dir'],
                                   logcat_filename)
        self.logcat_file = open(logcat_path, 'w')
        logcat_cmd = [self.adb_path, '-s', self.device_serial, 'logcat', '-v',
                      'threadtime', 'Trace:S', 'StrictMode:S',
                      'ExchangeService:S']
        self.info(' '.join(logcat_cmd))
        self.logcat_proc = subprocess.Popen(logcat_cmd, stdout=self.logcat_file,
                                            stdin=subprocess.PIPE)

    def logcat_stop(self):
        """
           Stop logcat process started by logcat_start.
        """
        if self.logcat_proc:
            self.info("Killing logcat pid %d." % self.logcat_proc.pid)
            self.logcat_proc.kill()
            self.logcat_file.close()

    def install_apk(self, apk):
        """
           Install the specified apk.
        """
        self.init()
        cmd = [self.adb_path, '-s', self.device_serial, 'shell',
               'getprop', 'ro.build.version.sdk']
        self.sdk_level, _ = self._run_with_timeout(30, cmd)

        install_ok = self._retry(3, 30, self._install_apk, "Install app APK")
        if not install_ok:
            self.fatal('INFRA-ERROR: Failed to install %s on %s' %
                       (self.installer_path, self.device_name),
                       EXIT_STATUS_DICT[TBPL_RETRY])
        return install_ok

    def screenshot(self, prefix):
        """
           Save a screenshot of the entire screen to the blob upload directory.
        """
        dirs = self.query_abs_dirs()
        utility = os.path.join(self.xre_path, "screentopng")
        if not os.path.exists(utility):
            self.warning("Unable to take screenshot: %s does not exist" % utility)
            return
        try:
            tmpfd, filename = tempfile.mkstemp(prefix=prefix, suffix='.png',
                                               dir=dirs['abs_blob_upload_dir'])
            os.close(tmpfd)
            self.info("Taking screenshot with %s; saving to %s" % (utility, filename))
            subprocess.call([utility, filename], env=self.query_env())
        except OSError, err:
            self.warning("Failed to take screenshot: %s" % err.strerror)

    def download_hostutils(self, xre_dir):
        """
           Download and install hostutils from tooltool.
        """
        xre_path = None
        self.rmtree(xre_dir)
        self.mkdir_p(xre_dir)
        if self.config["hostutils_manifest_path"]:
            url = self._get_repo_url(self.config["hostutils_manifest_path"])
            self._tooltool_fetch(url, xre_dir)
            for p in glob.glob(os.path.join(xre_dir, 'host-utils-*')):
                if os.path.isdir(p) and os.path.isfile(os.path.join(p, 'xpcshell')):
                    xre_path = p
            if not xre_path:
                self.fatal("xre path not found in %s" % xre_dir)
        else:
            self.fatal("configure hostutils_manifest_path!")
        return xre_path
