# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This file contains code for populating the virtualenv environment for
# Mozilla's build system. It is typically called as part of configure.

from __future__ import absolute_import, print_function, unicode_literals

import distutils.sysconfig
import os
import shutil
import subprocess
import sys
import warnings

from distutils.version import LooseVersion

IS_NATIVE_WIN = (sys.platform == 'win32' and os.sep == '\\')
IS_CYGWIN = (sys.platform == 'cygwin')

# Minimum versions of Python required to build.
MINIMUM_PYTHON_VERSIONS = {
    2: LooseVersion('2.7.3'),
    3: LooseVersion('3.5.0')
}

PY2 = sys.version_info[0] == 2
PY3 = sys.version_info[0] == 3

UPGRADE_WINDOWS = '''
Please upgrade to the latest MozillaBuild development environment. See
https://developer.mozilla.org/en-US/docs/Developer_Guide/Build_Instructions/Windows_Prerequisites
'''.lstrip()

UPGRADE_OTHER = '''
Run |mach bootstrap| to ensure your system is up to date.

If you still receive this error, your shell environment is likely detecting
another Python version. Ensure a modern Python can be found in the paths
defined by the $PATH environment variable and try again.
'''.lstrip()

here = os.path.abspath(os.path.dirname(__file__))


class VirtualenvManager(object):
    """Contains logic for managing virtualenvs for building the tree."""

    def __init__(self, topsrcdir, topobjdir, virtualenv_path, log_handle,
                 manifest_path):
        """Create a new manager.

        Each manager is associated with a source directory, a path where you
        want the virtualenv to be created, and a handle to write output to.
        """
        assert os.path.isabs(
            manifest_path), "manifest_path must be an absolute path: %s" % (manifest_path)
        self.topsrcdir = topsrcdir
        self.topobjdir = topobjdir
        self.virtualenv_root = virtualenv_path

        # Record the Python executable that was used to create the Virtualenv
        # so we can check this against sys.executable when verifying the
        # integrity of the virtualenv.
        self.exe_info_path = os.path.join(self.virtualenv_root,
                                          'python_exe.txt')

        self.log_handle = log_handle
        self.manifest_path = manifest_path

    @property
    def virtualenv_script_path(self):
        """Path to virtualenv's own populator script."""
        return os.path.join(self.topsrcdir, 'third_party', 'python',
                            'virtualenv', 'virtualenv.py')

    @property
    def bin_path(self):
        # virtualenv.py provides a similar API via path_locations(). However,
        # we have a bit of a chicken-and-egg problem and can't reliably
        # import virtualenv. The functionality is trivial, so just implement
        # it here.
        if IS_CYGWIN or IS_NATIVE_WIN:
            return os.path.join(self.virtualenv_root, 'Scripts')

        return os.path.join(self.virtualenv_root, 'bin')

    @property
    def python_path(self):
        binary = 'python'
        if sys.platform in ('win32', 'cygwin'):
            binary += '.exe'

        return os.path.join(self.bin_path, binary)

    @property
    def version_info(self):
        return eval(subprocess.check_output([
            self.python_path, '-c', 'import sys; print(sys.version_info[:])']))

    @property
    def activate_path(self):
        return os.path.join(self.bin_path, 'activate_this.py')

    def get_exe_info(self):
        """Returns the version and file size of the python executable that was in
        use when this virtualenv was created.
        """
        with open(self.exe_info_path, 'r') as fh:
            version, size = fh.read().splitlines()
        return int(version), int(size)

    def write_exe_info(self, python):
        """Records the the version of the python executable that was in use when
        this virtualenv was created. We record this explicitly because
        on OS X our python path may end up being a different or modified
        executable.
        """
        ver = self.python_executable_hexversion(python)
        with open(self.exe_info_path, 'w') as fh:
            fh.write("%s\n" % ver)
            fh.write("%s\n" % os.path.getsize(python))

    def python_executable_hexversion(self, python):
        """Run a Python executable and return its sys.hexversion value."""
        program = 'import sys; print(sys.hexversion)'
        out = subprocess.check_output([python, '-c', program]).rstrip()
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
        if not os.path.exists(self.virtualenv_root) or \
                not os.path.exists(self.activate_path):
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
        orig_version, orig_size = self.get_exe_info()
        python_size = os.path.getsize(python)
        hexversion = self.python_executable_hexversion(python)
        if ((python, python_size) != (self.python_path, os.path.getsize(self.python_path)) and
                (hexversion, python_size) != (orig_version, orig_size)):
            return False

        # recursively check sub packages.txt files
        submanifests = [i[1] for i in self.packages()
                        if i[0] == 'packages.txt']
        for submanifest in submanifests:
            submanifest = os.path.join(self.topsrcdir, submanifest)
            submanager = VirtualenvManager(self.topsrcdir,
                                           self.topobjdir,
                                           self.virtualenv_root,
                                           self.log_handle,
                                           submanifest)
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
        # __PYVENV_LAUNCHER__ confuses pip about the python interpreter
        # See https://bugzilla.mozilla.org/show_bug.cgi?id=1607470
        os.environ.pop('__PYVENV_LAUNCHER__', None)
        if self.up_to_date(python):
            return self.virtualenv_root
        return self.build(python)

    def _log_process_output(self, *args, **kwargs):
        env = kwargs.pop('env', None) or os.environ.copy()
        # PYTHONEXECUTABLE can mess up the creation of virtualenvs when set.
        env.pop('PYTHONEXECUTABLE', None)
        kwargs['env'] = ensure_subprocess_env(env)

        if hasattr(self.log_handle, 'fileno'):
            return subprocess.call(*args, stdout=self.log_handle,
                                   stderr=subprocess.STDOUT, **kwargs)

        proc = subprocess.Popen(*args, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, **kwargs)

        for line in proc.stdout:
            if PY2:
                self.log_handle.write(line)
            else:
                self.log_handle.write(line.decode('UTF-8'))

        return proc.wait()

    def create(self, python):
        """Create a new, empty virtualenv.

        Receives the path to virtualenv's virtualenv.py script (which will be
        called out to), the path to create the virtualenv in, and a handle to
        write output to.
        """
        existed = os.path.exists(self.virtualenv_root)

        args = [python, self.virtualenv_script_path,
                # Without this, virtualenv.py may attempt to contact the outside
                # world and search for or download a newer version of pip,
                # setuptools, or wheel. This is bad for security, reproducibility,
                # and speed.
                '--no-download',
                self.virtualenv_root]

        result = self._log_process_output(args)

        if result and existed:
            # Try again after removing the previous env, see bug 1628644.
            shutil.rmtree(self.virtualenv_root)
            result = self._log_process_output(args)

        if result:
            raise Exception(
                'Failed to create virtualenv: %s (virtualenv.py retcode: %s)' % (
                    self.virtualenv_root, result))

        self.write_exe_info(python)

        return self.virtualenv_root

    def packages(self):
        mode = 'rU' if PY2 else 'r'
        with open(self.manifest_path, mode) as fh:
            packages = [line.rstrip().split(':')
                        for line in fh]
        return packages

    def populate(self):
        """Populate the virtualenv.

        The manifest file consists of colon-delimited fields. The first field
        specifies the action. The remaining fields are arguments to that
        action. The following actions are supported:

        setup.py -- Invoke setup.py for a package. Expects the arguments:
            1. relative path directory containing setup.py.
            2. argument(s) to setup.py. e.g. "develop". Each program argument
               is delimited by a colon. Arguments with colons are not yet
               supported.

        filename.pth -- Adds the path given as argument to filename.pth under
            the virtualenv site packages directory.

        optional -- This denotes the action as optional. The requested action
            is attempted. If it fails, we issue a warning and go on. The
            initial "optional" field is stripped then the remaining line is
            processed like normal. e.g.
            "optional:setup.py:python/foo:built_ext:-i"

        copy -- Copies the given file in the virtualenv site packages
            directory.

        packages.txt -- Denotes that the specified path is a child manifest. It
            will be read and processed as if its contents were concatenated
            into the manifest being read.

        objdir -- Denotes a relative path in the object directory to add to the
            search path. e.g. "objdir:build" will add $topobjdir/build to the
            search path.

        windows -- This denotes that the action should only be taken when run
            on Windows.

        !windows -- This denotes that the action should only be taken when run
            on non-Windows systems.

        python3 -- This denotes that the action should only be taken when run
            on Python 3.

        python2 -- This denotes that the action should only be taken when run
            on python 2.

        Note that the Python interpreter running this function should be the
        one from the virtualenv. If it is the system Python or if the
        environment is not configured properly, packages could be installed
        into the wrong place. This is how virtualenv's work.
        """
        packages = self.packages()
        python_lib = distutils.sysconfig.get_python_lib()

        def handle_package(package):
            if package[0] == 'setup.py':
                assert len(package) >= 2

                self.call_setup(os.path.join(self.topsrcdir, package[1]),
                                package[2:])

                return True

            if package[0] == 'copy':
                assert len(package) == 2

                src = os.path.join(self.topsrcdir, package[1])
                dst = os.path.join(python_lib, os.path.basename(package[1]))

                shutil.copy(src, dst)

                return True

            if package[0] == 'packages.txt':
                assert len(package) == 2

                src = os.path.join(self.topsrcdir, package[1])
                assert os.path.isfile(src), "'%s' does not exist" % src
                submanager = VirtualenvManager(self.topsrcdir,
                                               self.topobjdir,
                                               self.virtualenv_root,
                                               self.log_handle,
                                               src)
                submanager.populate()

                return True

            if package[0].endswith('.pth'):
                assert len(package) == 2

                path = os.path.join(self.topsrcdir, package[1])

                with open(os.path.join(python_lib, package[0]), 'a') as f:
                    # This path is relative to the .pth file.  Using a
                    # relative path allows the srcdir/objdir combination
                    # to be moved around (as long as the paths relative to
                    # each other remain the same).
                    try:
                        f.write("%s\n" % os.path.relpath(path, python_lib))
                    except ValueError:
                        # When objdir is on a separate drive, relpath throws
                        f.write("%s\n" % os.path.join(python_lib, path))

                return True

            if package[0] == 'optional':
                try:
                    handle_package(package[1:])
                    return True
                except Exception:
                    print('Error processing command. Ignoring',
                          'because optional. (%s)' % ':'.join(package),
                          file=self.log_handle)
                    return False

            if package[0] in ('windows', '!windows'):
                for_win = not package[0].startswith('!')
                is_win = sys.platform == 'win32'
                if is_win == for_win:
                    handle_package(package[1:])
                return True

            if package[0] in ('python2', 'python3'):
                for_python3 = package[0].endswith('3')
                if PY3 == for_python3:
                    handle_package(package[1:])
                return True

            if package[0] == 'objdir':
                assert len(package) == 2
                path = os.path.join(self.topobjdir, package[1])

                with open(os.path.join(python_lib, 'objdir.pth'), 'a') as f:
                    f.write('%s\n' % path)

                return True

            raise Exception('Unknown action: %s' % package[0])

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
        IGNORE_ENV_VARIABLES = ('CC', 'CXX', 'CFLAGS', 'CXXFLAGS', 'LDFLAGS')

        try:
            old_target = os.environ.get('MACOSX_DEPLOYMENT_TARGET', None)
            sysconfig_target = \
                distutils.sysconfig.get_config_var('MACOSX_DEPLOYMENT_TARGET')

            if sysconfig_target is not None:
                os.environ['MACOSX_DEPLOYMENT_TARGET'] = sysconfig_target

            old_env_variables = {}
            for k in IGNORE_ENV_VARIABLES:
                if k not in os.environ:
                    continue

                old_env_variables[k] = os.environ[k]
                del os.environ[k]

            # HACK ALERT.
            #
            # The following adjustment to the VSNNCOMNTOOLS environment
            # variables are wrong. This is done as a hack to facilitate the
            # building of binary Python packages - notably psutil - on Windows
            # machines that don't have the Visual Studio 2008 binaries
            # installed. This hack assumes the Python on that system was built
            # with Visual Studio 2008. The hack is wrong for the reasons
            # explained at
            # http://stackoverflow.com/questions/3047542/building-lxml-for-python-2-7-on-windows/5122521#5122521.
            if sys.platform in ('win32', 'cygwin') and \
                    'VS90COMNTOOLS' not in os.environ:

                warnings.warn('Hacking environment to allow binary Python '
                              'extensions to build. You can make this warning go away '
                              'by installing Visual Studio 2008. You can download the '
                              'Express Edition installer from '
                              'http://go.microsoft.com/?linkid=7729279')

                # We list in order from oldest to newest to prefer the closest
                # to 2008 so differences are minimized.
                for ver in ('100', '110', '120'):
                    var = 'VS%sCOMNTOOLS' % ver
                    if var in os.environ:
                        os.environ['VS90COMNTOOLS'] = os.environ[var]
                        break

            for package in packages:
                handle_package(package)

            sitecustomize = os.path.join(
                os.path.dirname(os.__file__), 'sitecustomize.py')
            with open(sitecustomize, 'w') as f:
                f.write(
                    '# Importing mach_bootstrap has the side effect of\n'
                    '# installing an import hook\n'
                    'import mach_bootstrap\n'
                )

        finally:
            os.environ.pop('MACOSX_DEPLOYMENT_TARGET', None)

            if old_target is not None:
                os.environ['MACOSX_DEPLOYMENT_TARGET'] = old_target

            os.environ.update(old_env_variables)

    def call_setup(self, directory, arguments):
        """Calls setup.py in a directory."""
        setup = os.path.join(directory, 'setup.py')

        program = [self.python_path, setup]
        program.extend(arguments)

        # We probably could call the contents of this file inside the context
        # of this interpreter using execfile() or similar. However, if global
        # variables like sys.path are adjusted, this could cause all kinds of
        # havoc. While this may work, invoking a new process is safer.

        try:
            output = subprocess.check_output(program, cwd=directory, stderr=subprocess.STDOUT)
            print(output)
        except subprocess.CalledProcessError as e:
            if 'Python.h: No such file or directory' in e.output:
                print('WARNING: Python.h not found. Install Python development headers.')
            else:
                print(e.output)

            raise Exception('Error installing package: %s' % directory)

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
        if os.path.splitext(__file__)[1] in ('.pyc', '.pyo'):
            thismodule = __file__[:-1]
        else:
            thismodule = __file__

        args = [self.python_path, thismodule, 'populate', self.topsrcdir,
                self.topobjdir, self.virtualenv_root, self.manifest_path]

        result = self._log_process_output(args, cwd=self.topsrcdir)

        if result != 0:
            raise Exception('Error populating virtualenv.')

        os.utime(self.activate_path, None)

        return self.virtualenv_root

    def activate(self):
        """Activate the virtualenv in this Python context.

        If you run a random Python script and wish to "activate" the
        virtualenv, you can simply instantiate an instance of this class
        and call .ensure() and .activate() to make the virtualenv active.
        """

        exec(open(self.activate_path).read(), dict(__file__=self.activate_path))
        if PY2 and isinstance(os.environ['PATH'], unicode):
            os.environ['PATH'] = os.environ['PATH'].encode('utf-8')

    def install_pip_package(self, package, vendored=False):
        """Install a package via pip.

        The supplied package is specified using a pip requirement specifier.
        e.g. 'foo' or 'foo==1.0'.

        If the package is already installed, this is a no-op.

        If vendored is True, no package index will be used and no dependencies
        will be installed.
        """
        from pip._internal.req.constructors import install_req_from_line

        req = install_req_from_line(package)
        req.check_if_exists(use_user_site=False)
        if req.satisfied_by is not None:
            return

        args = [
            'install',
            package,
        ]

        if vendored:
            args.extend([
                '--no-deps',
                '--no-index',
                # The setup will by default be performed in an isolated build
                # environment, and since we're running with --no-index, this
                # means that pip will be unable to install in the isolated build
                # environment any dependencies that might be specified in a
                # setup_requires directive for the package. Since we're manually
                # controlling our build environment, build isolation isn't a
                # concern and we can disable that feature. Note that this is
                # safe and doesn't risk trampling any other packages that may be
                # installed due to passing `--no-deps --no-index` as well.
                '--no-build-isolation',
            ])

        return self._run_pip(args)

    def install_pip_requirements(self, path, require_hashes=True, quiet=False, vendored=False):
        """Install a pip requirements.txt file.

        The supplied path is a text file containing pip requirement
        specifiers.

        If require_hashes is True, each specifier must contain the
        expected hash of the downloaded package. See:
        https://pip.pypa.io/en/stable/reference/pip_install/#hash-checking-mode
        """

        if not os.path.isabs(path):
            path = os.path.join(self.topsrcdir, path)

        args = [
            'install',
            '--requirement',
            path,
        ]

        if require_hashes:
            args.append('--require-hashes')

        if quiet:
            args.append('--quiet')

        if vendored:
            args.extend([
                '--no-deps',
                '--no-index',
            ])

        return self._run_pip(args)

    def _run_pip(self, args):
        # It's tempting to call pip natively via pip.main(). However,
        # the current Python interpreter may not be the virtualenv python.
        # This will confuse pip and cause the package to attempt to install
        # against the executing interpreter. By creating a new process, we
        # force the virtualenv's interpreter to be used and all is well.
        # It /might/ be possible to cheat and set sys.executable to
        # self.python_path. However, this seems more risk than it's worth.
        pip = os.path.join(self.bin_path, 'pip')
        subprocess.check_call([pip] + args, stderr=subprocess.STDOUT, cwd=self.topsrcdir,
                              universal_newlines=PY3)

    def activate_pipenv(self, pipfile=None, populate=False, python=None):
        """Activate a virtual environment managed by pipenv

        If ``pipfile`` is not ``None`` then the Pipfile located at the path
        provided will be used to create the virtual environment. If
        ``populate`` is ``True`` then the virtual environment will be
        populated from the manifest file. The optional ``python`` argument
        indicates the version of Python for pipenv to use.
        """
        pipenv = os.path.join(self.bin_path, 'pipenv')
        env = ensure_subprocess_env(os.environ.copy())
        env.update(ensure_subprocess_env({
            'PIPENV_IGNORE_VIRTUALENVS': '1',
            'WORKON_HOME': str(os.path.normpath(os.path.join(self.topobjdir, '_virtualenvs')))
        }))
        # On mac, running pipenv with LC_CTYPE set to "UTF-8" (which happens
        # when wrapping with run-task on automation) fails.
        # Unsetting it doesn't really matter for what pipenv does.
        env.pop('LC_CTYPE', None)

        if python is not None:
            env.update(ensure_subprocess_env({
                'PIPENV_DEFAULT_PYTHON_VERSION': str(python),
                'PIPENV_PYTHON': str(python)
            }))

        def ensure_venv():
            """Create virtual environment if needed and return path"""
            venv = get_venv()
            if venv is not None:
                return venv
            if python is not None:
                subprocess.check_call(
                    [pipenv, '--python', python],
                    stderr=subprocess.STDOUT,
                    env=env)
            return get_venv()

        def get_venv():
            """Return path to virtual environment or None"""
            try:
                return subprocess.check_output(
                        [pipenv, '--venv'],
                        stderr=subprocess.STDOUT,
                        env=env, universal_newlines=True).rstrip()

            except subprocess.CalledProcessError:
                # virtual environment does not exist
                return None

        if pipfile is not None:
            # Install from Pipfile
            env.update(ensure_subprocess_env({
                'PIPENV_PIPFILE': str(pipfile)
            }))
            subprocess.check_call([pipenv, 'install'], stderr=subprocess.STDOUT, env=env)

        self.virtualenv_root = ensure_venv()

        if populate:
            # Populate from the manifest
            subprocess.check_call([
                pipenv, 'run', 'python', os.path.join(here, 'virtualenv.py'), 'populate',
                self.topsrcdir, self.topobjdir, self.virtualenv_root, self.manifest_path],
                stderr=subprocess.STDOUT, env=env)

        self.activate()


