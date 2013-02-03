# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozpack.path
from mozpack.files import (
    FileFinder,
    DeflatedFile,
    ManifestFile,
)
from mozpack.chrome.manifest import (
    parse_manifest,
    ManifestEntryWithRelPath,
    ManifestResource,
    is_manifest,
)
from mozpack.mozjar import JarReader
from mozpack.copier import (
    FileRegistry,
    FileCopier,
)
from mozpack.packager import SimplePackager
from mozpack.packager.formats import (
    FlatFormatter,
    STARTUP_CACHE_PATHS,
)
from urlparse import urlparse


class UnpackFinder(FileFinder):
    '''
    Special FileFinder that treats the source package directory as if it were
    in the flat chrome format, whatever chrome format it actually is in.

    This means that for example, paths like chrome/browser/content/... match
    files under jar:chrome/browser.jar!/content/... in case of jar chrome
    format.
    '''
    def __init__(self, *args, **kargs):
        FileFinder.__init__(self, *args, **kargs)
        self.files = FileRegistry()
        self.kind = 'flat'
        self.omnijar = None
        self.jarlogs = {}
        self.optimizedjars = False

        jars = set()

        for p, f in FileFinder.find(self, '*'):
            # Skip the precomplete file, which is generated at packaging time.
            if p == 'precomplete':
                continue
            base = mozpack.path.dirname(p)
            # If the file is a zip/jar that is not a .xpi, and contains a
            # chrome.manifest, it is an omnijar. All the files it contains
            # go in the directory containing the omnijar. Manifests are merged
            # if there is a corresponding manifest in the directory.
            if not p.endswith('.xpi') and self._maybe_zip(f) and \
                    (mozpack.path.basename(p) == self.omnijar or
                     not self.omnijar):
                jar = self._open_jar(p, f)
                if 'chrome.manifest' in jar:
                    self.kind = 'omni'
                    self.omnijar = mozpack.path.basename(p)
                    self._fill_with_omnijar(base, jar)
                    continue
            # If the file is a manifest, scan its entries for some referencing
            # jar: urls. If there are some, the files contained in the jar they
            # point to, go under a directory named after the jar.
            if is_manifest(p):
                m = self.files[p] if self.files.contains(p) \
                    else ManifestFile(base)
                for e in parse_manifest(self.base, p, f.open()):
                    m.add(self._handle_manifest_entry(e, jars))
                if self.files.contains(p):
                    continue
                f = m
            if not p in jars:
                self.files.add(p, f)

    def _fill_with_omnijar(self, base, jar):
        for j in jar:
            path = mozpack.path.join(base, j.filename)
            if is_manifest(j.filename):
                m = self.files[path] if self.files.contains(path) \
                    else ManifestFile(mozpack.path.dirname(path))
                for e in parse_manifest(None, path, j):
                    m.add(e)
                if not self.files.contains(path):
                    self.files.add(path, m)
                continue
            else:
                self.files.add(path, DeflatedFile(j))

    def _handle_manifest_entry(self, entry, jars):
        jarpath = None
        if isinstance(entry, ManifestEntryWithRelPath) and \
                urlparse(entry.relpath).scheme == 'jar':
            jarpath, entry = self._unjarize(entry, entry.relpath)
        elif isinstance(entry, ManifestResource) and \
                urlparse(entry.target).scheme == 'jar':
            jarpath, entry = self._unjarize(entry, entry.target)
        if jarpath:
            # Don't defer unpacking the jar file. If we already saw
            # it, take (and remove) it from the registry. If we
            # haven't, try to find it now.
            if self.files.contains(jarpath):
                jar = self.files[jarpath]
                self.files.remove(jarpath)
            else:
                jar = [f for p, f in FileFinder.find(self, jarpath)]
                assert len(jar) == 1
                jar = jar[0]
            if not jarpath in jars:
                base = mozpack.path.splitext(jarpath)[0]
                for j in self._open_jar(jarpath, jar):
                    self.files.add(mozpack.path.join(base,
                                                     j.filename),
                                   DeflatedFile(j))
            jars.add(jarpath)
            self.kind = 'jar'
        return entry

    def _open_jar(self, path, file):
        '''
        Return a JarReader for the given BaseFile instance, keeping a log of
        the preloaded entries it has.
        '''
        jar = JarReader(fileobj=file.open())
        if jar.is_optimized:
            self.optimizedjars = True
        if jar.last_preloaded:
            jarlog = jar.entries.keys()
            self.jarlogs[path] = jarlog[:jarlog.index(jar.last_preloaded) + 1]
        return jar

    def find(self, path):
        for p in self.files.match(path):
            yield p, self.files[p]

    def _maybe_zip(self, file):
        '''
        Return whether the given BaseFile looks like a ZIP/Jar.
        '''
        header = file.open().read(8)
        return len(header) == 8 and (header[0:2] == 'PK' or
                                     header[4:6] == 'PK')

    def _unjarize(self, entry, relpath):
        '''
        Transform a manifest entry pointing to chrome data in a jar in one
        pointing to the corresponding unpacked path. Return the jar path and
        the new entry.
        '''
        base = entry.base
        jar, relpath = urlparse(relpath).path.split('!', 1)
        entry = entry.rebase(mozpack.path.join(base, 'jar:%s!' % jar)) \
            .move(mozpack.path.join(base, mozpack.path.splitext(jar)[0])) \
            .rebase(base)
        return mozpack.path.join(base, jar), entry


def unpack(source):
    '''
    Transform a jar chrome or omnijar packaged directory into a flat package.
    '''
    copier = FileCopier()
    finder = UnpackFinder(source)
    packager = SimplePackager(FlatFormatter(copier))
    for p, f in finder.find('*'):
        if mozpack.path.split(p)[0] not in STARTUP_CACHE_PATHS:
            packager.add(p, f)
    packager.close()
    copier.copy(source, skip_if_older=False)
