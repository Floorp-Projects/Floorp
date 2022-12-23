# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import os
import sys

import mozpack.path as mozpath
from mozpack.files import FileFinder


def make_archive(archive_name, base, exclude, include):
    compress = ["**/*.sym"]
    finder = FileFinder(base, ignore=exclude)
    if not include:
        include = ["*"]
    archive_basename = os.path.basename(archive_name)

    def fill_archive(add_file):
        for pat in include:
            for p, f in finder.find(pat):
                print('  Adding to "%s":\n\t"%s"' % (archive_basename, p))
                add_file(p, f)

    with open(archive_name, "wb") as fh:
        if archive_basename.endswith(".zip"):
            from mozpack.mozjar import JarWriter

            with JarWriter(fileobj=fh, compress_level=5) as writer:

                def add_file(p, f):
                    should_compress = any(mozpath.match(p, pat) for pat in compress)
                    writer.add(
                        p.encode("utf-8"),
                        f,
                        mode=f.mode,
                        compress=should_compress,
                        skip_duplicates=True,
                    )

                fill_archive(add_file)
        elif archive_basename.endswith(".tar.zst"):
            import tarfile

            import zstandard

            ctx = zstandard.ZstdCompressor(threads=-1)
            with ctx.stream_writer(fh) as zstdwriter:
                with tarfile.open(
                    mode="w|", fileobj=zstdwriter, bufsize=1024 * 1024
                ) as tar:

                    def add_file(p, f):
                        info = tar.gettarinfo(os.path.join(base, p), p)
                        tar.addfile(info, f.open())

                    fill_archive(add_file)
        else:
            raise Exception(
                "Unsupported archive format for {}".format(archive_basename)
            )


def main(argv):
    parser = argparse.ArgumentParser(description="Produce a symbols archive")
    parser.add_argument("archive", help="Which archive to generate")
    parser.add_argument("base", help="Base directory to package")
    parser.add_argument(
        "--full-archive", action="store_true", help="Generate a full symbol archive"
    )

    args = parser.parse_args(argv)

    excludes = []
    includes = []

    if args.full_archive:
        # We allow symbols for tests to be included when building on try
        if os.environ.get("MH_BRANCH", "unknown") != "try":
            excludes = ["*test*", "*Test*"]
    else:
        includes = ["**/*.sym"]

    make_archive(args.archive, args.base, excludes, includes)


if __name__ == "__main__":
    main(sys.argv[1:])
