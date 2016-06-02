# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import difflib
import errno
import os
import shutil
import ssl
import stat
import sys
import subprocess

from distutils.version import LooseVersion

from configobj import ConfigObjError
from StringIO import StringIO

from mozversioncontrol import get_hg_path, get_hg_version

from .update import MercurialUpdater
from .config import (
    config_file,
    MercurialConfig,
    ParseException,
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

# This should match MODERN_MERCURIAL_VERSION in
# python/mozboot/mozboot/base.py.
OLDEST_NON_LEGACY_VERSION = LooseVersion('3.7.3')
LEGACY_MERCURIAL = '''
You are running an out of date Mercurial client (%s).

For a faster and better Mercurial experience, we HIGHLY recommend you
upgrade.

Legacy versions of Mercurial have known security vulnerabilities. Failure
to upgrade may leave you exposed. You are highly encouraged to upgrade
in case you aren't running a patched version.
'''.strip()

MISSING_USERNAME = '''
You don't have a username defined in your Mercurial config file. In order to
send patches to Mozilla, you'll need to attach a name and email address. If you
aren't comfortable giving us your full name, pseudonames are acceptable.

(Relevant config option: ui.username)
'''.strip()

BAD_DIFF_SETTINGS = '''
Mozilla developers produce patches in a standard format, but your Mercurial is
not configured to produce patches in that format.

(Relevant config options: diff.git, diff.showfunc, diff.unified)
'''.strip()

BZEXPORT_INFO = '''
If you plan on uploading patches to Mozilla, there is an extension called
bzexport that makes it easy to upload patches from the command line via the
|hg bzexport| command. More info is available at
https://hg.mozilla.org/hgcustom/version-control-tools/file/default/hgext/bzexport/README

(Relevant config option: extensions.bzexport)

Would you like to activate bzexport
'''.strip()

FINISHED = '''
Your Mercurial should now be properly configured and recommended extensions
should be up to date!
'''.strip()

REVIEWBOARD_MINIMUM_VERSION = LooseVersion('3.5')

REVIEWBOARD_INCOMPATIBLE = '''
Your Mercurial is too old to use the reviewboard extension, which is necessary
to conduct code review.

Please upgrade to Mercurial %s or newer to use this extension.
'''.strip()

MISSING_BUGZILLA_CREDENTIALS = '''
You do not have your Bugzilla API Key defined in your Mercurial config.

Various extensions make use of a Bugzilla API Key to interface with
Bugzilla to enrich your development experience.

The Bugzilla API Key is optional. If you do not provide one, associated
functionality will not be enabled, we will attempt to find a Bugzilla cookie
from a Firefox profile, or you will be prompted for your Bugzilla credentials
when they are needed.

You should only need to configure a Bugzilla API Key once.
'''.lstrip()

BUGZILLA_API_KEY_INSTRUCTIONS = '''
Bugzilla API Keys can only be obtained through the Bugzilla web interface.

Please perform the following steps:

  1) Open https://bugzilla.mozilla.org/userprefs.cgi?tab=apikey
  2) Generate a new API Key
  3) Copy the generated key and paste it here
'''.lstrip()

LEGACY_BUGZILLA_CREDENTIALS_DETECTED = '''
Your existing Mercurial config uses a legacy method for defining Bugzilla
credentials. Bugzilla API Keys are the most secure and preferred method
for defining Bugzilla credentials. Bugzilla API Keys are also required
if you have enabled 2 Factor Authentication in Bugzilla.

All consumers formerly looking at these options should support API Keys.
'''.lstrip()

BZPOST_MINIMUM_VERSION = LooseVersion('3.5')

BZPOST_INFO = '''
The bzpost extension automatically records the URLs of pushed commits to
referenced Bugzilla bugs after push.

(Relevant config option: extensions.bzpost)

Would you like to activate bzpost
'''.strip()

FIREFOXTREE_MINIMUM_VERSION = LooseVersion('3.5')

FIREFOXTREE_INFO = '''
The firefoxtree extension makes interacting with the multiple Firefox
repositories easier:

* Aliases for common trees are pre-defined. e.g. `hg pull central`
* Pulling from known Firefox trees will create "remote refs" appearing as
  tags. e.g. pulling from fx-team will produce a "fx-team" tag.
* The `hg fxheads` command will list the heads of all pulled Firefox repos
  for easy reference.
* `hg push` will limit itself to pushing a single head when pushing to
  Firefox repos.
* A pre-push hook will prevent you from pushing multiple heads to known
  Firefox repos. This acts quicker than a server-side hook.

The firefoxtree extension is *strongly* recommended if you:

a) aggregate multiple Firefox repositories into a single local repo
b) perform head/bookmark-based development (as opposed to mq)

(Relevant config option: extensions.firefoxtree)

Would you like to activate firefoxtree
'''.strip()

PUSHTOTRY_MINIMUM_VERSION = LooseVersion('3.5')

PUSHTOTRY_INFO = '''
The push-to-try extension generates a temporary commit with a given
try syntax and pushes it to the try server. The extension is intended
to be used in concert with other tools generating try syntax so that
they can push to try without depending on mq or other workarounds.

(Relevant config option: extensions.push-to-try)

Would you like to activate push-to-try
'''.strip()

CLONEBUNDLES_INFO = '''
Mercurial 3.6 and hg.mozilla.org support transparently cloning from a CDN,
making clones faster and more reliable.

(Relevant config option: experimental.clonebundles)

Would you like to activate this feature and have faster clones
'''.strip()

BUNDLECLONE_MINIMUM_VERSION = LooseVersion('3.1')

BUNDLECLONE_INFO = '''
The bundleclone extension makes cloning faster and saves server resources.

We highly recommend you activate this extension.

(Relevant config option: extensions.bundleclone)

Would you like to activate bundleclone
'''.strip()

WIP_INFO = '''
It is common to want a quick view of changesets that are in progress.

The ``hg wip`` command provides should a view.

Example Usage:

  $ hg wip
  o   4084:fcfa34d0387b dminor  @
  |  mozreview: use repository name when displaying treeherder results (bug 1230548) r=mcote
  | @   4083:786baf6d476a gps
  | |  mozreview: create child review requests from batch API
  | o   4082:3f100fa4a94f gps
  | |  mozreview: copy more read-only processing code; r?smacleod
  | o   4081:939417680cbe gps
  |/   mozreview: add web API to submit an entire series of commits (bug 1229468); r?smacleod

(Not shown are the colors that help denote the state each changeset
is in.)

(Relevant config options: alias.wip, revsetalias.wip, templates.wip)

Would you like to install the `hg wip` alias?
'''.strip()

HGWATCHMAN_MINIMUM_VERSION = LooseVersion('3.5.2')
FSMONITOR_MINIMUM_VERSION = LooseVersion('3.8')

FSMONITOR_INFO = '''
Filesystem monitor integrates the watchman filesystem watching
tool with Mercurial. Commands like `hg status`, `hg diff`, and
`hg commit` that need to examine filesystem state can query watchman
and obtain filesystem state nearly instantaneously. The result is much
faster command execution.

When enabled, the filesystem monitor tool will launch a background
watchman file watching daemon for accessed Mercurial repositories.

Would you like to enable filesystem monitor tool
'''.strip()

FILE_PERMISSIONS_WARNING = '''
Your hgrc file is currently readable by others.

Sensitive information such as your Bugzilla credentials could be
stolen if others have access to this file/machine.
'''.strip()

MULTIPLE_VCT = '''
*** WARNING ***

Multiple version-control-tools repositories are referenced in your
Mercurial config. Extensions and other code within the
version-control-tools repository could run with inconsistent results.

Please manually edit the following file to reference a single
version-control-tools repository:

    %s
'''.lstrip()


class MercurialSetupWizard(object):
    """Command-line wizard to help users configure Mercurial."""

    def __init__(self, state_dir):
        # We use normpath since Mercurial expects the hgrc to use native path
        # separators, but state_dir uses unix style paths even on Windows.
        self.state_dir = os.path.normpath(state_dir)
        self.ext_dir = os.path.join(self.state_dir, 'mercurial', 'extensions')
        self.vcs_tools_dir = os.path.join(self.state_dir, 'version-control-tools')
        self.updater = MercurialUpdater(state_dir)

    def run(self, config_paths):
        try:
            os.makedirs(self.ext_dir)
        except OSError as e:
            if e.errno != errno.EEXIST:
                raise

        hg = get_hg_path()
        config_path = config_file(config_paths)

        try:
            c = MercurialConfig(config_path)
        except ConfigObjError as e:
            print('Error importing existing Mercurial config: %s\n' % config_path)
            for error in e.errors:
                print(error.message)

            return 1
        except ParseException as e:
            print('Error importing existing Mercurial config: %s\n' % config_path)
            print('Line %d: %s' % (e.line, e.message))

            return 1

        self.updater.update_all()

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

        # Progress is built into core and enabled by default in Mercurial 3.5.
        if hg_version < LooseVersion('3.5'):
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

        # hgwatchman is provided by MozillaBuild and we don't yet support
        # Linux/BSD.
        # Note that the hgwatchman project has been renamed to fsmonitor and has
        # been moved into Mercurial core, as of version 3.8. So, if your Mercurial
        # version is modern enough (>=3.8), you could set fsmonitor in hgrc file
        # directly.
        if ('hgwatchman' not in c.extensions
            and sys.platform.startswith('darwin')
            and hg_version >= HGWATCHMAN_MINIMUM_VERSION
            and self._prompt_yn(FSMONITOR_INFO)):
            if (hg_version >= FSMONITOR_MINIMUM_VERSION):
                c.activate_extension('fsmonitor')
            else:
                # Unlike other extensions, we need to run an installer
                # to compile a Python C extension.
                try:
                    subprocess.check_output(
                        ['make', 'local'],
                        cwd=self.updater.hgwatchman_dir,
                        stderr=subprocess.STDOUT)

                    ext_path = os.path.join(self.updater.hgwatchman_dir,
                                            'hgwatchman')
                    if self.can_use_extension(c, 'hgwatchman', ext_path):
                        c.activate_extension('hgwatchman', ext_path)
                except subprocess.CalledProcessError as e:
                    print('Error compiling hgwatchman; will not install hgwatchman')
                    print(e.output)

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

        self.prompt_external_extension(c, 'bzexport', BZEXPORT_INFO)

        if hg_version >= BZPOST_MINIMUM_VERSION:
            self.prompt_external_extension(c, 'bzpost', BZPOST_INFO)

        if hg_version >= FIREFOXTREE_MINIMUM_VERSION:
            self.prompt_external_extension(c, 'firefoxtree', FIREFOXTREE_INFO)

        # Functionality from bundleclone is experimental in Mercurial 3.6.
        # There was a bug in 3.6, so look for 3.6.1.
        if hg_version >= LooseVersion('3.6.1'):
            if not c.have_clonebundles() and self._prompt_yn(CLONEBUNDLES_INFO):
                c.activate_clonebundles()
                print('Enabled the clonebundles feature.\n')
        elif hg_version >= BUNDLECLONE_MINIMUM_VERSION:
            self.prompt_external_extension(c, 'bundleclone', BUNDLECLONE_INFO)

        if hg_version >= PUSHTOTRY_MINIMUM_VERSION:
            self.prompt_external_extension(c, 'push-to-try', PUSHTOTRY_INFO)

        if not c.have_wip():
            if self._prompt_yn(WIP_INFO):
                c.install_wip_alias()

        if 'reviewboard' in c.extensions or 'bzpost' in c.extensions:
            bzuser, bzpass, bzuserid, bzcookie, bzapikey = c.get_bugzilla_credentials()

            if not bzuser or not bzapikey:
                print(MISSING_BUGZILLA_CREDENTIALS)

            if not bzuser:
                bzuser = self._prompt('What is your Bugzilla email address? (optional)',
                    allow_empty=True)

            if bzuser and not bzapikey:
                print(BUGZILLA_API_KEY_INSTRUCTIONS)
                bzapikey = self._prompt('Please enter a Bugzilla API Key: (optional)',
                    allow_empty=True)

            if bzuser or bzapikey:
                c.set_bugzilla_credentials(bzuser, bzapikey)

            if bzpass or bzuserid or bzcookie:
                print(LEGACY_BUGZILLA_CREDENTIALS_DETECTED)

                # Clear legacy credentials automatically if an API Key is
                # found as it supercedes all other credentials.
                if bzapikey:
                    print('The legacy credentials have been removed.\n')
                    c.clear_legacy_bugzilla_credentials()
                elif self._prompt_yn('Remove legacy credentials'):
                    c.clear_legacy_bugzilla_credentials()

        # Look for and clean up old extensions.
        for ext in {'bzexport', 'qimportbz', 'mqext'}:
            path = os.path.join(self.ext_dir, ext)
            if os.path.exists(path):
                if self._prompt_yn('Would you like to remove the old and no '
                    'longer referenced repository at %s' % path):
                    print('Cleaning up old repository: %s' % path)
                    shutil.rmtree(path)

        # Python + Mercurial didn't have terrific TLS handling until Python
        # 2.7.9 and Mercurial 3.4. For this reason, it was recommended to pin
        # certificates in Mercurial config files. In modern versions of
        # Mercurial, the system CA store is used and old, legacy TLS protocols
        # are disabled. The default connection/security setting should
        # be sufficient and pinning certificates is no longer needed.
        have_modern_ssl = hasattr(ssl, 'SSLContext')
        if hg_version < LooseVersion('3.4') or not have_modern_ssl:
            c.add_mozilla_host_fingerprints()

        # We always update fingerprints if they are present. We /could/ offer to
        # remove fingerprints if running modern Python and Mercurial. But that
        # just adds more UI complexity and isn't worth it.
        c.update_mozilla_host_fingerprints()

        # References to multiple version-control-tools checkouts can confuse
        # version-control-tools, since various Mercurial extensions resolve
        # dependencies via __file__ and repos could reference another copy.
        seen_vct = set()
        for k, v in c.config.get('extensions', {}).items():
            if 'version-control-tools' not in v:
                continue

            i = v.index('version-control-tools')
            vct = v[0:i + len('version-control-tools')]
            seen_vct.add(os.path.realpath(os.path.expanduser(vct)))

        if len(seen_vct) > 1:
            print(MULTIPLE_VCT % c.config_path)

        # At this point the config should be finalized.

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

        if sys.platform != 'win32':
            # Config file may contain sensitive content, such as passwords.
            # Prompt to remove global permissions.
            mode = os.stat(config_path).st_mode
            if mode & (stat.S_IRWXG | stat.S_IRWXO):
                print(FILE_PERMISSIONS_WARNING)
                if self._prompt_yn('Remove permissions for others to '
                                   'read your hgrc file'):
                    # We don't care about sticky and set UID bits because
                    # this is a regular file.
                    mode = mode & stat.S_IRWXU
                    print('Changing permissions of %s' % config_path)
                    os.chmod(config_path, mode)

        print(FINISHED)
        return 0

    def prompt_native_extension(self, c, name, prompt_text):
        # Ask the user if the specified extension bundled with Mercurial should be enabled.
        if name in c.extensions:
            return
        if self._prompt_yn(prompt_text):
            c.activate_extension(name)
            print('Activated %s extension.\n' % name)

    def can_use_extension(self, c, name, path=None):
        # Load extension to hg and search stdout for printed exceptions
        if not path:
            path = os.path.join(self.vcs_tools_dir, 'hgext', name)
        result = subprocess.check_output(['hg',
             '--config', 'extensions.testmodule=%s' % path,
             '--config', 'ui.traceback=true'],
            stderr=subprocess.STDOUT)
        return b"Traceback" not in result

    def prompt_external_extension(self, c, name, prompt_text, path=None):
        # Ask the user if the specified extension should be enabled. Defaults
        # to treating the extension as one in version-control-tools/hgext/
        # in a directory with the same name as the extension and thus also
        # flagging the version-control-tools repo as needing an update.
        if name not in c.extensions:
            if not self.can_use_extension(c, name, path):
                return
            print(name)
            print('=' * len(name))
            print('')
            if not self._prompt_yn(prompt_text):
                print('')
                return
        if not path:
            # We replace the user's home directory with ~ so the
            # config file doesn't depend on the path to the home
            # directory
            path = os.path.join(self.vcs_tools_dir.replace(os.path.expanduser('~'), '~'), 'hgext', name)
        c.activate_extension(name, path)
        print('Activated %s extension.\n' % name)

    def _prompt(self, msg, allow_empty=False):
        print(msg)

        while True:
            response = raw_input().decode('utf-8')

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
