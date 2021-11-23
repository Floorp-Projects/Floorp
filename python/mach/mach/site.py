# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This file contains code for managing the Python import scope for Mach. This
# generally involves populating a Python virtualenv.

from __future__ import absolute_import, print_function, unicode_literals

import enum
import functools
import json
import os
import platform
import shutil
import site
import subprocess
import sys
from pathlib import Path

from mach.requirements import MachEnvRequirements

PTH_FILENAME = "mach.pth"
METADATA_FILENAME = "moz_virtualenv_metadata.json"


class VirtualenvOutOfDateException(Exception):
    pass


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


class SitePackagesSource(enum.Enum):
    NONE = enum.auto()
    SYSTEM = enum.auto()
    VENV = enum.auto()


class MachSiteManager:
    """Represents the activate-able "import scope" Mach needs

    Whether running independently, using the system packages, or automatically managing
    dependencies with "pip install", this class provides an easy handle to verify
    that the "site" is up-to-date (whether than means that system packages don't
    collide with vendored packages, or that the on-disk virtualenv needs rebuilding).

    Note that, this is a *virtual* site: an on-disk Python virtualenv
    is only created if there will be "pip installs" into the Mach site.
    """

    def __init__(
        self,
        topsrcdir,
        state_dir,
        requirements,
        active_metadata,
        site_packages_source,
    ):
        self._topsrcdir = topsrcdir
        self._active_metadata = active_metadata
        self._site_packages_source = site_packages_source
        self._requirements = requirements
        self._virtualenv_root = os.path.join(state_dir, "_virtualenvs", "mach")
        self._metadata = MozSiteMetadata(
            sys.hexversion,
            "mach",
            self._virtualenv_root,
        )

    @classmethod
    def from_environment(
        cls, topsrcdir, state_dir, is_mach_create_mach_env_command=False
    ):
        requirements = _requirements(topsrcdir, "mach")
        # Mach needs to operate in environments in which no pip packages are installed
        # yet, and the system isn't guaranteed to have the packages we need. For example,
        # "./mach bootstrap" can't have any dependencies.
        # So, all external dependencies of Mach's must be optional.
        assert (
            not requirements.pypi_requirements
        ), "Mach pip package requirements must be optional."

        if not (
            os.environ.get("MACH_USE_SYSTEM_PYTHON") or os.environ.get("MOZ_AUTOMATION")
        ):
            active_metadata = MozSiteMetadata.from_runtime()
            if active_metadata and active_metadata.site_name == "mach":
                source = SitePackagesSource.VENV
            elif not is_mach_create_mach_env_command:
                # We're in an environment where we normally *would* use the Mach
                # virtualenv, but we're running a "nativecmd" such as
                # "create-mach-environment". So, operate Mach without leaning on the
                # on-disk virtualenv or system packages.
                source = SitePackagesSource.NONE
            else:
                # This catches the case where "./mach create-mach-environment" was run:
                # * On Mach initialization, the previous clause is hit, and Mach
                #   operates without any extra site packages
                # * When the actual "create-mach-environment" function runs,
                #   MachPythonEnvironment.from_environment() runs again, and this clause
                #   is triggered. We set the site packages source to the on-disk venv so
                #   it gets populated.
                source = SitePackagesSource.VENV
        else:
            has_pip = (
                subprocess.run(
                    [sys.executable, "-c", "import pip"], stderr=subprocess.DEVNULL
                ).returncode
                == 0
            )

            if has_pip:
                if os.environ.get("MACH_USE_SYSTEM_PYTHON") and not os.environ.get(
                    "MOZ_AUTOMATION"
                ):
                    print(
                        "Since Mach has been requested to use the system Python "
                        "environment, it will need to verify compatibility before "
                        "running the current command. This may take a couple seconds.\n"
                        "Note: you can avoid this delay by unsetting the "
                        "MACH_USE_SYSTEM_PYTHON environment variable."
                    )
                source = SitePackagesSource.SYSTEM
            else:
                source = SitePackagesSource.NONE

        return cls(
            topsrcdir,
            state_dir,
            requirements,
            MozSiteMetadata.from_runtime(),
            source,
        )

    def up_to_date(self):
        if self._site_packages_source == SitePackagesSource.NONE:
            return True
        elif self._site_packages_source == SitePackagesSource.SYSTEM:
            pip = [sys.executable, "-m", "pip"]
            check_result = subprocess.run(
                pip + ["check"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT
            )
            if check_result.returncode:
                print(check_result.stdout, file=sys.stderr)
                subprocess.check_call(pip + ["list", "-v"], stdout=sys.stderr)
                raise Exception(
                    'According to "pip check", the current Python '
                    "environment has package-compatibility issues."
                )

            package_result = self._requirements.validate_environment_packages(pip)
            if not package_result.has_all_packages:
                print(package_result.report())
                raise Exception(
                    "Your system Python packages aren't compatible with the "
                    '"Mach" virtualenv'
                )
            return True
        elif self._site_packages_source == SitePackagesSource.VENV:
            environment = self._virtualenv()
            return _is_venv_up_to_date(
                self._topsrcdir,
                environment,
                self._requirements,
                self._metadata,
            )

    def ensure(self, *, force=False):
        up_to_date = self.up_to_date()
        if force or not up_to_date:
            if Path(sys.prefix) == Path(self._metadata.prefix):
                # If the Mach virtualenv is already activated, then the changes caused
                # by rebuilding the virtualenv won't take effect until the next time
                # Mach is used, which can lead to confusing one-off errors.
                # Instead, request that the user resolve the out-of-date situation,
                # *then* come back and run the intended command.
                raise VirtualenvOutOfDateException()
            self._build()
        return up_to_date

    def activate(self):
        self.ensure()
        if self._site_packages_source == SitePackagesSource.SYSTEM:
            # Add our Mach modules to the sys.path.
            # Note that "[0:0]" is used to ensure that Mach's modules are prioritized
            # over the system modules. Since Mach-env activation happens so early in
            # the Mach lifecycle, we can assume that no system packages have been
            # imported yet, and this is a safe operation to do.
            sys.path[0:0] = self._requirements.pths_as_absolute(self._topsrcdir)
        elif self._site_packages_source == SitePackagesSource.NONE:
            # Since the system packages aren't used, remove them from the sys.path
            sys.path = [
                path
                for path in sys.path
                if path
                not in set(site.getsitepackages() + [site.getusersitepackages()])
            ]
            sys.path[0:0] = self._requirements.pths_as_absolute(self._topsrcdir)
        elif self._site_packages_source == SitePackagesSource.VENV:
            if Path(sys.prefix) != Path(self._metadata.prefix):
                raise Exception(
                    "In-process activation of the Mach virtualenv is "
                    "not currently supported. Please invoke `./mach` using "
                    "the Mach virtualenv python binary directly."
                )

            # Otherwise, if our prefix matches the Mach virtualenv, then it's already
            # activated.

    def _build(self):
        if self._site_packages_source != SitePackagesSource.VENV:
            # The Mach virtualenv doesn't have a physical virtualenv on-disk if it won't
            # be "pip install"-ing. So, there's no build work to do.
            return

        environment = self._virtualenv()
        _create_venv_with_pthfile(
            self._topsrcdir,
            environment,
            self._site_packages_source,
            self._requirements,
            self._metadata,
        )

    def _virtualenv(self):
        assert self._site_packages_source == SitePackagesSource.VENV
        return MozVirtualenv(self._topsrcdir, "mach", self._metadata.prefix)


class CommandSiteManager:
    """Public interface for activating virtualenvs and ad-hoc-installing pip packages"""

    def __init__(
        self,
        topsrcdir,
        virtualenvs_dir,
        site_name,
    ):
        self.topsrcdir = topsrcdir
        self._virtualenv_name = site_name
        self.virtualenv_root = os.path.join(virtualenvs_dir, site_name)
        self._virtualenv = MozVirtualenv(topsrcdir, site_name, self.virtualenv_root)
        self.bin_path = self._virtualenv.bin_path
        self.python_path = self._virtualenv.python_path
        self._metadata = MozSiteMetadata(
            sys.hexversion,
            site_name,
            self._virtualenv.prefix,
        )

    def ensure(self):
        """Ensure the site is present and up to date."""
        requirements = _requirements(self.topsrcdir, self._virtualenv_name)
        if not _is_venv_up_to_date(
            self.topsrcdir,
            self._virtualenv,
            requirements,
            self._metadata,
        ):
            _create_venv_with_pthfile(
                self.topsrcdir,
                self._virtualenv,
                SitePackagesSource.VENV,
                requirements,
                self._metadata,
            )

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
        if os.path.normcase(sys.executable).startswith(os.path.normcase(self.bin_path)):
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


@functools.lru_cache(maxsize=None)
def _requirements(topsrcdir, virtualenv_name):
    manifest_path = os.path.join(
        topsrcdir, "build", f"{virtualenv_name}_virtualenv_packages.txt"
    )
    if not os.path.exists(manifest_path):
        raise Exception(
            f'The current command is using the "{virtualenv_name}" '
            "virtualenv. However, that virtualenv is missing its associated "
            f'requirements definition file at "{manifest_path}".'
        )

    thunderbird_dir = os.path.join(topsrcdir, "comm")
    is_thunderbird = os.path.exists(thunderbird_dir) and bool(
        os.listdir(thunderbird_dir)
    )
    return MachEnvRequirements.from_requirements_definition(
        topsrcdir,
        is_thunderbird,
        virtualenv_name in ("mach", "build"),
        manifest_path,
    )


def _create_venv_with_pthfile(
    topsrcdir,
    target_venv,
    site_packages_source,
    requirements,
    metadata,
):
    virtualenv_root = target_venv.prefix
    if os.path.exists(virtualenv_root):
        shutil.rmtree(virtualenv_root)

    subprocess.check_call(
        [
            sys.executable,
            os.path.join(
                topsrcdir,
                "third_party",
                "python",
                "virtualenv",
                "virtualenv.py",
            ),
            # pip, setuptools and wheel are vendored and inserted into the virtualenv
            # scope automatically, so "virtualenv" doesn't need to seed it.
            "--no-seed",
            virtualenv_root,
        ]
    )

    os.utime(target_venv.activate_path, None)

    site_packages_dir = target_venv.site_packages_dir()
    pthfile_contents = "\n".join(requirements.pths_as_absolute(topsrcdir))
    with open(os.path.join(site_packages_dir, PTH_FILENAME), "w") as f:
        f.write(pthfile_contents)

    if site_packages_source == SitePackagesSource.VENV:
        for requirement in requirements.pypi_requirements:
            target_venv.pip_install([str(requirement.requirement)])

        for requirement in requirements.pypi_optional_requirements:
            try:
                target_venv.pip_install([str(requirement.requirement)])
            except subprocess.CalledProcessError:
                print(
                    f"Could not install {requirement.requirement.name}, so "
                    f"{requirement.repercussion}. Continuing."
                )

    os.utime(target_venv.activate_path, None)
    metadata.write()


def _is_venv_up_to_date(
    topsrcdir,
    target_venv,
    requirements,
    expected_metadata,
):
    if not os.path.exists(target_venv.prefix) or not os.path.exists(
        target_venv.activate_path
    ):
        return False

    virtualenv_package = os.path.join(
        topsrcdir,
        "third_party",
        "python",
        "virtualenv",
        "virtualenv",
        "version.py",
    )
    deps = [virtualenv_package] + requirements.requirements_paths

    # Modifications to any of the following files mean the virtualenv should be
    # rebuilt:
    # * This file
    # * The `virtualenv` package
    # * Any of our requirements manifest files
    activate_mtime = os.path.getmtime(target_venv.activate_path)
    dep_mtime = max(os.path.getmtime(p) for p in deps)
    if dep_mtime > activate_mtime:
        return False

    try:
        existing_metadata = MozSiteMetadata.from_path(target_venv.prefix)
    except MozSiteMetadataOutOfDateError:
        # The metadata is missing required fields, so must be out-of-date.
        return False

    if existing_metadata != expected_metadata:
        # The metadata doesn't exist or some fields have different values.
        return False

    site_packages_dir = target_venv.site_packages_dir()
    try:
        with open(os.path.join(site_packages_dir, PTH_FILENAME)) as file:
            current_pthfile_contents = file.read().strip()
    except FileNotFoundError:
        return False

    expected_pthfile_contents = "\n".join(requirements.pths_as_absolute(topsrcdir))
    if current_pthfile_contents != expected_pthfile_contents:
        return False

    return True
