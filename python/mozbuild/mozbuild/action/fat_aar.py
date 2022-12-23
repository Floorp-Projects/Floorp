# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Fetch and unpack architecture-specific Maven zips, verify cross-architecture
compatibility, and ready inputs to an Android multi-architecture fat AAR build.
"""

import argparse
import sys
from collections import OrderedDict, defaultdict
from hashlib import sha1  # We don't need a strong hash to compare inputs.
from io import BytesIO
from zipfile import ZipFile

import mozpack.path as mozpath
import six
from mozpack.copier import FileCopier
from mozpack.files import JarFinder
from mozpack.mozjar import JarReader
from mozpack.packager.unpack import UnpackFinder


def fat_aar(distdir, aars_paths, no_process=False, no_compatibility_check=False):
    if no_process:
        print("Not processing architecture-specific artifact Maven AARs.")
        return 0

    # Map {filename: {fingerprint: [arch1, arch2, ...]}}.
    diffs = defaultdict(lambda: defaultdict(list))
    missing_arch_prefs = set()
    # Collect multi-architecture inputs to the fat AAR.
    copier = FileCopier()

    for arch, aar_path in aars_paths.items():
        # Map old non-architecture-specific path to new architecture-specific path.
        old_rewrite_map = {
            "greprefs.js": "{}/greprefs.js".format(arch),
            "defaults/pref/geckoview-prefs.js": "defaults/pref/{}/geckoview-prefs.js".format(
                arch
            ),
        }

        # Architecture-specific preferences files.
        arch_prefs = set(old_rewrite_map.values())
        missing_arch_prefs |= set(arch_prefs)

        jar_finder = JarFinder(aar_path, JarReader(aar_path))
        for path, fileobj in UnpackFinder(jar_finder):
            # Native libraries go straight through.
            if mozpath.match(path, "jni/**"):
                copier.add(path, fileobj)

            elif path in arch_prefs:
                copier.add(path, fileobj)

            elif path in ("classes.jar", "annotations.zip"):
                # annotations.zip differs due to timestamps, but the contents should not.

                # `JarReader` fails on the non-standard `classes.jar` produced by Gradle/aapt,
                # and it's not worth working around, so we use Python's zip functionality
                # instead.
                z = ZipFile(BytesIO(fileobj.open().read()))
                for r in z.namelist():
                    fingerprint = sha1(z.open(r).read()).hexdigest()
                    diffs["{}!/{}".format(path, r)][fingerprint].append(arch)

            else:
                fingerprint = sha1(six.ensure_binary(fileobj.open().read())).hexdigest()
                # There's no need to distinguish `target.maven.zip` from `assets/omni.ja` here,
                # since in practice they will never overlap.
                diffs[path][fingerprint].append(arch)

            missing_arch_prefs.discard(path)

    # Some differences are allowed across the architecture-specific AARs.  We could allow-list
    # the actual content, but it's not necessary right now.
    allow_pattern_list = {
        "AndroidManifest.xml",  # Min SDK version is different for 32- and 64-bit builds.
        "classes.jar!/org/mozilla/gecko/util/HardwareUtils.class",  # Min SDK as well.
        "classes.jar!/org/mozilla/geckoview/BuildConfig.class",
        # Each input captures its CPU architecture.
        "chrome/toolkit/content/global/buildconfig.html",
        # Bug 1556162: localized resources are not deterministic across
        # per-architecture builds triggered from the same push.
        "**/*.ftl",
        "**/*.dtd",
        "**/*.properties",
    }

    not_allowed = OrderedDict()

    def format_diffs(ds):
        # Like '  armeabi-v7a, arm64-v8a -> XXX\n  x86, x86_64 -> YYY'.
        return "\n".join(
            sorted(
                "  {archs} -> {fingerprint}".format(
                    archs=", ".join(sorted(archs)), fingerprint=fingerprint
                )
                for fingerprint, archs in ds.items()
            )
        )

    for p, ds in sorted(diffs.items()):
        if len(ds) <= 1:
            # Only one hash across all inputs: roll on.
            continue

        if any(mozpath.match(p, pat) for pat in allow_pattern_list):
            print(
                'Allowed: Path "{path}" has architecture-specific versions:\n{ds_repr}'.format(
                    path=p, ds_repr=format_diffs(ds)
                )
            )
            continue

        not_allowed[p] = ds

    for p, ds in not_allowed.items():
        print(
            'Disallowed: Path "{path}" has architecture-specific versions:\n{ds_repr}'.format(
                path=p, ds_repr=format_diffs(ds)
            )
        )

    for missing in sorted(missing_arch_prefs):
        print(
            "Disallowed: Inputs missing expected architecture-specific input: {missing}".format(
                missing=missing
            )
        )

    if not no_compatibility_check and (missing_arch_prefs or not_allowed):
        return 1

    output_dir = mozpath.join(distdir, "output")
    copier.copy(output_dir)

    return 0


_ALL_ARCHS = ("armeabi-v7a", "arm64-v8a", "x86_64", "x86")


def main(argv):
    description = """Unpack architecture-specific Maven AARs, verify cross-architecture
compatibility, and ready inputs to an Android multi-architecture fat AAR build."""

    parser = argparse.ArgumentParser(description=description)
    parser.add_argument(
        "--no-process", action="store_true", help="Do not process Maven AARs."
    )
    parser.add_argument(
        "--no-compatibility-check",
        action="store_true",
        help="Do not fail if Maven AARs are not compatible.",
    )
    parser.add_argument("--distdir", required=True)

    for arch in _ALL_ARCHS:
        command_line_flag = arch.replace("_", "-")
        parser.add_argument("--{}".format(command_line_flag), dest=arch)

    args = parser.parse_args(argv)

    args_dict = vars(args)

    aars_paths = {
        arch: args_dict.get(arch) for arch in _ALL_ARCHS if args_dict.get(arch)
    }

    if not aars_paths:
        raise ValueError("You must provide at least one AAR file!")

    return fat_aar(
        args.distdir,
        aars_paths,
        no_process=args.no_process,
        no_compatibility_check=args.no_compatibility_check,
    )


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
