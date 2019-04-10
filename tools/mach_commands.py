# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import sys

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
    SubCommand,
)

from mozbuild.base import MachCommandBase, MozbuildObject


@CommandProvider
class BustedProvider(object):
    @Command('busted', category='misc',
             description='Query known bugs in our tooling, and file new ones.')
    def busted_default(self):
        import requests
        payload = {'include_fields': 'id,summary,last_change_time',
                   'blocks': 1543241,
                   'resolution': '---'}
        response = requests.get('https://bugzilla.mozilla.org/rest/bug', payload)
        response.raise_for_status()
        json_response = response.json()
        if 'bugs' in json_response and len(json_response['bugs']) > 0:
            # Display most recently modifed bugs first.
            bugs = sorted(json_response['bugs'], key=lambda item: item['last_change_time'],
                          reverse=True)
            for bug in bugs:
                print("Bug %s - %s" % (bug['id'], bug['summary']))
        else:
            print("No known tooling issues found.")

    @SubCommand('busted',
                'file',
                description='File a bug for busted tooling.')
    def busted_file(self):
        import webbrowser
        uri = ('https://bugzilla.mozilla.org/enter_bug.cgi?'
               'product=Firefox%20Build%20System&component=General&blocked=1543241')
        webbrowser.open_new_tab(uri)


@CommandProvider
class SearchProvider(object):
    @Command('searchfox', category='misc',
             description='Search for something in Searchfox.')
    @CommandArgument('term', nargs='+', help='Term(s) to search for.')
    def searchfox(self, term):
        import webbrowser
        term = ' '.join(term)
        uri = 'https://searchfox.org/mozilla-central/search?q=%s' % term
        webbrowser.open_new_tab(uri)
    @Command('dxr', category='misc',
             description='Search for something in DXR.')
    @CommandArgument('term', nargs='+', help='Term(s) to search for.')
    def dxr(self, term):
        import webbrowser
        term = ' '.join(term)
        uri = 'http://dxr.mozilla.org/mozilla-central/search?q=%s&redirect=true' % term
        webbrowser.open_new_tab(uri)

    @Command('mdn', category='misc',
             description='Search for something on MDN.')
    @CommandArgument('term', nargs='+', help='Term(s) to search for.')
    def mdn(self, term):
        import webbrowser
        term = ' '.join(term)
        uri = 'https://developer.mozilla.org/search?q=%s' % term
        webbrowser.open_new_tab(uri)

    @Command('google', category='misc',
             description='Search for something on Google.')
    @CommandArgument('term', nargs='+', help='Term(s) to search for.')
    def google(self, term):
        import webbrowser
        term = ' '.join(term)
        uri = 'https://www.google.com/search?q=%s' % term
        webbrowser.open_new_tab(uri)

    @Command('search', category='misc',
             description='Search for something on the Internets. '
             'This will open 4 new browser tabs and search for the term on Google, '
             'MDN, DXR, and Searchfox.')
    @CommandArgument('term', nargs='+', help='Term(s) to search for.')
    def search(self, term):
        self.google(term)
        self.mdn(term)
        self.dxr(term)
        self.searchfox(term)


@CommandProvider
class UUIDProvider(object):
    @Command('uuid', category='misc',
             description='Generate a uuid.')
    @CommandArgument('--format', '-f', choices=['idl', 'cpp', 'c++'],
                     help='Output format for the generated uuid.')
    def uuid(self, format=None):
        import uuid
        u = uuid.uuid4()
        if format in [None, 'idl']:
            print(u)
            if format is None:
                print('')
        if format in [None, 'cpp', 'c++']:
            u = u.hex
            print('{ 0x%s, 0x%s, 0x%s, \\' % (u[0:8], u[8:12], u[12:16]))
            pairs = tuple(map(lambda n: u[n:n+2], range(16, 32, 2)))
            print(('  { ' + '0x%s, ' * 7 + '0x%s } }') % pairs)


def REMOVED(cls):
    """Command no longer exists!

    This functionality is no longer supported in mach.
    """
    return False


@CommandProvider
class PastebinProvider(object):
    @Command('pastebin', category='misc', conditions=[REMOVED])
    def pastebin(self):
        pass


def mozregression_import():
    # Lazy loading of mozregression.
    # Note that only the mach_interface module should be used from this file.
    try:
        import mozregression.mach_interface
    except ImportError:
        return None
    return mozregression.mach_interface


def mozregression_create_parser():
    # Create the mozregression command line parser.
    # if mozregression is not installed, or not up to date, it will
    # first be installed.
    cmd = MozbuildObject.from_environment()
    cmd._activate_virtualenv()
    mozregression = mozregression_import()
    if not mozregression:
        # mozregression is not here at all, install it
        cmd.virtualenv_manager.install_pip_package('mozregression')
        print("mozregression was installed. please re-run your"
              " command. If you keep getting this message please "
              " manually run: 'pip install -U mozregression'.")
    else:
        # check if there is a new release available
        release = mozregression.new_release_on_pypi()
        if release:
            print(release)
            # there is one, so install it. Note that install_pip_package
            # does not work here, so just run pip directly.
            cmd.virtualenv_manager._run_pip([
                'install',
                'mozregression==%s' % release
            ])
            print("mozregression was updated to version %s. please"
                  " re-run your command." % release)
        else:
            # mozregression is up to date, return the parser.
            return mozregression.parser()
    # exit if we updated or installed mozregression because
    # we may have already imported mozregression and running it
    # as this may cause issues.
    sys.exit(0)


@CommandProvider
class MozregressionCommand(MachCommandBase):
    @Command('mozregression',
             category='misc',
             description=("Regression range finder for nightly"
                          " and inbound builds."),
             parser=mozregression_create_parser)
    def run(self, **options):
        self._activate_virtualenv()
        mozregression = mozregression_import()
        mozregression.run(options)
