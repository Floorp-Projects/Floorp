# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import sys
import os

from mozpack.files import FileFinder
import mozpack.path as mozpath


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
            import mozfile
            import subprocess
            import tarfile
            from buildconfig import topsrcdir

            # Ideally, we'd do this:
            #   import zstandard
            #   ctx = zstandard.ZstdCompressor(threads=-1)
            #   zstdwriter = ctx.stream_writer(fh)
            # and use `zstdwriter` as the fileobj input for `tarfile.open`.
            # But this script is invoked with `PYTHON3` in a Makefile, which
            # uses the virtualenv python where zstandard is not installed.
            # Both `sys.executable` and `buildconfig.substs['PYTHON3']` would
            # be the same. Instead, search within PATH to find another python3,
            # which hopefully has zstandard available (and that's the case on
            # automation, where this code path is expected to be followed).
            python_path = mozpath.normpath(os.path.dirname(sys.executable))
            path = [
                p
                for p in os.environ["PATH"].split(os.pathsep)
                if mozpath.normpath(p) != python_path
            ]
            python3 = mozfile.which("python3", path=path)
            proc = subprocess.Popen(
                [
                    python3,
                    os.path.join(topsrcdir, "taskcluster", "scripts", "misc", "zstdpy"),
                    "-T0",
                ],
                stdin=subprocess.PIPE,
                stdout=fh,
            )

            with tarfile.open(
                mode="w|", fileobj=proc.stdin, bufsize=1024 * 1024
            ) as tar:

                def add_file(p, f):
                    info = tar.gettarinfo(os.path.join(base, p), p)
                    tar.addfile(info, f.open())

                fill_archive(add_file)
            proc.stdin.close()
            proc.wait()
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
