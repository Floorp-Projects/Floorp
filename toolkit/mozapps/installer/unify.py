# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import os
import buildconfig
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
    app_finder = UnifiedBuildFinder(app1_finder, app2_finder)

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
        for path, log in app1_finder.jarlogs.items():
            assert isinstance(copier[path], Jarrer)
            copier[path].preload(log)

    copier.copy(options.app1, skip_if_older=False)


if __name__ == "__main__":
    main()
