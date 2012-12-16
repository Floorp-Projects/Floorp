# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import os
import re
import subprocess
import sys
import tempfile
import urllib2

from distutils.version import StrictVersion

from mozboot.base import BaseBootstrapper

HOMEBREW_BOOTSTRAP = 'http://raw.github.com/mxcl/homebrew/go'
XCODE_APP_STORE = 'macappstore://itunes.apple.com/app/id497799835?mt=12'
XCODE_LEGACY = 'https://developer.apple.com/downloads/download.action?path=Developer_Tools/xcode_3.2.6_and_ios_sdk_4.3__final/xcode_3.2.6_and_ios_sdk_4.3.dmg'
HOMEBREW_AUTOCONF213 = 'https://raw.github.com/Homebrew/homebrew-versions/master/autoconf213.rb'

RE_CLANG_VERSION = re.compile('Apple clang version (\d+\.\d+)')

APPLE_CLANG_MINIMUM_VERSION = StrictVersion('4.0')

XCODE_REQUIRED = '''
Xcode is required to build Firefox. Please complete the install of Xcode
through the App Store.
'''

XCODE_REQUIRED_LEGACY = '''
You will need to download and install Xcode to build Firefox.

Please complete the Xcode download and then relaunch this script.
'''

XCODE_COMMAND_LINE_TOOLS_MISSING = '''
The Xcode command line tools are required to build Firefox.
'''

INSTALL_XCODE_COMMAND_LINE_TOOLS_STEPS = '''
Perform the following steps to install the Xcode command line tools:

    1) Open Xcode.app
    2) Click through any first-run prompts
    3) From the main Xcode menu, select Preferences (Command ,)
    4) Go to the Download tab (near the right)
    5) Install the "Command Line Tools"

When that has finished installing, please relaunch this script.
'''

UPGRADE_XCODE_COMMAND_LINE_TOOLS = '''
An old version of the Xcode command line tools is installed. You will need to
install a newer version in order to compile Firefox.
'''

HOMEBREW_INSTALL = '''
We will install the Homebrew package manager to install required packages.

You will be prompted to install Homebrew with its default settings. If you
would prefer to do this manually, hit CTRL+c, install Homebrew yourself, ensure
"brew" is in your $PATH, and relaunch bootstrap.
'''

HOMEBREW_PACKAGES = '''
We are now installing all required packages via Homebrew. You will see a lot of
output as packages are built.
'''

HOMEBREW_OLD_CLANG = '''
We require a newer compiler than what is provided by your version of Xcode.

We will install a modern version of Clang through Homebrew.
'''


class OSXBootstrapper(BaseBootstrapper):
    def __init__(self, version):
        BaseBootstrapper.__init__(self)

        self.os_version = StrictVersion(version)

        if self.os_version < StrictVersion('10.6'):
            raise Exception('OS X 10.6 or above is required.')

    def install_system_packages(self):
        self.ensure_xcode()
        self.ensure_homebrew()
        self.ensure_homebrew_packages()

    def ensure_xcode(self):
        if self.os_version < StrictVersion('10.7'):
            if not os.path.exists('/Developer/Applications/Xcode.app'):
                print(XCODE_REQUIRED_LEGACY)

                subprocess.check_call(['open', XCODE_LEGACY])
                sys.exit(1)

        elif self.os_version >= StrictVersion('10.7'):
            if not os.path.exists('/Applications/Xcode.app'):
                print(XCODE_REQUIRED)

                subprocess.check_call(['open', XCODE_APP_STORE])

                print('Once the install has finished, please relaunch this script.')
                sys.exit(1)

        # Once Xcode is installed, you need to agree to the license before you can
        # use it.
        try:
            output = self.check_output(['/usr/bin/xcrun', 'clang'],
                stderr=subprocess.STDOUT)
        except subprocess.CalledProcessError as e:
            if 'license' in e.output:
                xcodebuild = self.which('xcodebuild')
                subprocess.check_call([xcodebuild, '-license'])

        # Even then we're not done! We need to install the Xcode command line tools.
        # As of Mountain Lion, apparently the only way to do this is to go through a
        # menu dialog inside Xcode itself. We're not making this up.
        if self.os_version >= StrictVersion('10.7'):
            if not os.path.exists('/usr/bin/clang'):
                print(XCODE_COMMAND_LINE_TOOLS_MISSING)
                print(INSTALL_XCODE_COMMAND_LINE_TOOLS_STEPS)
                sys.exit(1)

            output = self.check_output(['/usr/bin/clang', '--version'])
            match = RE_CLANG_VERSION.search(output)
            if match is None:
                raise Exception('Could not determine Clang version.')

            version = StrictVersion(match.group(1))

            if version < APPLE_CLANG_MINIMUM_VERSION:
                print(UPGRADE_XCODE_COMMAND_LINE_TOOLS)
                print(INSTALL_XCODE_COMMAND_LINE_TOOLS_STEPS)
                sys.exit(1)

    def ensure_homebrew(self):
        if self.which('brew') is not None:
            return

        print(HOMEBREW_INSTALL)
        bootstrap = urllib2.urlopen(url=HOMEBREW_BOOTSTRAP, timeout=20).read()
        with tempfile.NamedTemporaryFile() as tf:
            tf.write(bootstrap)
            tf.flush()

            subprocess.check_call(['ruby', tf.name])

    def ensure_homebrew_packages(self):
        brew = self.which('brew')
        assert brew is not None

        installed = self.check_output([brew, 'list']).split()

        packages = [
            # We need to install Python because Mercurial requires the Python
            # development headers which are missing from OS X (at least on
            # 10.8).
            ('python', 'python'),
            ('mercurial', 'mercurial'),
            ('git', 'git'),
            ('yasm', 'yasm'),
            ('autoconf213', HOMEBREW_AUTOCONF213),
        ]

        printed = False

        for name, package in packages:
            if name in installed:
                continue

            if not printed:
                print(HOMEBREW_PACKAGES)
                printed = True

            subprocess.check_call([brew, '-v', 'install', package])

        if self.os_version < StrictVersion('10.7') and 'llvm' not in installed:
            print(HOMEBREW_OLD_CLANG)

            subprocess.check_call([brew, '-v', 'install', 'llvm',
                '--with-clang', '--all-targets'])
