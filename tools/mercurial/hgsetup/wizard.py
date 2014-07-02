# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import difflib
import errno
import os
import shutil
import sys
import which

from distutils.version import StrictVersion

from configobj import ConfigObjError
from StringIO import StringIO

from mozversioncontrol import get_hg_version
from mozversioncontrol.repoupdate import (
    update_mercurial_repo,
    update_git_repo,
)

from .config import (
    HOST_FINGERPRINTS,
    MercurialConfig,
)


INITIAL_MESSAGE = '''
I'm going to help you ensure your Mercurial is configured for optimal
development on Mozilla projects.

If your environment is missing some recommended settings, I'm going to prompt
you whether you want me to make changes: I won't change anything you might not
want me changing without your permission!

If your config is up-to-date, I'm just going to ensure all 3rd party extensions
are up to date and you won't have to do anything.

To begin, press the enter/return key.
'''.strip()

OLDEST_NON_LEGACY_VERSION = StrictVersion('3.0')
LEGACY_MERCURIAL = '''
You are running an out of date Mercurial client (%s).

For a faster and better Mercurial experience, we HIGHLY recommend you
upgrade.
'''.strip()

MISSING_USERNAME = '''
You don't have a username defined in your Mercurial config file. In order to
send patches to Mozilla, you'll need to attach a name and email address. If you
aren't comfortable giving us your full name, pseudonames are acceptable.
'''.strip()

BAD_DIFF_SETTINGS = '''
Mozilla developers produce patches in a standard format, but your Mercurial is
not configured to produce patches in that format.
'''.strip()

MQ_INFO = '''
The mq extension manages patches as separate files. It provides an
alternative to the recommended bookmark-based development workflow.

If you are a newcomer to Mercurial or are coming from Git, it is
recommended to avoid mq.

Would you like to activate the mq extension
'''.strip()

BZEXPORT_INFO = '''
If you plan on uploading patches to Mozilla, there is an extension called
bzexport that makes it easy to upload patches from the command line via the
|hg bzexport| command. More info is available at
https://hg.mozilla.org/hgcustom/version-control-tools/file/default/hgext/bzexport/README

Would you like to activate bzexport
'''.strip()

MQEXT_INFO = '''
The mqext extension (https://bitbucket.org/sfink/mqext) provides a number of
useful abilities to Mercurial, including automatically committing changes to
your mq patch queue.

Would you like to activate mqext
'''.strip()

QIMPORTBZ_INFO = '''
The qimportbz extension
(https://hg.mozilla.org/hgcustom/version-control-tools/file/default/hgext/qimportbz/README) makes it possible to
import patches from Bugzilla using a friendly bz:// URL handler. e.g.
|hg qimport bz://123456|.

Would you like to activate qimportbz
'''.strip()

QNEWCURRENTUSER_INFO = '''
The mercurial queues command |hg qnew|, which creates new patches in your patch
queue does not set patch author information by default. Author information
should be included when uploading for review.
'''.strip()

FINISHED = '''
Your Mercurial should now be properly configured and recommended extensions
should be up to date!
'''.strip()

REVIEWBOARD_MINIMUM_VERSION = StrictVersion('3.0.1')

REVIEWBOARD_INCOMPATIBLE = '''
Your Mercurial is too old to use the reviewboard extension, which is necessary
to conduct code review.

Please upgrade to Mercurial %s or newer to use this extension.
'''.strip()

MISSING_BUGZILLA_CREDENTIALS = '''
You do not have your Bugzilla credentials defined in your Mercurial config.

Various extensions make use of your Bugzilla credentials to interface with
Bugzilla to enrich your development experience.

Bugzilla credentials are optional. If you do not provide them, associated
functionality will not be enabled or you will be prompted for your
Bugzilla credentials when they are needed.
'''.lstrip()

