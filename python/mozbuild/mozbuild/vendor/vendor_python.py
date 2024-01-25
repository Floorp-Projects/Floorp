# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
import subprocess
import sys
from pathlib import Path

import mozfile
from mozfile import TemporaryDirectory
from mozpack.files import FileFinder

from mozbuild.base import MozbuildObject

EXCLUDED_PACKAGES = {
    # dlmanager's package on PyPI only has metadata, but is missing the code.
    # https://github.com/parkouss/dlmanager/issues/1
    "dlmanager",
    # gyp's package on PyPI doesn't have any downloadable files.
    "gyp",
    # We keep some wheels vendored in "_venv" for use in Mozharness
    "_venv",
    # We manage vendoring "vsdownload" with a moz.yaml file (there is no module
    # on PyPI).
    "vsdownload",
    # The moz.build file isn't a vendored module, so don't delete it.
    "moz.build",
    "requirements.in",
    # The ansicon package contains DLLs and we don't want to arbitrarily vendor
    # them since they could be unsafe. This module should rarely be used in practice
    # (it's a fallback for old versions of windows). We've intentionally vendored a
    # modified 'dummy' version of it so that the dependency checks still succeed, but
    # if it ever is attempted to be used, it will fail gracefully.
    "ansicon",
    # Non-permanent exclusion of pip to avoid vendoring removing a solution to
    # pkgutil's deprecation. See https://bugzilla.mozilla.org/show_bug.cgi?id=1857470
    # for more information.
    "pip",
}


class VendorPython(MozbuildObject):
    def __init__(self, *args, **kwargs):
        MozbuildObject.__init__(self, *args, virtualenv_name="vendor", **kwargs)

    def vendor(self, keep_extra_files=False):
        from mach.python_lockfile import PoetryHandle

        self.populate_logger()
        self.log_manager.enable_unstructured()

        vendor_dir = Path(self.topsrcdir) / "third_party" / "python"
        requirements_in = vendor_dir / "requirements.in"
        poetry_lockfile = vendor_dir / "poetry.lock"
        _sort_requirements_in(requirements_in)

        with TemporaryDirectory() as work_dir:
            work_dir = Path(work_dir)
            poetry = PoetryHandle(work_dir)
            poetry.add_requirements_in_file(requirements_in)
            poetry.reuse_existing_lockfile(poetry_lockfile)
            lockfiles = poetry.generate_lockfiles(do_update=False)

            # Vendoring packages is only viable if it's possible to have a single
            # set of packages that work regardless of which environment they're used in.
            # So, we scrub environment markers, so that we essentially ask pip to
            # download "all dependencies for all environments". Pip will then either
            # fetch them as requested, or intelligently raise an error if that's not
            # possible (e.g.: if different versions of Python would result in different
            # packages/package versions).
            pip_lockfile_without_markers = work_dir / "requirements.no-markers.txt"
            shutil.copy(str(lockfiles.pip_lockfile), str(pip_lockfile_without_markers))
            remove_environment_markers_from_requirements_txt(
                pip_lockfile_without_markers
            )

            with TemporaryDirectory() as tmp:
                # use requirements.txt to download archived source distributions of all
                # packages
                subprocess.check_call(
                    [
                        sys.executable,
                        "-m",
                        "pip",
                        "download",
                        "-r",
                        str(pip_lockfile_without_markers),
                        "--no-deps",
                        "--dest",
                        tmp,
                        "--abi",
                        "none",
                        "--platform",
                        "any",
                    ]
                )
                _purge_vendor_dir(vendor_dir)
                self._extract(tmp, vendor_dir, keep_extra_files)

            requirements_out = vendor_dir / "requirements.txt"

            # since requirements.out and poetry.lockfile are both outputs from
            # third party code, they may contain carriage returns on Windows. We
            # should strip the carriage returns to maintain consistency in our output
            # regardless of which platform is doing the vendoring. We can do this and
            # the copying at the same time to minimize reads and writes.
            _copy_file_strip_carriage_return(lockfiles.pip_lockfile, requirements_out)
            _copy_file_strip_carriage_return(lockfiles.poetry_lockfile, poetry_lockfile)
            self.repository.add_remove_files(vendor_dir)

    def _extract(self, src, dest, keep_extra_files=False):
        """extract source distribution into vendor directory"""

        ignore = ()
        if not keep_extra_files:
            ignore = ("*/doc", "*/docs", "*/test", "*/tests", "**/.git")
        finder = FileFinder(src)
        for archive, _ in finder.find("*"):
            _, ext = os.path.splitext(archive)
            archive_path = os.path.join(finder.base, archive)
            if ext == ".whl":
                # Archive is named like "$package-name-1.0-py2.py3-none-any.whl", and should
                # have four dashes that aren't part of the package name.
                package_name, version, spec, abi, platform_and_suffix = archive.rsplit(
                    "-", 4
                )

                if package_name in EXCLUDED_PACKAGES:
                    print(
                        f"'{package_name}' is on the exclusion list and will not be vendored."
                    )
                    continue

                target_package_dir = os.path.join(dest, package_name)
                os.mkdir(target_package_dir)

                # Extract all the contents of the wheel into the package subdirectory.
                # We're expecting at least a code directory and a ".dist-info" directory,
                # though there may be a ".data" directory as well.
                mozfile.extract(archive_path, target_package_dir, ignore=ignore)
                _denormalize_symlinks(target_package_dir)
            else:
                # Archive is named like "$package-name-1.0.tar.gz", and the rightmost
                # dash should separate the package name from the rest of the archive
                # specifier.
                package_name, archive_postfix = archive.rsplit("-", 1)
                package_dir = os.path.join(dest, package_name)

                if package_name in EXCLUDED_PACKAGES:
                    print(
                        f"'{package_name}' is on the exclusion list and will not be vendored."
                    )
                    continue

                # The archive should only contain one top-level directory, which has
                # the source files. We extract this directory directly to
                # the vendor directory.
                extracted_files = mozfile.extract(archive_path, dest, ignore=ignore)
                assert len(extracted_files) == 1
                extracted_package_dir = extracted_files[0]

                # The extracted package dir includes the version in the name,
                # which we don't we don't want.
                mozfile.move(extracted_package_dir, package_dir)
                _denormalize_symlinks(package_dir)


