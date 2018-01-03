# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# If we add unicode_literals, Python 2.6.1 (required for OS X 10.6) breaks.
from __future__ import absolute_import, print_function

import platform
import sys
import os
import subprocess

# Don't forgot to add new mozboot modules to the bootstrap download
# list in bin/bootstrap.py!
from mozboot.base import MODERN_RUST_VERSION
from mozboot.centosfedora import CentOSFedoraBootstrapper
from mozboot.debian import DebianBootstrapper
from mozboot.freebsd import FreeBSDBootstrapper
from mozboot.gentoo import GentooBootstrapper
from mozboot.osx import OSXBootstrapper
from mozboot.openbsd import OpenBSDBootstrapper
from mozboot.archlinux import ArchlinuxBootstrapper
from mozboot.windows import WindowsBootstrapper
from mozboot.mozillabuild import MozillaBuildBootstrapper
from mozboot.util import (
    get_state_dir,
)

APPLICATION_CHOICE = '''
Note on Artifact Mode:

Firefox for Desktop and Android supports a fast build mode called
artifact mode. Artifact mode downloads pre-built C++ components rather
than building them locally, trading bandwidth for time.

Artifact builds will be useful to many developers who are not working
with compiled code. If you want to work on look-and-feel of Firefox,
you want "Firefox for Desktop Artifact Mode".

Similarly, if you want to work on the look-and-feel of Firefox for Android,
you want "Firefox for Android Artifact Mode".

To work on the Gecko technology platform, you would need to opt to full,
non-artifact mode. Gecko is Mozilla's web rendering engine, similar to Edge,
Blink, and WebKit. Gecko is implemented in C++ and JavaScript. If you
want to work on web rendering, you want "Firefox for Desktop", or
"Firefox for Android".

If you don't know what you want, start with just Artifact Mode of the desired
platform. Your builds will be much shorter than if you build Gecko as well.
But don't worry! You can always switch configurations later.

You can learn more about Artifact mode builds at
https://developer.mozilla.org/en-US/docs/Artifact_builds.

Please choose the version of Firefox you want to build:
%s
Your choice: '''

APPLICATIONS_LIST = [
    ('Firefox for Desktop Artifact Mode', 'browser_artifact_mode'),
    ('Firefox for Desktop', 'browser'),
    ('Firefox for Android Artifact Mode', 'mobile_android_artifact_mode'),
    ('Firefox for Android', 'mobile_android'),
]

# This is a workaround for the fact that we must support python2.6 (which has
# no OrderedDict)
APPLICATIONS = dict(
    browser_artifact_mode=APPLICATIONS_LIST[0],
    browser=APPLICATIONS_LIST[1],
    mobile_android_artifact_mode=APPLICATIONS_LIST[2],
    mobile_android=APPLICATIONS_LIST[3],
)

STATE_DIR_INFO = '''
The Firefox build system and related tools store shared, persistent state
in a common directory on the filesystem. On this machine, that directory
is:

  {statedir}

If you would like to use a different directory, hit CTRL+c and set the
MOZBUILD_STATE_PATH environment variable to the directory you'd like to
use and re-run the bootstrapper.

Would you like to create this directory?

  1. Yes
  2. No

Your choice: '''

STYLO_DIRECTORY_MESSAGE = '''
Stylo packages require a directory to store shared, persistent state.
On this machine, that directory is:

  {statedir}

Please restart bootstrap and create that directory when prompted.
'''

STYLO_REQUIRES_CLONE = '''
Installing Stylo packages requires a checkout of mozilla-central. Once you
have such a checkout, please re-run `./mach bootstrap` from the checkout
directory.
'''

FINISHED = '''
Your system should be ready to build %s!
'''

SOURCE_ADVERTISE = '''
Source code can be obtained by running

    hg clone https://hg.mozilla.org/mozilla-unified

Or, if you prefer Git, you should install git-cinnabar, and follow the
instruction here to clone from the Mercurial repository:

    https://github.com/glandium/git-cinnabar/wiki/Mozilla:-A-git-workflow-for-Gecko-development

Or, if you really prefer vanilla flavor Git:

    git clone https://github.com/mozilla/gecko-dev.git
'''

