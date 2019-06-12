# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''
Fetch and unpack architecture-specific Maven zips, verify cross-architecture
compatibility, and ready inputs to an Android multi-architecture fat AAR build.
'''

from __future__ import absolute_import, unicode_literals, print_function

import argparse
import buildconfig
import subprocess
import sys

from collections import (
    defaultdict,
    OrderedDict,
)
from hashlib import sha1  # We don't need a strong hash to compare inputs.
from zipfile import ZipFile
from io import BytesIO

from mozpack.copier import FileCopier
from mozpack.files import JarFinder
from mozpack.mozjar import JarReader
from mozpack.packager.unpack import UnpackFinder
import mozpack.path as mozpath


def _download_zips(distdir, architectures):
    # The mapping from Android CPU architecture to TC job is defined here, and the TC index
    # lookup is mediated by python/mozbuild/mozbuild/artifacts.py and
    # python/mozbuild/mozbuild/artifact_builds.py.
    jobs = {
        'arm64-v8a': 'android-aarch64-opt',
        'armeabi-v7a': 'android-api-16-opt',
        'x86': 'android-x86-opt',
        'x86_64': 'android-x86_64-opt',
    }

    for arch in architectures:
        # It's unfortunate that we must couple tightly, but that's the current API for
        # dispatching.  In automation, MOZ_ARTIFACT_TASK* environment variables will ensure
        # that the correct tasks are chosen as install sources.
        subprocess.check_call([sys.executable, mozpath.join(buildconfig.topsrcdir, 'mach'),
                               'artifact', 'install',
                               '--job', jobs[arch],
                               '--distdir', mozpath.join(distdir, 'input', arch),
                               '--no-tests', '--no-process', '--maven-zip'])


def fat_aar(distdir, architectures=[],
            no_download=False, no_process=False, no_compatibility_check=False,
            rewrite_old_archives=False):
    if not no_download:
        _download_zips(distdir, architectures)
    else:
        print('Not downloading architecture-specific artifact Maven zips.')

    if no_process:
        print('Not processing architecture-specific artifact Maven zips.')
        return 0

    # Map {filename: {fingerprint: [arch1, arch2, ...]}}.
    diffs = defaultdict(lambda: defaultdict(list))
    missing_arch_prefs = set()
    # Collect multi-architecture inputs to the fat AAR.
    copier = FileCopier()

    for arch in architectures:
        # Map old non-architecture-specific path to new architecture-specific path.
        old_rewrite_map = {
            'greprefs.js': '{}/greprefs.js'.format(arch),
            'defaults/pref/geckoview-prefs.js': 'defaults/pref/{}/geckoview-prefs.js'.format(arch),
        }

        # Architecture-specific preferences files.
        arch_prefs = set(old_rewrite_map.values())
        missing_arch_prefs |= set(arch_prefs)

        path = mozpath.join(distdir, 'input', arch, 'target.maven.zip')

        aars = list(JarFinder(path, JarReader(path)).find('**/geckoview-*.aar'))
        if len(aars) != 1:
            raise ValueError('Maven zip "{path}" with more than one candidate AAR found: {aars}'
                             .format(path=path, aars=tuple(sorted(p for p, _ in aars))))

        [aar_path, aar_file] = aars[0]

        jar_finder = JarFinder(aar_file.file.filename, JarReader(fileobj=aar_file.open()))
        for path, fileobj in UnpackFinder(jar_finder):
            # Native libraries go straight through.
            if mozpath.match(path, 'jni/**'):
                copier.add(path, fileobj)

            elif path in arch_prefs:
                copier.add(path, fileobj)

            elif rewrite_old_archives and path in old_rewrite_map:
                # Ease testing during transition by allowing old omnijars that don't have
                # architecture-specific files yet.
                new_path = old_rewrite_map[path]
                print('Rewrote old path "{path}" to new path "{new_path}"'.format(
                    path=path, new_path=new_path))
                copier.add(new_path, fileobj)

            elif path in ('classes.jar', 'annotations.zip'):
                # annotations.zip differs due to timestamps, but the contents should not.

                # `JarReader` fails on the non-standard `classes.jar` produced by Gradle/aapt,
                # and it's not worth working around, so we use Python's zip functionality
                # instead.
                z = ZipFile(BytesIO(fileobj.open().read()))
                for r in z.namelist():
                    fingerprint = sha1(z.open(r).read()).hexdigest()
                    diffs['{}!/{}'.format(path, r)][fingerprint].append(arch)

            else:
                fingerprint = sha1(fileobj.open().read()).hexdigest()
                # There's no need to distinguish `target.maven.zip` from `assets/omni.ja` here,
                # since in practice they will never overlap.
                diffs[path][fingerprint].append(arch)

            missing_arch_prefs.discard(path)

    # Some differences are allowed across the architecture-specific AARs.  We could allow-list
    # the actual content, but it's not necessary right now.
    allow_pattern_list = {
        'AndroidManifest.xml',  # Min SDK version is different for 32- and 64-bit builds.
        'classes.jar!/org/mozilla/gecko/util/HardwareUtils.class',  # Min SDK as well.
        'classes.jar!/org/mozilla/geckoview/BuildConfig.class',
        # Each input captures its CPU architecture.
        'chrome/toolkit/content/global/buildconfig.html',
        # Bug 1556162: localized resources are not deterministic across
        # per-architecture builds triggered from the same push.
        '**/*.ftl',
        '**/*.dtd',
        '**/*.properties',
    }

    not_allowed = OrderedDict()

    def format_diffs(ds):
        # Like '  armeabi-v7a, arm64-v8a -> XXX\n  x86, x86_64 -> YYY'.
        return '\n'.join(sorted(
            '  {archs} -> {fingerprint}'.format(archs=', '.join(sorted(archs)),
                                                fingerprint=fingerprint)
            for fingerprint, archs in ds.iteritems()))

    for p, ds in sorted(diffs.iteritems()):
        if len(ds) <= 1:
            # Only one hash across all inputs: roll on.
            continue

        if any(mozpath.match(p, pat) for pat in allow_pattern_list):
            print('Allowed: Path "{path}" has architecture-specific versions:\n{ds_repr}'.format(
                path=p, ds_repr=format_diffs(ds)))
            continue

        not_allowed[p] = ds

    for p, ds in not_allowed.iteritems():
        print('Disallowed: Path "{path}" has architecture-specific versions:\n{ds_repr}'.format(
                 path=p, ds_repr=format_diffs(ds)))

    for missing in sorted(missing_arch_prefs):
        print('Disallowed: Inputs missing expected architecture-specific input: {missing}'.format(
            missing=missing))

    if not no_compatibility_check and (missing_arch_prefs or not_allowed):
        return 1

    copier.copy(mozpath.join(distdir, 'output'))

    return 0


def main(argv):
    description = '''Fetch and unpack architecture-specific Maven zips, verify cross-architecture
compatibility, and ready inputs to an Android multi-architecture fat AAR build.'''

    parser = argparse.ArgumentParser(description=description)
    parser.add_argument('--no-download', action='store_true',
                        help='Do not fetch Maven zips.')
    parser.add_argument('--no-process', action='store_true',
                        help='Do not process Maven zips.')
    parser.add_argument('--no-compatibility-check', action='store_true',
                        help='Do not fail if Maven zips are not compatible.')
    parser.add_argument('--rewrite-old-archives', action='store_true',
                        help='Rewrite Maven zips containing omnijars that do not contain '
                             'architecture-specific preference files.')
    parser.add_argument('--distdir', required=True)
    parser.add_argument('architectures', nargs='+',
                        choices=('armeabi-v7a', 'arm64-v8a', 'x86', 'x86_64'))

    args = parser.parse_args(argv)

    return fat_aar(
        args.distdir, architectures=args.architectures,
        no_download=args.no_download, no_process=args.no_process,
        no_compatibility_check=args.no_compatibility_check,
        rewrite_old_archives=args.rewrite_old_archives)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
