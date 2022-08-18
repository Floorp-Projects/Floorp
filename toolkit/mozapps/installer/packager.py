# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals, print_function

from mozpack.packager.formats import (
    FlatFormatter,
    JarFormatter,
    OmniJarFormatter,
)
from mozpack.packager import (
    preprocess_manifest,
    preprocess,
    Component,
    SimpleManifestSink,
)
from mozpack.files import (
    GeneratedFile,
    FileFinder,
    File,
)
from mozpack.copier import (
    FileCopier,
    Jarrer,
)
from mozpack.errors import errors
from mozpack.files import ExecutableFile
import mozpack.path as mozpath
import buildconfig
from argparse import ArgumentParser
from collections import OrderedDict
from createprecomplete import generate_precomplete
import os
import plistlib
from io import StringIO
import subprocess


class PackagerFileFinder(FileFinder):
    def get(self, path):
        f = super(PackagerFileFinder, self).get(path)
        # Normalize Info.plist files, and remove the MozillaDeveloper*Path
        # entries which are only needed on unpackaged builds.
        if mozpath.basename(path) == "Info.plist":
            info = plistlib.load(f.open(), dict_type=OrderedDict)
            info.pop("MozillaDeveloperObjPath", None)
            info.pop("MozillaDeveloperRepoPath", None)
            return GeneratedFile(plistlib.dumps(info, sort_keys=False))
        return f


class RemovedFiles(GeneratedFile):
    """
    File class for removed-files. Is used as a preprocessor parser.
    """

    def __init__(self, copier):
        self.copier = copier
        GeneratedFile.__init__(self, "")

    def handle_line(self, f):
        f = f.strip()
        if not f:
            return
        if self.copier.contains(f):
            errors.error("Removal of packaged file(s): %s" % f)
        self.content = f + "\n"


def split_define(define):
    """
    Give a VAR[=VAL] string, returns a (VAR, VAL) tuple, where VAL defaults to
    1. Numeric VALs are returned as ints.
    """
    if "=" in define:
        name, value = define.split("=", 1)
        try:
            value = int(value)
        except ValueError:
            pass
        return (name, value)
    return (define, 1)


class NoPkgFilesRemover(object):
    """
    Formatter wrapper to handle NO_PKG_FILES.
    """

    def __init__(self, formatter, has_manifest):
        assert "NO_PKG_FILES" in os.environ
        self._formatter = formatter
        self._files = os.environ["NO_PKG_FILES"].split()
        if has_manifest:
            self._error = errors.error
            self._msg = "NO_PKG_FILES contains file listed in manifest: %s"
        else:
            self._error = errors.warn
            self._msg = "Skipping %s"

    def add_base(self, base, *args):
        self._formatter.add_base(base, *args)

    def add(self, path, content):
        if not any(mozpath.match(path, spec) for spec in self._files):
            self._formatter.add(path, content)
        else:
            self._error(self._msg % path)

    def add_manifest(self, entry):
        self._formatter.add_manifest(entry)

    def contains(self, path):
        return self._formatter.contains(path)


