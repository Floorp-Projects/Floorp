# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''
Replace localized parts of a packaged directory with data from a langpack
directory.
'''

import os
import mozpack.path
from mozpack.packager.formats import (
    FlatFormatter,
    JarFormatter,
    OmniJarFormatter,
)
from mozpack.packager import SimplePackager
from mozpack.files import ManifestFile
from mozpack.copier import (
    FileCopier,
    Jarrer,
)
from mozpack.chrome.manifest import (
    ManifestLocale,
    ManifestEntryWithRelPath,
    is_manifest,
    ManifestChrome,
    Manifest,
)
from mozpack.errors import errors
from mozpack.packager.unpack import UnpackFinder
from createprecomplete import generate_precomplete
from argparse import ArgumentParser
import buildconfig

# Set of files or directories not listed in a chrome.manifest but that are
# localized.
NON_CHROME = set([
    '**/crashreporter*.ini',
    'searchplugins',
    'dictionaries',
    'hyphenation',
    'defaults/profile',
    'defaults/pref*/*-l10n.js',
    'update.locale',
    'extensions/langpack-*@*',
    'distribution/extensions/langpack-*@*',
])


def repack(source, l10n, non_resources=[]):
    finder = UnpackFinder(source)
    l10n_finder = UnpackFinder(l10n)
    copier = FileCopier()
    if finder.kind == 'flat':
        formatter = FlatFormatter(copier)
    elif finder.kind == 'jar':
        formatter = JarFormatter(copier, optimize=finder.optimizedjars)
    elif finder.kind == 'omni':
        formatter = OmniJarFormatter(copier, finder.omnijar,
                                     optimize=finder.optimizedjars,
                                     non_resources=non_resources)

    # Read all manifest entries from the packaged directory.
    manifests = dict((p, m) for p, m in finder.find('**/*.manifest')
                     if is_manifest(p))
    assert all(isinstance(m, ManifestFile) for m in manifests.itervalues())
    entries = [e for m in manifests.itervalues() for e in m if e.localized]
    # Find unique locales used in these manifest entries.
    locales = list(set(e.id for e in entries if isinstance(e, ManifestLocale)))
    # Find all paths whose manifest are included by no other manifest.
    includes = set(mozpack.path.join(e.base, e.relpath)
                   for m in manifests.itervalues()
                   for e in m if isinstance(e, Manifest))
    bases = [mozpack.path.dirname(p) for p in set(manifests.keys()) - includes]

    # Read all manifest entries from the langpack directory.
    manifests = [m for p, m in l10n_finder.find('**/*.manifest')
                 if is_manifest(p)]
    assert all(isinstance(m, ManifestFile) for m in manifests)
    l10n_entries = [e for m in manifests for e in m if e.localized]
    # Find unique locales used in these manifest entries.
    l10n_locales = list(set(e.id for e in l10n_entries
                            if isinstance(e, ManifestLocale)))

    # The code further below assumes there's only one locale replaced with
    # another one.
    if len(locales) > 1 or len(l10n_locales) > 1:
        errors.fatal("Multiple locales aren't supported")
    locale = locales[0]
    l10n_locale = l10n_locales[0]

    # For each base directory, store what path a locale chrome package name
    # corresponds to.
    # e.g., for the following entry under app/chrome:
    #     locale foo en-US path/to/files
    # keep track that the locale path for foo in app is
    # app/chrome/path/to/files.
    l10n_paths = {}
    for e in l10n_entries:
        if isinstance(e, ManifestChrome):
            base = mozpack.path.basedir(e.path, bases)
            if not base in l10n_paths:
                l10n_paths[base] = {}
            l10n_paths[base][e.name] = e.path

    # For chrome and non chrome files or directories, store what langpack path
    # corresponds to a package path.
    paths = dict((e.path,
                  l10n_paths[mozpack.path.basedir(e.path, bases)][e.name])
                 for e in entries if isinstance(e, ManifestEntryWithRelPath))

    for path in NON_CHROME:
        left = set(p for p, f in finder.find(path))
        right = set(p for p, f in l10n_finder.find(path))
        for p in right:
            paths[p] = p
        for p in left - right:
            paths[p] = None

    # Create a new package, with non localized bits coming from the original
    # package, and localized bits coming from the langpack.
    packager = SimplePackager(formatter)
    for p, f in finder:
        if is_manifest(p):
            # Remove localized manifest entries.
            for e in [e for e in f if e.localized]:
                f.remove(e)
        # If the path is one that needs a locale replacement, use the
        # corresponding file from the langpack.
        path = None
        if p in paths:
            path = paths[p]
            if not path:
                continue
        else:
            base = mozpack.path.basedir(p, paths.keys())
            if base:
                subpath = mozpack.path.relpath(p, base)
                path = mozpack.path.normpath(mozpack.path.join(paths[base],
                                                               subpath))
        if path:
            files = [f for p, f in l10n_finder.find(path)]
            if not len(files):
                if base not in NON_CHROME:
                    errors.error("Missing file: %s" % os.path.join(l10n, path))
            else:
                packager.add(path, files[0])
        else:
            packager.add(p, f)

    # Add localized manifest entries from the langpack.
    l10n_manifests = []
    for base in set(e.base for e in l10n_entries):
        m = ManifestFile(base)
        for e in l10n_entries:
            if e.base == base:
                m.add(e)
        path = mozpack.path.join(base, 'chrome.%s.manifest' % l10n_locale)
        l10n_manifests.append((path, m))
    bases = packager.get_bases()
    for path, m in l10n_manifests:
        base = mozpack.path.basedir(path, bases)
        packager.add(path, m)
        # Add a "manifest $path" entry in the top manifest under that base.
        m = ManifestFile(base)
        m.add(Manifest(base, mozpack.path.relpath(path, base)))
        packager.add(mozpack.path.join(base, 'chrome.manifest'), m)

    packager.close()

    # Add any remaining non chrome files.
    for base in NON_CHROME:
        for p, f in l10n_finder.find(base):
            if not formatter.contains(p):
                formatter.add(p, f)

    # Transplant jar preloading information.
    for path, log in finder.jarlogs.iteritems():
        assert isinstance(copier[path], Jarrer)
        copier[path].preload([l.replace(locale, l10n_locale) for l in log])

    copier.copy(source, skip_if_older=False)
    generate_precomplete(source)


def main():
    parser = ArgumentParser()
    parser.add_argument('build',
                        help='Directory containing the build to repack')
    parser.add_argument('l10n',
                        help='Directory containing the staged langpack')
    parser.add_argument('--non-resource', nargs='+', metavar='PATTERN',
                        default=[],
                        help='Extra files not to be considered as resources')
    args = parser.parse_args()

    buildconfig.substs['USE_ELF_HACK'] = False
    buildconfig.substs['PKG_SKIP_STRIP'] = True
    repack(args.build, args.l10n, args.non_resource)

if __name__ == "__main__":
    main()
