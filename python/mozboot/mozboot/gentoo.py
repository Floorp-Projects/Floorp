# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from mozboot.base import BaseBootstrapper
from mozboot.linux_common import StyloInstall


class GentooBootstrapper(StyloInstall, BaseBootstrapper):
    def __init__(self, version, dist_id, **kwargs):
        BaseBootstrapper.__init__(self, **kwargs)

        self.version = version
        self.dist_id = dist_id

    def install_system_packages(self):
        self.run_as_root(['emerge', '--noreplace', '--quiet', 'dev-vcs/git',
                          'mercurial', 'nodejs'])

    def install_browser_packages(self):
        self.ensure_browser_packages()

    def install_browser_artifact_mode_packages(self):
        self.ensure_browser_packages(artifact_mode=True)

    def install_mobile_android_packages(self):
        self.ensure_mobile_android_packages(artifact_mode=False)

    def install_mobile_android_artifact_mode_packages(self):
        self.ensure_mobile_android_packages(artifact_mode=True)

    def ensure_browser_packages(self, artifact_mode=False):
        # TODO: Figure out what not to install for artifact mode
        self.run_as_root(['emerge', '--onlydeps', '--quiet', 'firefox'])
        self.run_as_root(['emerge', '--noreplace', '--quiet', 'gtk+'])

    def ensure_mobile_android_packages(self, artifact_mode=False):
        import re
        import subprocess

        # For downloading the Oracle JDK, Android SDK and NDK.
        self.run_as_root(['emerge', '--noreplace', '--quiet', 'wget'])

        # Obtain the path held in the DISTDIR portage variable
        emerge_info = subprocess.check_output(['emerge', '--info'])
        distdir_re = re.compile('^DISTDIR="(.*)"$', re.MULTILINE)
        distdir = distdir_re.search(emerge_info).group(1)

        # Fetch the Oracle JDK since portage can't fetch it on its own
        base_url = 'http://download.oracle.com/otn-pub/java/jdk'
        jdk_dir = '8u152-b16/aa0333dd3019491ca4f6ddbe78cdb6d0'
        jdk_file = 'jdk-8u152-linux-x64.tar.gz'
        cookie = 'Cookie: oraclelicense=accept-securebackup-cookie'
        self.run_as_root(['wget', '-c', '-O', distdir + '/' + jdk_file,
                          '--header', cookie,
                          base_url + '/' + jdk_dir + '/' + jdk_file])

        # Install the Oracle JDK. We explicitly prompt the user to accept the
        # changes because this command might need to modify the portage
        # configuration files and doing so without user supervision is dangerous
        self.run_as_root(['emerge', '--noreplace', '--quiet',
                          '--autounmask-continue', '--ask',
                          '=dev-java/oracle-jdk-bin-1.8.0.152-r1'])

        from mozboot import android
        android.ensure_android('linux', artifact_mode=artifact_mode,
                               no_interactive=self.no_interactive)

    def suggest_mobile_android_mozconfig(self, artifact_mode=False):
        from mozboot import android
        android.suggest_mozconfig('linux', artifact_mode=artifact_mode)

    def suggest_mobile_android_artifact_mode_mozconfig(self):
        self.suggest_mobile_android_mozconfig(artifact_mode=True)

    def _update_package_manager(self):
        self.run_as_root(['emerge', '--sync'])

    def upgrade_mercurial(self, current):
        self.run_as_root(['emerge', '--update', 'mercurial'])
