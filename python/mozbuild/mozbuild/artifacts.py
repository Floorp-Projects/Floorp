# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Fetch build artifacts from a Firefox tree.

This provides an (at-the-moment special purpose) interface to download Android
artifacts from Mozilla's Task Cluster.

This module performs the following steps:

* find a candidate hg parent revision.  At one time we used the local pushlog,
  which required the mozext hg extension.  This isn't feasible with git, and it
  is only mildly less efficient to not use the pushlog, so we don't use it even
  when querying hg.

* map the candidate parent to candidate Task Cluster tasks and artifact
  locations.  Pushlog entries might not correspond to tasks (yet), and those
  tasks might not produce the desired class of artifacts.

* fetch fresh Task Cluster artifacts and purge old artifacts, using a simple
  Least Recently Used cache.

* post-process fresh artifacts, to speed future installation.  In particular,
  extract relevant files from Mac OS X DMG files into a friendly archive format
  so we don't have to mount DMG files frequently.

This module requires certain modules be importable from the ambient Python
environment.  ``mach artifact`` ensures these modules are available, but other
consumers will need to arrange this themselves.
"""


import collections
import functools
import glob
import logging
import operator
import os
import pickle
import re
import shutil
import stat
import subprocess
import tarfile
import tempfile
import zipfile
from contextlib import contextmanager
from io import BufferedReader
from urllib.parse import urlparse

import buildconfig
import mozinstall
import mozpack.path as mozpath
import pylru
import requests
import six
from mach.util import UserError
from mozpack import executables
from mozpack.files import JarFinder, TarFinder
from mozpack.mozjar import JarReader, JarWriter
from mozpack.packager.unpack import UnpackFinder
from taskgraph.util.taskcluster import find_task_id, get_artifact_url, list_artifacts

from mozbuild.artifact_builds import JOB_CHOICES
from mozbuild.artifact_cache import ArtifactCache
from mozbuild.util import FileAvoidWrite, ensureParentDir, mkdir

# Number of candidate pushheads to cache per parent changeset.
NUM_PUSHHEADS_TO_QUERY_PER_PARENT = 50

# Number of parent changesets to consider as possible pushheads.
# There isn't really such a thing as a reasonable default here, because we don't
# know how many pushheads we'll need to look at to find a build with our artifacts,
# and we don't know how many changesets will be in each push. For now we assume
# we'll find a build in the last 50 pushes, assuming each push contains 10 changesets.
NUM_REVISIONS_TO_QUERY = 500

MAX_CACHED_TASKS = 400  # Number of pushheads to cache Task Cluster task data for.

# Downloaded artifacts are cached, and a subset of their contents extracted for
# easy installation.  This is most noticeable on Mac OS X: since mounting and
# copying from DMG files is very slow, we extract the desired binaries to a
# separate archive for fast re-installation.
PROCESSED_SUFFIX = ".processed.jar"


class ArtifactJob(object):
    trust_domain = "gecko"
    default_candidate_trees = [
        "releases/mozilla-release",
    ]
    nightly_candidate_trees = [
        "mozilla-central",
        "integration/autoland",
    ]
    beta_candidate_trees = [
        "releases/mozilla-beta",
    ]
    # The list below list should be updated when we have new ESRs.
    esr_candidate_trees = [
        "releases/mozilla-esr115",
    ]
    try_tree = "try"

    # These are a subset of TEST_HARNESS_BINS in testing/mochitest/Makefile.in.
    # Each item is a pair of (pattern, (src_prefix, dest_prefix), where src_prefix
    # is the prefix of the pattern relevant to its location in the archive, and
    # dest_prefix is the prefix to be added that will yield the final path relative
    # to dist/.
    test_artifact_patterns = {
        ("bin/BadCertAndPinningServer", ("bin", "bin")),
        ("bin/DelegatedCredentialsServer", ("bin", "bin")),
        ("bin/EncryptedClientHelloServer", ("bin", "bin")),
        ("bin/FaultyServer", ("bin", "bin")),
        ("bin/GenerateOCSPResponse", ("bin", "bin")),
        ("bin/OCSPStaplingServer", ("bin", "bin")),
        ("bin/SanctionsTestServer", ("bin", "bin")),
        ("bin/certutil", ("bin", "bin")),
        ("bin/geckodriver", ("bin", "bin")),
        ("bin/pk12util", ("bin", "bin")),
        ("bin/screentopng", ("bin", "bin")),
        ("bin/ssltunnel", ("bin", "bin")),
        ("bin/xpcshell", ("bin", "bin")),
        ("bin/plugin-container", ("bin", "bin")),
        ("bin/http3server", ("bin", "bin")),
        ("bin/plugins/gmp-*/*/*", ("bin/plugins", "bin")),
        ("bin/plugins/*", ("bin/plugins", "plugins")),
    }

    # We can tell our input is a test archive by this suffix, which happens to
    # be the same across platforms.
    _test_zip_archive_suffix = ".common.tests.zip"
    _test_tar_archive_suffix = ".common.tests.tar.gz"

    # A map of extra archives to fetch and unpack.  An extra archive might
    # include optional build output to incorporate into the local artifact
    # build.  Test archives and crashreporter symbols could be extra archives
    # but they require special handling; this mechanism is generic and intended
    # only for the simplest cases.
    #
    # Each suffix key matches a candidate archive (i.e., an artifact produced by
    # an upstream build).  Each value is itself a dictionary that must contain
    # the following keys:
    #
    # - `description`: a purely informational string description.
    # - `src_prefix`: entry names in the archive with leading `src_prefix` will
    #   have the prefix stripped.
    # - `dest_prefix`: entry names in the archive will have `dest_prefix`
    #   prepended.
    #
    # The entries in the archive, suitably renamed, will be extracted into `dist`.
    _extra_archives = {
        ".xpt_artifacts.zip": {
            "description": "XPT Artifacts",
            "src_prefix": "",
            "dest_prefix": "xpt_artifacts",
        },
    }
    _extra_archive_suffixes = tuple(sorted(_extra_archives.keys()))

    def __init__(
        self,
        log=None,
        download_tests=True,
        download_symbols=False,
        download_maven_zip=False,
        substs=None,
        mozbuild=None,
    ):
        self._package_re = re.compile(self.package_re)
        self._tests_re = None
        if download_tests:
            self._tests_re = re.compile(
                r"public/build/(en-US/)?target\.common\.tests\.(zip|tar\.gz)$"
            )
        self._maven_zip_re = None
        if download_maven_zip:
            self._maven_zip_re = re.compile(r"public/build/target\.maven\.zip$")
        self._log = log
        self._substs = substs
        self._symbols_archive_suffix = None
        if download_symbols == "full":
            self._symbols_archive_suffix = "crashreporter-symbols-full.tar.zst"
        elif download_symbols:
            self._symbols_archive_suffix = "crashreporter-symbols.zip"
        self._mozbuild = mozbuild
        self._candidate_trees = None

    def log(self, *args, **kwargs):
        if self._log:
            self._log(*args, **kwargs)

    def find_candidate_artifacts(self, artifacts):
        # TODO: Handle multiple artifacts, taking the latest one.
        tests_artifact = None
        maven_zip_artifact = None
        for artifact in artifacts:
            name = artifact["name"]
            if self._maven_zip_re:
                if self._maven_zip_re.match(name):
                    maven_zip_artifact = name
                    yield name
                else:
                    continue
            elif self._package_re and self._package_re.match(name):
                yield name
            elif self._tests_re and self._tests_re.match(name):
                tests_artifact = name
                yield name
            elif self._symbols_archive_suffix and name.endswith(
                self._symbols_archive_suffix
            ):
                yield name
            elif name.endswith(ArtifactJob._extra_archive_suffixes):
                yield name
            else:
                self.log(
                    logging.DEBUG,
                    "artifact",
                    {"name": name},
                    "Not yielding artifact named {name} as a candidate artifact",
                )
        if self._tests_re and not tests_artifact:
            raise ValueError(
                'Expected tests archive matching "{re}", but '
                "found none!".format(re=self._tests_re)
            )
        if self._maven_zip_re and not maven_zip_artifact:
            raise ValueError(
                'Expected Maven zip archive matching "{re}", but '
                "found none!".format(re=self._maven_zip_re)
            )

    @contextmanager
    def get_writer(self, **kwargs):
        with JarWriter(**kwargs) as writer:
            yield writer

    def process_artifact(self, filename, processed_filename):
        if filename.endswith(ArtifactJob._test_zip_archive_suffix) and self._tests_re:
            return self.process_tests_zip_artifact(filename, processed_filename)
        if filename.endswith(ArtifactJob._test_tar_archive_suffix) and self._tests_re:
            return self.process_tests_tar_artifact(filename, processed_filename)
        if self._symbols_archive_suffix and filename.endswith(
            self._symbols_archive_suffix
        ):
            return self.process_symbols_archive(filename, processed_filename)
        if filename.endswith(ArtifactJob._extra_archive_suffixes):
            return self.process_extra_archive(filename, processed_filename)
        return self.process_package_artifact(filename, processed_filename)

    def process_package_artifact(self, filename, processed_filename):
        raise NotImplementedError(
            "Subclasses must specialize process_package_artifact!"
        )

    def process_tests_zip_artifact(self, filename, processed_filename):
        from mozbuild.action.test_archive import OBJDIR_TEST_FILES

        added_entry = False

        with self.get_writer(file=processed_filename, compress_level=5) as writer:
            reader = JarReader(filename)
            for filename, entry in six.iteritems(reader.entries):
                for pattern, (src_prefix, dest_prefix) in self.test_artifact_patterns:
                    if not mozpath.match(filename, pattern):
                        continue
                    destpath = mozpath.relpath(filename, src_prefix)
                    destpath = mozpath.join(dest_prefix, destpath)
                    self.log(
                        logging.DEBUG,
                        "artifact",
                        {"destpath": destpath},
                        "Adding {destpath} to processed archive",
                    )
                    mode = entry["external_attr"] >> 16
                    writer.add(destpath.encode("utf-8"), reader[filename], mode=mode)
                    added_entry = True
                    break

                if filename.endswith(".ini"):
                    # The artifact build writes test .ini files into the object
                    # directory; they don't come from the upstream test archive.
                    self.log(
                        logging.DEBUG,
                        "artifact",
                        {"filename": filename},
                        "Skipping test INI file {filename}",
                    )
                    continue

                for files_entry in OBJDIR_TEST_FILES.values():
                    origin_pattern = files_entry["pattern"]
                    leaf_filename = filename
                    if "dest" in files_entry:
                        dest = files_entry["dest"]
                        origin_pattern = mozpath.join(dest, origin_pattern)
                        leaf_filename = filename[len(dest) + 1 :]
                    if mozpath.match(filename, origin_pattern):
                        destpath = mozpath.join(
                            "..", files_entry["base"], leaf_filename
                        )
                        mode = entry["external_attr"] >> 16
                        writer.add(
                            destpath.encode("utf-8"), reader[filename], mode=mode
                        )

        if not added_entry:
            raise ValueError(
                'Archive format changed! No pattern from "{patterns}"'
                "matched an archive path.".format(
                    patterns=LinuxArtifactJob.test_artifact_patterns
                )
            )

    def process_tests_tar_artifact(self, filename, processed_filename):
        from mozbuild.action.test_archive import OBJDIR_TEST_FILES

        added_entry = False

        with self.get_writer(file=processed_filename, compress_level=5) as writer:
            with tarfile.open(filename) as reader:
                for filename, entry in TarFinder(filename, reader):
                    for (
                        pattern,
                        (src_prefix, dest_prefix),
                    ) in self.test_artifact_patterns:
                        if not mozpath.match(filename, pattern):
                            continue

                        destpath = mozpath.relpath(filename, src_prefix)
                        destpath = mozpath.join(dest_prefix, destpath)
                        self.log(
                            logging.DEBUG,
                            "artifact",
                            {"destpath": destpath},
                            "Adding {destpath} to processed archive",
                        )
                        mode = entry.mode
                        writer.add(destpath.encode("utf-8"), entry.open(), mode=mode)
                        added_entry = True
                        break

                    if filename.endswith(".ini"):
                        # The artifact build writes test .ini files into the object
                        # directory; they don't come from the upstream test archive.
                        self.log(
                            logging.DEBUG,
                            "artifact",
                            {"filename": filename},
                            "Skipping test INI file {filename}",
                        )
                        continue

                    for files_entry in OBJDIR_TEST_FILES.values():
                        origin_pattern = files_entry["pattern"]
                        leaf_filename = filename
                        if "dest" in files_entry:
                            dest = files_entry["dest"]
                            origin_pattern = mozpath.join(dest, origin_pattern)
                            leaf_filename = filename[len(dest) + 1 :]
                        if mozpath.match(filename, origin_pattern):
                            destpath = mozpath.join(
                                "..", files_entry["base"], leaf_filename
                            )
                            mode = entry.mode
                            writer.add(
                                destpath.encode("utf-8"), entry.open(), mode=mode
                            )

        if not added_entry:
            raise ValueError(
                'Archive format changed! No pattern from "{patterns}"'
                "matched an archive path.".format(
                    patterns=LinuxArtifactJob.test_artifact_patterns
                )
            )

    def process_symbols_archive(
        self, filename, processed_filename, skip_compressed=False
    ):
        with self.get_writer(file=processed_filename, compress_level=5) as writer:
            for filename, entry in self.iter_artifact_archive(filename):
                if skip_compressed and filename.endswith(".gz"):
                    self.log(
                        logging.DEBUG,
                        "artifact",
                        {"filename": filename},
                        "Skipping compressed ELF debug symbol file {filename}",
                    )
                    continue
                destpath = mozpath.join("crashreporter-symbols", filename)
                self.log(
                    logging.INFO,
                    "artifact",
                    {"destpath": destpath},
                    "Adding {destpath} to processed archive",
                )
                writer.add(destpath.encode("utf-8"), entry)

    def process_extra_archive(self, filename, processed_filename):
        for suffix, extra_archive in ArtifactJob._extra_archives.items():
            if filename.endswith(suffix):
                self.log(
                    logging.INFO,
                    "artifact",
                    {"filename": filename, "description": extra_archive["description"]},
                    '"{filename}" is a recognized extra archive ({description})',
                )
                break
        else:
            raise ValueError('"{}" is not a recognized extra archive!'.format(filename))

        src_prefix = extra_archive["src_prefix"]
        dest_prefix = extra_archive["dest_prefix"]

        with self.get_writer(file=processed_filename, compress_level=5) as writer:
            for filename, entry in self.iter_artifact_archive(filename):
                if not filename.startswith(src_prefix):
                    self.log(
                        logging.DEBUG,
                        "artifact",
                        {"filename": filename, "src_prefix": src_prefix},
                        "Skipping extra archive item {filename} "
                        "that does not start with {src_prefix}",
                    )
                    continue
                destpath = mozpath.relpath(filename, src_prefix)
                destpath = mozpath.join(dest_prefix, destpath)
                self.log(
                    logging.INFO,
                    "artifact",
                    {"destpath": destpath},
                    "Adding {destpath} to processed archive",
                )
                writer.add(destpath.encode("utf-8"), entry)

    def iter_artifact_archive(self, filename):
        if filename.endswith(".zip"):
            reader = JarReader(filename)
            for filename in reader.entries:
                yield filename, reader[filename]
        elif filename.endswith(".tar.zst") and self._mozbuild is not None:
            self._mozbuild._ensure_zstd()
            import zstandard

            ctx = zstandard.ZstdDecompressor()
            uncompressed = ctx.stream_reader(open(filename, "rb"))
            with tarfile.open(
                mode="r|", fileobj=uncompressed, bufsize=1024 * 1024
            ) as reader:
                while True:
                    info = reader.next()
                    if info is None:
                        break
                    yield info.name, reader.extractfile(info)
        else:
            raise RuntimeError("Unsupported archive type for %s" % filename)

    @property
    def candidate_trees(self):
        if not self._candidate_trees:
            self._candidate_trees = self.select_candidate_trees()
        return self._candidate_trees

    def select_candidate_trees(self):
        source_repo = buildconfig.substs.get("MOZ_SOURCE_REPO", "")
        version_display = buildconfig.substs.get("MOZ_APP_VERSION_DISPLAY")

        if "esr" in version_display or "esr" in source_repo:
            return self.esr_candidate_trees
        elif re.search("a\d+$", version_display):
            return self.nightly_candidate_trees
        elif re.search("b\d+$", version_display):
            return self.beta_candidate_trees

        return self.default_candidate_trees


class AndroidArtifactJob(ArtifactJob):
    package_re = r"public/build/geckoview_example\.apk$"
    product = "mobile"

    package_artifact_patterns = {"**/*.so"}

    def process_package_artifact(self, filename, processed_filename):
        # Extract all .so files into the root, which will get copied into dist/bin.
        with self.get_writer(file=processed_filename, compress_level=5) as writer:
            for p, f in UnpackFinder(JarFinder(filename, JarReader(filename))):
                if not any(
                    mozpath.match(p, pat) for pat in self.package_artifact_patterns
                ):
                    continue

                dirname, basename = os.path.split(p)
                self.log(
                    logging.DEBUG,
                    "artifact",
                    {"basename": basename},
                    "Adding {basename} to processed archive",
                )

                basedir = "bin"
                if not basename.endswith(".so"):
                    basedir = mozpath.join("bin", dirname.lstrip("assets/"))
                basename = mozpath.join(basedir, basename)
                writer.add(basename.encode("utf-8"), f.open())

    def process_symbols_archive(self, filename, processed_filename):
        ArtifactJob.process_symbols_archive(
            self, filename, processed_filename, skip_compressed=True
        )

        if not self._symbols_archive_suffix.startswith("crashreporter-symbols-full."):
            return

        import gzip

        with self.get_writer(file=processed_filename, compress_level=5) as writer:
            for filename, entry in self.iter_artifact_archive(filename):
                if not filename.endswith(".gz"):
                    continue

                # Uncompress "libxul.so/D3271457813E976AE7BF5DAFBABABBFD0/libxul.so.dbg.gz"
                # into "libxul.so.dbg".
                #
                # After running `settings append target.debug-file-search-paths $file`,
                # where file=/path/to/topobjdir/dist/crashreporter-symbols,
                # Android Studio's lldb (7.0.0, at least) will find the ELF debug symbol files.
                #
                # There are other paths that will work but none seem more desireable.  See
                # https://github.com/llvm-mirror/lldb/blob/882670690ca69d9dd96b7236c620987b11894af9/source/Host/common/Symbols.cpp#L324.
                basename = os.path.basename(filename).replace(".gz", "")
                destpath = mozpath.join("crashreporter-symbols", basename)
                self.log(
                    logging.DEBUG,
                    "artifact",
                    {"destpath": destpath},
                    "Adding uncompressed ELF debug symbol file "
                    "{destpath} to processed archive",
                )
                writer.add(destpath.encode("utf-8"), gzip.GzipFile(fileobj=entry))


class LinuxArtifactJob(ArtifactJob):
    package_re = r"public/build/target\.tar\.bz2$"
    product = "firefox"

    _package_artifact_patterns = {
        "{product}/crashreporter",
        "{product}/dependentlibs.list",
        "{product}/{product}",
        "{product}/{product}-bin",
        "{product}/minidump-analyzer",
        "{product}/pingsender",
        "{product}/plugin-container",
        "{product}/updater",
        "{product}/glxtest",
        "{product}/v4l2test",
        "{product}/vaapitest",
        "{product}/**/*.so",
        # Preserve signatures when present.
        "{product}/**/*.sig",
    }

    @property
    def package_artifact_patterns(self):
        return {p.format(product=self.product) for p in self._package_artifact_patterns}

    def process_package_artifact(self, filename, processed_filename):
        added_entry = False

        with self.get_writer(file=processed_filename, compress_level=5) as writer:
            with tarfile.open(filename) as reader:
                for p, f in UnpackFinder(TarFinder(filename, reader)):
                    if not any(
                        mozpath.match(p, pat) for pat in self.package_artifact_patterns
                    ):
                        continue

                    # We strip off the relative "firefox/" bit from the path,
                    # but otherwise preserve it.
                    destpath = mozpath.join("bin", mozpath.relpath(p, self.product))
                    self.log(
                        logging.DEBUG,
                        "artifact",
                        {"destpath": destpath},
                        "Adding {destpath} to processed archive",
                    )
                    writer.add(destpath.encode("utf-8"), f.open(), mode=f.mode)
                    added_entry = True

        if not added_entry:
            raise ValueError(
                'Archive format changed! No pattern from "{patterns}" '
                "matched an archive path.".format(
                    patterns=LinuxArtifactJob.package_artifact_patterns
                )
            )


class ResignJarWriter(JarWriter):
    def __init__(self, job, **kwargs):
        super().__init__(**kwargs)
        self._job = job

    def add(self, name, data, mode=None):
        if self._job._substs["HOST_OS_ARCH"] == "Darwin":
            # Wrap in a BufferedReader so that executable.get_type can peek at the
            # data signature without subsequent read() being affected.
            data = BufferedReader(data)
            if executables.get_type(data) == executables.MACHO:
                # If the file is a Mach-O binary, we run `codesign -s - -f` against
                # it to force a local codesign against the original binary, which is
                # likely unsigned. As of writing, only arm64 macs require codesigned
                # binaries, but it doesn't hurt to do it on intel macs as well
                # preemptively, because they could end up with the same requirement
                # in future versions of macOS.
                tmp = tempfile.NamedTemporaryFile(delete=False)
                try:
                    shutil.copyfileobj(data, tmp)
                    tmp.close()
                    self._job.log(
                        logging.DEBUG,
                        "artifact",
                        {"path": name.decode("utf-8")},
                        "Re-signing {path}",
                    )
                    subprocess.check_call(
                        ["codesign", "-s", "-", "-f", tmp.name],
                        stdout=subprocess.DEVNULL,
                        stderr=subprocess.DEVNULL,
                    )
                    data = open(tmp.name, "rb")
                finally:
                    os.unlink(tmp.name)
        super().add(name, data, mode=mode)


class MacArtifactJob(ArtifactJob):
    package_re = r"public/build/target\.dmg$"
    product = "firefox"

    # These get copied into dist/bin without the path, so "root/a/b/c" -> "dist/bin/c".
    _paths_no_keep_path = (
        "Contents/MacOS",
        [
            "crashreporter.app/Contents/MacOS/crashreporter",
            "{product}",
            "{product}-bin",
            "*.dylib",
            "minidump-analyzer",
            "pingsender",
            "plugin-container.app/Contents/MacOS/plugin-container",
            "updater.app/Contents/MacOS/org.mozilla.updater",
            # 'xpcshell',
            "XUL",
        ],
    )

    @property
    def paths_no_keep_path(self):
        root, paths = self._paths_no_keep_path
        return (root, [p.format(product=self.product) for p in paths])

    @contextmanager
    def get_writer(self, **kwargs):
        with ResignJarWriter(self, **kwargs) as writer:
            yield writer

    def process_package_artifact(self, filename, processed_filename):
        tempdir = tempfile.mkdtemp()
        oldcwd = os.getcwd()
        try:
            self.log(
                logging.DEBUG,
                "artifact",
                {"tempdir": tempdir},
                "Unpacking DMG into {tempdir}",
            )
            if self._substs["HOST_OS_ARCH"] == "Linux":
                # This is a cross build, use hfsplus and dmg tools to extract the dmg.
                os.chdir(tempdir)
                with open(os.devnull, "wb") as devnull:
                    subprocess.check_call(
                        [
                            self._substs["DMG_TOOL"],
                            "extract",
                            filename,
                            "extracted_img",
                        ],
                        stdout=devnull,
                    )
                    subprocess.check_call(
                        [self._substs["HFS_TOOL"], "extracted_img", "extractall"],
                        stdout=devnull,
                    )
            else:
                mozinstall.install(filename, tempdir)

            bundle_dirs = glob.glob(mozpath.join(tempdir, "*.app"))
            if len(bundle_dirs) != 1:
                raise ValueError(
                    "Expected one source bundle, found: {}".format(bundle_dirs)
                )
            [source] = bundle_dirs

            # These get copied into dist/bin with the path, so "root/a/b/c" -> "dist/bin/a/b/c".
            paths_keep_path = [
                (
                    "Contents/Resources",
                    [
                        "browser/components/libbrowsercomps.dylib",
                        "dependentlibs.list",
                        # 'firefox',
                        "gmp-clearkey/0.1/libclearkey.dylib",
                        # 'gmp-fake/1.0/libfake.dylib',
                        # 'gmp-fakeopenh264/1.0/libfakeopenh264.dylib',
                    ],
                )
            ]

            with self.get_writer(file=processed_filename, compress_level=5) as writer:
                root, paths = self.paths_no_keep_path
                finder = UnpackFinder(mozpath.join(source, root))
                for path in paths:
                    for p, f in finder.find(path):
                        self.log(
                            logging.DEBUG,
                            "artifact",
                            {"path": p},
                            "Adding {path} to processed archive",
                        )
                        destpath = mozpath.join("bin", os.path.basename(p))
                        writer.add(destpath.encode("utf-8"), f.open(), mode=f.mode)

                for root, paths in paths_keep_path:
                    finder = UnpackFinder(mozpath.join(source, root))
                    for path in paths:
                        for p, f in finder.find(path):
                            self.log(
                                logging.DEBUG,
                                "artifact",
                                {"path": p},
                                "Adding {path} to processed archive",
                            )
                            destpath = mozpath.join("bin", p)
                            writer.add(destpath.encode("utf-8"), f.open(), mode=f.mode)

        finally:
            os.chdir(oldcwd)
            try:
                shutil.rmtree(tempdir)
            except (OSError, IOError):
                self.log(
                    logging.WARN,
                    "artifact",
                    {"tempdir": tempdir},
                    "Unable to delete {tempdir}",
                )
                pass


class WinArtifactJob(ArtifactJob):
    package_re = r"public/build/target\.(zip|tar\.gz)$"
    product = "firefox"

    _package_artifact_patterns = {
        "{product}/dependentlibs.list",
        "{product}/**/*.dll",
        "{product}/*.exe",
        "{product}/*.tlb",
    }

    @property
    def package_artifact_patterns(self):
        return {p.format(product=self.product) for p in self._package_artifact_patterns}

    # These are a subset of TEST_HARNESS_BINS in testing/mochitest/Makefile.in.
    test_artifact_patterns = {
        ("bin/BadCertAndPinningServer.exe", ("bin", "bin")),
        ("bin/DelegatedCredentialsServer.exe", ("bin", "bin")),
        ("bin/EncryptedClientHelloServer.exe", ("bin", "bin")),
        ("bin/FaultyServer.exe", ("bin", "bin")),
        ("bin/GenerateOCSPResponse.exe", ("bin", "bin")),
        ("bin/OCSPStaplingServer.exe", ("bin", "bin")),
        ("bin/SanctionsTestServer.exe", ("bin", "bin")),
        ("bin/certutil.exe", ("bin", "bin")),
        ("bin/geckodriver.exe", ("bin", "bin")),
        ("bin/minidumpwriter.exe", ("bin", "bin")),
        ("bin/pk12util.exe", ("bin", "bin")),
        ("bin/screenshot.exe", ("bin", "bin")),
        ("bin/ssltunnel.exe", ("bin", "bin")),
        ("bin/xpcshell.exe", ("bin", "bin")),
        ("bin/http3server.exe", ("bin", "bin")),
        ("bin/content_analysis_sdk_agent.exe", ("bin", "bin")),
        ("bin/plugins/gmp-*/*/*", ("bin/plugins", "bin")),
        ("bin/plugins/*", ("bin/plugins", "plugins")),
        ("bin/components/*", ("bin/components", "bin/components")),
    }

    def process_package_artifact(self, filename, processed_filename):
        added_entry = False
        with self.get_writer(file=processed_filename, compress_level=5) as writer:
            for p, f in UnpackFinder(JarFinder(filename, JarReader(filename))):
                if not any(
                    mozpath.match(p, pat) for pat in self.package_artifact_patterns
                ):
                    continue

                # strip off the relative "firefox/" bit from the path:
                basename = mozpath.relpath(p, self.product)
                basename = mozpath.join("bin", basename)
                self.log(
                    logging.DEBUG,
                    "artifact",
                    {"basename": basename},
                    "Adding {basename} to processed archive",
                )
                writer.add(basename.encode("utf-8"), f.open(), mode=f.mode)
                added_entry = True

        if not added_entry:
            raise ValueError(
                'Archive format changed! No pattern from "{patterns}"'
                "matched an archive path.".format(patterns=self.artifact_patterns)
            )


class ThunderbirdMixin(object):
    trust_domain = "comm"
    product = "thunderbird"
    try_tree = "try-comm-central"
    default_candidate_trees = [
        "releases/comm-release",
    ]
    nightly_candidate_trees = [
        "comm-central",
    ]
    beta_candidate_trees = [
        "releases/comm-beta",
    ]
    # The list below list should be updated when we have new ESRs.
    esr_candidate_trees = [
        "releases/comm-esr115",
    ]


class LinuxThunderbirdArtifactJob(ThunderbirdMixin, LinuxArtifactJob):
    pass


class MacThunderbirdArtifactJob(ThunderbirdMixin, MacArtifactJob):
    pass


class WinThunderbirdArtifactJob(ThunderbirdMixin, WinArtifactJob):
    pass


def startswithwhich(s, prefixes):
    for prefix in prefixes:
        if s.startswith(prefix):
            return prefix


MOZ_JOB_DETAILS = {
    j: {
        "android": AndroidArtifactJob,
        "linux": LinuxArtifactJob,
        "macosx": MacArtifactJob,
        "win": WinArtifactJob,
    }[startswithwhich(j, ("android", "linux", "macosx", "win"))]
    for j in JOB_CHOICES
}
COMM_JOB_DETAILS = {
    j: {
        "android": None,
        "linux": LinuxThunderbirdArtifactJob,
        "macosx": MacThunderbirdArtifactJob,
        "win": WinThunderbirdArtifactJob,
    }[startswithwhich(j, ("android", "linux", "macosx", "win"))]
    for j in JOB_CHOICES
}


def cachedmethod(cachefunc):
    """Decorator to wrap a class or instance method with a memoizing callable that
    saves results in a (possibly shared) cache.
    """

    def decorator(method):
        def wrapper(self, *args, **kwargs):
            mapping = cachefunc(self)
            if mapping is None:
                return method(self, *args, **kwargs)
            key = (method.__name__, args, tuple(sorted(kwargs.items())))
            try:
                value = mapping[key]
                return value
            except KeyError:
                pass
            result = method(self, *args, **kwargs)
            mapping[key] = result
            return result

        return functools.update_wrapper(wrapper, method)

    return decorator


class CacheManager(object):
    """Maintain an LRU cache.  Provide simple persistence, including support for
    loading and saving the state using a "with" block.  Allow clearing the cache
    and printing the cache for debugging.

    Provide simple logging.
    """

    def __init__(
        self,
        cache_dir,
        cache_name,
        cache_size,
        cache_callback=None,
        log=None,
        skip_cache=False,
    ):
        self._skip_cache = skip_cache
        self._cache = pylru.lrucache(cache_size, callback=cache_callback)
        self._cache_filename = mozpath.join(cache_dir, cache_name + "-cache.pickle")
        self._log = log
        mkdir(cache_dir, not_indexed=True)

    def log(self, *args, **kwargs):
        if self._log:
            self._log(*args, **kwargs)

    def load_cache(self):
        if self._skip_cache:
            self.log(
                logging.INFO, "artifact", {}, "Skipping cache: ignoring load_cache!"
            )
            return

        try:
            items = pickle.load(open(self._cache_filename, "rb"))
            for key, value in items:
                self._cache[key] = value
        except Exception as e:
            # Corrupt cache, perhaps?  Sadly, pickle raises many different
            # exceptions, so it's not worth trying to be fine grained here.
            # We ignore any exception, so the cache is effectively dropped.
            self.log(
                logging.INFO,
                "artifact",
                {"filename": self._cache_filename, "exception": repr(e)},
                "Ignoring exception unpickling cache file {filename}: {exception}",
            )
            pass

    def dump_cache(self):
        if self._skip_cache:
            self.log(
                logging.INFO, "artifact", {}, "Skipping cache: ignoring dump_cache!"
            )
            return

        ensureParentDir(self._cache_filename)
        pickle.dump(
            list(reversed(list(self._cache.items()))),
            open(self._cache_filename, "wb"),
            -1,
        )

    def clear_cache(self):
        if self._skip_cache:
            self.log(
                logging.INFO, "artifact", {}, "Skipping cache: ignoring clear_cache!"
            )
            return

        with self:
            self._cache.clear()

    def __enter__(self):
        self.load_cache()
        return self

    def __exit__(self, type, value, traceback):
        self.dump_cache()


class PushheadCache(CacheManager):
    """Helps map tree/revision pairs to parent pushheads according to the pushlog."""

    def __init__(self, cache_dir, log=None, skip_cache=False):
        CacheManager.__init__(
            self,
            cache_dir,
            "pushhead_cache",
            MAX_CACHED_TASKS,
            log=log,
            skip_cache=skip_cache,
        )

    @cachedmethod(operator.attrgetter("_cache"))
    def parent_pushhead_id(self, tree, revision):
        cset_url_tmpl = (
            "https://hg.mozilla.org/{tree}/json-pushes?"
            "changeset={changeset}&version=2&tipsonly=1"
        )
        req = requests.get(
            cset_url_tmpl.format(tree=tree, changeset=revision),
            headers={"Accept": "application/json"},
        )
        if req.status_code not in range(200, 300):
            raise ValueError
        result = req.json()
        [found_pushid] = result["pushes"].keys()
        return int(found_pushid)

    @cachedmethod(operator.attrgetter("_cache"))
    def pushid_range(self, tree, start, end):
        pushid_url_tmpl = (
            "https://hg.mozilla.org/{tree}/json-pushes?"
            "startID={start}&endID={end}&version=2&tipsonly=1"
        )

        req = requests.get(
            pushid_url_tmpl.format(tree=tree, start=start, end=end),
            headers={"Accept": "application/json"},
        )
        result = req.json()
        return [p["changesets"][-1] for p in result["pushes"].values()]


class TaskCache(CacheManager):
    """Map candidate pushheads to Task Cluster task IDs and artifact URLs."""

    def __init__(self, cache_dir, log=None, skip_cache=False):
        CacheManager.__init__(
            self,
            cache_dir,
            "artifact_url",
            MAX_CACHED_TASKS,
            log=log,
            skip_cache=skip_cache,
        )

    @cachedmethod(operator.attrgetter("_cache"))
    def artifacts(self, tree, job, artifact_job_class, rev):
        # Grab the second part of the repo name, which is generally how things
        # are indexed. Eg: 'integration/autoland' is indexed as
        # 'autoland'
        tree = tree.split("/")[1] if "/" in tree else tree

        if job.endswith("-opt"):
            tree += ".shippable"

        namespace = "{trust_domain}.v2.{tree}.revision.{rev}.{product}.{job}".format(
            trust_domain=artifact_job_class.trust_domain,
            rev=rev,
            tree=tree,
            product=artifact_job_class.product,
            job=job,
        )
        self.log(
            logging.DEBUG,
            "artifact",
            {"namespace": namespace},
            "Searching Taskcluster index with namespace: {namespace}",
        )
        try:
            taskId = find_task_id(namespace)
        except KeyError:
            # Not all revisions correspond to pushes that produce the job we
            # care about; and even those that do may not have completed yet.
            raise ValueError(
                "Task for {namespace} does not exist (yet)!".format(namespace=namespace)
            )

        return taskId, list_artifacts(taskId)


class Artifacts(object):
    """Maintain state to efficiently fetch build artifacts from a Firefox tree."""

    def __init__(
        self,
        tree,
        substs,
        defines,
        job=None,
        log=None,
        cache_dir=".",
        hg=None,
        git=None,
        skip_cache=False,
        topsrcdir=None,
        download_tests=True,
        download_symbols=False,
        download_maven_zip=False,
        no_process=False,
        mozbuild=None,
    ):
        if (hg and git) or (not hg and not git):
            raise ValueError("Must provide path to exactly one of hg and git")

        self._substs = substs
        self._defines = defines
        self._tree = tree
        self._job = job or self._guess_artifact_job()
        self._log = log
        self._hg = hg
        self._git = git
        self._cache_dir = cache_dir
        self._skip_cache = skip_cache
        self._topsrcdir = topsrcdir
        self._no_process = no_process

        app = self._substs.get("MOZ_BUILD_APP")
        job_details = COMM_JOB_DETAILS if app == "comm/mail" else MOZ_JOB_DETAILS

        try:
            cls = job_details[self._job]
            self._artifact_job = cls(
                log=self._log,
                download_tests=download_tests,
                download_symbols=download_symbols,
                download_maven_zip=download_maven_zip,
                substs=self._substs,
                mozbuild=mozbuild,
            )
        except KeyError:
            self.log(logging.INFO, "artifact", {"job": self._job}, "Unknown job {job}")
            raise KeyError("Unknown job")

        self._task_cache = TaskCache(
            self._cache_dir, log=self._log, skip_cache=self._skip_cache
        )
        self._artifact_cache = ArtifactCache(
            self._cache_dir, log=self._log, skip_cache=self._skip_cache
        )
        self._pushhead_cache = PushheadCache(
            self._cache_dir, log=self._log, skip_cache=self._skip_cache
        )

    def log(self, *args, **kwargs):
        if self._log:
            self._log(*args, **kwargs)

    def run_hg(self, *args, **kwargs):
        env = kwargs.get("env", {})
        env["HGPLAIN"] = "1"
        kwargs["universal_newlines"] = True
        return subprocess.check_output([self._hg] + list(args), **kwargs)

    def _guess_artifact_job(self):
        # Add the "-debug" suffix to the guessed artifact job name
        # if MOZ_DEBUG is enabled.
        if self._substs.get("MOZ_DEBUG"):
            target_suffix = "-debug"
        else:
            target_suffix = "-opt"

        if self._substs.get("MOZ_BUILD_APP", "") == "mobile/android":
            if self._substs["ANDROID_CPU_ARCH"] == "x86_64":
                return "android-x86_64" + target_suffix
            if self._substs["ANDROID_CPU_ARCH"] == "x86":
                return "android-x86" + target_suffix
            if self._substs["ANDROID_CPU_ARCH"] == "arm64-v8a":
                return "android-aarch64" + target_suffix
            return "android-arm" + target_suffix

        target_64bit = False
        if self._substs["target_cpu"] == "x86_64":
            target_64bit = True

        if self._defines.get("XP_LINUX", False):
            return ("linux64" if target_64bit else "linux") + target_suffix
        if self._defines.get("XP_WIN", False):
            if self._substs["target_cpu"] == "aarch64":
                return "win64-aarch64" + target_suffix
            return ("win64" if target_64bit else "win32") + target_suffix
        if self._defines.get("XP_MACOSX", False):
            # We only produce unified builds in automation, so the target_cpu
            # check is not relevant.
            return "macosx64" + target_suffix
        raise Exception("Cannot determine default job for |mach artifact|!")

    def _pushheads_from_rev(self, rev, count):
        """Queries hg.mozilla.org's json-pushlog for pushheads that are nearby
        ancestors or `rev`. Multiple trees are queried, as the `rev` may
        already have been pushed to multiple repositories. For each repository
        containing `rev`, the pushhead introducing `rev` and the previous
        `count` pushheads from that point are included in the output.
        """

        with self._pushhead_cache as pushhead_cache:
            found_pushids = {}

            search_trees = self._artifact_job.candidate_trees
            for tree in search_trees:
                self.log(
                    logging.DEBUG,
                    "artifact",
                    {"tree": tree, "rev": rev},
                    "Attempting to find a pushhead containing {rev} on {tree}.",
                )
                try:
                    pushid = pushhead_cache.parent_pushhead_id(tree, rev)
                    found_pushids[tree] = pushid
                except ValueError:
                    continue

            candidate_pushheads = collections.defaultdict(list)

            for tree, pushid in six.iteritems(found_pushids):
                end = pushid
                start = pushid - NUM_PUSHHEADS_TO_QUERY_PER_PARENT

                self.log(
                    logging.DEBUG,
                    "artifact",
                    {
                        "tree": tree,
                        "pushid": pushid,
                        "num": NUM_PUSHHEADS_TO_QUERY_PER_PARENT,
                    },
                    "Retrieving the last {num} pushheads starting with id {pushid} on {tree}",
                )
                for pushhead in pushhead_cache.pushid_range(tree, start, end):
                    candidate_pushheads[pushhead].append(tree)

        return candidate_pushheads

    def _get_hg_revisions_from_git(self):
        rev_list = subprocess.check_output(
            [
                self._git,
                "rev-list",
                "--topo-order",
                "--max-count={num}".format(num=NUM_REVISIONS_TO_QUERY),
                "HEAD",
            ],
            universal_newlines=True,
            cwd=self._topsrcdir,
        )

        hg_hash_list = subprocess.check_output(
            [self._git, "cinnabar", "git2hg"] + rev_list.splitlines(),
            universal_newlines=True,
            cwd=self._topsrcdir,
        )

        zeroes = "0" * 40

        hashes = []
        for hg_hash in hg_hash_list.splitlines():
            hg_hash = hg_hash.strip()
            if not hg_hash or hg_hash == zeroes:
                continue
            hashes.append(hg_hash)
        if not hashes:
            msg = (
                "Could not list any recent revisions in your clone. Does "
                "your clone have git-cinnabar metadata? If not, consider "
                "re-cloning using the directions at "
                "https://github.com/glandium/git-cinnabar/wiki/Mozilla:-A-"
                "git-workflow-for-Gecko-development"
            )
            try:
                subprocess.check_output(
                    [
                        self._git,
                        "cat-file",
                        "-e",
                        "05e5d33a570d48aed58b2d38f5dfc0a7870ff8d3^{commit}",
                    ],
                    stderr=subprocess.STDOUT,
                )
                # If the above commit exists, we're probably in a clone of
                # `gecko-dev`, and this documentation applies.
                msg += (
                    "\n\nNOTE: Consider following the directions "
                    "at https://github.com/glandium/git-cinnabar/wiki/"
                    "Mozilla:-Using-a-git-clone-of-gecko%E2%80%90dev-"
                    "to-push-to-mercurial to resolve this issue."
                )
            except subprocess.CalledProcessError:
                pass
            raise UserError(msg)
        return hashes

    def _get_recent_public_revisions(self):
        """Returns recent ancestors of the working parent that are likely to
        to be known to Mozilla automation.

        If we're using git, retrieves hg revisions from git-cinnabar.
        """
        if self._git:
            return self._get_hg_revisions_from_git()

        # Mercurial updated the ordering of "last" in 4.3. We use revision
        # numbers to order here to accommodate multiple versions of hg.
        last_revs = self.run_hg(
            "log",
            "--template",
            "{rev}:{node}\n",
            "-r",
            "last(public() and ::., {num})".format(num=NUM_REVISIONS_TO_QUERY),
            cwd=self._topsrcdir,
        ).splitlines()

        if len(last_revs) == 0:
            raise UserError(
                """\
