# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from mozboot.base import BaseBootstrapper
from mozboot.linux_common import (
    ClangStaticAnalysisInstall,
    NasmInstall,
    NodeInstall,
    SccacheInstall,
    StyloInstall,
)

try:
    from urllib2 import urlopen
except ImportError:
    from urllib.request import urlopen

import re
import subprocess


class GentooBootstrapper(NasmInstall, NodeInstall, StyloInstall, ClangStaticAnalysisInstall,
                         SccacheInstall, BaseBootstrapper):

    def __init__(self, version, dist_id, **kwargs):
        BaseBootstrapper.__init__(self, **kwargs)

        self.version = version
        self.dist_id = dist_id

    def install_system_packages(self):
        self.run_as_root(['emerge', '--noreplace', '--quiet', 'nodejs'])

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

    @staticmethod
    def _get_distdir():
        # Obtain the path held in the DISTDIR portage variable
        output = subprocess.check_output(
            ['emerge', '--info'], universal_newlines=True)
        match = re.search('^DISTDIR="(?P<distdir>.*)"$', output, re.MULTILINE)
        return match.group('distdir')

    @staticmethod
    def _get_jdk_filename(emerge_output):
        match = re.search(r'^ \* *(?P<tarball>jdk-.*-linux-.*.tar.gz)$',
                          emerge_output, re.MULTILINE)

        return match.group('tarball')

    @staticmethod
    def _get_jdk_page_urls(emerge_output):
        urls = re.findall(r'^ \* *(https?://.*\.html)$', emerge_output,
                          re.MULTILINE)
        return [re.sub('^http://', 'https://', url) for url in urls]

    @staticmethod
    def _get_jdk_url(filename, urls):
        for url in urls:
            contents = urlopen(url).read()
            match = re.search('.*(?P<url>https?://.*' + filename + ')',
                              contents, re.MULTILINE)
            if match:
                url = match.group('url')
                return re.sub('^http://', 'https://', url)

        raise Exception("Could not find the JDK tarball URL")

    def _fetch_jdk_tarball(self, filename, url):
        distdir = self._get_distdir()
        cookie = 'Cookie: oraclelicense=accept-securebackup-cookie'
        self.run_as_root(['wget', '--no-verbose', '--show-progress', '-c', '-O',
                          distdir + '/' + filename, '--header', cookie, url])

    def ensure_mobile_android_packages(self, artifact_mode=False):
        # For downloading the Oracle JDK, Android SDK and NDK.
        self.run_as_root(['emerge', '--noreplace', '--quiet', 'wget'])

        # Find the JDK file name and URL(s)
        try:
            output = self.check_output(['emerge', '--pretend', '--fetchonly',
                                        'oracle-jdk-bin'],
                                       env=None,
                                       stderr=subprocess.STDOUT,
                                       universal_newlines=True)
        except subprocess.CalledProcessError as e:
            output = e.output

        jdk_filename = self._get_jdk_filename(output)
        jdk_page_urls = self._get_jdk_page_urls(output)
        jdk_url = self._get_jdk_url(jdk_filename, jdk_page_urls)

        # Fetch the Oracle JDK since portage can't fetch it on its own
        self._fetch_jdk_tarball(jdk_filename, jdk_url)

        # Install the Oracle JDK. We explicitly prompt the user to accept the
        # changes because this command might need to modify the portage
        # configuration files and doing so without user supervision is dangerous
        self.run_as_root(['emerge', '--noreplace', '--quiet',
                          '--autounmask-continue', '--ask',
                          'dev-java/oracle-jdk-bin'])

        self.ensure_java()
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
