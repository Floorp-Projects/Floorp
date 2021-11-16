# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This file contains code for populating the virtualenv environment for
# Mozilla's build system. It is typically called as part of configure.

from __future__ import absolute_import, print_function, unicode_literals

import functools
import json
import os
import platform
import shutil
import subprocess
import sys

from mach.requirements import MachEnvRequirements

IS_NATIVE_WIN = sys.platform == "win32" and os.sep == "\\"
IS_CYGWIN = sys.platform == "cygwin"
PTH_FILENAME = "mach.pth"
METADATA_FILENAME = "moz_virtualenv_metadata.json"


class MozVirtualenvMetadataOutOfDateError(Exception):
    pass


class MozVirtualenvMetadata:
    """Moz-specific information that is encoded into a file at the root of a virtualenv"""

    def __init__(self, hex_version, virtualenv_name, file_path):
        self.hex_version = hex_version
        self.virtualenv_name = virtualenv_name
        self.file_path = file_path

    def write(self):
        raw = {"hex_version": self.hex_version, "virtualenv_name": self.virtualenv_name}
        with open(self.file_path, "w") as file:
            json.dump(raw, file)

    def __eq__(self, other):
        return (
            type(self) == type(other)
            and self.hex_version == other.hex_version
            and self.virtualenv_name == other.virtualenv_name
        )

    @classmethod
    def from_runtime(cls):
        return cls.from_path(os.path.join(sys.prefix, METADATA_FILENAME))

    @classmethod
    def from_path(cls, path):
        try:
            with open(path, "r") as file:
                raw = json.load(file)
            return cls(
                raw["hex_version"],
                raw["virtualenv_name"],
                path,
            )
        except FileNotFoundError:
            return None
        except KeyError:
            raise MozVirtualenvMetadataOutOfDateError(
                f'The virtualenv metadata at "{path}" is out-of-date.'
            )


class VirtualenvHelper(object):
    """Contains basic logic for getting information about virtualenvs."""

    def __init__(self, virtualenv_path):
        self.virtualenv_root = virtualenv_path

    @property
    def bin_path(self):
        # virtualenv.py provides a similar API via path_locations(). However,
        # we have a bit of a chicken-and-egg problem and can't reliably
        # import virtualenv. The functionality is trivial, so just implement
        # it here.
        if IS_CYGWIN or IS_NATIVE_WIN:
            return os.path.join(self.virtualenv_root, "Scripts")

        return os.path.join(self.virtualenv_root, "bin")

    @property
    def python_path(self):
        binary = "python"
        if sys.platform in ("win32", "cygwin"):
            binary += ".exe"

        return os.path.join(self.bin_path, binary)


