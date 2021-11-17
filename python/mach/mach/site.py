# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This file contains code for managing the Python import scope for Mach. This
# generally involves populating a Python virtualenv.

from __future__ import absolute_import, print_function, unicode_literals

import functools
import json
import os
import platform
import shutil
import subprocess
import sys

from mach.requirements import MachEnvRequirements

PTH_FILENAME = "mach.pth"
METADATA_FILENAME = "moz_virtualenv_metadata.json"


class MozSiteMetadataOutOfDateError(Exception):
    pass


class MozSiteMetadata:
    """Details about a Moz-managed python environment"""

    def __init__(self, hex_version, site_name, prefix):
        self.hex_version = hex_version
        self.site_name = site_name
        self.prefix = prefix

    def write(self):
        raw = {"hex_version": self.hex_version, "virtualenv_name": self.site_name}
        with open(os.path.join(self.prefix, METADATA_FILENAME), "w") as file:
            json.dump(raw, file)

    def __eq__(self, other):
        return (
            type(self) == type(other)
            and self.hex_version == other.hex_version
            and self.site_name == other.site_name
        )

    @classmethod
    def from_runtime(cls):
        return cls.from_path(sys.prefix)

    @classmethod
    def from_path(cls, prefix):
        metadata_path = os.path.join(prefix, METADATA_FILENAME)
        try:
            with open(metadata_path, "r") as file:
                raw = json.load(file)
            return cls(
                raw["hex_version"],
                raw["virtualenv_name"],
                prefix,
            )
        except FileNotFoundError:
            return None
        except KeyError:
            raise MozSiteMetadataOutOfDateError(
                f'The moz site metadata at "{metadata_path}" is out-of-date.'
            )


class MozSiteManager:
    """Contains logic for managing the Python import scope for building the tree."""

    def __init__(
        self,
        topsrcdir,
        virtualenvs_dir,
        site_name,
    ):
        self.virtualenv_root = os.path.join(virtualenvs_dir, site_name)
        self._virtualenv = MozVirtualenv(topsrcdir, site_name, self.virtualenv_root)
        self.bin_path = self._virtualenv.bin_path
        self.python_path = self._virtualenv.python_path
        self.topsrcdir = topsrcdir

        self._site_name = site_name
        self._metadata = MozSiteMetadata(
            sys.hexversion,
            site_name,
            self.virtualenv_root,
        )

    def up_to_date(self):
        """Returns whether the virtualenv is present and up to date."""

        # check if virtualenv exists
        if not os.path.exists(self.virtualenv_root) or not os.path.exists(
            self._virtualenv.activate_path
        ):
            return False

        env_requirements = self.requirements()

        virtualenv_package = os.path.join(
            self.topsrcdir,
            "third_party",
            "python",
            "virtualenv",
            "virtualenv",
            "version.py",
        )
        deps = [virtualenv_package] + env_requirements.requirements_paths

        # Modifications to any of the following files mean the virtualenv should be
        # rebuilt:
        # * This file
        # * The `virtualenv` package
        # * Any of our requirements manifest files
        activate_mtime = os.path.getmtime(self._virtualenv.activate_path)
        dep_mtime = max(os.path.getmtime(p) for p in deps)
        if dep_mtime > activate_mtime:
            return False

        # Verify that the metadata of the virtualenv on-disk is the same as what
        # we expect, e.g.
        # * If the metadata file doesn't exist, then the virtualenv wasn't fully
        #   built
        # * If the "hex_version" doesn't match, then the system Python has changed/been
        #   upgraded.
        try:
            existing_metadata = MozSiteMetadata.from_path(self._virtualenv.prefix)
            if existing_metadata != self._metadata:
                return False
        except MozSiteMetadataOutOfDateError:
            return False

        if env_requirements.pth_requirements or env_requirements.vendored_requirements:
            try:
                with open(
                    os.path.join(self._virtualenv.site_packages_dir(), PTH_FILENAME)
                ) as file:
                    current_paths = file.read().strip().split("\n")
            except FileNotFoundError:
                return False

            if current_paths != env_requirements.pths_as_absolute(self.topsrcdir):
                return False

        return True

    def ensure(self):
        """Ensure the site is present and up to date.

        If the site is up to date, this does nothing. Otherwise, it
        creates and populates a virtualenv as necessary.

        This should be the main API used from this class as it is the
        highest-level.
        """
        if self.up_to_date():
            return self.virtualenv_root
        return self.build()

    @functools.lru_cache(maxsize=None)
    def requirements(self):
        manifest_path = os.path.join(
            self.topsrcdir, "build", f"{self._site_name}_virtualenv_packages.txt"
        )

        if not os.path.exists(manifest_path):
            raise Exception(
                f'The current command is using the "{self._site_name}" '
                "site. However, that site is missing its associated "
                f'requirements definition file at "{manifest_path}".'
            )

        thunderbird_dir = os.path.join(self.topsrcdir, "comm")
        is_thunderbird = os.path.exists(thunderbird_dir) and bool(
            os.listdir(thunderbird_dir)
        )
        return MachEnvRequirements.from_requirements_definition(
            self.topsrcdir,
            is_thunderbird,
            self._site_name in ("mach", "build"),
            manifest_path,
        )

    def build(self):
        """Build a virtualenv per tree conventions.

        This returns the path of the created virtualenv.
        """
        if os.path.exists(self.virtualenv_root):
            shutil.rmtree(self.virtualenv_root)

        subprocess.run(
            [
                sys.executable,
                os.path.join(
                    self.topsrcdir,
                    "third_party",
                    "python",
                    "virtualenv",
                    "virtualenv.py",
                ),
                # pip, setuptools and wheel are vendored and inserted into the virtualenv
                # scope automatically, so "virtualenv" doesn't need to seed it.
                "--no-seed",
                self.virtualenv_root,
            ],
            check=True,
        )

        env_requirements = self.requirements()
        site_packages_dir = self._virtualenv.site_packages_dir()
        pthfile_lines = self.requirements().pths_as_absolute(self.topsrcdir)
        with open(os.path.join(site_packages_dir, PTH_FILENAME), "a") as f:
            f.write("\n".join(pthfile_lines))

        for requirement in env_requirements.pypi_requirements:
            self._virtualenv.pip_install([str(requirement.requirement)])

        for requirement in env_requirements.pypi_optional_requirements:
            try:
                self._virtualenv.pip_install([str(requirement.requirement)])
            except subprocess.CalledProcessError:
                print(
                    f"Could not install {requirement.requirement.name}, so "
                    f"{requirement.repercussion}. Continuing."
                )

        os.utime(self._virtualenv.activate_path, None)
        self._metadata.write()

        return self.virtualenv_root

    def activate(self):
        """Activate this site in the current Python context.

        If you run a random Python script and wish to "activate" the
        site, you can simply instantiate an instance of this class
        and call .activate() to make the virtualenv active.
        """

        self.ensure()
        activate_path = self._virtualenv.activate_path
        exec(open(activate_path).read(), dict(__file__=activate_path))

    def install_pip_package(self, package):
        """Install a package via pip.

        The supplied package is specified using a pip requirement specifier.
        e.g. 'foo' or 'foo==1.0'.

        If the package is already installed, this is a no-op.
        """
        if sys.executable.startswith(self.bin_path):
            # If we're already running in this interpreter, we can optimize in
            # the case that the package requirement is already satisfied.
            from pip._internal.req.constructors import install_req_from_line

            req = install_req_from_line(package)
            req.check_if_exists(use_user_site=False)
            if req.satisfied_by is not None:
                return

        self._virtualenv.pip_install([package])

    def install_pip_requirements(self, path, require_hashes=True, quiet=False):
        """Install a pip requirements.txt file.

        The supplied path is a text file containing pip requirement
        specifiers.

        If require_hashes is True, each specifier must contain the
        expected hash of the downloaded package. See:
        https://pip.pypa.io/en/stable/reference/pip_install/#hash-checking-mode
        """

        if not os.path.isabs(path):
            path = os.path.join(self.topsrcdir, path)

        args = ["--requirement", path]

        if require_hashes:
            args.append("--require-hashes")

        if quiet:
            args.append("--quiet")

        self._virtualenv.pip_install(args)


