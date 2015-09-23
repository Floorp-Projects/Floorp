# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys

from mozboot.base import BaseBootstrapper


MERCURIAL_INSTALL_PROMPT = '''
Mercurial releases a new version every 3 months and your distro's package
may become out of date. This may cause incompatibility with some
Mercurial extensions that rely on new Mercurial features. As a result,
you may not have an optimal version control experience.

To have the best Mercurial experience possible, we recommend installing
Mercurial via the "pip" Python packaging utility. This will likely result
in files being placed in /usr/local/bin and /usr/local/lib.

How would you like to continue?

1) Install a modern Mercurial via pip (recommended)
2) Install a legacy Mercurial via apt
3) Do not install Mercurial

Choice:
'''.strip()


class DebianBootstrapper(BaseBootstrapper):
    # These are common packages for all Debian-derived distros (such as
    # Ubuntu).
    COMMON_PACKAGES = [
        'autoconf2.13',
        'build-essential',
        'ccache',
        'python-dev',
        'python-pip',
        'python-setuptools',
        'unzip',
        'uuid',
        'zip',
    ]

    # Subclasses can add packages to this variable to have them installed.
    DISTRO_PACKAGES = []

    # These are common packages for building Firefox for Desktop
    # (browser) for all Debian-derived distros (such as Ubuntu).
    BROWSER_COMMON_PACKAGES = [
        'libasound2-dev',
        'libcurl4-openssl-dev',
        'libdbus-1-dev',
        'libdbus-glib-1-dev',
        'libgconf2-dev',
        'libgstreamer0.10-dev',
        'libgstreamer-plugins-base0.10-dev',
        'libgtk2.0-dev',
        'libgtk-3-dev',
        'libiw-dev',
        'libnotify-dev',
        'libpulse-dev',
        'libxt-dev',
        'mesa-common-dev',
        'python-dbus',
        'yasm',
        'xvfb',
    ]

    # Subclasses can add packages to this variable to have them installed.
    BROWSER_DISTRO_PACKAGES = []

    # These are common packages for building Firefox for Android
    # (mobile/android) for all Debian-derived distros (such as Ubuntu).
    MOBILE_ANDROID_COMMON_PACKAGES = [
        'zlib1g-dev',  # mobile/android requires system zlib.
        'openjdk-7-jdk',
        'ant',
        'wget',  # For downloading the Android SDK and NDK.
        'libncurses5:i386',  # See comments about i386 below.
        'libstdc++6:i386',
        'zlib1g:i386',
    ]

    # Subclasses can add packages to this variable to have them installed.
    MOBILE_ANDROID_DISTRO_PACKAGES = []

    def __init__(self, version, dist_id, **kwargs):
        BaseBootstrapper.__init__(self, **kwargs)

        self.version = version
        self.dist_id = dist_id

        self.packages = self.COMMON_PACKAGES + self.DISTRO_PACKAGES
        self.browser_packages = self.BROWSER_COMMON_PACKAGES + self.BROWSER_DISTRO_PACKAGES
        self.mobile_android_packages = self.MOBILE_ANDROID_COMMON_PACKAGES + self.MOBILE_ANDROID_DISTRO_PACKAGES


    def install_system_packages(self):
        self.apt_install(*self.packages)

    def install_browser_packages(self):
        self.apt_install(*self.browser_packages)

    def install_mobile_android_packages(self):
        import android

        # Multi-part process:
        # 1. System packages.
        # 2. Android SDK and NDK.
        # 3. Android packages.

        # 1. This is hard to believe, but the Android SDK binaries are 32-bit
        # and that conflicts with 64-bit Debian and Ubuntu installations out of
        # the box.  The solution is to add the i386 architecture.  See
        # "Troubleshooting Ubuntu" at
        # http://developer.android.com/sdk/installing/index.html?pkg=tools.
        self.run_as_root(['dpkg', '--add-architecture', 'i386'])
        # After adding a new arch, the list of packages has to be updated
        self.apt_update()
        self.apt_install(*self.mobile_android_packages)

        # 2. The user may have an external Android SDK (in which case we save
        # them a lengthy download), or they may have already completed the
        # download. We unpack to ~/.mozbuild/{android-sdk-linux, android-ndk-r10e}.
        mozbuild_path = os.environ.get('MOZBUILD_STATE_PATH', os.path.expanduser(os.path.join('~', '.mozbuild')))
        self.sdk_path = os.environ.get('ANDROID_SDK_HOME', os.path.join(mozbuild_path, 'android-sdk-linux'))
        self.ndk_path = os.environ.get('ANDROID_NDK_HOME', os.path.join(mozbuild_path, 'android-ndk-r10e'))
        self.sdk_url = 'https://dl.google.com/android/android-sdk_r24.0.1-linux.tgz'
        is_64bits = sys.maxsize > 2**32
        if is_64bits:
            self.ndk_url = 'https://dl.google.com/android/ndk/android-ndk-r10e-linux-x86_64.bin'
        else:
            self.ndk_url = 'https://dl.google.com/android/ndk/android-ndk-r10e-linux-x86.bin'
        android.ensure_android_sdk_and_ndk(path=mozbuild_path,
                                           sdk_path=self.sdk_path, sdk_url=self.sdk_url,
                                           ndk_path=self.ndk_path, ndk_url=self.ndk_url)

        # 3. We expect the |android| tool to at
        # ~/.mozbuild/android-sdk-linux/tools/android.
        android_tool = os.path.join(self.sdk_path, 'tools', 'android')
        android.ensure_android_packages(android_tool=android_tool)

    def suggest_mobile_android_mozconfig(self):
        import android
        android.suggest_mozconfig(sdk_path=self.sdk_path,
                                  ndk_path=self.ndk_path)

    def _update_package_manager(self):
        self.apt_update()

    def upgrade_mercurial(self, current):
        """Install Mercurial from pip because Debian packages typically lag."""
        if self.no_interactive:
            # Install via Apt in non-interactive mode because it is the more
            # conservative option and less likely to make people upset.
            self.apt_install('mercurial')
            return

        res = self.prompt_int(MERCURIAL_INSTALL_PROMPT, 1, 3)

        # Apt.
        if res == 2:
            self.apt_install('mercurial')
            return False

        # No Mercurial.
        if res == 3:
            print('Not installing Mercurial.')
            return False

        # pip.
        assert res == 1
        self.run_as_root(['pip', 'install', '--upgrade', 'Mercurial'])
