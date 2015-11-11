# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from mozpack.chrome.manifest import (
    Manifest,
    ManifestInterfaces,
    ManifestChrome,
    ManifestBinaryComponent,
    ManifestResource,
)
from urlparse import urlparse
import mozpack.path as mozpath
from mozpack.files import (
    ManifestFile,
    XPTFile,
)
from mozpack.copier import (
    FileRegistry,
    Jarrer,
)

STARTUP_CACHE_PATHS = [
    'jsloader',
    'jssubloader',
]

'''
Formatters are classes receiving packaging instructions and creating the
appropriate package layout.

There are three distinct formatters, each handling one of the different chrome
formats:
    - flat: essentially, copies files from the source with the same file system
      layout. Manifests entries are grouped in a single manifest per directory,
      as well as XPT interfaces.
    - jar: chrome content is packaged in jar files.
    - omni: chrome content, modules, non-binary components, and many other
      elements are packaged in an omnijar file for each base directory.

The base interface provides the following methods:
    - add_base(path [, addon])
        Register a base directory for an application or GRE, or an addon.
        Base directories usually contain a root manifest (manifests not
        included in any other manifest) named chrome.manifest.
        The optional boolean addon argument tells whether the base directory
        is that of an addon.
    - add(path, content)
        Add the given content (BaseFile instance) at the given virtual path
    - add_interfaces(path, content)
        Add the given content (BaseFile instance) and link it to other
        interfaces in the parent directory of the given virtual path.
    - add_manifest(entry)
        Add a ManifestEntry.
    - contains(path)
        Returns whether the given virtual path is known of the formatter.

The virtual paths mentioned above are paths as they would be with a flat
chrome.

Formatters all take a FileCopier instance they will fill with the packaged
data.
'''


class FlatFormatter(object):
    '''
    Formatter for the flat package format.
    '''
    def __init__(self, copier):
        assert isinstance(copier, FileRegistry)
        self.copier = copier
        self._bases = []
        self._addons = []
        self._frozen_bases = False

    def add_base(self, base, addon=False):
        # Only allow to add a base directory before calls to _get_base()
        assert not self._frozen_bases
        if not base in self._bases:
            self._bases.append(base)
        if addon and base not in self._addons:
            self._addons.append(base)

    def _get_base(self, path):
        '''
        Return the deepest base directory containing the given path.
        '''
        self._frozen_bases = True
        return mozpath.basedir(path, self._bases)

    def add(self, path, content):
        self.copier.add(path, content)

    def add_manifest(self, entry):
        # Store manifest entries in a single manifest per directory, named
        # after their parent directory, except for root manifests, all named
        # chrome.manifest.
        base = self._get_base(entry.base)
        assert base is not None
        if entry.base == base:
            name = 'chrome'
        else:
            name = mozpath.basename(entry.base)
        path = mozpath.normpath(mozpath.join(entry.base,
                                                       '%s.manifest' % name))
        if not self.copier.contains(path):
            assert mozpath.basedir(entry.base, [base]) == base
            # Add a reference to the manifest file in the parent manifest, if
            # the manifest file is not a root manifest.
            if len(entry.base) > len(base):
                parent = mozpath.dirname(entry.base)
                relbase = mozpath.basename(entry.base)
                relpath = mozpath.join(relbase,
                                            mozpath.basename(path))
                FlatFormatter.add_manifest(self, Manifest(parent, relpath))
            self.copier.add(path, ManifestFile(entry.base))
        self.copier[path].add(entry)

    def add_interfaces(self, path, content):
        # Interfaces in the same directory are all linked together in an
        # interfaces.xpt file.
        interfaces_path = mozpath.join(mozpath.dirname(path),
                                            'interfaces.xpt')
        if not self.copier.contains(interfaces_path):
            FlatFormatter.add_manifest(self, ManifestInterfaces(
                mozpath.dirname(path), 'interfaces.xpt'))
            self.copier.add(interfaces_path, XPTFile())
        self.copier[interfaces_path].add(content)

    def contains(self, path):
        assert '*' not in path
        return self.copier.contains(path)


