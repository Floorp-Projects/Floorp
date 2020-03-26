# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from collections import OrderedDict

import platform
import sys
import os
import subprocess

# NOTE: This script is intended to be run with a vanilla Python install.  We
# have to rely on the standard library instead of Python 2+3 helpers like
# the six module.
if sys.version_info < (3,):
    from ConfigParser import (
        Error as ConfigParserError,
        RawConfigParser,
    )
    input = raw_input  # noqa
else:
    from configparser import (
        Error as ConfigParserError,
        RawConfigParser,
    )

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
from mozboot.solus import SolusBootstrapper
from mozboot.windows import WindowsBootstrapper
from mozboot.mozillabuild import MozillaBuildBootstrapper
from mozboot.util import (
    get_state_dir,
)

# Use distro package to retrieve linux platform information
import distro

APPLICATION_CHOICE = '''
Note on Artifact Mode:

Artifact builds download prebuilt C++ components rather than building
them locally. Artifact builds are faster!

Artifact builds are recommended for people working on Firefox or
Firefox for Android frontends, or the GeckoView Java API. They are unsuitable
for those working on C++ code. For more information see:
https://developer.mozilla.org/en-US/docs/Artifact_builds.

Please choose the version of Firefox you want to build:
%s
Your choice: '''

APPLICATIONS = OrderedDict([
    ('Firefox for Desktop Artifact Mode', 'browser_artifact_mode'),
    ('Firefox for Desktop', 'browser'),
    ('GeckoView/Firefox for Android Artifact Mode', 'mobile_android_artifact_mode'),
    ('GeckoView/Firefox for Android', 'mobile_android'),
])

VCS_CHOICE = '''
Firefox can be cloned using either Git or Mercurial.

Please choose the VCS you want to use:
1. Mercurial
2. Git
Your choice: '''

STATE_DIR_INFO = '''
The Firefox build system and related tools store shared, persistent state
in a common directory on the filesystem. On this machine, that directory
is:

  {statedir}

If you would like to use a different directory, hit CTRL+c and set the
MOZBUILD_STATE_PATH environment variable to the directory you'd like to
use and re-run the bootstrapper.

Would you like to create this directory?'''

STYLO_NODEJS_DIRECTORY_MESSAGE = '''
Stylo and NodeJS packages require a directory to store shared, persistent
state.  On this machine, that directory is:

  {statedir}

Please restart bootstrap and create that directory when prompted.
'''

STYLE_NODEJS_REQUIRES_CLONE = '''
Installing Stylo and NodeJS packages requires a checkout of mozilla-central
(or mozilla-unified). Once you have such a checkout, please re-run
`./mach bootstrap` from the checkout directory.
'''

FINISHED = '''
Your system should be ready to build %s!
'''

SOURCE_ADVERTISE = '''
Source code can be obtained by running

    hg clone https://hg.mozilla.org/mozilla-unified

Or, if you prefer Git, by following the instruction here to clone from the
Mercurial repository:

    https://github.com/glandium/git-cinnabar/wiki/Mozilla:-A-git-workflow-for-Gecko-development
'''

CONFIGURE_MERCURIAL = '''
Mozilla recommends a number of changes to Mercurial to enhance your
experience with it.

Would you like to run a configuration wizard to ensure Mercurial is
optimally configured?'''

CONFIGURE_GIT = '''
Mozilla recommends using git-cinnabar to work with mozilla-central (or
mozilla-unified).

Would you like to run a few configuration steps to ensure Git is
optimally configured?'''

CLONE_VCS = '''
If you would like to clone the {} {} repository, please
enter the destination path below.
'''

CLONE_VCS_PROMPT = '''
Destination directory for {} clone (leave empty to not clone): '''.lstrip()

CLONE_VCS_NOT_EMPTY = '''\
Destination directory '{}' is not empty.

Would you like to clone to '{}' instead?
  1. Yes
  2. No, let me enter another path
  3. No, stop cloning
Your choice: '''

CLONE_VCS_NOT_EMPTY_FALLBACK_FAILED = '''
ERROR! Destination directory '{}' is not empty and '{}' exists.
'''

CLONE_VCS_NOT_DIR = '''
ERROR! Destination '{}' exists but is not a directory.
'''

CLONE_MERCURIAL_PULL_FAIL = '''
Failed to pull from hg.mozilla.org.

This is most likely because of unstable network connection.
Try running `hg pull https://hg.mozilla.org/mozilla-unified` manually, or
download mercurial bundle and use it:
https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Source_Code/Mercurial/Bundles'''

