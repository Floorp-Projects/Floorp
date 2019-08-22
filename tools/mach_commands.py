# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

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


MACH_PASTEBIN_DURATIONS = {
    'onetime': 'onetime',
    'hour': '3600',
    'day': '86400',
    'week': '604800',
    'month': '2073600',
}

EXTENSION_TO_HIGHLIGHTER = {
    '.hgrc': 'ini',
    'Dockerfile': 'docker',
    'Makefile': 'make',
    'applescript': 'applescript',
    'arduino': 'arduino',
    'bash': 'bash',
    'bat': 'bat',
    'c': 'c',
    'clojure': 'clojure',
    'cmake': 'cmake',
    'coffee': 'coffee-script',
    'console': 'console',
    'cpp': 'cpp',
    'cs': 'csharp',
    'css': 'css',
    'cu': 'cuda',
    'cuda': 'cuda',
    'dart': 'dart',
    'delphi': 'delphi',
    'diff': 'diff',
    'django': 'django',
    'docker': 'docker',
    'elixir': 'elixir',
    'erlang': 'erlang',
    'go': 'go',
    'h': 'c',
    'handlebars': 'handlebars',
    'haskell': 'haskell',
    'hs': 'haskell',
    'html': 'html',
    'ini': 'ini',
    'ipy': 'ipythonconsole',
    'ipynb': 'ipythonconsole',
    'irc': 'irc',
    'j2': 'django',
    'java': 'java',
    'js': 'js',
    'json': 'json',
    'jsx': 'jsx',
    'kt': 'kotlin',
    'less': 'less',
    'lisp': 'common-lisp',
    'lsp': 'common-lisp',
    'lua': 'lua',
    'm': 'objective-c',
    'make': 'make',
    'matlab': 'matlab',
    'md': '_markdown',
    'nginx': 'nginx',
    'numpy': 'numpy',
    'patch': 'diff',
    'perl': 'perl',
    'php': 'php',
    'pm': 'perl',
    'postgresql': 'postgresql',
    'py': 'python',
    'rb': 'rb',
    'rs': 'rust',
    'rst': 'rst',
    'sass': 'sass',
    'scss': 'scss',
    'sh': 'bash',
    'sol': 'sol',
    'sql': 'sql',
    'swift': 'swift',
    'tex': 'tex',
    'typoscript': 'typoscript',
    'vim': 'vim',
    'xml': 'xml',
    'xslt': 'xslt',
    'yaml': 'yaml',
    'yml': 'yaml'
}


def guess_highlighter_from_path(path):
    '''Return a known highlighter from a given path

    Attempt to select a highlighter by checking the file extension in the mapping
    of extensions to highlighter. If that fails, attempt to pass the basename of
    the file. Return `_code` as the default highlighter if that fails.
    '''
    import os

    _name, ext = os.path.splitext(path)

    if ext.startswith('.'):
        ext = ext[1:]

    if ext in EXTENSION_TO_HIGHLIGHTER:
        return EXTENSION_TO_HIGHLIGHTER[ext]

    basename = os.path.basename(path)

    return EXTENSION_TO_HIGHLIGHTER.get(basename, '_code')


PASTEMO_MAX_CONTENT_LENGTH = 250 * 1024 * 1024

PASTEMO_URL = 'https://paste.mozilla.org/api/'

MACH_PASTEBIN_DESCRIPTION = '''
Command line interface to paste.mozilla.org.

Takes either a filename whose content should be pasted, or reads
content from standard input. If a highlighter is specified it will
be used, otherwise the file name will be used to determine an
appropriate highlighter.
'''


@CommandProvider
class PastebinProvider(object):
    @Command('pastebin', category='misc',
             description=MACH_PASTEBIN_DESCRIPTION)
    @CommandArgument('--list-highlighters', action='store_true',
                     help='List known highlighters and exit')
    @CommandArgument('--highlighter', default=None,
                     help='Syntax highlighting to use for paste')
    @CommandArgument('--expires', default='week',
                     choices=sorted(MACH_PASTEBIN_DURATIONS.keys()),
                     help='Expire paste after given time duration (default: %(default)s)')
    @CommandArgument('--verbose', action='store_true',
                     help='Print extra info such as selected syntax highlighter')
    @CommandArgument('path', nargs='?', default=None,
                     help='Path to file for upload to paste.mozilla.org')
    def pastebin(self, list_highlighters, highlighter, expires, verbose, path):
        import requests

        def verbose_print(*args, **kwargs):
            '''Print a string if `--verbose` flag is set'''
            if verbose:
                print(*args, **kwargs)

        # Show known highlighters and exit.
        if list_highlighters:
            lexers = set(EXTENSION_TO_HIGHLIGHTER.values())
            print('Available lexers:\n'
                  '    - %s' % '\n    - '.join(sorted(lexers)))
            return 0

        # Get a correct expiry value.
        try:
            verbose_print('Setting expiry from %s' % expires)
            expires = MACH_PASTEBIN_DURATIONS[expires]
            verbose_print('Using %s as expiry' % expires)
        except KeyError:
            print('%s is not a valid duration.\n'
                  '(hint: try one of %s)' %
                  (expires, ', '.join(MACH_PASTEBIN_DURATIONS.keys())))
            return 1

        data = {
            'format': 'json',
            'expires': expires,
        }

        # Get content to be pasted.
        if path:
            verbose_print('Reading content from %s' % path)
            try:
                with open(path, 'r') as f:
                    content = f.read()
            except IOError:
                print('ERROR. No such file %s' % path)
                return 1

            lexer = guess_highlighter_from_path(path)
            if lexer:
                data['lexer'] = lexer
        else:
            verbose_print('Reading content from stdin')
            content = sys.stdin.read()

        # Assert the length of content to be posted does not exceed the maximum.
        content_length = len(content)
        verbose_print('Checking size of content is okay (%d)' % content_length)
        if content_length > PASTEMO_MAX_CONTENT_LENGTH:
            print('Paste content is too large (%d, maximum %d)' %
                  (content_length, PASTEMO_MAX_CONTENT_LENGTH))
            return 1

        data['content'] = content

        # Highlight as specified language, overwriting value set from filename.
        if highlighter:
            verbose_print('Setting %s as highlighter' % highlighter)
            data['lexer'] = highlighter

        try:
            verbose_print('Sending request to %s' % PASTEMO_URL)
            resp = requests.post(PASTEMO_URL, data=data)

            # Error code should always be 400.
            # Response content will include a helpful error message,
            # so print it here (for example, if an invalid highlighter is
            # provided, it will return a list of valid highlighters).
            if resp.status_code >= 400:
                print('Error code %d: %s' % (resp.status_code, resp.content))
                return 1

            verbose_print('Pasted successfully')

            response_json = resp.json()

            verbose_print('Paste highlighted as %s' % response_json['lexer'])
            print(response_json['url'])

            return 0
        except Exception as e:
            print('ERROR. Paste failed.')
            print('%s' % e)
        return 1


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