class MercurialSetupWizard(object):
    """Command-line wizard to help users configure Mercurial."""

    def __init__(self, state_dir):
        # We use normpath since Mercurial expects the hgrc to use native path
        # separators, but state_dir uses unix style paths even on Windows.
        self.state_dir = os.path.normpath(state_dir)
        self.ext_dir = os.path.join(self.state_dir, 'mercurial', 'extensions')
        self.vcs_tools_dir = os.path.join(self.state_dir, 'version-control-tools')
        self.update_vcs_tools = False

    def run(self, config_paths):
        try:
            os.makedirs(self.ext_dir)
        except OSError as e:
            if e.errno != errno.EEXIST:
                raise

        try:
            hg = which.which('hg')
        except which.WhichError as e:
            print(e)
            print('Try running |mach bootstrap| to ensure your environment is '
                'up to date.')
            return 1

        try:
            c = MercurialConfig(config_paths)
        except ConfigObjError as e:
            print('Error importing existing Mercurial config!\n'
                  '%s\n'
                  'If using quotes, they must wrap the entire string.' % e)
            return 1

        print(INITIAL_MESSAGE)
        raw_input()

        hg_version = get_hg_version(hg)
        if hg_version < OLDEST_NON_LEGACY_VERSION:
            print(LEGACY_MERCURIAL % hg_version)
            print('')

            if os.name == 'nt':
                print('Please upgrade to the latest MozillaBuild to upgrade '
                    'your Mercurial install.')
                print('')
            else:
                print('Please run |mach bootstrap| to upgrade your Mercurial '
                    'install.')
                print('')

            if not self._prompt_yn('Would you like to continue using an old '
                'Mercurial version'):
                return 1

        if not c.have_valid_username():
            print(MISSING_USERNAME)
            print('')

            name = self._prompt('What is your name?')
            email = self._prompt('What is your email address?')
            c.set_username(name, email)
            print('Updated your username.')
            print('')

        if not c.have_recommended_diff_settings():
            print(BAD_DIFF_SETTINGS)
            print('')
            if self._prompt_yn('Would you like me to fix this for you'):
                c.ensure_recommended_diff_settings()
                print('Fixed patch settings.')
                print('')

        self.prompt_native_extension(c, 'progress',
            'Would you like to see progress bars during Mercurial operations')

        self.prompt_native_extension(c, 'color',
            'Would you like Mercurial to colorize output to your terminal')

        self.prompt_native_extension(c, 'rebase',
            'Would you like to enable the rebase extension to allow you to move'
            ' changesets around (which can help maintain a linear history)')

        self.prompt_native_extension(c, 'histedit',
            'Would you like to enable the histedit extension to allow history '
            'rewriting via the "histedit" command (similar to '
            '`git rebase -i`)')

        self.prompt_native_extension(c, 'mq', MQ_INFO)

        self.prompt_external_extension(c, 'bzexport', BZEXPORT_INFO)

        if 'reviewboard' not in c.extensions:
            if hg_version < REVIEWBOARD_MINIMUM_VERSION:
                print(REVIEWBOARD_INCOMPATIBLE % REVIEWBOARD_MINIMUM_VERSION)
            else:
                p = os.path.join(self.vcs_tools_dir, 'hgext', 'reviewboard',
                    'client.py')
                self.prompt_external_extension(c, 'reviewboard',
                    'Would you like to enable the reviewboard extension so '
                    'you can easily initiate code reviews against Mozilla '
                    'projects',
                    path=p)

        if 'mq' in c.extensions:
            self.prompt_external_extension(c, 'mqext', MQEXT_INFO,
                                           os.path.join(self.ext_dir, 'mqext'))

            if 'mqext' in c.extensions:
                self.update_mercurial_repo(
                    hg,
                    'https://bitbucket.org/sfink/mqext',
                    os.path.join(self.ext_dir, 'mqext'),
                    'default',
                    'Ensuring mqext extension is up to date...')

            if 'mqext' in c.extensions and not c.have_mqext_autocommit_mq():
                if self._prompt_yn('Would you like to configure mqext to '
                    'automatically commit changes as you modify patches'):
                    c.ensure_mqext_autocommit_mq()
                    print('Configured mqext to auto-commit.\n')

            self.prompt_external_extension(c, 'qimportbz', QIMPORTBZ_INFO)

            if not c.have_qnew_currentuser_default():
                print(QNEWCURRENTUSER_INFO)
                if self._prompt_yn('Would you like qnew to set patch author by '
                                   'default'):
                    c.ensure_qnew_currentuser_default()
                    print('Configured qnew to set patch author by default.')
                    print('')

        if 'reviewboard' in c.extensions:
            bzuser, bzpass = c.get_bugzilla_credentials()

            if not bzuser or not bzpass:
                print(MISSING_BUGZILLA_CREDENTIALS)

            if not bzuser:
                bzuser = self._prompt('What is your Bugzilla email address?',
                    allow_empty=True)

            if bzuser and not bzpass:
                bzpass = self._prompt('What is your Bugzilla password?',
                    allow_empty=True)

            if bzuser or bzpass:
                c.set_bugzilla_credentials(bzuser, bzpass)

        if self.update_vcs_tools:
            self.update_mercurial_repo(
                hg,
                'https://hg.mozilla.org/hgcustom/version-control-tools',
                self.vcs_tools_dir,
                'default',
                'Ensuring version-control-tools is up to date...')

        # Look for and clean up old extensions.
        for ext in {'bzexport', 'qimportbz'}:
            path = os.path.join(self.ext_dir, ext)
            if os.path.exists(path):
                if self._prompt_yn('Would you like to remove the old and no '
                    'longer referenced repository at %s' % path):
                    print('Cleaning up old repository: %s' % path)
                    shutil.rmtree(path)

        c.add_mozilla_host_fingerprints()

        b = StringIO()
        c.write(b)
        new_lines = [line.rstrip() for line in b.getvalue().splitlines()]
        old_lines = []

        config_path = c.config_path
        if os.path.exists(config_path):
            with open(config_path, 'rt') as fh:
                old_lines = [line.rstrip() for line in fh.readlines()]

        diff = list(difflib.unified_diff(old_lines, new_lines,
            'hgrc.old', 'hgrc.new'))

        if len(diff):
            print('Your Mercurial config file needs updating. I can do this '
                'for you if you like!')
            if self._prompt_yn('Would you like to see a diff of the changes '
                'first'):
                for line in diff:
                    print(line)
                print('')

            if self._prompt_yn('Would you like me to update your hgrc file'):
                with open(config_path, 'wt') as fh:
                    c.write(fh)
                print('Wrote changes to %s.' % config_path)
            else:
                print('hgrc changes not written to file. I would have '
                    'written the following:\n')
                c.write(sys.stdout)
                return 1

        print(FINISHED)
        return 0

    def prompt_native_extension(self, c, name, prompt_text):
        # Ask the user if the specified extension bundled with Mercurial should be enabled.
        if name in c.extensions:
            return
        if self._prompt_yn(prompt_text):
            c.activate_extension(name)
            print('Activated %s extension.\n' % name)

    def prompt_external_extension(self, c, name, prompt_text, path=None):
        # Ask the user if the specified extension should be enabled. Defaults
        # to treating the extension as one in version-control-tools/hgext/
        # in a directory with the same name as the extension and thus also
        # flagging the version-control-tools repo as needing an update.
        if name not in c.extensions:
            if not self._prompt_yn(prompt_text):
                return
            print('Activated %s extension.\n' % name)
        if not path:
            path = os.path.join(self.vcs_tools_dir, 'hgext', name)
            self.update_vcs_tools = True
        c.activate_extension(name, path)

    def update_mercurial_repo(self, hg, url, dest, branch, msg):
        # We always pass the host fingerprints that we "know" to be canonical
        # because the existing config may have outdated fingerprints and this
        # may cause Mercurial to abort.
        return self._update_repo(hg, url, dest, branch, msg,
            update_mercurial_repo, hostfingerprints=HOST_FINGERPRINTS)

    def update_git_repo(self, git, url, dest, ref, msg):
        return self._update_repo(git, url, dest, ref, msg, update_git_repo)

    def _update_repo(self, binary, url, dest, branch, msg, fn, *args, **kwargs):
        print('=' * 80)
        print(msg)
        try:
            fn(binary, url, dest, branch, *args, **kwargs)
        finally:
            print('=' * 80)
            print('')

    def _prompt(self, msg, allow_empty=False):
        print(msg)

        while True:
            response = raw_input()

            if response:
                return response

            if allow_empty:
                return None

            print('You must type something!')

    def _prompt_yn(self, msg):
        print('%s? [Y/n]' % msg)

        while True:
            choice = raw_input().lower().strip()

            if not choice:
                return True

            if choice in ('y', 'yes'):
                return True

            if choice in ('n', 'no'):
                return False

            print('Must reply with one of {yes, no, y, n}.')