There are no public revisions.
This can happen if the repository is created from bundle file and never pulled
from remote.  Please run `hg pull` and build again.
https://firefox-source-docs.mozilla.org/contributing/vcs/mercurial_bundles.html
"""
            )

        self.log(
            logging.DEBUG,
            "artifact",
            {"len": len(last_revs)},
            "hg suggested {len} candidate revisions",
        )

        def to_pair(line):
            rev, node = line.split(":", 1)
            return (int(rev), node)

        pairs = [to_pair(r) for r in last_revs]

        # Python's tuple sort orders by first component: here, the (local)
        # revision number.
        nodes = [pair[1] for pair in sorted(pairs, reverse=True)]

        for node in nodes[:20]:
            self.log(
                logging.DEBUG,
                "artifact",
                {"node": node},
                "hg suggested candidate revision: {node}",
            )
        self.log(
            logging.DEBUG,
            "artifact",
            {"remaining": max(0, len(nodes) - 20)},
            "hg suggested candidate revision: and {remaining} more",
        )

        return nodes

    def _find_pushheads(self):
        """Returns an iterator of recent pushhead revisions, starting with the
        working parent.
        """

        last_revs = self._get_recent_public_revisions()
        candidate_pushheads = self._pushheads_from_rev(
            last_revs[0].rstrip(), NUM_PUSHHEADS_TO_QUERY_PER_PARENT
        )
        count = 0
        for rev in last_revs:
            rev = rev.rstrip()
            if not rev:
                continue
            if rev not in candidate_pushheads:
                continue
            count += 1
            yield candidate_pushheads[rev], rev

        if not count:
            raise Exception(
                "Could not find any candidate pushheads in the last {num} revisions.\n"
                "Search started with {rev}, which must be known to Mozilla automation.\n\n"
                "see https://firefox-source-docs.mozilla.org/contributing/build/artifact_builds.html".format(  # noqa E501
                    rev=last_revs[0], num=NUM_PUSHHEADS_TO_QUERY_PER_PARENT
                )
            )

    def find_pushhead_artifacts(self, task_cache, job, tree, pushhead):
        try:
            taskId, artifacts = task_cache.artifacts(
                tree, job, self._artifact_job.__class__, pushhead
            )
        except ValueError:
            return None

        urls = []
        for artifact_name in self._artifact_job.find_candidate_artifacts(artifacts):
            url = get_artifact_url(taskId, artifact_name)
            urls.append(url)
        if urls:
            self.log(
                logging.DEBUG,
                "artifact",
                {"pushhead": pushhead, "tree": tree},
                "Installing from remote pushhead {pushhead} on {tree}",
            )
            return urls
        return None

    def install_from_file(self, filename, distdir):
        self.log(
            logging.DEBUG,
            "artifact",
            {"filename": filename},
            "Installing from {filename}",
        )

        # Copy all .so files, avoiding modification where possible.
        ensureParentDir(mozpath.join(distdir, ".dummy"))

        if self._no_process:
            orig_basename = os.path.basename(filename)
            # Turn 'HASH-target...' into 'target...' if possible.  It might not
            # be possible if the file is given directly on the command line.
            before, _sep, after = orig_basename.rpartition("-")
            if re.match(r"[0-9a-fA-F]{16}$", before):
                orig_basename = after
            path = mozpath.join(distdir, orig_basename)
            with FileAvoidWrite(path, readmode="rb") as fh:
                shutil.copyfileobj(open(filename, mode="rb"), fh)
            self.log(
                logging.DEBUG,
                "artifact",
                {"path": path},
                "Copied unprocessed artifact: to {path}",
            )
            return

        # Do we need to post-process?
        processed_filename = filename + PROCESSED_SUFFIX

        if self._skip_cache and os.path.exists(processed_filename):
            self.log(
                logging.INFO,
                "artifact",
                {"path": processed_filename},
                "Skipping cache: removing cached processed artifact {path}",
            )
            os.remove(processed_filename)

        if not os.path.exists(processed_filename):
            self.log(
                logging.DEBUG,
                "artifact",
                {"filename": filename},
                "Processing contents of {filename}",
            )
            self.log(
                logging.DEBUG,
                "artifact",
                {"processed_filename": processed_filename},
                "Writing processed {processed_filename}",
            )
            try:
                self._artifact_job.process_artifact(filename, processed_filename)
            except Exception as e:
                # Delete the partial output of failed processing.
                try:
                    os.remove(processed_filename)
                except FileNotFoundError:
                    pass
                raise e

        self._artifact_cache._persist_limit.register_file(processed_filename)

        self.log(
            logging.DEBUG,
            "artifact",
            {"processed_filename": processed_filename},
            "Installing from processed {processed_filename}",
        )

        with zipfile.ZipFile(processed_filename) as zf:
            for info in zf.infolist():
                n = mozpath.join(distdir, info.filename)
                fh = FileAvoidWrite(n, readmode="rb")
                shutil.copyfileobj(zf.open(info), fh)
                file_existed, file_updated = fh.close()
                self.log(
                    logging.DEBUG,
                    "artifact",
                    {
                        "updating": "Updating" if file_updated else "Not updating",
                        "filename": n,
                    },
                    "{updating} {filename}",
                )
                if not file_existed or file_updated:
                    # Libraries and binaries may need to be marked executable,
                    # depending on platform.
                    perms = (
                        info.external_attr >> 16
                    )  # See http://stackoverflow.com/a/434689.
                    perms |= (
                        stat.S_IWUSR | stat.S_IRUSR | stat.S_IRGRP | stat.S_IROTH
                    )  # u+w, a+r.
                    os.chmod(n, perms)
        return 0

    def install_from_url(self, url, distdir):
        self.log(logging.DEBUG, "artifact", {"url": url}, "Installing from {url}")
        filename = self._artifact_cache.fetch(url)
        return self.install_from_file(filename, distdir)

    def _install_from_hg_pushheads(self, hg_pushheads, distdir):
        """Iterate pairs (hg_hash, {tree-set}) associating hg revision hashes
        and tree-sets they are known to be in, trying to download and
        install from each.
        """

        urls = None
        count = 0
        # with blocks handle handle persistence.
        with self._task_cache as task_cache:
            for trees, hg_hash in hg_pushheads:
                for tree in trees:
                    count += 1
                    self.log(
                        logging.DEBUG,
                        "artifact",
                        {"hg_hash": hg_hash, "tree": tree},
                        "Trying to find artifacts for hg revision {hg_hash} on tree {tree}.",
                    )
                    urls = self.find_pushhead_artifacts(
                        task_cache, self._job, tree, hg_hash
                    )
                    if urls:
                        for url in urls:
                            if self.install_from_url(url, distdir):
                                return 1
                        return 0

        self.log(
            logging.ERROR,
            "artifact",
            {"count": count},
            "Tried {count} pushheads, no built artifacts found.",
        )
        return 1

    def install_from_recent(self, distdir):
        hg_pushheads = self._find_pushheads()
        return self._install_from_hg_pushheads(hg_pushheads, distdir)

    def install_from_revset(self, revset, distdir):
        revision = None
        try:
            if self._hg:
                revision = self.run_hg(
                    "log", "--template", "{node}\n", "-r", revset, cwd=self._topsrcdir
                ).strip()
            elif self._git:
                revset = subprocess.check_output(
                    [self._git, "rev-parse", "%s^{commit}" % revset],
                    stderr=open(os.devnull, "w"),
                    universal_newlines=True,
                    cwd=self._topsrcdir,
                ).strip()
            else:
                # Fallback to the exception handling case from both hg and git
                raise subprocess.CalledProcessError()
        except subprocess.CalledProcessError:
            # If the mercurial of git commands above failed, it means the given
            # revset is not known locally to the VCS. But if the revset looks
            # like a complete sha1, assume it is a mercurial sha1 that hasn't
            # been pulled, and use that.
            if re.match(r"^[A-Fa-f0-9]{40}$", revset):
                revision = revset

        if revision is None and self._git:
            revision = subprocess.check_output(
                [self._git, "cinnabar", "git2hg", revset],
                universal_newlines=True,
                cwd=self._topsrcdir,
            ).strip()

        if revision == "0" * 40 or revision is None:
            raise ValueError(
                "revision specification must resolve to a commit known to hg"
            )
        if len(revision.split("\n")) != 1:
            raise ValueError(
                "revision specification must resolve to exactly one commit"
            )

        self.log(
            logging.INFO,
            "artifact",
            {"revset": revset, "revision": revision},
            "Will only accept artifacts from a pushhead at {revision} "
            '(matched revset "{revset}").',
        )
        # Include try in our search to allow pulling from a specific push.
        pushheads = [
            (
                self._artifact_job.candidate_trees + [self._artifact_job.try_tree],
                revision,
            )
        ]
        return self._install_from_hg_pushheads(pushheads, distdir)

    def install_from_task(self, taskId, distdir):
        artifacts = list_artifacts(taskId)

        urls = []
        for artifact_name in self._artifact_job.find_candidate_artifacts(artifacts):
            url = get_artifact_url(taskId, artifact_name)
            urls.append(url)
        if not urls:
            raise ValueError(
                "Task {taskId} existed, but no artifacts found!".format(taskId=taskId)
            )
        for url in urls:
            if self.install_from_url(url, distdir):
                return 1
        return 0

    def install_from(self, source, distdir):
        """Install artifacts from a ``source`` into the given ``distdir``."""
        if (source and os.path.isfile(source)) or "MOZ_ARTIFACT_FILE" in os.environ:
            source = source or os.environ["MOZ_ARTIFACT_FILE"]
            for source in source.split(os.pathsep):
                ret = self.install_from_file(source, distdir)
                if ret:
                    return ret
            return 0

        if (source and urlparse(source).scheme) or "MOZ_ARTIFACT_URL" in os.environ:
            source = source or os.environ["MOZ_ARTIFACT_URL"]
            for source in source.split():
                ret = self.install_from_url(source, distdir)
                if ret:
                    return ret
            return 0

        if source or "MOZ_ARTIFACT_REVISION" in os.environ:
            source = source or os.environ["MOZ_ARTIFACT_REVISION"]
            return self.install_from_revset(source, distdir)

        for var in (
            "MOZ_ARTIFACT_TASK_%s" % self._job.upper().replace("-", "_"),
            "MOZ_ARTIFACT_TASK",
        ):
            if var in os.environ:
                return self.install_from_task(os.environ[var], distdir)

        return self.install_from_recent(distdir)

    def clear_cache(self):
        self.log(logging.INFO, "artifact", {}, "Deleting cached artifacts and caches.")
        self._task_cache.clear_cache()
        self._artifact_cache.clear_cache()
        self._pushhead_cache.clear_cache()