class VirtualenvManager(VirtualenvHelper):
    """Contains logic for managing virtualenvs for building the tree."""

    def __init__(
        self,
        topsrcdir,
        virtualenvs_dir,
        virtualenv_name,
        *,
        manifest_path=None,
    ):
        virtualenv_path = os.path.join(virtualenvs_dir, virtualenv_name)
        super(VirtualenvManager, self).__init__(virtualenv_path)

        # __PYVENV_LAUNCHER__ confuses pip, telling it to use the system
        # python interpreter rather than the local virtual environment interpreter.
        # See https://bugzilla.mozilla.org/show_bug.cgi?id=1607470
        os.environ.pop("__PYVENV_LAUNCHER__", None)
        self.topsrcdir = topsrcdir

        # Record the Python executable that was used to create the Virtualenv
        # so we can check this against sys.executable when verifying the
        # integrity of the virtualenv.
        self.exe_info_path = os.path.join(self.virtualenv_root, "python_exe.txt")

        self._virtualenv_name = virtualenv_name
        self._manifest_path = manifest_path or os.path.join(
            topsrcdir, "build", f"{virtualenv_name}_virtualenv_packages.txt"
        )

        self._metadata = MozVirtualenvMetadata(
            sys.hexversion,
            virtualenv_name,
            os.path.join(self.virtualenv_root, METADATA_FILENAME),
        )

    @property
    def activate_path(self):
        return os.path.join(self.bin_path, "activate_this.py")

    def up_to_date(self, skip_pip_package_check=False):
        """Returns whether the virtualenv is present and up to date.

        Args:
            skip_pip_package_check: Don't check that the pip state on-disk still meets
                our requirements.
        """

        # check if virtualenv exists
        if not os.path.exists(self.virtualenv_root) or not os.path.exists(
            self.activate_path
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
        activate_mtime = os.path.getmtime(self.activate_path)
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
            existing_metadata = MozVirtualenvMetadata.from_path(
                self._metadata.file_path
            )
            if existing_metadata != self._metadata:
                return False
        except MozVirtualenvMetadataOutOfDateError:
            return False

        if env_requirements.pth_requirements or env_requirements.vendored_requirements:
            try:
                with open(
                    os.path.join(self._site_packages_dir(), PTH_FILENAME)
                ) as file:
                    current_paths = file.read().strip().split("\n")
            except FileNotFoundError:
                return False

            if current_paths != env_requirements.pths_as_absolute(self.topsrcdir):
                return False

        if not skip_pip_package_check:
            pip = [self.python_path, "-m", "pip"]
            package_result = env_requirements.validate_environment_packages(pip)
            if not package_result.has_all_packages:
                return False

        return True

    def ensure(self):
        """Ensure the virtualenv is present and up to date.

        If the virtualenv is up to date, this does nothing. Otherwise, it
        creates and populates the virtualenv as necessary.

        This should be the main API used from this class as it is the
        highest-level.
        """
        if self.up_to_date():
            return self.virtualenv_root
        return self.build()

    def create(self):
        """Create a new, empty virtualenv.

        Receives the path to virtualenv's virtualenv.py script (which will be
        called out to), the path to create the virtualenv in, and a handle to
        write output to.
        """
        if os.path.exists(self.virtualenv_root):
            shutil.rmtree(self.virtualenv_root)

        result = subprocess.run(
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
        )

        if result.returncode:
            raise Exception(
                "Failed to create virtualenv: %s (virtualenv.py retcode: %s)"
                % (self.virtualenv_root, result)
            )

        return self.virtualenv_root

    @functools.lru_cache(maxsize=None)
    def requirements(self):
        if not os.path.exists(self._manifest_path):
            raise Exception(
                f'The current command is using the "{self._virtualenv_name}" '
                "virtualenv. However, that virtualenv is missing its associated "
                f'requirements definition file at "{self._manifest_path}".'
            )

        thunderbird_dir = os.path.join(self.topsrcdir, "comm")
        is_thunderbird = os.path.exists(thunderbird_dir) and bool(
            os.listdir(thunderbird_dir)
        )
        return MachEnvRequirements.from_requirements_definition(
            self.topsrcdir,
            is_thunderbird,
            self._virtualenv_name in ("mach", "build"),
            self._manifest_path,
        )

    def build(self):
        """Build a virtualenv per tree conventions.

        This returns the path of the created virtualenv.
        """
        self.create()
        env_requirements = self.requirements()
        site_packages_dir = self._site_packages_dir()
        pthfile_lines = self.requirements().pths_as_absolute(self.topsrcdir)
        with open(os.path.join(site_packages_dir, PTH_FILENAME), "a") as f:
            f.write("\n".join(pthfile_lines))

        pip = [self.python_path, "-m", "pip"]
        for pypi_requirement in env_requirements.pypi_requirements:
            subprocess.check_call(pip + ["install", str(pypi_requirement.requirement)])

        for requirement in env_requirements.pypi_optional_requirements:
            try:
                subprocess.check_call(pip + ["install", str(requirement.requirement)])
            except subprocess.CalledProcessError:
                print(
                    f"Could not install {requirement.requirement.name}, so "
                    f"{requirement.repercussion}. Continuing."
                )

        os.utime(self.activate_path, None)
        self._metadata.write()

        return self.virtualenv_root

    def activate(self):
        """Activate the virtualenv in this Python context.

        If you run a random Python script and wish to "activate" the
        virtualenv, you can simply instantiate an instance of this class
        and call .ensure() and .activate() to make the virtualenv active.
        """

        exec(open(self.activate_path).read(), dict(__file__=self.activate_path))

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

        self._run_pip(["install", package])

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

        args = ["install", "--requirement", path]

        if require_hashes:
            args.append("--require-hashes")

        if quiet:
            args.append("--quiet")

        self._run_pip(args)

    def _run_pip(self, args):
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
            [self.python_path, "-m", "pip"] + args,
            cwd=self.topsrcdir,
            env=env,
            universal_newlines=True,
            stderr=subprocess.STDOUT,
            check=True,
        )

    def _site_packages_dir(self):
        # Defer "distutils" import until this function is called so that
        # "mach bootstrap" doesn't fail due to Linux distro python-distutils
        # package not being installed.
        # By the time this function is called, "distutils" must be installed
        # because it's needed by the "virtualenv" package.
        from distutils import dist

        normalized_venv_root = os.path.normpath(self.virtualenv_root)

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