DEBIAN_DISTROS = (
    'debian',
    'ubuntu',
    'linuxmint',
    'elementary',
    'neon',
)

ADD_GIT_TOOLS_PATH = '''
To add git-cinnabar to the PATH, edit your shell initialization script, which
may be called ~/.bashrc or ~/.bash_profile or ~/.profile, and add the following
lines:

    export PATH="{}:$PATH"

Then restart your shell.
'''

TELEMETRY_OPT_IN_PROMPT = '''
Build system telemetry

Mozilla collects data about local builds in order to make builds faster and
improve developer tooling. To learn more about the data we intend to collect
read here:
https://firefox-source-docs.mozilla.org/build/buildsystem/telemetry.html.

If you have questions, please ask in #build in irc.mozilla.org. If you would
like to opt out of data collection, select (N) at the prompt.

Would you like to enable build system telemetry?'''


MOZ_PHAB_ADVERTISE = '''
If you plan on submitting changes to Firefox use the following command to
install the review submission tool "moz-phab":

  mach install-moz-phab
'''


def update_or_create_build_telemetry_config(path):
    """Write a mach config file enabling build telemetry to `path`. If the file does not exist,
    create it. If it exists, add the new setting to the existing data.

    This is standalone from mach's `ConfigSettings` so we can use it during bootstrap
    without a source checkout.
    """
    config = RawConfigParser()
    if os.path.exists(path):
        try:
            config.read([path])
        except ConfigParserError as e:
            print('Your mach configuration file at `{path}` is not parseable:\n{error}'.format(
                path=path, error=e))
            return False
    if not config.has_section('build'):
        config.add_section('build')
    config.set('build', 'telemetry', 'true')
    with open(path, 'w') as f:
        config.write(f)
    return True


