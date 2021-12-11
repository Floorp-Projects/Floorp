# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This file contains code for populating the virtualenv environment for
# Mozilla's build system. It is typically called as part of configure.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys

IS_NATIVE_WIN = sys.platform == "win32" and os.sep == "\\"
IS_CYGWIN = sys.platform == "cygwin"
PTH_FILENAME = "mach.pth"
METADATA_FILENAME = "moz_virtualenv_metadata.json"


UPGRADE_WINDOWS = """
Please upgrade to the latest MozillaBuild development environment. See
https://developer.mozilla.org/en-US/docs/Developer_Guide/Build_Instructions/Windows_Prerequisites
""".lstrip()

UPGRADE_OTHER = """
Run |mach bootstrap| to ensure your system is up to date.

If you still receive this error, your shell environment is likely detecting
another Python version. Ensure a modern Python can be found in the paths
defined by the $PATH environment variable and try again.
""".lstrip()


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
    def from_path(cls, path):
        try:
            with open(path, "r") as file:
                raw = json.load(file)
            return cls(
                raw["hex_version"],
                raw["virtualenv_name"],
                path,
            )
        except (FileNotFoundError, KeyError):
            return None


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
        populate_local_paths=True,
        log_handle=sys.stdout,
        base_python=sys.executable,
        manifest_path=None,
    ):
        """Create a new manager.

        Each manager is associated with a source directory, a path where you
        want the virtualenv to be created, and a handle to write output to.
        """
        virtualenv_path = os.path.join(virtualenvs_dir, virtualenv_name)
        super(VirtualenvManager, self).__init__(virtualenv_path)

        # __PYVENV_LAUNCHER__ confuses pip, telling it to use the system
        # python interpreter rather than the local virtual environment interpreter.
        # See https://bugzilla.mozilla.org/show_bug.cgi?id=1607470
        os.environ.pop("__PYVENV_LAUNCHER__", None)
        self.topsrcdir = topsrcdir
        self._base_python = base_python

        # Record the Python executable that was used to create the Virtualenv
        # so we can check this against sys.executable when verifying the
        # integrity of the virtualenv.
        self.exe_info_path = os.path.join(self.virtualenv_root, "python_exe.txt")

        self.log_handle = log_handle
        self.populate_local_paths = populate_local_paths
        self._virtualenv_name = virtualenv_name
        self._manifest_path = manifest_path or os.path.join(
            topsrcdir, "build", f"{virtualenv_name}_virtualenv_packages.txt"
        )

        hex_version = subprocess.check_output(
            [self._base_python, "-c", "import sys; print(sys.hexversion)"]
        )
        hex_version = int(hex_version.rstrip())
        self._metadata = MozVirtualenvMetadata(
            hex_version,
            virtualenv_name,
            os.path.join(self.virtualenv_root, METADATA_FILENAME),
        )

    def version_info(self):
        return eval(
            subprocess.check_output(
                [self.python_path, "-c", "import sys; print(sys.version_info[:])"]
            )
        )

    @property
    def activate_path(self):
        return os.path.join(self.bin_path, "activate_this.py")

    def up_to_date(self):
        """Returns whether the virtualenv is present and up to date.

        Args:
            python: Full path string to the Python executable that this virtualenv
                should be running.  If the Python executable passed in to this
                argument is not the same version as the Python the virtualenv was
                built with then this method will return False.
        """

        # check if virtualenv exists
        if not os.path.exists(self.virtualenv_root) or not os.path.exists(
            self.activate_path
        ):
            return False

        env_requirements = self._requirements()

        virtualenv_package = os.path.join(
            self.topsrcdir,
            "third_party",
            "python",
            "virtualenv",
            "virtualenv",
            "version.py",
        )
        deps = [__file__, virtualenv_package] + env_requirements.requirements_paths

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
        existing_metadata = MozVirtualenvMetadata.from_path(self._metadata.file_path)
        if existing_metadata != self._metadata:
            return False

        if (
            env_requirements.pth_requirements or env_requirements.vendored_requirements
        ) and self.populate_local_paths:
            try:
                with open(
                    os.path.join(self._site_packages_dir(), PTH_FILENAME)
                ) as file:
                    pth_lines = file.read().strip().split("\n")
            except FileNotFoundError:
                return False

            current_paths = [
                os.path.normcase(
                    os.path.abspath(os.path.join(self._site_packages_dir(), path))
                )
                for path in pth_lines
            ]

            required_paths = [
                os.path.normcase(
                    os.path.abspath(os.path.join(self.topsrcdir, pth.path))
                )
                for pth in env_requirements.pth_requirements
                + env_requirements.vendored_requirements
            ]

            if current_paths != required_paths:
                return False

        pip = os.path.join(self.bin_path, "pip")
        package_result = env_requirements.validate_environment_packages([pip])
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

    def _log_process_output(self, *args, **kwargs):
        if hasattr(self.log_handle, "fileno"):
            return subprocess.call(
                *args, stdout=self.log_handle, stderr=subprocess.STDOUT, **kwargs
            )

        proc = subprocess.Popen(
            *args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, **kwargs
        )

        for line in proc.stdout:
            self.log_handle.write(line.decode("UTF-8"))

        return proc.wait()

    def create(self):
        """Create a new, empty virtualenv.

        Receives the path to virtualenv's virtualenv.py script (which will be
        called out to), the path to create the virtualenv in, and a handle to
        write output to.
        """
        if os.path.exists(self.virtualenv_root):
            shutil.rmtree(self.virtualenv_root)

        args = [
            self._base_python,
            os.path.join(
                self.topsrcdir, "third_party", "python", "virtualenv", "virtualenv.py"
            ),
            # Without this, virtualenv.py may attempt to contact the outside
            # world and search for or download a newer version of pip,
            # setuptools, or wheel. This is bad for security, reproducibility,
            # and speed.
            "--no-download",
            self.virtualenv_root,
        ]

        result = self._log_process_output(args)

        if result:
            raise Exception(
                "Failed to create virtualenv: %s (virtualenv.py retcode: %s)"
                % (self.virtualenv_root, result)
            )

        self._disable_pip_outdated_warning()
        return self.virtualenv_root

    def _requirements(self):
        from mach.requirements import MachEnvRequirements

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

    def populate(self):
        """Populate the virtualenv.

        Note that the Python interpreter running this function should be the
        one from the virtualenv. If it is the system Python or if the
        environment is not configured properly, packages could be installed
        into the wrong place. This is how virtualenv's work.
        """
        import distutils.sysconfig

        # We ignore environment variables that may have been altered by
        # configure or a mozconfig activated in the current shell. We trust
        # Python is smart enough to find a proper compiler and to use the
        # proper compiler flags. If it isn't your Python is likely broken.
        IGNORE_ENV_VARIABLES = ("CC", "CXX", "CFLAGS", "CXXFLAGS", "LDFLAGS")

        try:
            old_env_variables = {}
            for k in IGNORE_ENV_VARIABLES:
                if k not in os.environ:
                    continue

                old_env_variables[k] = os.environ[k]
                del os.environ[k]

            env_requirements = self._requirements()
            if self.populate_local_paths:
                python_lib = distutils.sysconfig.get_python_lib()
                with open(os.path.join(python_lib, PTH_FILENAME), "a") as f:
                    for pth_requirement in (
                        env_requirements.pth_requirements
                        + env_requirements.vendored_requirements
                    ):
                        path = os.path.join(self.topsrcdir, pth_requirement.path)
                        # This path is relative to the .pth file.  Using a
                        # relative path allows the srcdir/objdir combination
                        # to be moved around (as long as the paths relative to
                        # each other remain the same).
                        f.write("{}\n".format(os.path.relpath(path, python_lib)))

            for pypi_requirement in env_requirements.pypi_requirements:
                self.install_pip_package(str(pypi_requirement.requirement))

            for requirement in env_requirements.pypi_optional_requirements:
                try:
                    self.install_pip_package(str(requirement.requirement))
                except subprocess.CalledProcessError:
                    print(
                        f"Could not install {requirement.requirement.name}, so "
                        f"{requirement.repercussion}. Continuing."
                    )

        finally:
            os.environ.update(old_env_variables)

    def build(self):
        """Build a virtualenv per tree conventions.

        This returns the path of the created virtualenv.
        """
        self.create()

        # We need to populate the virtualenv using the Python executable in
        # the virtualenv for paths to be proper.

        # If this module was run from Python 2 then the __file__ attribute may
        # point to a Python 2 .pyc file. If we are generating a Python 3
        # virtualenv from Python 2 make sure we call Python 3 with the path to
        # the module and not the Python 2 .pyc file.
        if os.path.splitext(__file__)[1] in (".pyc", ".pyo"):
            thismodule = __file__[:-1]
        else:
            thismodule = __file__

        args = [
            self.python_path,
            thismodule,
            "populate",
            self.topsrcdir,
            os.path.dirname(self.virtualenv_root),
            self._virtualenv_name,
            self._manifest_path,
        ]
        if self.populate_local_paths:
            args.append("--populate-local-paths")

        result = self._log_process_output(args, cwd=self.topsrcdir)

        if result != 0:
            raise Exception("Error populating virtualenv.")

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

        return self._run_pip(["install", package], stderr=subprocess.STDOUT)

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

        return self._run_pip(args, stderr=subprocess.STDOUT)

    def _disable_pip_outdated_warning(self):
        """Disables the pip outdated warning by changing pip's 'installer'

        "pip" has behaviour to ensure that it doesn't print it's "outdated"
        warning if it's part of a Linux distro package. This is because
        Linux distros generally have a slightly out-of-date pip package
        that they know to be stable, and users aren't always able to
        (or want to) update it.

        This behaviour works by checking if the "pip" installer
        (encoded in the dist-info/INSTALLER file) is "pip" itself,
        or a different value (e.g.: a distro).

        We can take advantage of this behaviour by telling pip
        that it was installed by "mach", so it won't print the
        warning.

        https://github.com/pypa/pip/blob/5ee933aab81273da3691c97f2a6e7016ecbe0ef9/src/pip/_internal/self_outdated_check.py#L100-L101 # noqa F401
        """
        site_packages = self._site_packages_dir()
        pip_dist_info = next(
            (
                file
                for file in os.listdir(site_packages)
                if file.startswith("pip-") and file.endswith(".dist-info")
            ),
            None,
        )
        if not pip_dist_info:
            raise Exception("Failed to find pip dist-info in new virtualenv")

        with open(os.path.join(site_packages, pip_dist_info, "INSTALLER"), "w") as file:
            file.write("mach")

    def _run_pip(self, args, **kwargs):
        kwargs.setdefault("check", True)

        env = os.environ.copy()
        env.setdefault("ARCHFLAGS", get_archflags())

        # It's tempting to call pip natively via pip.main(). However,
        # the current Python interpreter may not be the virtualenv python.
        # This will confuse pip and cause the package to attempt to install
        # against the executing interpreter. By creating a new process, we
        # force the virtualenv's interpreter to be used and all is well.
        # It /might/ be possible to cheat and set sys.executable to
        # self.python_path. However, this seems more risk than it's worth.
        pip = os.path.join(self.bin_path, "pip")
        return subprocess.run(
            [pip] + args, cwd=self.topsrcdir, env=env, universal_newlines=True, **kwargs
        )

    def _site_packages_dir(self):
        # Defer "distutils" import until this function is called so that
        # "mach bootstrap" doesn't fail due to Linux distro python-distutils
        # package not being installed.
        # By the time this function is called, "distutils" must be installed
        # because it's needed by the "virtualenv" package.
        from distutils import dist

        distribution = dist.Distribution({"script_args": "--no-user-cfg"})
        installer = distribution.get_command_obj("install")
        installer.prefix = os.path.normpath(self.virtualenv_root)
        installer.finalize_options()

        # Path to virtualenv's "site-packages" directory
        return installer.install_purelib


