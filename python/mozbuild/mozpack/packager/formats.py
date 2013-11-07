# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozpack.chrome.manifest import (
    Manifest,
    ManifestInterfaces,
    ManifestChrome,
    ManifestBinaryComponent,
    ManifestResource,
)
from urlparse import urlparse
import mozpack.path
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
    - add_base(path)
        Register a base directory for an application or GRE. Base directories
        usually contain a root manifest (manifests not included in any other
        manifest) named chrome.manifest.
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
        self._bases = ['']
        self._frozen_bases = False

    def add_base(self, base):
        # Only allow to add a base directory before calls to _get_base()
        assert not self._frozen_bases
        if not base in self._bases:
            self._bases.append(base)

    def _get_base(self, path):
        '''
        Return the deepest base directory containing the given path.
        '''
        self._frozen_bases = True
        return mozpack.path.basedir(path, self._bases)

    def add(self, path, content):
        self.copier.add(path, content)

    def add_manifest(self, entry):
        # Store manifest entries in a single manifest per directory, named
        # after their parent directory, except for root manifests, all named
        # chrome.manifest.
        base = self._get_base(entry.base)
        if entry.base == base:
            name = 'chrome'
        else:
            name = mozpack.path.basename(entry.base)
        path = mozpack.path.normpath(mozpack.path.join(entry.base,
                                                       '%s.manifest' % name))
        if not self.copier.contains(path):
            assert mozpack.path.basedir(entry.base, [base]) == base
            # Add a reference to the manifest file in the parent manifest, if
            # the manifest file is not a root manifest.
            if len(entry.base) > len(base):
                parent = mozpack.path.dirname(entry.base)
                relbase = mozpack.path.basename(entry.base)
                relpath = mozpack.path.join(relbase,
                                            mozpack.path.basename(path))
                FlatFormatter.add_manifest(self, Manifest(parent, relpath))
            self.copier.add(path, ManifestFile(entry.base))
        self.copier[path].add(entry)

    def add_interfaces(self, path, content):
        # Interfaces in the same directory are all linked together in an
        # interfaces.xpt file.
        interfaces_path = mozpack.path.join(mozpack.path.dirname(path),
                                            'interfaces.xpt')
        if not self.copier.contains(interfaces_path):
            FlatFormatter.add_manifest(self, ManifestInterfaces(
                mozpack.path.dirname(path), 'interfaces.xpt'))
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
        return mozpack.path.basedir(path, self._chrome)

    def add(self, path, content):
        chrome = self._chromepath(path)
        if chrome:
            jar = chrome + '.jar'
            if not self.copier.contains(jar):
                self.copier.add(jar, Jarrer(self._compress, self._optimize))
            if not self.copier[jar].contains(mozpack.path.relpath(path,
                                                                  chrome)):
                self.copier[jar].add(mozpack.path.relpath(path, chrome),
                                     content)
        else:
            FlatFormatter.add(self, path, content)

    def _jarize(self, entry, relpath):
        '''
        Transform a manifest entry in one pointing to chrome data in a jar.
        Return the corresponding chrome path and the new entry.
        '''
        base = entry.base
        basepath = mozpack.path.split(relpath)[0]
        chromepath = mozpack.path.join(base, basepath)
        entry = entry.rebase(chromepath) \
            .move(mozpack.path.join(base, 'jar:%s.jar!' % basepath)) \
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
            contains(mozpack.path.relpath(path, chrome))


class OmniJarFormatter(FlatFormatter):
    '''
    Formatter for the omnijar package format.
    '''
    def __init__(self, copier, omnijar_name, compress=True, optimize=True,
                 non_resources=[]):
        FlatFormatter.__init__(self, copier)
        self.omnijars = {}
        self._omnijar_name = omnijar_name
        self._compress = compress
        self._optimize = optimize
        self._non_resources = non_resources

    def _get_omnijar(self, path, create=True):
        '''
        Return the omnijar corresponding to the given path, its base directory
        and the path translated to be under the omnijar..
        '''
        base = self._get_base(path)
        if not base in self.omnijars:
            if not create:
                return None, '', path
            omnijar = Jarrer(self._compress, self._optimize)
            self.omnijars[base] = FlatFormatter(omnijar)
            self.copier.add(mozpack.path.join(base, self._omnijar_name),
                            omnijar)
        return self.omnijars[base], base, mozpack.path.relpath(path, base)

    def add(self, path, content):
        if self.is_resource(path):
            formatter, base, path = self._get_omnijar(path)
        else:
            formatter = self
        FlatFormatter.add(formatter, path, content)

    def add_manifest(self, entry):
        if isinstance(entry, ManifestBinaryComponent):
            formatter, base = self, ''
        else:
            formatter, base, path = self._get_omnijar(entry.base)
        entry = entry.move(mozpack.path.relpath(entry.base, base))
        FlatFormatter.add_manifest(formatter, entry)

    def add_interfaces(self, path, content):
        formatter, base, path = self._get_omnijar(path)
        FlatFormatter.add_interfaces(formatter, path, content)

    def contains(self, path):
        assert '*' not in path
        if self.copier.contains(path):
            return True
        for base, copier in self.omnijars.iteritems():
            if copier.contains(mozpack.path.relpath(path, base)):
                return True
        return False

    def is_resource(self, path):
        '''
        Return whether the given path corresponds to a resource to be put in an
        omnijar archive.
        '''
        base = self._get_base(path)
        path = mozpack.path.relpath(path, base)
        if any(mozpack.path.match(path, p.replace('*', '**'))
               for p in self._non_resources):
            return False
        path = mozpack.path.split(path)
        if path[0] == 'chrome':
            return len(path) == 1 or path[1] != 'icons'
        if path[0] == 'components':
            return path[-1].endswith('.js')
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