CONFIGURE_MERCURIAL = '''
Mozilla recommends a number of changes to Mercurial to enhance your
experience with it.

Would you like to run a configuration wizard to ensure Mercurial is
optimally configured?

  1. Yes
  2. No

Please enter your reply: '''

CLONE_MERCURIAL = '''
If you would like to clone the mozilla-unified Mercurial repository, please
enter the destination path below.

(If you prefer to use Git, leave this blank.)
'''

CLONE_MERCURIAL_PROMPT = '''
Destination directory for Mercurial clone (leave empty to not clone): '''.lstrip()

CLONE_MERCURIAL_NOT_EMPTY = '''
ERROR! Destination directory '{}' is not empty.

Would you like to clone to '{}'?

  1. Yes
  2. No, let me enter another path
  3. No, stop cloning

Please enter your reply: '''.lstrip()

CLONE_MERCURIAL_NOT_EMPTY_FALLBACK_FAILED = '''
ERROR! Destination directory '{}' is not empty.
'''

CLONE_MERCURIAL_NOT_DIR = '''
ERROR! Destination '{}' exists but is not a directory.
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
    '"elementary"'
)


class Bootstrapper(object):
    """Main class that performs system bootstrap."""

    def __init__(self, finished=FINISHED, choice=None, no_interactive=False,
                 hg_configure=False):
        self.instance = None
        self.finished = finished
        self.choice = choice
        self.hg_configure = hg_configure
        cls = None
        args = {'no_interactive': no_interactive}

        if sys.platform.startswith('linux'):
            distro, version, dist_id = platform.linux_distribution()

            if distro in ('CentOS', 'CentOS Linux', 'Fedora'):
                cls = CentOSFedoraBootstrapper
                args['distro'] = distro
            elif distro in DEBIAN_DISTROS:
                cls = DebianBootstrapper
                args['distro'] = distro
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

        elif sys.platform.startswith('win32') or sys.platform.startswith('msys'):
            if 'MOZILLABUILD' in os.environ:
                cls = MozillaBuildBootstrapper
            else:
                cls = WindowsBootstrapper

        if cls is None:
            raise NotImplementedError('Bootstrap support is not yet available '
                                      'for your OS.')

        self.instance = cls(**args)

    def input_clone_dest(self):
        print(CLONE_MERCURIAL)

        while True:
            dest = raw_input(CLONE_MERCURIAL_PROMPT)
            dest = dest.strip()
            if not dest:
                return ''

            dest = os.path.expanduser(dest)
            if not os.path.exists(dest):
                return dest

            if not os.path.isdir(dest):
                print(CLONE_MERCURIAL_NOT_DIR.format(dest))
                continue

            if os.listdir(dest) == []:
                return dest

            newdest = os.path.join(dest, 'mozilla-unified')
            if os.path.exists(newdest):
                print(CLONE_MERCURIAL_NOT_EMPTY_FALLBACK_FAILED.format(dest))
                continue

            choice = self.instance.prompt_int(prompt=CLONE_MERCURIAL_NOT_EMPTY.format(dest,
                                              newdest), low=1, high=3)
            if choice == 1:
                return newdest
            if choice == 2:
                continue
            return ''

    def bootstrap(self):
        if self.choice is None:
            # Like ['1. Firefox for Desktop', '2. Firefox for Android Artifact Mode', ...].
            labels = ['%s. %s' % (i + 1, name) for (i, (name, _)) in enumerate(APPLICATIONS_LIST)]
            prompt = APPLICATION_CHOICE % '\n'.join(labels)
            prompt_choice = self.instance.prompt_int(prompt=prompt, low=1, high=len(APPLICATIONS))
            name, application = APPLICATIONS_LIST[prompt_choice-1]
        elif self.choice not in APPLICATIONS.keys():
            raise Exception('Please pick a valid application choice: (%s)' %
                            '/'.join(APPLICATIONS.keys()))
        else:
            name, application = APPLICATIONS[self.choice]

        self.instance.install_system_packages()

        # Like 'install_browser_packages' or 'install_mobile_android_packages'.
        getattr(self.instance, 'install_%s_packages' % application)()

        hg_installed, hg_modern = self.instance.ensure_mercurial_modern()
        self.instance.ensure_python_modern()
        self.instance.ensure_rust_modern()

        # The state directory code is largely duplicated from mach_bootstrap.py.
        # We can't easily import mach_bootstrap.py because the bootstrapper may
        # run in self-contained mode and only the files in this directory will
        # be available. We /could/ refactor parts of mach_bootstrap.py to be
        # part of this directory to avoid the code duplication.
        state_dir, _ = get_state_dir()

        if not os.path.exists(state_dir):
            if not self.instance.no_interactive:
                choice = self.instance.prompt_int(
                    prompt=STATE_DIR_INFO.format(statedir=state_dir),
                    low=1,
                    high=2)

                if choice == 1:
                    print('Creating global state directory: %s' % state_dir)
                    os.makedirs(state_dir, mode=0o770)

        state_dir_available = os.path.exists(state_dir)

        r = current_firefox_checkout(check_output=self.instance.check_output,
                                     hg=self.instance.which('hg'))
        (checkout_type, checkout_root) = r

        # Possibly configure Mercurial, but not if the current checkout is Git.
        # TODO offer to configure Git.
        if hg_installed and state_dir_available and checkout_type != 'git':
            configure_hg = False
            if not self.instance.no_interactive:
                choice = self.instance.prompt_int(prompt=CONFIGURE_MERCURIAL,
                                                  low=1, high=2)
                if choice == 1:
                    configure_hg = True
            else:
                configure_hg = self.hg_configure

            if configure_hg:
                configure_mercurial(self.instance.which('hg'), state_dir)

        # Offer to clone if we're not inside a clone.
        have_clone = False

        if checkout_type:
            have_clone = True
        elif hg_installed and not self.instance.no_interactive:
            dest = self.input_clone_dest()
            if dest:
                have_clone = clone_firefox(self.instance.which('hg'), dest)
                checkout_root = dest

        if not have_clone:
            print(SOURCE_ADVERTISE)

        # Install the clang packages needed for developing stylo.
        if not self.instance.no_interactive:
            # The best place to install our packages is in the state directory
            # we have.  If the user doesn't have one, we need them to re-run
            # bootstrap and create the directory.
            #
            # XXX Android bootstrap just assumes the existence of the state
            # directory and writes the NDK into it.  Should we do the same?
            if not state_dir_available:
                print(STYLO_DIRECTORY_MESSAGE.format(statedir=state_dir))
                sys.exit(1)

            if not have_clone:
                print(STYLO_REQUIRES_CLONE)
                sys.exit(1)

            self.instance.state_dir = state_dir
            self.instance.ensure_stylo_packages(state_dir, checkout_root)

            if 'mobile_android' in application:
                self.instance.ensure_proguard_packages(state_dir, checkout_root)

        print(self.finished % name)
        if not (self.instance.which('rustc') and self.instance._parse_version('rustc')
                >= MODERN_RUST_VERSION):
            print("To build %s, please restart the shell (Start a new terminal window)" % name)

        # Like 'suggest_browser_mozconfig' or 'suggest_mobile_android_mozconfig'.
        getattr(self.instance, 'suggest_%s_mozconfig' % application)()


def update_vct(hg, root_state_dir):
    """Ensure version-control-tools in the state directory is up to date."""
    vct_dir = os.path.join(root_state_dir, 'version-control-tools')

    # Ensure the latest revision of version-control-tools is present.
    update_mercurial_repo(hg, 'https://hg.mozilla.org/hgcustom/version-control-tools',
                          vct_dir, '@')

    return vct_dir


def configure_mercurial(hg, root_state_dir):
    """Run the Mercurial configuration wizard."""
    vct_dir = update_vct(hg, root_state_dir)

    # Run the config wizard from v-c-t.
    args = [
        hg,
        '--config', 'extensions.configwizard=%s/hgext/configwizard' % vct_dir,
        'configwizard',
    ]
    subprocess.call(args)


def update_mercurial_repo(hg, url, dest, revision):
    """Perform a clone/pull + update of a Mercurial repository."""
    # Disable common extensions whose older versions may cause `hg`
    # invocations to abort.
    disable_exts = [
        'bzexport',
        'bzpost',
        'firefoxtree',
        'hgwatchman',
        'mozext',
        'mqext',
        'qimportbz',
        'push-to-try',
        'reviewboard',
    ]

    def disable_extensions(args):
        for ext in disable_exts:
            args.extend(['--config', 'extensions.%s=!' % ext])

    pull_args = [hg]
    disable_extensions(pull_args)

    if os.path.exists(dest):
        pull_args.extend(['pull', url])
        cwd = dest
    else:
        pull_args.extend(['clone', '--noupdate', url, dest])
        cwd = '/'

    update_args = [hg]
    disable_extensions(update_args)
    update_args.extend(['update', '-r', revision])

    print('=' * 80)
    print('Ensuring %s is up to date at %s' % (url, dest))

    try:
        subprocess.check_call(pull_args, cwd=cwd)
        subprocess.check_call(update_args, cwd=dest)
    finally:
        print('=' * 80)


def clone_firefox(hg, dest):
    """Clone the Firefox repository to a specified destination."""
    print('Cloning Firefox Mercurial repository to %s' % dest)

    # We create an empty repo then modify the config before adding data.
    # This is necessary to ensure storage settings are optimally
    # configured.
    args = [
        hg,
        # The unified repo is generaldelta, so ensure the client is as
        # well.
        '--config', 'format.generaldelta=true',
        'init',
        dest
    ]
    res = subprocess.call(args)
    if res:
        print('unable to create destination repo; please try cloning manually')
        return False

    # Strictly speaking, this could overwrite a config based on a template
    # the user has installed. Let's pretend this problem doesn't exist
    # unless someone complains about it.
    with open(os.path.join(dest, '.hg', 'hgrc'), 'ab') as fh:
        fh.write('[paths]\n')
        fh.write('default = https://hg.mozilla.org/mozilla-unified\n')
        fh.write('\n')

        # The server uses aggressivemergedeltas which can blow up delta chain
        # length. This can cause performance to tank due to delta chains being
        # too long. Limit the delta chain length to something reasonable
        # to bound revlog read time.
        fh.write('[format]\n')
        fh.write('# This is necessary to keep performance in check\n')
        fh.write('maxchainlen = 10000\n')

    res = subprocess.call([hg, 'pull', 'https://hg.mozilla.org/mozilla-unified'], cwd=dest)
    print('')
    if res:
        print('error pulling; try running `hg pull https://hg.mozilla.org/mozilla-unified` '
              'manually')
        return False

    print('updating to "central" - the development head of Gecko and Firefox')
    res = subprocess.call([hg, 'update', '-r', 'central'], cwd=dest)
    if res:
        print('error updating; you will need to `hg update` manually')

    print('Firefox source code available at %s' % dest)
    return True


def current_firefox_checkout(check_output, hg=None):
    """Determine whether we're in a Firefox checkout.

    Returns one of None, ``git``, or ``hg``.
    """
    HG_ROOT_REVISIONS = set([
        # From mozilla-central.
        '8ba995b74e18334ab3707f27e9eb8f4e37ba3d29',
    ])

    path = os.getcwd()
    while path:
        hg_dir = os.path.join(path, '.hg')
        git_dir = os.path.join(path, '.git')
        if hg and os.path.exists(hg_dir):
            # Verify the hg repo is a Firefox repo by looking at rev 0.
            try:
                node = check_output([hg, 'log', '-r', '0', '--template', '{node}'], cwd=path)
                if node in HG_ROOT_REVISIONS:
                    return ('hg', path)
                # Else the root revision is different. There could be nested
                # repos. So keep traversing the parents.
            except subprocess.CalledProcessError:
                pass

        # Just check for known-good files in the checkout, to prevent attempted
        # foot-shootings.  Determining a canonical git checkout of mozilla-central
        # is...complicated
        elif os.path.exists(git_dir):
            moz_configure = os.path.join(path, 'moz.configure')
            if os.path.exists(moz_configure):
                return ('git', path)

        path, child = os.path.split(path)
        if child == '':
            break

    return (None, None)
