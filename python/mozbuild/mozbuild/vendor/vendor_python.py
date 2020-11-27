# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import shutil
import subprocess

import mozfile
import mozpack.path as mozpath
from mozbuild.base import MozbuildObject
from mozfile import TemporaryDirectory
from mozpack.files import FileFinder


class VendorPython(MozbuildObject):
    def vendor(self, packages=None, keep_extra_files=False):
        self.populate_logger()
        self.log_manager.enable_unstructured()

        vendor_dir = mozpath.join(self.topsrcdir, os.path.join("third_party", "python"))

        packages = packages or []

        self.activate_virtualenv()
        pip_compile = os.path.join(self.virtualenv_manager.bin_path, "pip-compile")
        if not os.path.exists(pip_compile):
            path = os.path.normpath(
                os.path.join(self.topsrcdir, "third_party", "python", "pip-tools")
            )
            self.virtualenv_manager.install_pip_package(path, vendored=True)
        spec = os.path.join(vendor_dir, "requirements.in")
        requirements = os.path.join(vendor_dir, "requirements.txt")

        with TemporaryDirectory() as spec_dir:
            tmpspec = "requirements-mach-vendor-python.in"
            tmpspec_absolute = os.path.join(spec_dir, tmpspec)
            shutil.copyfile(spec, tmpspec_absolute)
            self._update_packages(tmpspec_absolute, packages)

            # resolve the dependencies and update requirements.txt
            subprocess.check_output(
                [
                    pip_compile,
                    tmpspec,
                    "--no-header",
                    "--no-index",
                    "--output-file",
                    requirements,
                    "--generate-hashes",
                ],
                # Run pip-compile from within the temporary directory so that the "via"
                # annotations don't have the non-deterministic temporary path in them.
                cwd=spec_dir,
            )

            with TemporaryDirectory() as tmp:
                # use requirements.txt to download archived source distributions of all packages
                self.virtualenv_manager._run_pip(
                    [
                        "download",
                        "-r",
                        requirements,
                        "--no-deps",
                        "--dest",
                        tmp,
                        "--no-binary",
                        ":all:",
                        "--disable-pip-version-check",
                    ]
                )
                self._extract(tmp, vendor_dir, keep_extra_files)

            shutil.copyfile(tmpspec_absolute, spec)
            self.repository.add_remove_files(vendor_dir)

    def _update_packages(self, spec, packages):
        for package in packages:
            if not all(package.partition("==")):
                raise Exception(
                    "Package {} must be in the format name==version".format(package)
                )

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

        for package in packages:
            name, version = package.split("==")
            requirements[name] = version, []

        with open(spec, "w") as f:
            for name, (version, comments) in sorted(requirements.items()):
                if comments:
                    f.write("{}\n".format("\n".join(comments)))
                f.write("{}=={}\n".format(name, version))

    def _extract(self, src, dest, keep_extra_files=False):
        """extract source distribution into vendor directory"""

        ignore = ()
        if not keep_extra_files:
            ignore = (
                "*/doc",
                "*/docs",
                "*/test",
                "*/tests",
            )
        finder = FileFinder(src)
        for path, _ in finder.find("*"):
            base, ext = os.path.splitext(path)
            # packages extract into package-version directory name and we strip the version
            tld = mozfile.extract(os.path.join(finder.base, path), dest, ignore=ignore)[
                0
            ]
            target = os.path.join(dest, tld.rpartition("-")[0])
            mozfile.remove(target)  # remove existing version of vendored package
            mozfile.move(tld, target)

            # If any files inside the vendored package were symlinks, turn them into normal files
            # because hg.mozilla.org forbids symlinks in the repository.
            link_finder = FileFinder(target)
            for _, f in link_finder.find("**"):
                if os.path.islink(f.path):
                    link_target = os.path.realpath(f.path)
                    os.unlink(f.path)
                    shutil.copyfile(link_target, f.path)
