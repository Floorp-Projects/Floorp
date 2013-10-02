# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)


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
        uri = 'http://dxr.mozilla.org/search?tree=mozilla-central&q=%s' % term
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
        import mozpack.path
        from tempfile import mkdtemp

        finder = FileFinder(path, find_executables=False)
        # Avoid creating xpidllex and xpidlyacc in the current directory.
        tmpdir = mkdtemp()
        try:
            parser = xpidl.IDLParser(outputdir=tmpdir)
            registry = InterfaceRegistry()
            for p, f in finder.find('**/*.idl'):
                p = mozpack.path.join(path, p)
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