class Bootstrapper(object):
    """Main class that performs system bootstrap."""

    def __init__(self, finished=FINISHED, choice=None, no_interactive=False,
                 hg_configure=False, no_system_changes=False, mach_context=None,
                 vcs=None):
        self.instance = None
        self.finished = finished
        self.choice = choice
        self.hg_configure = hg_configure
        self.no_system_changes = no_system_changes
        self.mach_context = mach_context
        self.vcs = vcs
        cls = None
        args = {'no_interactive': no_interactive,
                'no_system_changes': no_system_changes}

        if sys.platform.startswith('linux'):
            # distro package provides reliable ids for popular distributions so
            # we use those instead of the full distribution name
            dist_id, version, codename = distro.linux_distribution(full_distribution_name=False)

            if dist_id in ('centos', 'fedora'):
                cls = CentOSFedoraBootstrapper
                args['distro'] = dist_id
            elif dist_id in DEBIAN_DISTROS:
                cls = DebianBootstrapper
                args['distro'] = dist_id
            elif dist_id in ('gentoo', 'funtoo'):
                cls = GentooBootstrapper
            elif dist_id in ('solus'):
                cls = SolusBootstrapper
            elif dist_id in ('arch') or os.path.exists('/etc/arch-release'):
                cls = ArchlinuxBootstrapper
            else:
                raise NotImplementedError('Bootstrap support for this Linux '
                                          'distro not yet available: ' + dist_id)

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

    def input_clone_dest(self, with_hg=True):
        repo_name = 'mozilla-unified'
        vcs = 'Mercurial'
        if not with_hg:
            vcs = 'Git'
        print(CLONE_VCS.format(repo_name, vcs))

        while True:
            dest = input(CLONE_VCS_PROMPT.format(vcs))
            dest = dest.strip()
            if not dest:
                return ''

            dest = os.path.expanduser(dest)
            if not os.path.exists(dest):
                return dest

            if not os.path.isdir(dest):
                print(CLONE_VCS_NOT_DIR.format(dest))
                continue

            if os.listdir(dest) == []:
                return dest

            newdest = os.path.join(dest, repo_name)
            if os.path.exists(newdest):
                print(CLONE_VCS_NOT_EMPTY_FALLBACK_FAILED.format(dest, newdest))
                continue

            choice = self.instance.prompt_int(prompt=CLONE_VCS_NOT_EMPTY.format(dest,
                                              newdest), low=1, high=3)
            if choice == 1:
                return newdest
            if choice == 2:
                continue
            return ''

    # The state directory code is largely duplicated from mach_bootstrap.py.
    # We can't easily import mach_bootstrap.py because the bootstrapper may
    # run in self-contained mode and only the files in this directory will
    # be available. We /could/ refactor parts of mach_bootstrap.py to be
    # part of this directory to avoid the code duplication.
    def try_to_create_state_dir(self):
        state_dir = get_state_dir()

        if not os.path.exists(state_dir):
            should_create_state_dir = True
            if not self.instance.no_interactive:
                should_create_state_dir = self.instance.prompt_yesno(
                    prompt=STATE_DIR_INFO.format(statedir=state_dir))

            # This directory is by default in $HOME, or overridden via an env
            # var, so we probably shouldn't gate it on --no-system-changes.
            if should_create_state_dir:
                print('Creating global state directory: %s' % state_dir)
                os.makedirs(state_dir, mode=0o770)

        state_dir_available = os.path.exists(state_dir)
        return state_dir_available, state_dir

    def maybe_install_private_packages_or_exit(self, state_dir,
                                               state_dir_available,
                                               have_clone,
                                               checkout_root):
        # Install the clang packages needed for building the style system, as
        # well as the version of NodeJS that we currently support.
        # Also install the clang static-analysis package by default
        # The best place to install our packages is in the state directory
        # we have.  We should have created one above in non-interactive mode.
        if not state_dir_available:
            print(STYLO_NODEJS_DIRECTORY_MESSAGE.format(statedir=state_dir))
            sys.exit(1)

        if not have_clone:
            print(STYLE_NODEJS_REQUIRES_CLONE)
            sys.exit(1)

        self.instance.state_dir = state_dir
        self.instance.ensure_node_packages(state_dir, checkout_root)
        self.instance.ensure_fix_stacks_packages(state_dir, checkout_root)
        if not self.instance.artifact_mode:
            self.instance.ensure_stylo_packages(state_dir, checkout_root)
            self.instance.ensure_clang_static_analysis_package(state_dir, checkout_root)
            self.instance.ensure_nasm_packages(state_dir, checkout_root)
            self.instance.ensure_sccache_packages(state_dir, checkout_root)
            self.instance.ensure_lucetc_packages(state_dir, checkout_root)
            self.instance.ensure_wasi_sysroot_packages(state_dir, checkout_root)
            self.instance.ensure_dump_syms_packages(state_dir, checkout_root)

    def check_telemetry_opt_in(self, state_dir):
        # We can't prompt the user.
        if self.instance.no_interactive:
            return
        # Don't prompt if the user already has a setting for this value.
        if self.mach_context is not None and 'telemetry' in self.mach_context.settings.build:
            return
        choice = self.instance.prompt_yesno(prompt=TELEMETRY_OPT_IN_PROMPT)
        if choice:
            cfg_file = os.path.join(state_dir, 'machrc')
            if update_or_create_build_telemetry_config(cfg_file):
                print('\nThanks for enabling build telemetry! You can change this setting at ' +
                      'any time by editing the config file `{}`\n'.format(cfg_file))

    def bootstrap(self):
        if self.choice is None:
            # Like ['1. Firefox for Desktop', '2. Firefox for Android Artifact Mode', ...].
            labels = ['%s. %s' % (i, name) for i, name in enumerate(APPLICATIONS.keys(), 1)]
            prompt = APPLICATION_CHOICE % '\n'.join('  {}'.format(label) for label in labels)
            prompt_choice = self.instance.prompt_int(prompt=prompt, low=1, high=len(APPLICATIONS))
            name, application = list(APPLICATIONS.items())[prompt_choice-1]
        elif self.choice in APPLICATIONS.keys():
            name, application = self.choice, APPLICATIONS[self.choice]
        elif self.choice in APPLICATIONS.values():
            name, application = next((k, v) for k, v in APPLICATIONS.items() if v == self.choice)
        else:
            raise Exception('Please pick a valid application choice: (%s)' %
                            '/'.join(APPLICATIONS.keys()))

        self.instance.application = application
        self.instance.artifact_mode = 'artifact_mode' in application

        if self.instance.no_system_changes:
            state_dir_available, state_dir = self.try_to_create_state_dir()
            # We need to enable the loading of hgrc in case extensions are
            # required to open the repo.
            r = current_firefox_checkout(
                check_output=self.instance.check_output,
                env=self.instance._hg_cleanenv(load_hgrc=True),
                hg=self.instance.which('hg'))
            (checkout_type, checkout_root) = r
            have_clone = bool(checkout_type)

            if state_dir_available:
                self.check_telemetry_opt_in(state_dir)
            self.maybe_install_private_packages_or_exit(state_dir,
                                                        state_dir_available,
                                                        have_clone,
                                                        checkout_root)
            sys.exit(0)

        self.instance.install_system_packages()

        # Like 'install_browser_packages' or 'install_mobile_android_packages'.
        getattr(self.instance, 'install_%s_packages' % application)()

        hg_installed, hg_modern = self.instance.ensure_mercurial_modern()
        self.instance.ensure_python_modern()
        if not self.instance.artifact_mode:
            self.instance.ensure_rust_modern()

        state_dir_available, state_dir = self.try_to_create_state_dir()

        # We need to enable the loading of hgrc in case extensions are
        # required to open the repo.
        r = current_firefox_checkout(check_output=self.instance.check_output,
                                     env=self.instance._hg_cleanenv(load_hgrc=True),
                                     hg=self.instance.which('hg'))
        (checkout_type, checkout_root) = r

        # If we didn't specify a VCS, and we aren't in an exiting clone,
        # offer a choice
        if not self.vcs:
            if checkout_type:
                vcs = checkout_type
            elif self.instance.no_interactive:
                vcs = "hg"
            else:
                prompt_choice = self.instance.prompt_int(prompt=VCS_CHOICE, low=1, high=2)
                vcs = ["hg", "git"][prompt_choice - 1]
        else:
            vcs = self.vcs

        # Possibly configure Mercurial, but not if the current checkout or repo
        # type is Git.
        if hg_installed and state_dir_available and (checkout_type == 'hg' or vcs == 'hg'):
            configure_hg = False
            if not self.instance.no_interactive:
                configure_hg = self.instance.prompt_yesno(prompt=CONFIGURE_MERCURIAL)
            else:
                configure_hg = self.hg_configure

            if configure_hg:
                configure_mercurial(self.instance.which('hg'), state_dir)

        # Offer to configure Git, if the current checkout or repo type is Git.
        elif self.instance.which('git') and (checkout_type == 'git' or vcs == 'git'):
            should_configure_git = False
            if not self.instance.no_interactive:
                should_configure_git = self.instance.prompt_yesno(prompt=CONFIGURE_GIT)
            else:
                # Assuming default configuration setting applies to all VCS.
                should_configure_git = self.hg_configure

            if should_configure_git:
                configure_git(self.instance.which('git'), state_dir,
                              checkout_root)

        # Offer to clone if we're not inside a clone.
        have_clone = False

        if checkout_type:
            have_clone = True
        elif hg_installed and not self.instance.no_interactive and vcs == 'hg':
            dest = self.input_clone_dest()
            if dest:
                have_clone = hg_clone_firefox(self.instance.which('hg'), dest)
                checkout_root = dest
        elif self.instance.which('git') and vcs == 'git':
            dest = self.input_clone_dest(False)
            if dest:
                git = self.instance.which('git')
                watchman = self.instance.which('watchman')
                have_clone = git_clone_firefox(git, dest, watchman)
                checkout_root = dest

        if not have_clone:
            print(SOURCE_ADVERTISE)

        if state_dir_available:
            self.check_telemetry_opt_in(state_dir)
        self.maybe_install_private_packages_or_exit(state_dir,
                                                    state_dir_available,
                                                    have_clone,
                                                    checkout_root)

        print(self.finished % name)
        if not (self.instance.which('rustc') and self.instance._parse_version('rustc')
                >= MODERN_RUST_VERSION):
            print("To build %s, please restart the shell (Start a new terminal window)" % name)

        if not self.instance.which("moz-phab"):
            print(MOZ_PHAB_ADVERTISE)

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