def verify_python_version(log_handle):
    """Ensure the current version of Python is sufficient."""
    major, minor, micro = sys.version_info[:3]

    our = LooseVersion('%d.%d.%d' % (major, minor, micro))

    if major not in MINIMUM_PYTHON_VERSIONS or our < MINIMUM_PYTHON_VERSIONS[major]:
        log_handle.write('One of the following Python versions are required to build:\n')
        for minver in MINIMUM_PYTHON_VERSIONS.values():
            log_handle.write('* Python %s or greater\n' % minver)
        log_handle.write('You are running Python %s.\n' % our)

        if os.name in ('nt', 'ce'):
            log_handle.write(UPGRADE_WINDOWS)
        else:
            log_handle.write(UPGRADE_OTHER)

        sys.exit(1)


def ensure_subprocess_env(env, encoding='utf-8'):
    """Ensure the environment is in the correct format for the `subprocess`
    module.

    This method uses the method with same name from mozbuild.utils as
    virtualenv.py must be a standalone module.

    This will convert all keys and values to bytes on Python 2, and text on
    Python 3.

    Args:
        env (dict): Environment to ensure.
        encoding (str): Encoding to use when converting to/from bytes/text
                        (default: utf-8).
    """
    # We can't import six.ensure_binary() or six.ensure_text() because this module
    # has to run stand-alone.  Instead we'll implement an abbreviated version of the
    # checks it does.

    if PY3:
        text_type = str
        binary_type = bytes
    else:
        text_type = unicode
        binary_type = str

    def ensure_binary(s):
        if isinstance(s, text_type):
            return s.encode(encoding, errors='strict')
        elif isinstance(s, binary_type):
            return s
        else:
            raise TypeError("not expecting type '%s'" % type(s))

    def ensure_text(s):
        if isinstance(s, binary_type):
            return s.decode(encoding, errors='strict')
        elif isinstance(s, text_type):
            return s
        else:
            raise TypeError("not expecting type '%s'" % type(s))

    ensure = ensure_binary if PY2 else ensure_text

    try:
        return {ensure(k): ensure(v) for k, v in env.iteritems()}
    except AttributeError:
        return {ensure(k): ensure(v) for k, v in env.items()}


if __name__ == '__main__':
    if len(sys.argv) < 5:
        print(
            'Usage: populate_virtualenv.py /path/to/topsrcdir '
            '/path/to/topobjdir /path/to/virtualenv /path/to/virtualenv_manifest')
        sys.exit(1)

    verify_python_version(sys.stdout)

    topsrcdir, topobjdir, virtualenv_path, manifest_path = sys.argv[1:5]
    populate = False

    # This should only be called internally.
    if sys.argv[1] == 'populate':
        populate = True
        topsrcdir, topobjdir, virtualenv_path, manifest_path = sys.argv[2:]

    manager = VirtualenvManager(topsrcdir, topobjdir, virtualenv_path,
                                sys.stdout, manifest_path)

    if populate:
        manager.populate()
    else:
        manager.ensure()