class JarFormatter(FlatFormatter):
    '''
    Formatter for the jar package format. Assumes manifest entries related to
    chrome are registered before the chrome data files are added. Also assumes
    manifest entries for resources are registered after chrome manifest
    entries.
    '''
    def __init__(self, copier, compress=True, optimize=True):
        FlatFormatter.__init__(self, copier)
        self._chrome = set()
        self._frozen_chrome = False
        self._compress = compress
        self._optimize = optimize

    def _chromepath(self, path):
        '''
        Return the chrome base directory under which the given path is. Used to
        detect under which .jar (if any) the path should go.
        '''
        self._frozen_chrome = True
        return mozpath.basedir(path, self._chrome)

    def add(self, path, content):
        chrome = self._chromepath(path)
        if chrome:
            jar = chrome + '.jar'
            if not self.copier.contains(jar):
                self.copier.add(jar, Jarrer(self._compress, self._optimize))
            if not self.copier[jar].contains(mozpath.relpath(path,
                                                                  chrome)):
                self.copier[jar].add(mozpath.relpath(path, chrome),
                                     content)
        else:
            FlatFormatter.add(self, path, content)

    def _jarize(self, entry, relpath):
        '''
        Transform a manifest entry in one pointing to chrome data in a jar.
        Return the corresponding chrome path and the new entry.
        '''
        base = entry.base
        basepath = mozpath.split(relpath)[0]
        chromepath = mozpath.join(base, basepath)
        entry = entry.rebase(chromepath) \
            .move(mozpath.join(base, 'jar:%s.jar!' % basepath)) \
            .rebase(base)
        return chromepath, entry

    def add_manifest(self, entry):
        if isinstance(entry, ManifestChrome) and \
                not urlparse(entry.relpath).scheme:
            chromepath, entry = self._jarize(entry, entry.relpath)
            assert not self._frozen_chrome
            self._chrome.add(chromepath)
        elif isinstance(entry, ManifestResource) and \
                not urlparse(entry.target).scheme:
            chromepath, new_entry = self._jarize(entry, entry.target)
            if chromepath in self._chrome:
                entry = new_entry
        FlatFormatter.add_manifest(self, entry)

    def contains(self, path):
        assert '*' not in path
        chrome = self._chromepath(path)
        if not chrome:
            return self.copier.contains(path)
        if not self.copier.contains(chrome + '.jar'):
            return False
        return self.copier[chrome + '.jar']. \
            contains(mozpath.relpath(path, chrome))


class OmniJarFormatter(JarFormatter):
    '''
    Formatter for the omnijar package format.
    '''
    def __init__(self, copier, omnijar_name, compress=True, optimize=True,
                 non_resources=[]):
        JarFormatter.__init__(self, copier, compress, optimize)
        self.omnijars = {}
        self._omnijar_name = omnijar_name
        self._non_resources = non_resources

    def _get_formatter(self, path, is_resource=None):
        '''
        Return the (sub)formatter corresponding to the given path, its base
        directory and the path relative to that base.
        '''
        base = self._get_base(path)
        use_omnijar = base not in self._addons
        if use_omnijar:
            if is_resource is None:
                is_resource = self.is_resource(path, base)
            use_omnijar = is_resource
        if not use_omnijar:
            return super(OmniJarFormatter, self), '', path
        if not base in self.omnijars:
            omnijar = Jarrer(self._compress, self._optimize)
            self.omnijars[base] = FlatFormatter(omnijar)
            self.omnijars[base].add_base('')
            self.copier.add(mozpath.join(base, self._omnijar_name),
                            omnijar)
        return self.omnijars[base], base, mozpath.relpath(path, base)

    def add(self, path, content):
        formatter, base, path = self._get_formatter(path)
        formatter.add(path, content)

    def add_manifest(self, entry):
        if isinstance(entry, ManifestBinaryComponent):
            formatter, base = super(OmniJarFormatter, self), ''
        else:
            formatter, base, path = self._get_formatter(entry.base,
                                                        is_resource=True)
        entry = entry.move(mozpath.relpath(entry.base, base))
        formatter.add_manifest(entry)

    def add_interfaces(self, path, content):
        formatter, base, path = self._get_formatter(path)
        formatter.add_interfaces(path, content)

    def contains(self, path):
        assert '*' not in path
        if JarFormatter.contains(self, path):
            return True
        for base, copier in self.omnijars.iteritems():
            if copier.contains(mozpath.relpath(path, base)):
                return True
        return False

    def is_resource(self, path, base=None):
        '''
        Return whether the given path corresponds to a resource to be put in an
        omnijar archive.
        '''
        if base is None:
            base = self._get_base(path)
        if base is None:
            return False
        path = mozpath.relpath(path, base)
        if any(mozpath.match(path, p.replace('*', '**'))
               for p in self._non_resources):
            return False
        path = mozpath.split(path)
        if path[0] == 'chrome':
            return len(path) == 1 or path[1] != 'icons'
        if path[0] == 'components':
            return path[-1].endswith(('.js', '.xpt'))
        if path[0] == 'res':
            return len(path) == 1 or \
                (path[1] != 'cursors' and path[1] != 'MainMenu.nib')
        if path[0] == 'defaults':
            return len(path) != 3 or \
                not (path[2] == 'channel-prefs.js' and
                     path[1] in ['pref', 'preferences'])
        return path[0] in [
            'modules',
            'greprefs.js',
            'hyphenation',
            'update.locale',
        ] or path[0] in STARTUP_CACHE_PATHS