def hg_clone_firefox(hg, dest):
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
    with open(os.path.join(dest, '.hg', 'hgrc'), 'a') as fh:
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
        print(CLONE_MERCURIAL_PULL_FAIL)
        return False

    print('updating to "central" - the development head of Gecko and Firefox')
    res = subprocess.call([hg, 'update', '-r', 'central'], cwd=dest)
    if res:
        print('error updating; you will need to `hg update` manually')

    print('Firefox source code available at %s' % dest)
    return True


def current_firefox_checkout(check_output, env, hg=None):
    """Determine whether we're in a Firefox checkout.

    Returns one of None, ``git``, or ``hg``.
    """
    HG_ROOT_REVISIONS = set([
        # From mozilla-unified.
        '8ba995b74e18334ab3707f27e9eb8f4e37ba3d29',
    ])

    path = os.getcwd()
    while path:
        hg_dir = os.path.join(path, '.hg')
        git_dir = os.path.join(path, '.git')
        if hg and os.path.exists(hg_dir):
            # Verify the hg repo is a Firefox repo by looking at rev 0.
            try:
                node = check_output([hg, 'log', '-r', '0', '--template', '{node}'],
                                    cwd=path,
                                    env=env,
                                    universal_newlines=True)
                if node in HG_ROOT_REVISIONS:
                    return ('hg', path)
                # Else the root revision is different. There could be nested
                # repos. So keep traversing the parents.
            except subprocess.CalledProcessError:
                pass

        # Just check for known-good files in the checkout, to prevent attempted
        # foot-shootings.  Determining a canonical git checkout of mozilla-unified
        # is...complicated
        elif os.path.exists(git_dir):
            moz_configure = os.path.join(path, 'moz.configure')
            if os.path.exists(moz_configure):
                return ('git', path)

        path, child = os.path.split(path)
        if child == '':
            break

    return (None, None)


