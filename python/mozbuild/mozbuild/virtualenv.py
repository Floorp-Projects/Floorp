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

here = os.path.abspath(os.path.dirname(__file__))


# We can't import six.ensure_binary() or six.ensure_text() because this module
# has to run stand-alone.  Instead we'll implement an abbreviated version of the
# checks it does.


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
        virtualenv_path,
        log_handle,
        manifest_path,
        populate_local_paths=True,
    ):
        """Create a new manager.

        Each manager is associated with a source directory, a path where you
        want the virtualenv to be created, and a handle to write output to.
        """
        super(VirtualenvManager, self).__init__(virtualenv_path)

        # __PYVENV_LAUNCHER__ confuses pip, telling it to use the system
        # python interpreter rather than the local virtual environment interpreter.
        # See https://bugzilla.mozilla.org/show_bug.cgi?id=1607470
        os.environ.pop("__PYVENV_LAUNCHER__", None)

        assert os.path.isabs(
            manifest_path
        ), "manifest_path must be an absolute path: %s" % (manifest_path)
        self.topsrcdir = topsrcdir

        # Record the Python executable that was used to create the Virtualenv
        # so we can check this against sys.executable when verifying the
        # integrity of the virtualenv.
        self.exe_info_path = os.path.join(self.virtualenv_root, "python_exe.txt")

        self.log_handle = log_handle
        self.manifest_path = manifest_path
        self.populate_local_paths = populate_local_paths

    @property
    def virtualenv_script_path(self):
        """Path to virtualenv's own populator script."""
        return os.path.join(
            self.topsrcdir, "third_party", "python", "virtualenv", "virtualenv.py"
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

    def get_exe_info(self):
        """Returns the version of the python executable that was in use when
        this virtualenv was created.
        """
        with open(self.exe_info_path, "r") as fh:
            version = fh.read()
        return int(version)

    def write_exe_info(self, python):
        """Records the the version of the python executable that was in use when
        this virtualenv was created. We record this explicitly because
        on OS X our python path may end up being a different or modified
        executable.
        """
        ver = self.python_executable_hexversion(python)
        with open(self.exe_info_path, "w") as fh:
            fh.write("%s\n" % ver)

    def python_executable_hexversion(self, python):
        """Run a Python executable and return its sys.hexversion value."""
        program = "import sys; print(sys.hexversion)"
        out = subprocess.check_output([python, "-c", program]).rstrip()
        return int(out)

    def up_to_date(self, python):
        """Returns whether the virtualenv is present and up to date.

        Args:
            python: Full path string to the Python executable that this virtualenv
                should be running.  If the Python executable passed in to this
                argument is not the same version as the Python the virtualenv was
                built with then this method will return False.
        """

        deps = [self.manifest_path, __file__]

        # check if virtualenv exists
        if not os.path.exists(self.virtualenv_root) or not os.path.exists(
            self.activate_path
        ):
            return False

        # Modifications to our package dependency list or to this file mean the
        # virtualenv should be rebuilt.
        activate_mtime = os.path.getmtime(self.activate_path)
        dep_mtime = max(os.path.getmtime(p) for p in deps)
        if dep_mtime > activate_mtime:
            return False

        # Verify that the Python we're checking here is either the virutalenv
        # python, or we have the Python version that was used to create the
        # virtualenv. If this fails, it is likely system Python has been
        # upgraded, and our virtualenv would not be usable.
        orig_version = self.get_exe_info()
        hexversion = self.python_executable_hexversion(python)
        if (python != self.python_path) and (hexversion != orig_version):
            return False

        packages = self.packages()
        pypi_packages = [package for action, package in packages if action == "pypi"]
        if pypi_packages:
            pip_json = self._run_pip(
                ["list", "--format", "json"], capture_output=True
            ).stdout
            installed_packages = json.loads(pip_json)
            installed_packages = {
                package["name"]: package["version"] for package in installed_packages
            }
            for pypi_package in pypi_packages:
                name, version = pypi_package.split("==")
                if installed_packages.get(name, None) != version:
                    return False

        # recursively check sub packages.txt files
        submanifests = [
            package for action, package in packages if action == "packages.txt"
        ]
        for submanifest in submanifests:
            submanifest = os.path.join(self.topsrcdir, submanifest)
            submanager = VirtualenvManager(
                self.topsrcdir, self.virtualenv_root, self.log_handle, submanifest
            )
            if not submanager.up_to_date(python):
                return False

        return True

    def ensure(self, python=sys.executable):
        """Ensure the virtualenv is present and up to date.

        If the virtualenv is up to date, this does nothing. Otherwise, it
        creates and populates the virtualenv as necessary.

        This should be the main API used from this class as it is the
        highest-level.
        """
        if self.up_to_date(python):
            return self.virtualenv_root
        return self.build(python)

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

    def create(self, python):
        """Create a new, empty virtualenv.

        Receives the path to virtualenv's virtualenv.py script (which will be
        called out to), the path to create the virtualenv in, and a handle to
        write output to.
        """
        if os.path.exists(self.virtualenv_root):
            shutil.rmtree(self.virtualenv_root)

        args = [
            python,
            self.virtualenv_script_path,
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

        self.write_exe_info(python)
        self._disable_pip_outdated_warning()

        return self.virtualenv_root

    def packages(self):
        with open(self.manifest_path, "r") as fh:
            return [line.rstrip().split(":", maxsplit=1) for line in fh]

    def populate(self):
        """Populate the virtualenv.

        The manifest file consists of colon-delimited fields. The first field
        specifies the action. The remaining fields are arguments to that
        action. The following actions are supported:

        pth -- Adds the path given as argument to "mach.pth" under
            the virtualenv site packages directory.

        pypi -- Fetch the package, plus dependencies, from PyPI.

        thunderbird -- This denotes the action as to only occur for Thunderbird
            checkouts. The initial "thunderbird" field is stripped, then the
            remaining line is processed like normal. e.g.
            "thunderbird:pth:python/foo"

        packages.txt -- Denotes that the specified path is a child manifest. It
            will be read and processed as if its contents were concatenated
            into the manifest being read.

        Note that the Python interpreter running this function should be the
        one from the virtualenv. If it is the system Python or if the
        environment is not configured properly, packages could be installed
        into the wrong place. This is how virtualenv's work.
        """
        import distutils.sysconfig

        thunderbird_dir = os.path.join(self.topsrcdir, "comm")
        is_thunderbird = os.path.exists(thunderbird_dir) and bool(
            os.listdir(thunderbird_dir)
        )
        python_lib = distutils.sysconfig.get_python_lib()

        def handle_package(action, package):
            if action == "packages.txt":
                src = os.path.join(self.topsrcdir, package)
                assert os.path.isfile(src), "'%s' does not exist" % src
                submanager = VirtualenvManager(
                    self.topsrcdir,
                    self.virtualenv_root,
                    self.log_handle,
                    src,
                    populate_local_paths=self.populate_local_paths,
                )
                submanager.populate()
            elif action == "pth":
                if not self.populate_local_paths:
                    return

                path = os.path.join(self.topsrcdir, package)

                with open(os.path.join(python_lib, "mach.pth"), "a") as f:
                    # This path is relative to the .pth file.  Using a
                    # relative path allows the srcdir/objdir combination
                    # to be moved around (as long as the paths relative to
                    # each other remain the same).
                    f.write("%s\n" % os.path.relpath(path, python_lib))
            elif action == "thunderbird":
                if is_thunderbird:
                    handle_package(*package.split(":", maxsplit=1))
            elif action == "pypi":
                if len(package.split("==")) != 2:
                    raise Exception(
                        "Expected pypi package version to be pinned in the "
                        'format "package==version", found "{}"'.format(package)
                    )
                self.install_pip_package(package)
            else:
                raise Exception("Unknown action: %s" % action)

        # We always target the OS X deployment target that Python itself was
        # built with, regardless of what's in the current environment. If we
        # don't do # this, we may run into a Python bug. See
        # http://bugs.python.org/issue9516 and bug 659881.
        #
        # Note that this assumes that nothing compiled in the virtualenv is
        # shipped as part of a distribution. If we do ship anything, the
        # deployment target here may be different from what's targeted by the
        # shipping binaries and # virtualenv-produced binaries may fail to
        # work.
        #
        # We also ignore environment variables that may have been altered by
        # configure or a mozconfig activated in the current shell. We trust
        # Python is smart enough to find a proper compiler and to use the
        # proper compiler flags. If it isn't your Python is likely broken.
        IGNORE_ENV_VARIABLES = ("CC", "CXX", "CFLAGS", "CXXFLAGS", "LDFLAGS")

        try:
            old_target = os.environ.get("MACOSX_DEPLOYMENT_TARGET", None)
            sysconfig_target = distutils.sysconfig.get_config_var(
                "MACOSX_DEPLOYMENT_TARGET"
            )

            if sysconfig_target is not None:
                # MACOSX_DEPLOYMENT_TARGET is usually a string (e.g.: "10.14.6"), but
                # in some cases it is an int (e.g.: 11). Since environment variables
                # must all be str, explicitly convert it.
                os.environ["MACOSX_DEPLOYMENT_TARGET"] = str(sysconfig_target)

            old_env_variables = {}
            for k in IGNORE_ENV_VARIABLES:
                if k not in os.environ:
                    continue

                old_env_variables[k] = os.environ[k]
                del os.environ[k]

            for current_action, current_package in self.packages():
                handle_package(current_action, current_package)

        finally:
            os.environ.pop("MACOSX_DEPLOYMENT_TARGET", None)

            if old_target is not None:
                os.environ["MACOSX_DEPLOYMENT_TARGET"] = old_target

            os.environ.update(old_env_variables)

    def call_setup(self, directory, arguments):
        """Calls setup.py in a directory."""
        setup = os.path.join(directory, "setup.py")

        program = [self.python_path, setup]
        program.extend(arguments)

        # We probably could call the contents of this file inside the context
        # of this interpreter using execfile() or similar. However, if global
        # variables like sys.path are adjusted, this could cause all kinds of
        # havoc. While this may work, invoking a new process is safer.

        try:
            env = os.environ.copy()
            env.setdefault("ARCHFLAGS", get_archflags())
            output = subprocess.check_output(
                program,
                cwd=directory,
                env=env,
                stderr=subprocess.STDOUT,
                universal_newlines=True,
            )
            print(output)
        except subprocess.CalledProcessError as e:
            if "Python.h: No such file or directory" in e.output:
                print(
                    "WARNING: Python.h not found. Install Python development headers."
                )
            else:
                print(e.output)

            raise Exception("Error installing package: %s" % directory)

    def build(self, python):
        """Build a virtualenv per tree conventions.

        This returns the path of the created virtualenv.
        """
        self.create(python)

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
            self.virtualenv_root,
            self.manifest_path,
        ]
        if self.populate_local_paths:
            args.append("--populate-local-paths")

        result = self._log_process_output(args, cwd=self.topsrcdir)

        if result != 0:
            raise Exception("Error populating virtualenv.")

        os.utime(self.activate_path, None)

        return self.virtualenv_root

    def activate(self):
        """Activate the virtualenv in this Python context.

        If you run a random Python script and wish to "activate" the
        virtualenv, you can simply instantiate an instance of this class
        and call .ensure() and .activate() to make the virtualenv active.
        """

        exec(open(self.activate_path).read(), dict(__file__=self.activate_path))

    def install_pip_package(self, package, vendored=False):
        """Install a package via pip.

        The supplied package is specified using a pip requirement specifier.
        e.g. 'foo' or 'foo==1.0'.

        If the package is already installed, this is a no-op.

        If vendored is True, no package index will be used and no dependencies
        will be installed.
        """
        import mozfile
        from mozfile import TemporaryDirectory

        if sys.executable.startswith(self.bin_path):
            # If we're already running in this interpreter, we can optimize in
            # the case that the package requirement is already satisfied.
            from pip._internal.req.constructors import install_req_from_line

            req = install_req_from_line(package)
            req.check_if_exists(use_user_site=False)
            if req.satisfied_by is not None:
                return

        args = ["install"]
        vendored_dist_info_dir = None

        if vendored:
            args.extend(
                [
                    "--no-deps",
                    "--no-index",
                    # The setup will by default be performed in an isolated build
                    # environment, and since we're running with --no-index, this
                    # means that pip will be unable to install in the isolated build
                    # environment any dependencies that might be specified in a
                    # setup_requires directive for the package. Since we're manually
                    # controlling our build environment, build isolation isn't a
                    # concern and we can disable that feature. Note that this is
                    # safe and doesn't risk trampling any other packages that may be
                    # installed due to passing `--no-deps --no-index` as well.
                    "--no-build-isolation",
                ]
            )
            vendored_dist_info_dir = next(
                (d for d in os.listdir(package) if d.endswith(".dist-info")), None
            )

        with TemporaryDirectory() as tmp:
            if vendored_dist_info_dir:
                # This is a vendored wheel. We have to re-pack it in order for pip
                # to install it.
                wheel_file = os.path.join(
                    tmp, "{}-1.0-py3-none-any.whl".format(os.path.basename(package))
                )
                shutil.make_archive(wheel_file, "zip", package)
                mozfile.move("{}.zip".format(wheel_file), wheel_file)
                package = wheel_file

            args.append(package)
            return self._run_pip(args, stderr=subprocess.STDOUT)

    def install_pip_requirements(
        self, path, require_hashes=True, quiet=False, vendored=False
    ):
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

        if vendored:
            args.extend(["--no-deps", "--no-index"])

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
        site_packages = installer.install_purelib

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
    parser.add_argument("virtualenv_path")
    parser.add_argument("manifest_path")
    parser.add_argument("--populate-local-paths", action="store_true")

    if sys.argv[1] == "populate":
        # This should only be called internally.
        populate = True
        opts = parser.parse_args(sys.argv[2:])
    else:
        populate = False
        opts = parser.parse_args(sys.argv[1:])

    manager = VirtualenvManager(
        opts.topsrcdir,
        opts.virtualenv_path,
        sys.stdout,
        opts.manifest_path,
        populate_local_paths=opts.populate_local_paths,
    )

    if populate:
        manager.populate()
    else:
        manager.ensure()