def main():
    parser = ArgumentParser()
    parser.add_argument(
        "-D",
        dest="defines",
        action="append",
        metavar="VAR[=VAL]",
        help="Define a variable",
    )
    parser.add_argument(
        "--format",
        default="omni",
        help="Choose the chrome format for packaging "
        + "(omni, jar or flat ; default: %(default)s)",
    )
    parser.add_argument("--removals", default=None, help="removed-files source file")
    parser.add_argument(
        "--ignore-errors",
        action="store_true",
        default=False,
        help="Transform errors into warnings.",
    )
    parser.add_argument(
        "--ignore-broken-symlinks",
        action="store_true",
        default=False,
        help="Do not fail when processing broken symlinks.",
    )
    parser.add_argument(
        "--minify",
        action="store_true",
        default=False,
        help="Make some files more compact while packaging",
    )
    parser.add_argument(
        "--minify-js",
        action="store_true",
        help="Minify JavaScript files while packaging.",
    )
    parser.add_argument(
        "--js-binary",
        help="Path to js binary. This is used to verify "
        "minified JavaScript. If this is not defined, "
        "minification verification will not be performed.",
    )
    parser.add_argument(
        "--jarlog", default="", help="File containing jar " + "access logs"
    )
    parser.add_argument(
        "--compress",
        choices=("none", "deflate"),
        default="deflate",
        help="Use given jar compression (default: deflate)",
    )
    parser.add_argument("manifest", default=None, nargs="?", help="Manifest file name")
    parser.add_argument("source", help="Source directory")
    parser.add_argument("destination", help="Destination directory")
    parser.add_argument(
        "--non-resource",
        nargs="+",
        metavar="PATTERN",
        default=[],
        help="Extra files not to be considered as resources",
    )
    args = parser.parse_args()

    defines = dict(buildconfig.defines["ALLDEFINES"])
    if args.ignore_errors:
        errors.ignore_errors()

    if args.defines:
        for name, value in [split_define(d) for d in args.defines]:
            defines[name] = value

    compress = {
        "none": False,
        "deflate": True,
    }[args.compress]

    copier = FileCopier()
    if args.format == "flat":
        formatter = FlatFormatter(copier)
    elif args.format == "jar":
        formatter = JarFormatter(copier, compress=compress)
    elif args.format == "omni":
        formatter = OmniJarFormatter(
            copier,
            buildconfig.substs["OMNIJAR_NAME"],
            compress=compress,
            non_resources=args.non_resource,
        )
    else:
        errors.fatal("Unknown format: %s" % args.format)

    # Adjust defines according to the requested format.
    if isinstance(formatter, OmniJarFormatter):
        defines["MOZ_OMNIJAR"] = 1
    elif "MOZ_OMNIJAR" in defines:
        del defines["MOZ_OMNIJAR"]

    respath = ""
    if "RESPATH" in defines:
        respath = SimpleManifestSink.normalize_path(defines["RESPATH"])
    while respath.startswith("/"):
        respath = respath[1:]

    with errors.accumulate():
        finder_args = dict(
            minify=args.minify,
            minify_js=args.minify_js,
            ignore_broken_symlinks=args.ignore_broken_symlinks,
        )
        if args.js_binary:
            finder_args["minify_js_verify_command"] = [
                args.js_binary,
                os.path.join(
                    os.path.abspath(os.path.dirname(__file__)), "js-compare-ast.js"
                ),
            ]
        finder = PackagerFileFinder(args.source, find_executables=True, **finder_args)
        if "NO_PKG_FILES" in os.environ:
            sinkformatter = NoPkgFilesRemover(formatter, args.manifest is not None)
        else:
            sinkformatter = formatter
        sink = SimpleManifestSink(finder, sinkformatter)
        if args.manifest:
            preprocess_manifest(sink, args.manifest, defines)
        else:
            sink.add(Component(""), "bin/*")
        sink.close(args.manifest is not None)

        if args.removals:
            removals_in = StringIO(open(args.removals).read())
            removals_in.name = args.removals
            removals = RemovedFiles(copier)
            preprocess(removals_in, removals, defines)
            copier.add(mozpath.join(respath, "removed-files"), removals)

    # If a pdb file is present and we were instructed to copy it, include it.
    # Run on all OSes to capture MinGW builds
    if buildconfig.substs.get("MOZ_COPY_PDBS"):
        # We want to mutate the copier while we're iterating through it, so copy
        # the items to a list first.
        copier_items = [(p, f) for p, f in copier]
        for p, f in copier_items:
            if isinstance(f, ExecutableFile):
                pdbname = os.path.splitext(f.inputs()[0])[0] + ".pdb"
                if os.path.exists(pdbname):
                    copier.add(os.path.basename(pdbname), File(pdbname))

    # Setup preloading
    if args.jarlog:
        if not os.path.exists(args.jarlog):
            raise Exception("Cannot find jar log: %s" % args.jarlog)
        omnijars = []
        if isinstance(formatter, OmniJarFormatter):
            omnijars = [
                mozpath.join(base, buildconfig.substs["OMNIJAR_NAME"])
                for base in sink.packager.get_bases(addons=False)
            ]

        from mozpack.mozjar import JarLog

        log = JarLog(args.jarlog)
        for p, f in copier:
            if not isinstance(f, Jarrer):
                continue
            if respath:
                p = mozpath.relpath(p, respath)
            if p in log:
                f.preload(log[p])
            elif p in omnijars:
                raise Exception("No jar log data for %s" % p)

    copier.copy(args.destination)
    generate_precomplete(os.path.normpath(os.path.join(args.destination, respath)))


if __name__ == "__main__":
    main()