def _sort_requirements_in(requirements_in: Path):
    requirements = {}
    with requirements_in.open(mode="r", newline="\n") as f:
        comments = []
        for line in f.readlines():
            line = line.strip()
            if not line or line.startswith("#"):
                comments.append(line)
                continue
            name, version = line.split("==")
            requirements[name] = version, comments
            comments = []

    with requirements_in.open(mode="w", newline="\n") as f:
        for name, (version, comments) in sorted(requirements.items()):
            if comments:
                f.write("{}\n".format("\n".join(comments)))
            f.write("{}=={}\n".format(name, version))


def remove_environment_markers_from_requirements_txt(requirements_txt: Path):
    with requirements_txt.open(mode="r", newline="\n") as f:
        lines = f.readlines()
    markerless_lines = []
    continuation_token = " \\"
    for line in lines:
        line = line.rstrip()

        if not line.startswith(" ") and not line.startswith("#") and ";" in line:
            has_continuation_token = line.endswith(continuation_token)
            # The first line of each requirement looks something like:
            #   package-name==X.Y; python_version>=3.7
            # We can scrub the environment marker by splitting on the semicolon
            line = line.split(";")[0]
            if has_continuation_token:
                line += continuation_token
            markerless_lines.append(line)
        else:
            markerless_lines.append(line)

    with requirements_txt.open(mode="w", newline="\n") as f:
        f.write("\n".join(markerless_lines))


def _purge_vendor_dir(vendor_dir):
    for child in Path(vendor_dir).iterdir():
        if child.name not in EXCLUDED_PACKAGES:
            mozfile.remove(str(child))


def _denormalize_symlinks(target):
    # If any files inside the vendored package were symlinks, turn them into normal files
    # because hg.mozilla.org forbids symlinks in the repository.
    link_finder = FileFinder(target)
    for _, f in link_finder.find("**"):
        if os.path.islink(f.path):
            link_target = os.path.realpath(f.path)
            os.unlink(f.path)
            shutil.copyfile(link_target, f.path)


def _copy_file_strip_carriage_return(file_src: Path, file_dst):
    shutil.copyfileobj(file_src.open(mode="r"), file_dst.open(mode="w", newline="\n"))
