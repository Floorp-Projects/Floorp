# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import sys

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

from mozbuild.base import MachCommandBase, MozbuildObject


@CommandProvider
class SearchProvider(object):
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
             'This will open 3 new browser tabs and search for the term on Google, '
             'MDN, and DXR.')
    @CommandArgument('term', nargs='+', help='Term(s) to search for.')
    def search(self, term):
        self.google(term)
        self.mdn(term)
        self.dxr(term)


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


@CommandProvider
class PastebinProvider(object):
    @Command('pastebin', category='misc',
             description='Command line interface to pastebin.mozilla.org.')
    @CommandArgument('--language', default=None,
                     help='Language to use for syntax highlighting')
    @CommandArgument('--poster', default='',
                     help='Specify your name for use with pastebin.mozilla.org')
    @CommandArgument('--duration', default='day',
                     choices=['d', 'day', 'm', 'month', 'f', 'forever'],
                     help='Keep for specified duration (default: %(default)s)')
    @CommandArgument('file', nargs='?', default=None,
                     help='Specify the file to upload to pastebin.mozilla.org')
    def pastebin(self, language, poster, duration, file):
        import urllib
        import urllib2

        URL = 'https://pastebin.mozilla.org/'

        FILE_TYPES = [{'value': 'text', 'name': 'None', 'extension': 'txt'},
                      {'value': 'bash', 'name': 'Bash', 'extension': 'sh'},
                      {'value': 'c', 'name': 'C', 'extension': 'c'},
                      {'value': 'cpp', 'name': 'C++', 'extension': 'cpp'},
                      {'value': 'html4strict', 'name': 'HTML', 'extension': 'html'},
                      {'value': 'javascript', 'name': 'Javascript', 'extension': 'js'},
                      {'value': 'javascript', 'name': 'Javascript', 'extension': 'jsm'},
                      {'value': 'lua', 'name': 'Lua', 'extension': 'lua'},
                      {'value': 'perl', 'name': 'Perl', 'extension': 'pl'},
                      {'value': 'php', 'name': 'PHP', 'extension': 'php'},
                      {'value': 'python', 'name': 'Python', 'extension': 'py'},
                      {'value': 'ruby', 'name': 'Ruby', 'extension': 'rb'},
                      {'value': 'css', 'name': 'CSS', 'extension': 'css'},
                      {'value': 'diff', 'name': 'Diff', 'extension': 'diff'},
                      {'value': 'ini', 'name': 'INI file', 'extension': 'ini'},
                      {'value': 'java', 'name': 'Java', 'extension': 'java'},
                      {'value': 'xml', 'name': 'XML', 'extension': 'xml'},
                      {'value': 'xml', 'name': 'XML', 'extension': 'xul'}]

        lang = ''

        if file:
            try:
                with open(file, 'r') as f:
                    content = f.read()
                # TODO: Use mime-types instead of extensions; suprocess('file <f_name>')
                # Guess File-type based on file extension
                extension = file.split('.')[-1]
                for l in FILE_TYPES:
                    if extension == l['extension']:
                        print('Identified file as %s' % l['name'])
                        lang = l['value']
            except IOError:
                print('ERROR. No such file')
                return 1
        else:
            content = sys.stdin.read()
        duration = duration[0]

        if language:
            lang = language

        params = [
            ('parent_pid', ''),
            ('format', lang),
            ('code2', content),
            ('poster', poster),
            ('expiry', duration),
            ('paste', 'Send')]

        data = urllib.urlencode(params)
        print('Uploading ...')
        try:
            req = urllib2.Request(URL, data)
            response = urllib2.urlopen(req)
            http_response_code = response.getcode()
            if http_response_code == 200:
                pasteurl = response.geturl()
                if pasteurl == URL:
                    if "Query failure: Data too long for column" in response.readline():
                        print('ERROR. Request too large. Limit is 64KB.')
                    else:
                        print('ERROR. Unknown error')
                else:
                    print(pasteurl)
            else:
                print('Could not upload the file, '
                      'HTTP Response Code %s' % (http_response_code))
        except urllib2.URLError:
            print('ERROR. Could not connect to pastebin.mozilla.org.')
            return 1
        return 0


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
