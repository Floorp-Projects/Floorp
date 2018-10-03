#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import glob
import os
import re
import subprocess
import tempfile
from mozharness.mozilla.automation import TBPL_RETRY, EXIT_STATUS_DICT


class AndroidMixin(object):
    """
       Mixin class used by Android test scripts.
    """

    def __init__(self, **kwargs):
        self._adb_path = None
        self._device = None
        self.device_name = os.environ.get('DEVICE_NAME', None)
        self.device_serial = os.environ.get('DEVICE_SERIAL', None)
        self.device_ip = os.environ.get('DEVICE_IP', None)
        self.logcat_proc = None
        self.logcat_file = None
        super(AndroidMixin, self).__init__(**kwargs)

    @property
    def adb_path(self):
        '''Get the path to the adb executable.

        Defer the use of query_exe() since it is defined by the
        BaseScript Mixin which hasn't finished initialing by the
        time the AndroidMixin is first initialized.
        '''
        if not self._adb_path:
            try:
                self._adb_path = self.query_exe('adb')
            except AttributeError:
                # Ignore attribute errors since BaseScript will
                # attempt to access properties before the other Mixins
                # have completed initialization. We recover afterwards
                # when additional attemps occur after initialization
                # is completed.
                pass
        return self._adb_path

    @property
    def device(self):
        if not self._device:
            try:
                import mozdevice
                self._device = mozdevice.ADBAndroid(adb=self.adb_path,
                                                    device=self.device_serial,
                                                    verbose=True)
                self.info("New mozdevice with adb=%s, device=%s" %
                          (self.adb_path, self.device_serial))
            except Exception:
                # As in adb_path, above.
                pass
        return self._device

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

    def dump_perf_info(self):
        '''
        Dump some host and android device performance-related information
        to an artifact file, to help understand task performance.
        '''
        dir = self.query_abs_dirs()['abs_blob_upload_dir']
        perf_path = os.path.join(dir, "android-performance.log")
        with open(perf_path, "w") as f:

            f.write('\n\nHost /proc/cpuinfo:\n')
            out = subprocess.check_output(['cat', '/proc/cpuinfo'])
            f.write(out)

            f.write('\n\nHost /proc/meminfo:\n')
            out = subprocess.check_output(['cat', '/proc/meminfo'])
            f.write(out)

            f.write('\n\nHost process list:\n')
            out = subprocess.check_output(['ps', '-ef'])
            f.write(out)

            f.write('\n\nDevice /proc/cpuinfo:\n')
            cmd = 'cat /proc/cpuinfo'
            out = self.shell_output(cmd)
            f.write(out)
            cpuinfo = out

            f.write('\n\nDevice /proc/meminfo:\n')
            cmd = 'cat /proc/meminfo'
            out = self.shell_output(cmd)
            f.write(out)

            f.write('\n\nDevice process list:\n')
            cmd = 'ps'
            out = self.shell_output(cmd)
            f.write(out)

        # Search android cpuinfo for "BogoMIPS"; if found and < (minimum), retry
        # this task, in hopes of getting a higher-powered environment.
        # (Carry on silently if BogoMIPS is not found -- this may vary by
        # Android implementation -- no big deal.)
        # See bug 1321605: Sometimes the emulator is really slow, and
        # low bogomips can be a good predictor of that condition.
        bogomips_minimum = int(self.config.get('bogomips_minimum') or 0)
        for line in cpuinfo.split('\n'):
            m = re.match("BogoMIPS.*: (\d*)", line)
            if m:
                bogomips = int(m.group(1))
                if bogomips_minimum > 0 and bogomips < bogomips_minimum:
                    self.fatal('INFRA-ERROR: insufficient Android bogomips (%d < %d)' %
                               (bogomips, bogomips_minimum),
                               EXIT_STATUS_DICT[TBPL_RETRY])
                self.info("Found Android bogomips: %d" % bogomips)
                break

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
        import mozdevice
        try:
            self.device.install_app(apk)
        except mozdevice.ADBError:
            self.fatal('INFRA-ERROR: Failed to install %s on %s' %
                       (self.installer_path, self.device_name),
                       EXIT_STATUS_DICT[TBPL_RETRY])

    def is_boot_completed(self):
        out = self.device.get_prop('sys.boot_completed', timeout=30)
        if out.strip() == '1':
            return True
        return False

    def shell_output(self, cmd):
        return self.device.shell_output(cmd, timeout=30)

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

    def query_package_name(self):
        if self.app_name is None:
            # For convenience, assume geckoview.test/geckoview_example when install
            # target looks like geckoview.
            if 'androidTest' in self.installer_path:
                self.app_name = 'org.mozilla.geckoview.test'
            elif 'geckoview' in self.installer_path:
                self.app_name = 'org.mozilla.geckoview_example'
        if self.app_name is None:
            # Find appname from package-name.txt - assumes download-and-extract
            # has completed successfully.
            # The app/package name will typically be org.mozilla.fennec,
            # but org.mozilla.firefox for release builds, and there may be
            # other variations. 'aapt dump badging <apk>' could be used as an
            # alternative to package-name.txt, but introduces a dependency
            # on aapt, found currently in the Android SDK build-tools component.
            apk_dir = self.abs_dirs['abs_work_dir']
            self.apk_path = os.path.join(apk_dir, self.installer_path)
            unzip = self.query_exe("unzip")
            package_path = os.path.join(apk_dir, 'package-name.txt')
            unzip_cmd = [unzip, '-q', '-o', self.apk_path]
            self.run_command(unzip_cmd, cwd=apk_dir, halt_on_failure=True)
            self.app_name = str(self.read_from_file(package_path, verbose=True)).rstrip()
        return self.app_name
