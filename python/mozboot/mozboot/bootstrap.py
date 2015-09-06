# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# If we add unicode_literals, Python 2.6.1 (required for OS X 10.6) breaks.
from __future__ import print_function

import platform
import sys
import os.path

# Don't forgot to add new mozboot modules to the bootstrap download
# list in bin/bootstrap.py!
from mozboot.centos import CentOSBootstrapper
from mozboot.debian import DebianBootstrapper
from mozboot.fedora import FedoraBootstrapper
from mozboot.freebsd import FreeBSDBootstrapper
from mozboot.gentoo import GentooBootstrapper
from mozboot.osx import OSXBootstrapper
from mozboot.openbsd import OpenBSDBootstrapper
from mozboot.archlinux import ArchlinuxBootstrapper

APPLICATION_CHOICE = '''
Please choose the version of Firefox you want to build:
%s
Your choice:
'''

APPLICATIONS_LIST=[
    ('Firefox for Desktop', 'browser'),
    ('Firefox for Android', 'mobile_android')
]

# This is a workaround for the fact that we must support python2.6 (which has
# no OrderedDict)
APPLICATIONS = dict(
    desktop=APPLICATIONS_LIST[0],
    android=APPLICATIONS_LIST[1],
)

FINISHED = '''
Your system should be ready to build %s! If you have not already,
obtain a copy of the source code by running:

    hg clone https://hg.mozilla.org/mozilla-central

Or, if you prefer Git:

    git clone https://git.mozilla.org/integration/gecko-dev.git
'''

DEBIAN_DISTROS = (
    'Debian',
    'debian',
    'Ubuntu',
    # Most Linux Mint editions are based on Ubuntu. One is based on Debian.
    # The difference is reported in dist_id from platform.linux_distribution.
    # But it doesn't matter since we share a bootstrapper between Debian and
    # Ubuntu.
    'Mint',
    'LinuxMint',
    'Elementary OS',
    'Elementary',
    '"elementary OS"',
)


class Bootstrapper(object):
    """Main class that performs system bootstrap."""

    def __init__(self, finished=FINISHED, choice=None, no_interactive=False):
        self.instance = None
        self.finished = finished
        self.choice = choice
        cls = None
        args = {'no_interactive': no_interactive}

        if sys.platform.startswith('linux'):
            distro, version, dist_id = platform.linux_distribution()

            if distro in ('CentOS', 'CentOS Linux'):
                cls = CentOSBootstrapper
            elif distro in DEBIAN_DISTROS:
                cls = DebianBootstrapper
            elif distro == 'Fedora':
                cls = FedoraBootstrapper
            elif distro == 'Gentoo Base System':
                cls = GentooBootstrapper
            elif os.path.exists('/etc/arch-release'):
                # Even on archlinux, platform.linux_distribution() returns ['','','']
                cls = ArchlinuxBootstrapper
            else:
                raise NotImplementedError('Bootstrap support for this Linux '
                                          'distro not yet available.')

            args['version'] = version
            args['dist_id'] = dist_id

        elif sys.platform.startswith('darwin'):
            # TODO Support Darwin platforms that aren't OS X.
            osx_version = platform.mac_ver()[0]

            cls = OSXBootstrapper
            args['version'] = osx_version

        elif sys.platform.startswith('openbsd'):
            cls = OpenBSDBootstrapper
            args['version'] = platform.uname()[2]

        elif sys.platform.startswith('dragonfly') or \
                sys.platform.startswith('freebsd'):
            cls = FreeBSDBootstrapper
            args['version'] = platform.release()
            args['flavor'] = platform.system()

        if cls is None:
            raise NotImplementedError('Bootstrap support is not yet available '
                                      'for your OS.')

        self.instance = cls(**args)

    def bootstrap(self):
        if self.choice is None:
            # Like ['1. Firefox for Desktop', '2. Firefox for Android'].
            labels = ['%s. %s' % (i + 1, name) for (i, (name, _)) in enumerate(APPLICATIONS_LIST)]
            prompt = APPLICATION_CHOICE % '\n'.join(labels)
            prompt_choice = self.instance.prompt_int(prompt=prompt, low=1, high=len(APPLICATIONS))
            name, application = APPLICATIONS_LIST[prompt_choice-1]
        elif self.choice not in APPLICATIONS.keys():
            raise Exception('Please pick a valid application choice: (%s)' % '/'.join(APPLICATIONS.keys()))
        else:
            name, application = APPLICATIONS[self.choice]

        self.instance.install_system_packages()

        # Like 'install_browser_packages' or 'install_mobile_android_packages'.
        getattr(self.instance, 'install_%s_packages' % application)()

        self.instance.ensure_mercurial_modern()
        self.instance.ensure_python_modern()

        print(self.finished % name)

        # Like 'suggest_browser_mozconfig' or 'suggest_mobile_android_mozconfig'.
        getattr(self.instance, 'suggest_%s_mozconfig' % application)()
