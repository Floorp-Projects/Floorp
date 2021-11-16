# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import shutil
import subprocess
from pathlib import Path

import mozfile
import mozpack.path as mozpath
from mozbuild.base import MozbuildObject
from mozfile import TemporaryDirectory
from mozpack.files import FileFinder


class VendorPython(MozbuildObject):
    def vendor(self, keep_extra_files=False):
        self.populate_logger()
        self.log_manager.enable_unstructured()

        vendor_dir = mozpath.join(self.topsrcdir, os.path.join("third_party", "python"))
        self.activate_virtualenv()
        spec = os.path.join(vendor_dir, "requirements.in")
        requirements = os.path.join(vendor_dir, "requirements.txt")

        with TemporaryDirectory() as spec_dir:
            tmpspec = "requirements-mach-vendor-python.in"
            tmpspec_absolute = os.path.join(spec_dir, tmpspec)
            shutil.copyfile(spec, tmpspec_absolute)
            self._update_packages(tmpspec_absolute)

            tmp_requirements_absolute = os.path.join(spec_dir, "requirements.txt")
            # Copy the existing "requirements.txt" file so that the versions
            # of transitive dependencies aren't implicitly changed.
            shutil.copy(requirements, tmp_requirements_absolute)

            # resolve the dependencies and update requirements.txt.
            # "--allow-unsafe" is required to vendor pip and setuptools.
            subprocess.check_output(
                [
                    self.virtualenv_manager.python_path,
                    "-m",
                    "piptools",
                    "compile",
                    tmpspec,
                    "--no-header",
                    "--no-emit-index-url",
                    "--output-file",
                    tmp_requirements_absolute,
                    "--generate-hashes",
                    "--allow-unsafe",
                ],
                # Run pip-compile from within the temporary directory so that the "via"
                # annotations don't have the non-deterministic temporary path in them.
                cwd=spec_dir,
            )

            with TemporaryDirectory() as tmp:
                # use requirements.txt to download archived source distributions of all packages
                subprocess.check_call(
                    [
                        self.virtualenv_manager.python_path,
                        "-m",
                        "pip",
                        "download",
                        "-r",
                        tmp_requirements_absolute,
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

            shutil.copyfile(tmpspec_absolute, spec)
            shutil.copy(tmp_requirements_absolute, requirements)
            self.repository.add_remove_files(vendor_dir)

    def _update_packages(self, spec):
        requirements = {}
        with open(spec, "r") as f:
            comments = []
            for line in f.readlines():
                line = line.strip()
                if not line or line.startswith("#"):
                    comments.append(line)
                    continue
                name, version = line.split("==")
                requirements[name] = version, comments
                comments = []

        with open(spec, "w") as f:
            for name, (version, comments) in sorted(requirements.items()):
                if comments:
                    f.write("{}\n".format("\n".join(comments)))
                f.write("{}=={}\n".format(name, version))

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


def _purge_vendor_dir(vendor_dir):
    excluded_packages = [
        # dlmanager's package on PyPI only has metadata, but is missing the code.
        # https://github.com/parkouss/dlmanager/issues/1
        "dlmanager",
        # gyp's package on PyPI doesn't have any downloadable files.
        "gyp",
        # We manage installing "virtualenv" package manually, and we have a custom
        # "virtualenv.py" entry module.
        "virtualenv",
        # The moz.build file isn't a vendored module, so don't delete it.
        "moz.build",
    ]

    for child in Path(vendor_dir).iterdir():
        if child.name not in excluded_packages:
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
