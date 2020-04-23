# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from mozboot.base import BaseBootstrapper
from mozboot.linux_common import (
    ClangStaticAnalysisInstall,
    FixStacksInstall,
    LucetcInstall,
    NasmInstall,
    NodeInstall,
    SccacheInstall,
    StyloInstall,
    WasiSysrootInstall,
)


class GentooBootstrapper(
        ClangStaticAnalysisInstall,
        FixStacksInstall,
        LucetcInstall,
        NasmInstall,
        NodeInstall,
        SccacheInstall,
        StyloInstall,
        WasiSysrootInstall,
        BaseBootstrapper):

    def __init__(self, version, dist_id, **kwargs):
        BaseBootstrapper.__init__(self, **kwargs)

        self.version = version
        self.dist_id = dist_id

    def install_system_packages(self):
        self.ensure_system_packages()

    def install_browser_packages(self):
        self.ensure_browser_packages()

    def install_browser_artifact_mode_packages(self):
        self.ensure_browser_packages(artifact_mode=True)

    def install_mobile_android_packages(self):
        self.ensure_mobile_android_packages(artifact_mode=False)

    def install_mobile_android_artifact_mode_packages(self):
        self.ensure_mobile_android_packages(artifact_mode=True)

    def ensure_system_packages(self):
        self.run_as_root(['emerge', '--noreplace', '--quiet',
                          'app-arch/zip',
                          'sys-devel/autoconf:2.1'
                          ])

    def ensure_browser_packages(self, artifact_mode=False):
        # TODO: Figure out what not to install for artifact mode
        self.run_as_root(['emerge',
                          '--oneshot', '--noreplace', '--quiet', '--newuse',
                          'dev-lang/yasm',
                          'dev-libs/dbus-glib',
                          'media-sound/pulseaudio',
                          'x11-libs/gtk+:2',
                          'x11-libs/gtk+:3',
                          'x11-libs/libXt'
                          ])

    def ensure_mobile_android_packages(self, artifact_mode=False):
        self.run_as_root(['emerge', '--noreplace', '--quiet',
                          'dev-java/openjdk-bin'])

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