def update_git_tools(git, root_state_dir, top_src_dir):
    """Update git tools, hooks and extensions"""
    # Bug 1481425 - delete the git-mozreview
    # commit message hook in .git/hooks dir
    if top_src_dir:
        mozreview_commit_hook = os.path.join(top_src_dir, '.git/hooks/commit-msg')
        if os.path.exists(mozreview_commit_hook):
            with open(mozreview_commit_hook, 'rb') as f:
                contents = f.read()

            if b'MozReview' in contents:
                print('removing git-mozreview commit message hook...')
                os.remove(mozreview_commit_hook)
                print('git-mozreview commit message hook removed.')

    # Ensure git-cinnabar is up to date.
    cinnabar_dir = os.path.join(root_state_dir, 'git-cinnabar')

    # Ensure the latest revision of git-cinnabar is present.
    update_git_repo(git, 'https://github.com/glandium/git-cinnabar.git',
                    cinnabar_dir)

    # Perform a download of cinnabar.
    download_args = [git, 'cinnabar', 'download']

    try:
        subprocess.check_call(download_args, cwd=cinnabar_dir)
    except subprocess.CalledProcessError as e:
        print(e)
    return cinnabar_dir


def update_git_repo(git, url, dest):
    """Perform a clone/pull + update of a Git repository."""
    pull_args = [git]

    if os.path.exists(dest):
        pull_args.extend(['pull'])
        cwd = dest
    else:
        pull_args.extend(['clone', '--no-checkout', url, dest])
        cwd = '/'

    update_args = [git, 'checkout']

    print('=' * 80)
    print('Ensuring %s is up to date at %s' % (url, dest))

    try:
        subprocess.check_call(pull_args, cwd=cwd)
        subprocess.check_call(update_args, cwd=dest)
    finally:
        print('=' * 80)


def configure_git(git, root_state_dir, top_src_dir):
    """Run the Git configuration steps."""
    cinnabar_dir = update_git_tools(git, root_state_dir, top_src_dir)

    print(ADD_GIT_TOOLS_PATH.format(cinnabar_dir))


def git_clone_firefox(git, dest, watchman=None):
    """Clone the Firefox repository to a specified destination."""
    print('Cloning Firefox repository to %s' % dest)

    try:
        # Configure git per the git-cinnabar requirements.
        subprocess.check_call([git, 'clone', '-b', 'bookmarks/central',
                               'hg::https://hg.mozilla.org/mozilla-unified', dest])
        subprocess.check_call([git, 'remote', 'add', 'inbound',
                               'hg::ssh://hg.mozilla.org/integration/mozilla-inbound'],
                              cwd=dest)
        subprocess.check_call([git, 'config', 'remote.inbound.skipDefaultUpdate',
                               'true'], cwd=dest)
        subprocess.check_call([git, 'config', 'remote.inbound.push',
                               '+HEAD:refs/heads/branches/default/tip'], cwd=dest)
        subprocess.check_call([git, 'config', 'fetch.prune', 'true'], cwd=dest)
        subprocess.check_call([git, 'config', 'pull.ff', 'only'], cwd=dest)

        watchman_sample = os.path.join(dest, '.git/hooks/fsmonitor-watchman.sample')
        # Older versions of git didn't include fsmonitor-watchman.sample.
        if watchman and watchman_sample:
            print('Configuring watchman')
            watchman_config = os.path.join(dest, '.git/hooks/query-watchman')
            if not os.path.exists(watchman_config):
                print('Copying %s to %s' % (watchman_sample, watchman_config))
                copy_args = ['cp', '.git/hooks/fsmonitor-watchman.sample',
                             '.git/hooks/query-watchman']
                subprocess.check_call(copy_args, cwd=dest)

            config_args = [git, 'config', 'core.fsmonitor', '.git/hooks/query-watchman']
            subprocess.check_call(config_args, cwd=dest)
    except Exception as e:
        print(e)
        return False

    print('Firefox source code available at %s' % dest)
    return True
