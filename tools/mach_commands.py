# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import sys
import os
import stat
import platform
import urllib2
import errno

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

from mozbuild.base import MachCommandBase


@CommandProvider
class SearchProvider(object):
    @Command('mxr', category='misc',
        description='Search for something in MXR.')
    @CommandArgument('term', nargs='+', help='Term(s) to search for.')
    def mxr(self, term):
        import webbrowser
        term = ' '.join(term)
        uri = 'https://mxr.mozilla.org/mozilla-central/search?string=%s' % term
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
        'This will open 3 new browser tabs and search for the term on Google, '
        'MDN, and MXR.')
    @CommandArgument('term', nargs='+', help='Term(s) to search for.')
    def search(self, term):
        self.google(term)
        self.mdn(term)
        self.mxr(term)


class Interface(object):
    '''
    Represents an XPIDL interface, in what file it is defined, what it derives
    from, what its uuid is, and where in the source file the uuid is.
    '''
    def __init__(self, filename, production):
        import xpidl
        assert isinstance(production, xpidl.Interface)
        self.name = production.name
        self.base = production.base
        self.filename = filename
        self.uuid = production.attributes.uuid
        location = production.location
        data = location._lexdata
        attr_pos = data.rfind(b'[', 0, location._lexpos)
        # uuid is always lowercase, but actual file content may not be.
        self.uuid_pos = data[attr_pos:location._lexpos].lower() \
                        .rfind(self.uuid) + attr_pos


class InterfaceRegistry(object):
    '''
    Tracks XPIDL interfaces, and allow to search them by name and by the
    interface they derive from.
    '''
    def __init__(self):
        self.by_name = {}
        self.by_base = {}

    def get_by_name(self, name):
        return self.by_name.get(name, [])

    def get_by_base(self, base):
        return self.by_base.get(base, [])

    def add(self, interface):
        l = self.by_name.setdefault(interface.name, [])
        l.append(interface)
        l = self.by_base.setdefault(interface.base, [])
        l.append(interface)


class IDLUpdater(object):
    '''
    Updates interfaces uuids in IDL files.
    '''
    def __init__(self, interfaces):
        from mozpack.copier import FileRegistry
        self.interfaces = interfaces;
        self.registry = FileRegistry()

    def add(self, name):
        for interface in self.interfaces.get_by_name(name):
            self._add(interface)

    def _add(self, interface):
        from mozpack.files import GeneratedFile
        from uuid import uuid4
        path = interface.filename
        if not self.registry.contains(path):
            self.registry.add(path, GeneratedFile(open(path).read()))
        content = self.registry[path].content
        content = content[:interface.uuid_pos] + str(uuid4()) + \
                  content[interface.uuid_pos + len(interface.uuid):]
        self.registry[path].content = content

        # Recurse through all the interfaces deriving from this one
        for derived in self.interfaces.get_by_base(interface.name):
            self._add(derived)

    def update(self):
        for p, f in self.registry:
            f.copy(p)


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

    @Command('update-uuids', category='misc',
        description='Update IDL files with new UUIDs.')
    @CommandArgument('--path', default='.',
                     help='Base path under which uuids will be searched.')
    @CommandArgument('interfaces', nargs='+',
                     help='Changed interfaces whose UUIDs need to be updated. ' +
                          'Their descendants are updated as well.')
    def update_uuids(self, path, interfaces):
        import os
        import xpidl
        from mozpack.files import FileFinder
        import mozpack.path as mozpath
        from tempfile import mkdtemp

        finder = FileFinder(path, find_executables=False)
        # Avoid creating xpidllex and xpidlyacc in the current directory.
        tmpdir = mkdtemp()
        try:
            parser = xpidl.IDLParser(outputdir=tmpdir)
            registry = InterfaceRegistry()
            for p, f in finder.find('**/*.idl'):
                p = mozpath.join(path, p)
                try:
                    content = f.open().read()
                    idl = parser.parse(content, filename=p)
                except Exception:
                    continue
                for prod in idl.productions:
                    if isinstance(prod, xpidl.Interface):
                         registry.add(Interface(p, prod))
        finally:
            import shutil
            shutil.rmtree(tmpdir)

        updates = IDLUpdater(registry)

        for interface in interfaces:
            updates.add(interface)

        updates.update()

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
        import sys
        import urllib

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
                      'HTTP Response Code %s' %(http_response_code))
        except urllib2.URLError:
            print('ERROR. Could not connect to pastebin.mozilla.org.')
            return 1
        return 0


@CommandProvider
class FormatProvider(MachCommandBase):
    @Command('clang-format', category='misc',
        description='Run clang-format on current changes')
    @CommandArgument('--show', '-s', action = 'store_true',
        help = 'Show diff output on instead of applying changes')
    def clang_format(self, show=False):
        plat = platform.system()
        fmt = plat.lower() + "/clang-format-3.5"
        fmt_diff = "clang-format-diff-3.5"

        # We are currently using a modified version of clang-format hosted on people.mozilla.org.
        # This is a temporary work around until we upstream the necessary changes and we can use
        # a system version of clang-format. See bug 961541.
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
            if not self.locate_or_fetch(fmt):
                return 1
            clang_format_diff = self.locate_or_fetch(fmt_diff)
            if not clang_format_diff:
                return 1

        except urllib2.HTTPError as e:
            print("HTTP error {0}: {1}".format(e.code, e.reason))
            return 1

        from subprocess import Popen, PIPE

        if os.path.exists(".hg"):
            diff_process = Popen(["hg", "diff", "-U0", "-r", "tip^",
                                  "--include", "glob:**.c", "--include", "glob:**.cpp", "--include", "glob:**.h",
                                  "--exclude", "listfile:.clang-format-ignore"], stdout=PIPE)
        else:
            git_process = Popen(["git", "diff", "-U0", "HEAD^"], stdout=PIPE)
            try:
                diff_process = Popen(["filterdiff", "--include=*.h", "--include=*.cpp",
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

    def locate_or_fetch(self, root):
        target = os.path.join(self._mach_context.state_dir, os.path.basename(root))
        if not os.path.exists(target):
            site = "https://people.mozilla.org/~ajones/clang-format/"
            if self.prompt and raw_input("Download clang-format executables from {0} (yN)? ".format(site)).lower() != 'y':
                print("Download aborted.")
                return 1
            self.prompt = False

            u = site + root
            print("Downloading {0} to {1}".format(u, target))
            data = urllib2.urlopen(url=u).read()
            temp = target + ".tmp"
            with open(temp, "wb") as fh:
                fh.write(data)
                fh.close()
            os.chmod(temp, os.stat(temp).st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
            os.rename(temp, target)
        return target

