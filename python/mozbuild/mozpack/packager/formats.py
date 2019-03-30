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
    ManifestMultiContent,
)
from mozpack.errors import errors
from urlparse import urlparse
import mozpack.path as mozpath
from mozpack.files import ManifestFile
from mozpack.copier import (
    FileRegistry,
    FileRegistrySubtree,
    Jarrer,
)

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
        The optional addon argument tells whether the base directory
        is that of a packed addon (True), unpacked addon ('unpacked') or
        otherwise (False).
        The method may only be called in sorted order of `path` (alphanumeric
        order, parents before children).
    - add(path, content)
        Add the given content (BaseFile instance) at the given virtual path
    - add_interfaces(path, content)
        Add the given content (BaseFile instance) as an interface. Equivalent
        to add(path, content) with the right add_manifest().
    - add_manifest(entry)
        Add a ManifestEntry.
    - contains(path)
        Returns whether the given virtual path is known of the formatter.

The virtual paths mentioned above are paths as they would be with a flat
chrome.

Formatters all take a FileCopier instance they will fill with the packaged
data.
'''


class PiecemealFormatter(object):
    '''
    Generic formatter that dispatches across different sub-formatters
    according to paths.
    '''

    def __init__(self, copier):
        assert isinstance(copier, (FileRegistry, FileRegistrySubtree))
        self.copier = copier
        self._sub_formatter = {}
        self._frozen_bases = False

    def add_base(self, base, addon=False):
        # Only allow to add a base directory before calls to _get_base()
        assert not self._frozen_bases
        assert base not in self._sub_formatter
        assert all(base > b for b in self._sub_formatter)
        self._add_base(base, addon)

    def _get_base(self, path):
        '''
        Return the deepest base directory containing the given path.
        '''
        self._frozen_bases = True
        base = mozpath.basedir(path, self._sub_formatter.keys())
        relpath = mozpath.relpath(path, base) if base else path
        return base, relpath

    def add(self, path, content):
        base, relpath = self._get_base(path)
        if base is None:
            return self.copier.add(relpath, content)
        return self._sub_formatter[base].add(relpath, content)

    def add_manifest(self, entry):
        base, relpath = self._get_base(entry.base)
        assert base is not None
        return self._sub_formatter[base].add_manifest(entry.move(relpath))

    def add_interfaces(self, path, content):
        base, relpath = self._get_base(path)
        assert base is not None
        return self._sub_formatter[base].add_interfaces(relpath, content)

    def contains(self, path):
        assert '*' not in path
        base, relpath = self._get_base(path)
        if base is None:
            return self.copier.contains(relpath)
        return self._sub_formatter[base].contains(relpath)


class FlatFormatter(PiecemealFormatter):
    '''
    Formatter for the flat package format.
    '''

    def _add_base(self, base, addon=False):
        self._sub_formatter[base] = FlatSubFormatter(
            FileRegistrySubtree(base, self.copier))


class FlatSubFormatter(object):
    '''
    Sub-formatter for the flat package format.
    '''

    def __init__(self, copier):
        assert isinstance(copier, (FileRegistry, FileRegistrySubtree))
        self.copier = copier
        self._chrome_db = {}

    def add(self, path, content):
        self.copier.add(path, content)

    def add_manifest(self, entry):
        # Store manifest entries in a single manifest per directory, named
        # after their parent directory, except for root manifests, all named
        # chrome.manifest.
        if entry.base:
            name = mozpath.basename(entry.base)
        else:
            name = 'chrome'
        path = mozpath.normpath(mozpath.join(entry.base, '%s.manifest' % name))
        if not self.copier.contains(path):
            # Add a reference to the manifest file in the parent manifest, if
            # the manifest file is not a root manifest.
            if entry.base:
                parent = mozpath.dirname(entry.base)
                relbase = mozpath.basename(entry.base)
                relpath = mozpath.join(relbase,
                                       mozpath.basename(path))
                self.add_manifest(Manifest(parent, relpath))
            self.copier.add(path, ManifestFile(entry.base))

        if isinstance(entry, ManifestChrome):
            data = self._chrome_db.setdefault(entry.name, {})
            if isinstance(entry, ManifestMultiContent):
                entries = data.setdefault(entry.type, {}) \
                              .setdefault(entry.id, [])
            else:
                entries = data.setdefault(entry.type, [])
            for e in entries:
                # Ideally, we'd actually check whether entry.flags are more
                # specific than e.flags, but in practice the following test
                # is enough for now.
                if entry == e:
                    errors.warn('"%s" is duplicated. Skipping.' % entry)
                    return
                if not entry.flags or e.flags and entry.flags == e.flags:
                    errors.fatal('"%s" overrides "%s"' % (entry, e))
            entries.append(entry)

        self.copier[path].add(entry)

    def add_interfaces(self, path, content):
        self.copier.add(path, content)
        self.add_manifest(ManifestInterfaces(mozpath.dirname(path),
                                             mozpath.basename(path)))

    def contains(self, path):
        assert '*' not in path
        return self.copier.contains(path)


class JarFormatter(PiecemealFormatter):
    '''
    Formatter for the jar package format. Assumes manifest entries related to
    chrome are registered before the chrome data files are added. Also assumes
    manifest entries for resources are registered after chrome manifest
    entries.
    '''

    def __init__(self, copier, compress=True):
        PiecemealFormatter.__init__(self, copier)
        self._compress = compress

    def _add_base(self, base, addon=False):
        if addon is True:
            jarrer = Jarrer(self._compress)
            self.copier.add(base + '.xpi', jarrer)
            self._sub_formatter[base] = FlatSubFormatter(jarrer)
        else:
            self._sub_formatter[base] = JarSubFormatter(
                FileRegistrySubtree(base, self.copier),
                self._compress)


class JarSubFormatter(PiecemealFormatter):
    '''
    Sub-formatter for the jar package format. It is a PiecemealFormatter that
    dispatches between further sub-formatter for each of the jar files it
    dispatches the chrome data to, and a FlatSubFormatter for the non-chrome
    files.
    '''

    def __init__(self, copier, compress=True):
        PiecemealFormatter.__init__(self, copier)
        self._frozen_chrome = False
        self._compress = compress
        self._sub_formatter[''] = FlatSubFormatter(copier)

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
            if chromepath not in self._sub_formatter:
                jarrer = Jarrer(self._compress)
                self.copier.add(chromepath + '.jar', jarrer)
                self._sub_formatter[chromepath] = FlatSubFormatter(jarrer)
        elif isinstance(entry, ManifestResource) and \
                not urlparse(entry.target).scheme:
            chromepath, new_entry = self._jarize(entry, entry.target)
            if chromepath in self._sub_formatter:
                entry = new_entry
        PiecemealFormatter.add_manifest(self, entry)


class OmniJarFormatter(JarFormatter):
    '''
    Formatter for the omnijar package format.
    '''

    def __init__(self, copier, omnijar_name, compress=True, non_resources=()):
        JarFormatter.__init__(self, copier, compress)
        self._omnijar_name = omnijar_name
        self._non_resources = non_resources

    def _add_base(self, base, addon=False):
        if addon:
            # Because add_base is always called with parents before children,
            # all the possible ancestry of `base` is already present in
            # `_sub_formatter`.
            parent_base = mozpath.basedir(base, self._sub_formatter.keys())
            rel_base = mozpath.relpath(base, parent_base)
            # If the addon is under a resource directory, package it in the
            # omnijar.
            parent_sub_formatter = self._sub_formatter[parent_base]
            if parent_sub_formatter.is_resource(rel_base):
                omnijar_sub_formatter = \
                    parent_sub_formatter._sub_formatter[self._omnijar_name]
                self._sub_formatter[base] = FlatSubFormatter(
                    FileRegistrySubtree(rel_base, omnijar_sub_formatter.copier))
                return
            JarFormatter._add_base(self, base, addon)
        else:
            # Initialize a chrome.manifest next to the omnijar file so that
            # there's always a chrome.manifest file, even an empty one.
            path = mozpath.normpath(mozpath.join(base, 'chrome.manifest'))
            if not self.copier.contains(path):
                self.copier.add(path, ManifestFile(''))
            self._sub_formatter[base] = OmniJarSubFormatter(
                FileRegistrySubtree(base, self.copier), self._omnijar_name,
                self._compress, self._non_resources)


class OmniJarSubFormatter(PiecemealFormatter):
    '''
    Sub-formatter for the omnijar package format. It is a PiecemealFormatter
    that dispatches between a FlatSubFormatter for the resources data and
    another FlatSubFormatter for the other files.
    '''

    def __init__(self, copier, omnijar_name, compress=True, non_resources=()):
        PiecemealFormatter.__init__(self, copier)
        self._omnijar_name = omnijar_name
        self._compress = compress
        self._non_resources = non_resources
        self._sub_formatter[''] = FlatSubFormatter(copier)
        jarrer = Jarrer(self._compress)
        self._sub_formatter[omnijar_name] = FlatSubFormatter(jarrer)

    def _get_base(self, path):
        base = self._omnijar_name if self.is_resource(path) else ''
        # Only add the omnijar file if something ends up in it.
        if base and not self.copier.contains(base):
            self.copier.add(base, self._sub_formatter[base].copier)
        return base, path

    def add_manifest(self, entry):
        base = ''
        if not isinstance(entry, ManifestBinaryComponent):
            base = self._omnijar_name
        formatter = self._sub_formatter[base]
        return formatter.add_manifest(entry)

    def is_resource(self, path):
        '''
        Return whether the given path corresponds to a resource to be put in an
        omnijar archive.
        '''
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
                (path[1] != 'cursors' and
                 path[1] != 'touchbar' and
                 path[1] != 'MainMenu.nib')
        if path[0] == 'defaults':
            return len(path) != 3 or \
                not (path[2] == 'channel-prefs.js' and
                     path[1] in ['pref', 'preferences'])
        return path[0] in [
            'modules',
            'actors',
            'dictionaries',
            'greprefs.js',
            'hyphenation',
            'localization',
            'update.locale',
            'contentaccessible',
        ]
