# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import codecs

from six.moves.urllib.parse import urlparse

import mozpack.path as mozpath
from mozpack.chrome.manifest import (
    ManifestEntryWithRelPath,
    ManifestResource,
    is_manifest,
    parse_manifest,
)
from mozpack.copier import FileCopier, FileRegistry
from mozpack.files import BaseFinder, DeflatedFile, FileFinder, ManifestFile
from mozpack.mozjar import JarReader
from mozpack.packager import SimplePackager
from mozpack.packager.formats import FlatFormatter


class UnpackFinder(BaseFinder):
    """
    Special Finder object that treats the source package directory as if it
    were in the flat chrome format, whatever chrome format it actually is in.

    This means that for example, paths like chrome/browser/content/... match
    files under jar:chrome/browser.jar!/content/... in case of jar chrome
    format.

    The only argument to the constructor is a Finder instance or a path.
    The UnpackFinder is populated with files from this Finder instance,
    or with files from a FileFinder using the given path as its root.
    """

    def __init__(self, source, omnijar_name=None, unpack_xpi=True, **kwargs):
        if isinstance(source, BaseFinder):
            assert not kwargs
            self._finder = source
        else:
            self._finder = FileFinder(source, **kwargs)
        self.base = self._finder.base
        self.files = FileRegistry()
        self.kind = "flat"
        if omnijar_name:
            self.omnijar = omnijar_name
        else:
            # Can't include globally because of bootstrapping issues.
            from buildconfig import substs

            self.omnijar = substs.get("OMNIJAR_NAME", "omni.ja")
        self.jarlogs = {}
        self.compressed = False
        self._unpack_xpi = unpack_xpi

        jars = set()

        for p, f in self._finder.find("*"):
            # Skip the precomplete file, which is generated at packaging time.
            if p == "precomplete":
                continue
            base = mozpath.dirname(p)
            # If the file matches the omnijar pattern, it is an omnijar.
            # All the files it contains go in the directory containing the full
            # pattern. Manifests are merged if there is a corresponding manifest
            # in the directory.
            if self._maybe_zip(f) and mozpath.match(p, "**/%s" % self.omnijar):
                jar = self._open_jar(p, f)
                if "chrome.manifest" in jar:
                    self.kind = "omni"
                    self._fill_with_jar(p[: -len(self.omnijar) - 1], jar)
                    continue
            # If the file is a manifest, scan its entries for some referencing
            # jar: urls. If there are some, the files contained in the jar they
            # point to, go under a directory named after the jar.
            if is_manifest(p):
                m = self.files[p] if self.files.contains(p) else ManifestFile(base)
                for e in parse_manifest(
                    self.base, p, codecs.getreader("utf-8")(f.open())
                ):
                    m.add(self._handle_manifest_entry(e, jars))
                if self.files.contains(p):
                    continue
                f = m
            # If we're unpacking packed addons and the file is a packed addon,
            # unpack it under a directory named after the xpi.
            if self._unpack_xpi and p.endswith(".xpi") and self._maybe_zip(f):
                self._fill_with_jar(p[:-4], self._open_jar(p, f))
                continue
            if p not in jars:
                self.files.add(p, f)

    def _fill_with_jar(self, base, jar):
        for j in jar:
            path = mozpath.join(base, j.filename)
            if is_manifest(j.filename):
                m = (
                    self.files[path]
                    if self.files.contains(path)
                    else ManifestFile(mozpath.dirname(path))
                )
                for e in parse_manifest(None, path, j):
                    m.add(e)
                if not self.files.contains(path):
                    self.files.add(path, m)
                continue
            else:
                self.files.add(path, DeflatedFile(j))

    def _handle_manifest_entry(self, entry, jars):
        jarpath = None
        if (
            isinstance(entry, ManifestEntryWithRelPath)
            and urlparse(entry.relpath).scheme == "jar"
        ):
            jarpath, entry = self._unjarize(entry, entry.relpath)
        elif (
            isinstance(entry, ManifestResource)
            and urlparse(entry.target).scheme == "jar"
        ):
            jarpath, entry = self._unjarize(entry, entry.target)
        if jarpath:
            # Don't defer unpacking the jar file. If we already saw
            # it, take (and remove) it from the registry. If we
            # haven't, try to find it now.
            if self.files.contains(jarpath):
                jar = self.files[jarpath]
                self.files.remove(jarpath)
            else:
                jar = [f for p, f in self._finder.find(jarpath)]
                assert len(jar) == 1
                jar = jar[0]
            if jarpath not in jars:
                base = mozpath.splitext(jarpath)[0]
                for j in self._open_jar(jarpath, jar):
                    self.files.add(mozpath.join(base, j.filename), DeflatedFile(j))
            jars.add(jarpath)
            self.kind = "jar"
        return entry

    def _open_jar(self, path, file):
        """
        Return a JarReader for the given BaseFile instance, keeping a log of
        the preloaded entries it has.
        """
        jar = JarReader(fileobj=file.open())
        self.compressed = max(self.compressed, jar.compression)
        if jar.last_preloaded:
            jarlog = list(jar.entries.keys())
            self.jarlogs[path] = jarlog[: jarlog.index(jar.last_preloaded) + 1]
        return jar

    def find(self, path):
        for p in self.files.match(path):
            yield p, self.files[p]

    def _maybe_zip(self, file):
        """
        Return whether the given BaseFile looks like a ZIP/Jar.
        """
        header = file.open().read(8)
        return len(header) == 8 and (header[0:2] == b"PK" or header[4:6] == b"PK")

    def _unjarize(self, entry, relpath):
        """
        Transform a manifest entry pointing to chrome data in a jar in one
        pointing to the corresponding unpacked path. Return the jar path and
        the new entry.
        """
        base = entry.base
        jar, relpath = urlparse(relpath).path.split("!", 1)
        entry = (
            entry.rebase(mozpath.join(base, "jar:%s!" % jar))
            .move(mozpath.join(base, mozpath.splitext(jar)[0]))
            .rebase(base)
        )
        return mozpath.join(base, jar), entry


def unpack_to_registry(source, registry, omnijar_name=None):
    """
    Transform a jar chrome or omnijar packaged directory into a flat package.

    The given registry is filled with the flat package.
    """
    finder = UnpackFinder(source, omnijar_name)
    packager = SimplePackager(FlatFormatter(registry))
    for p, f in finder.find("*"):
        packager.add(p, f)
    packager.close()


def unpack(source, omnijar_name=None):
    """
    Transform a jar chrome or omnijar packaged directory into a flat package.
    """
    copier = FileCopier()
    unpack_to_registry(source, copier, omnijar_name)
    copier.copy(source, skip_if_older=False)
