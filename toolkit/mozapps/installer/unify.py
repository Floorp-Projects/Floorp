# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import os
import six
import sys
import buildconfig
import mozpack.path as mozpath
from mozpack.packager.formats import (
    FlatFormatter,
    JarFormatter,
    OmniJarFormatter,
)
from mozpack.packager import SimplePackager
from mozpack.copier import (
    FileCopier,
    Jarrer,
)
from mozpack.errors import errors
from mozpack.files import FileFinder
from mozpack.mozjar import JAR_DEFLATED
from mozpack.packager.unpack import UnpackFinder
from mozpack.unify import UnifiedBuildFinder


# At the moment, rlbox is not supported on aarch64, so we need to allow
# the files to be missing on the aarch64 half of the build. This also
# means the precomplete file doesn't match, and we want to keep the x86_64
# version which contains the extra lines for the wasm libs.
WASM_LIBS = ("Contents/MacOS/librlbox.dylib",)


class UnifiedBuildFinderWasmHack(UnifiedBuildFinder):
    def unify_file(self, path, file1, file2):
        if path in WASM_LIBS:
            # When this assert hits, it means rlbox is supported on aarch64,
            # and this override, as well as precomplete below, can be removed.
            assert not file2
            return file1
        if file1 and file2 and path == "Contents/Resources/precomplete":
            # Check that the only differences are because of the missing wasm libs.
            wasm_lines = ['remove "{}"\n'.format(l).encode("utf-8") for l in WASM_LIBS]
            content1 = [
                l
                for l in file1.open().readlines()
                if not any(x == l for x in wasm_lines)
            ]
            content2 = file2.open().readlines()
            if content1 == content2:
                return file1
        return super(UnifiedBuildFinderWasmHack, self).unify_file(path, file1, file2)


def main():
    parser = argparse.ArgumentParser(
        description="Merge two builds of a Gecko-based application into a Universal build"
    )
    parser.add_argument("app1", help="Directory containing the application")
    parser.add_argument("app2", help="Directory containing the application to merge")
    parser.add_argument(
        "--non-resource",
        nargs="+",
        metavar="PATTERN",
        default=[],
        help="Extra files not to be considered as resources",
    )

    options = parser.parse_args()

    buildconfig.substs["OS_ARCH"] = "Darwin"
    buildconfig.substs["LIPO"] = os.environ.get("LIPO")

    app1_finder = UnpackFinder(FileFinder(options.app1, find_executables=True))
    app2_finder = UnpackFinder(FileFinder(options.app2, find_executables=True))
    app_finder = UnifiedBuildFinderWasmHack(app1_finder, app2_finder)

    copier = FileCopier()
    compress = min(app1_finder.compressed, JAR_DEFLATED)
    if app1_finder.kind == "flat":
        formatter = FlatFormatter(copier)
    elif app1_finder.kind == "jar":
        formatter = JarFormatter(copier, compress=compress)
    elif app1_finder.kind == "omni":
        formatter = OmniJarFormatter(
            copier,
            app1_finder.omnijar,
            compress=compress,
            non_resources=options.non_resource,
        )

    with errors.accumulate():
        packager = SimplePackager(formatter)
        for p, f in app_finder:
            packager.add(p, f)
        packager.close()

        # Transplant jar preloading information.
        for path, log in six.iteritems(app1_finder.jarlogs):
            assert isinstance(copier[path], Jarrer)
            copier[path].preload(log)

    copier.copy(options.app1, skip_if_older=False)


if __name__ == "__main__":
    main()
