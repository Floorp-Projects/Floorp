# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import sys
import os
import stat
import platform
import errno

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
                print(response.geturl())
            else:
                print('Could not upload the file, '
                      'HTTP Response Code %s' % (http_response_code))
        except urllib2.URLError:
            print('ERROR. Could not connect to pastebin.mozilla.org.')
            return 1
        return 0


@CommandProvider
class FormatProvider(MachCommandBase):
    @Command('clang-format', category='misc',
             description='Run clang-format on current changes')
    @CommandArgument('--show', '-s', action='store_true', default=False,
                     help='Show diff output on instead of applying changes')
    @CommandArgument('--path', '-p', nargs='+', default=None,
                     help='Specify the path(s) to reformat')
    def clang_format(self, show, path):
        # Run clang-format or clang-format-diff on the local changes
        # or files/directories
        import urllib2

        plat = platform.system()
        fmt = plat.lower() + "/clang-format-5.0~svn297730"
        fmt_diff = "clang-format-diff-5.0~svn297730"

        # We are currently using an unmodified snapshot of upstream clang-format.
        # This is a temporary work around until clang 5.0 has been released with our changes.
        if plat == "Windows":
            fmt += ".exe"
        else:
            arch = os.uname()[4]
            if (plat != "Linux" and plat != "Darwin") or arch != 'x86_64':
                print("Unsupported platform " + plat + "/" + arch +
                      ". Supported platforms are Windows/*, Linux/x86_64 and Darwin/x86_64")
                return 1

        os.chdir(self.topsrcdir)
        self.prompt = True

        try:
            clang_format = self.locate_or_fetch(fmt)
            if not clang_format:
                return 1
            clang_format_diff = self.locate_or_fetch(fmt_diff, python_script=True)
            if not clang_format_diff:
                return 1

        except urllib2.HTTPError as e:
            print("HTTP error {0}: {1}".format(e.code, e.reason))
            return 1

        if path is None:
            return self.run_clang_format_diff(clang_format_diff, show)
        else:
            return self.run_clang_format_path(clang_format, show, path)

    def locate_or_fetch(self, root, python_script=False):
        # Download the clang-format binary & python clang-format-diff if doesn't
        # exists
        import urllib2
        import hashlib
        bin_sha = {
            "Windows": "0cbfc306df48f01bfe804e5e89cef73b3abe8f884fb7a5208f8895897f19ec45c13760787298192bd37de057d0ded091640c7d504438e06ec880f071a38db89c",  # noqa: E501
            "Linux": "e6da4f6df074bfb15caefcf7767eb5670c02bb4768ba86ae4ab6b35235b53db012900a4f9e9a950ee140158a19532a71f21b986f511826bebc16f2ef83984e57",  # noqa: E501
            "Darwin": "18000940a11e5ab0c1fe950d4360292216c8e963dd708679c4c5fb8cc845f5919cef3f58a7e092555b8ea6b8d8a809d66153ea6d1e7c226a2c4f2b0b7ad1b2f3",  # noqa: E501
            "python_script": "34b6934a48a263ea3f88d48c2981d61ae6698823cfa689b9b0c8a607c224437ca0b9fdd434d260bd790d52a98455e2c2e2c745490d327ba84b4e22b7bb55b757",  # noqa: E501
        }

        target = os.path.join(self._mach_context.state_dir, os.path.basename(root))

        if not os.path.exists(target):
            tooltool_url = "https://api.pub.build.mozilla.org/tooltool/sha512/"
            if self.prompt and raw_input("Download clang-format executables from {0} (yN)? ".format(tooltool_url)).lower() != 'y':  # noqa: E501,F821
                print("Download aborted.")
                return None
            self.prompt = False
            plat = platform.system()
            if python_script:
                # We want to download the python script (clang-format-diff)
                dl = bin_sha["python_script"]
            else:
                dl = bin_sha[plat]
            u = tooltool_url + dl
            print("Downloading {0} to {1}".format(u, target))
            data = urllib2.urlopen(url=u).read()
            temp = target + ".tmp"
            # Check that the checksum of the downloaded data matches the hash
            # of the file
            sha512Hash = hashlib.sha512(data).hexdigest()
            if sha512Hash != dl:
                print("Checksum verification for {0} failed: {1} found instead of {2} ".format(target, sha512Hash, dl))  # noqa: E501
                return 1
            with open(temp, "wb") as fh:
                fh.write(data)
                fh.close()
            os.chmod(temp, os.stat(temp).st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
            os.rename(temp, target)
        return target

    def run_clang_format_diff(self, clang_format_diff, show):
        # Run clang-format on the diff
        # Note that this will potentially miss a lot things
        from subprocess import Popen, PIPE

        if os.path.exists(".hg"):
            diff_process = Popen(["hg", "diff", "-U0", "-r", ".^",
                                  "--include", "glob:**.c", "--include", "glob:**.cpp",
                                  "--include", "glob:**.h",
                                  "--exclude", "listfile:.clang-format-ignore"], stdout=PIPE)
        else:
            git_process = Popen(["git", "diff", "--no-color", "-U0", "HEAD^"], stdout=PIPE)
            try:
                diff_process = Popen(["filterdiff", "--include=*.h",
                                      "--include=*.cpp", "--include=*.c",
                                      "--exclude-from-file=.clang-format-ignore"],
                                     stdin=git_process.stdout, stdout=PIPE)
            except OSError as e:
                if e.errno == errno.ENOENT:
                    print("Can't find filterdiff. Please install patchutils.")
                else:
                    print("OSError {0}: {1}".format(e.code, e.reason))
                return 1

        args = [sys.executable, clang_format_diff, "-p1"]
        if not show:
            args.append("-i")
        cf_process = Popen(args, stdin=diff_process.stdout)
        return cf_process.communicate()[0]

    def generate_path_list(self, paths):
        pathToThirdparty = os.path.join(self.topsrcdir,
                                        "tools",
                                        "rewriting",
                                        "ThirdPartyPaths.txt")
        with open(pathToThirdparty) as f:
            # Normalize the path (no trailing /)
            ignored_dir = tuple(d.rstrip('/') for d in f.read().splitlines())

        extensions = ('.cpp', '.c', '.h')

        path_list = []
        for f in paths:
            if f.startswith(ignored_dir):
                print("clang-format: Ignored third party code '{0}'".format(f))
                continue

            if os.path.isdir(f):
                # Processing a directory, generate the file list
                for folder, subs, files in os.walk(f):
                    subs.sort()
                    for filename in sorted(files):
                        f_in_dir = os.path.join(folder, filename)
                        if f_in_dir.endswith(extensions):
                            # Supported extension
                            path_list.append(f_in_dir)
            else:
                if f.endswith(extensions):
                    path_list.append(f)

        return path_list

    def run_clang_format_path(self, clang_format, show, paths):
        # Run clang-format on files or directories directly
        from subprocess import Popen

        args = [clang_format, "-i"]

        path_list = self.generate_path_list(paths)

        if path_list == []:
            return

        args += path_list

        # Run clang-format
        cf_process = Popen(args)
        if show:
            # show the diff
            if os.path.exists(".hg"):
                cf_process = Popen(["hg", "diff"] + path_list)
            else:
                cf_process = Popen(["git", "diff"] + path_list)
        return cf_process.communicate()[0]


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