class PythonVirtualenv:
    """Calculates paths of interest for general python virtual environments"""

    def __init__(self, prefix):
        is_windows = sys.platform == "cygwin" or (
            sys.platform == "win32" and os.sep == "\\"
        )

        if is_windows:
            self.bin_path = os.path.join(prefix, "Scripts")
            self.python_path = os.path.join(self.bin_path, "python.exe")
        else:
            self.bin_path = os.path.join(prefix, "bin")
            self.python_path = os.path.join(self.bin_path, "python")
        self.prefix = prefix

    @functools.lru_cache(maxsize=None)
    def site_packages_dir(self):
        # Defer "distutils" import until this function is called so that
        # "mach bootstrap" doesn't fail due to Linux distro python-distutils
        # package not being installed.
        # By the time this function is called, "distutils" must be installed
        # because it's needed by the "virtualenv" package.
        from distutils import dist

        normalized_venv_root = os.path.normpath(self.prefix)

        distribution = dist.Distribution({"script_args": "--no-user-cfg"})
        installer = distribution.get_command_obj("install")
        installer.prefix = normalized_venv_root
        installer.finalize_options()

        # Path to virtualenv's "site-packages" directory
        path = installer.install_purelib
        local_folder = os.path.join(normalized_venv_root, "local")
        # Hack around https://github.com/pypa/virtualenv/issues/2208
        if path.startswith(local_folder):
            path = os.path.join(normalized_venv_root, path[len(local_folder) + 1 :])
        return path


class MozVirtualenv(PythonVirtualenv):
    """Interface to get useful paths and manage moz-owned virtual environments"""

    def __init__(self, topsrcdir, site_name, prefix):
        super().__init__(prefix)
        self.activate_path = os.path.join(self.bin_path, "activate_this.py")
        self._topsrcdir = topsrcdir
        self._site_name = site_name

    def pip_install(self, pip_install_args):
        # distutils will use the architecture of the running Python instance when building
        # packages. However, it's possible for the Xcode Python to be a universal binary
        # (x86_64 and arm64) without the associated macOS SDK supporting arm64, thereby
        # causing a build failure. To avoid this, we explicitly influence the build to
        # only target a single architecture - our current architecture.
        env = os.environ.copy()
        env.setdefault("ARCHFLAGS", "-arch {}".format(platform.machine()))

        # It's tempting to call pip natively via pip.main(). However,
        # the current Python interpreter may not be the virtualenv python.
        # This will confuse pip and cause the package to attempt to install
        # against the executing interpreter. By creating a new process, we
        # force the virtualenv's interpreter to be used and all is well.
        # It /might/ be possible to cheat and set sys.executable to
        # self.python_path. However, this seems more risk than it's worth.
        subprocess.run(
            [self.python_path, "-m", "pip", "install"] + pip_install_args,
            cwd=self._topsrcdir,
            env=env,
            universal_newlines=True,
            stderr=subprocess.STDOUT,
            check=True,
        )