def get_archflags():
    # distutils will use the architecture of the running Python instance when building packages.
    # However, it's possible for the Xcode Python to be a universal binary (x86_64 and
    # arm64) without the associated macOS SDK supporting arm64, thereby causing a build
    # failure. To avoid this, we explicitly influence the build to only target a single
    # architecture - our current architecture.
    return "-arch {}".format(platform.machine())


def verify_python_version(log_handle):
    """Ensure the current version of Python is sufficient."""
    from distutils.version import LooseVersion

    major, minor, micro = sys.version_info[:3]
    minimum_python_versions = {2: LooseVersion("2.7.3"), 3: LooseVersion("3.6.0")}
    our = LooseVersion("%d.%d.%d" % (major, minor, micro))

    if major not in minimum_python_versions or our < minimum_python_versions[major]:
        log_handle.write("One of the following Python versions are required:\n")
        for minver in minimum_python_versions.values():
            log_handle.write("* Python %s or greater\n" % minver)
        log_handle.write("You are running Python %s.\n" % our)

        if os.name in ("nt", "ce"):
            log_handle.write(UPGRADE_WINDOWS)
        else:
            log_handle.write(UPGRADE_OTHER)

        sys.exit(1)


if __name__ == "__main__":
    verify_python_version(sys.stdout)

    if len(sys.argv) < 2:
        print("Too few arguments", file=sys.stderr)
        sys.exit(1)

    parser = argparse.ArgumentParser()
    parser.add_argument("topsrcdir")
    parser.add_argument("virtualenvs_dir")
    parser.add_argument("virtualenv_name")
    parser.add_argument("manifest_path")
    parser.add_argument("--populate-local-paths", action="store_true")

    if sys.argv[1] == "populate":
        # This should only be called internally.
        populate = True
        opts = parser.parse_args(sys.argv[2:])
    else:
        populate = False
        opts = parser.parse_args(sys.argv[1:])

    # We want to be able to import the "mach.requirements" module.
    sys.path.append(os.path.join(opts.topsrcdir, "python", "mach"))
    # Virtualenv logic needs access to the vendored "packaging" library.
    sys.path.append(os.path.join(opts.topsrcdir, "third_party", "python", "pyparsing"))
    sys.path.append(os.path.join(opts.topsrcdir, "third_party", "python", "packaging"))

    manager = VirtualenvManager(
        opts.topsrcdir,
        opts.virtualenvs_dir,
        opts.virtualenv_name,
        populate_local_paths=opts.populate_local_paths,
        manifest_path=opts.manifest_path,
    )

    if populate:
        manager.populate()
    else:
        manager.ensure()
