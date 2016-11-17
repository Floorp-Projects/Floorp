# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import hashlib
import os
import re
import subprocess
import sys
import urllib2

from distutils.version import LooseVersion
from mozboot import rust

NO_MERCURIAL = '''
Could not find Mercurial (hg) in the current shell's path. Try starting a new
shell and running the bootstrapper again.
'''

MERCURIAL_UNABLE_UPGRADE = '''
You are currently running Mercurial %s. Running %s or newer is
recommended for performance and stability reasons.

Unfortunately, this bootstrapper currently does not know how to automatically
upgrade Mercurial on your machine.

You can usually install Mercurial through your package manager or by
downloading a package from http://mercurial.selenic.com/.
'''

MERCURIAL_UPGRADE_FAILED = '''
We attempted to upgrade Mercurial to a modern version (%s or newer).
However, you appear to have version %s still.

It's possible your package manager doesn't support a modern version of
Mercurial. It's also possible Mercurial is not being installed in the search
path for this shell. Try creating a new shell and run this bootstrapper again.

If it continues to fail, consider installing Mercurial by following the
instructions at http://mercurial.selenic.com/.
'''

PYTHON_UNABLE_UPGRADE = '''
You are currently running Python %s. Running %s or newer (but
not 3.x) is required.

Unfortunately, this bootstrapper does not currently know how to automatically
upgrade Python on your machine.

Please search the Internet for how to upgrade your Python and try running this
bootstrapper again to ensure your machine is up to date.
'''

PYTHON_UPGRADE_FAILED = '''
We attempted to upgrade Python to a modern version (%s or newer).
However, you appear to still have version %s.

It's possible your package manager doesn't yet expose a modern version of
Python. It's also possible Python is not being installed in the search path for
this shell. Try creating a new shell and run this bootstrapper again.

If this continues to fail and you are sure you have a modern Python on your
system, ensure it is on the $PATH and try again. If that fails, you'll need to
install Python manually and ensure the path with the python binary is listed in
the $PATH environment variable.

We recommend the following tools for installing Python:

    pyenv   -- https://github.com/yyuu/pyenv)
    pythonz -- https://github.com/saghul/pythonz
    official installers -- http://www.python.org/
'''

RUST_NOT_IN_PATH = '''
You have some rust files in %(cargo_bin)s, but they're not part of the
standard PATH.

To make these available, please add this directory to the PATH variable
in your shell initialization script, which may be called ~/.bashrc or
~/.bash_profile or ~/.profile. Edit this and add the following line:

    source %(cargo_home)s/env

Then restart your shell and run the bootstrap script again.
'''

RUSTUP_OLD = '''
We found an executable called `rustup` which we normally use to install
and upgrade Rust programming language support, but we didn't understand
its output. It may be an old version, or not be the installer from
https://rustup.rs/

Please move it out of the way and run the bootstrap script again.
Or if you prefer and know how, use the current rustup to install
a compatible version of the Rust programming language yourself.
'''

RUST_UPGRADE_FAILED = '''
We attempted to upgrade Rust to a modern version (%s or newer).
However, you appear to still have version %s.

It's possible rustup failed. It's also possible the new Rust is not being
installed in the search path for this shell. Try creating a new shell and
run this bootstrapper again.

If this continues to fail and you are sure you have a modern Rust on your
system, ensure it is on the $PATH and try again. If that fails, you'll need to
install Rust manually and ensure the path with the rustc and cargo  binaries
are listed in the $PATH environment variable.

We recommend the installer from https://rustup.rs/ for installing Rust,
but you may be able to get a recent enough version from a software install
tool or package manager on your system, or directly from https://rust-lang.org/
'''

BROWSER_ARTIFACT_MODE_MOZCONFIG = '''
Paste the lines between the chevrons (>>> and <<<) into your mozconfig file:

<<<
# Automatically download and use compiled C++ components:
ac_add_options --enable-artifact-builds
>>>
'''

# Upgrade Mercurial older than this.
# This should match OLDEST_NON_LEGACY_VERSION from
# the hg setup wizard in version-control-tools.
MODERN_MERCURIAL_VERSION = LooseVersion('3.7.3')

# Upgrade Python older than this.
MODERN_PYTHON_VERSION = LooseVersion('2.7.3')

# Upgrade rust older than this.
MODERN_RUST_VERSION = LooseVersion('1.13.0')

class BaseBootstrapper(object):
    """Base class for system bootstrappers."""

    def __init__(self, no_interactive=False):
        self.package_manager_updated = False
        self.no_interactive = no_interactive

    def install_system_packages(self):
        '''
        Install packages shared by all applications. These are usually
        packages required by the development (like mercurial) or the
        build system (like autoconf).
        '''
        raise NotImplementedError('%s must implement install_system_packages()' %
                                  __name__)

    def install_browser_packages(self):
        '''
        Install packages required to build Firefox for Desktop (application
        'browser').
        '''
        raise NotImplementedError('Cannot bootstrap Firefox for Desktop: '
                                  '%s does not yet implement install_browser_packages()' %
                                  __name__)

    def suggest_browser_mozconfig(self):
        '''
        Print a message to the console detailing what the user's mozconfig
        should contain.

        Firefox for Desktop can in simple cases determine its build environment
        entirely from configure.
        '''
        pass

    def install_browser_artifact_mode_packages(self):
        '''
        Install packages required to build Firefox for Desktop (application
        'browser') in Artifact Mode.
        '''
        raise NotImplementedError(
            'Cannot bootstrap Firefox for Desktop Artifact Mode: '
            '%s does not yet implement install_browser_artifact_mode_packages()' %
            __name__)

    def suggest_browser_artifact_mode_mozconfig(self):
        '''
        Print a message to the console detailing what the user's mozconfig
        should contain.

        Firefox for Desktop Artifact Mode needs to enable artifact builds and
        a path where the build artifacts will be written to.
        '''
        print(BROWSER_ARTIFACT_MODE_MOZCONFIG)

    def install_mobile_android_packages(self):
        '''
        Install packages required to build Firefox for Android (application
        'mobile/android', also known as Fennec).
        '''
        raise NotImplementedError('Cannot bootstrap Firefox for Android: '
                                  '%s does not yet implement install_mobile_android_packages()'
                                  % __name__)

    def suggest_mobile_android_mozconfig(self):
        '''
        Print a message to the console detailing what the user's mozconfig
        should contain.

        Firefox for Android needs an application and an ABI set, and it needs
        paths to the Android SDK and NDK.
        '''
        raise NotImplementedError('%s does not yet implement suggest_mobile_android_mozconfig()' %
                                  __name__)

    def install_mobile_android_artifact_mode_packages(self):
        '''
        Install packages required to build Firefox for Android (application
        'mobile/android', also known as Fennec) in Artifact Mode.
        '''
        raise NotImplementedError(
            'Cannot bootstrap Firefox for Android Artifact Mode: '
            '%s does not yet implement install_mobile_android_artifact_mode_packages()'
            % __name__)

    def suggest_mobile_android_artifact_mode_mozconfig(self):
        '''
        Print a message to the console detailing what the user's mozconfig
        should contain.

        Firefox for Android Artifact Mode needs an application and an ABI set,
        and it needs paths to the Android SDK.
        '''
        raise NotImplementedError(
            '%s does not yet implement suggest_mobile_android_artifact_mode_mozconfig()'
            % __name__)

    def which(self, name):
        """Python implementation of which.

        It returns the path of an executable or None if it couldn't be found.
        """
        for path in os.environ['PATH'].split(os.pathsep):
            test = os.path.join(path, name)
            if os.path.exists(test) and os.access(test, os.X_OK):
                return test

        return None

    def run_as_root(self, command):
        if os.geteuid() != 0:
            if self.which('sudo'):
                command.insert(0, 'sudo')
            else:
                command = ['su', 'root', '-c', ' '.join(command)]

        print('Executing as root:', subprocess.list2cmdline(command))

        subprocess.check_call(command, stdin=sys.stdin)

    def dnf_install(self, *packages):
        if self.which('dnf'):
            command = ['dnf', 'install']
        else:
            command = ['yum', 'install']

        if self.no_interactive:
            command.append('-y')
        command.extend(packages)

        self.run_as_root(command)

    def dnf_groupinstall(self, *packages):
        if self.which('dnf'):
            command = ['dnf', 'groupinstall']
        else:
            command = ['yum', 'groupinstall']

        if self.no_interactive:
            command.append('-y')
        command.extend(packages)

        self.run_as_root(command)

    def dnf_update(self, *packages):
        if self.which('dnf'):
            command = ['dnf', 'update']
        else:
            command = ['yum', 'update']

        if self.no_interactive:
            command.append('-y')
        command.extend(packages)

        self.run_as_root(command)

    def apt_install(self, *packages):
        command = ['apt-get', 'install']
        if self.no_interactive:
            command.append('-y')
        command.extend(packages)

        self.run_as_root(command)

    def apt_update(self):
        command = ['apt-get', 'update']
        if self.no_interactive:
            command.append('-y')

        self.run_as_root(command)

    def apt_add_architecture(self, arch):
        command = ['dpkg', '--add-architecture']
        command.extend(arch)

        self.run_as_root(command)

    def check_output(self, *args, **kwargs):
        """Run subprocess.check_output even if Python doesn't provide it."""
        fn = getattr(subprocess, 'check_output', BaseBootstrapper._check_output)

        return fn(*args, **kwargs)

    @staticmethod
    def _check_output(*args, **kwargs):
        """Python 2.6 compatible implementation of subprocess.check_output."""
        proc = subprocess.Popen(stdout=subprocess.PIPE, *args, **kwargs)
        output, unused_err = proc.communicate()
        retcode = proc.poll()
        if retcode:
            cmd = kwargs.get('args', args[0])
            e = subprocess.CalledProcessError(retcode, cmd)
            e.output = output
            raise e

        return output

    def prompt_int(self, prompt, low, high, limit=5):
        ''' Prompts the user with prompt and requires an integer between low and high. '''
        valid = False
        while not valid and limit > 0:
            try:
                choice = int(raw_input(prompt))
                if not low <= choice <= high:
                    print("ERROR! Please enter a valid option!")
                    limit -= 1
                else:
                    valid = True
            except ValueError:
                print("ERROR! Please enter a valid option!")
                limit -= 1

        if limit > 0:
            return choice
        else:
            raise Exception("Error! Reached max attempts of entering option.")

    def _ensure_package_manager_updated(self):
        if self.package_manager_updated:
            return

        self._update_package_manager()
        self.package_manager_updated = True

    def _update_package_manager(self):
        """Updates the package manager's manifests/package list.

        This should be defined in child classes.
        """

    def _parse_version(self, path, name=None, env=None):
        '''Execute the given path, returning the version.

        Invokes the path argument with the --version switch
        and returns a LooseVersion representing the output
        if successful. If not, returns None.

        An optional name argument gives the expected program
        name returned as part of the version string, if it's
        different from the basename of the executable.

        An optional env argument allows modifying environment
        variable during the invocation to set options, PATH,
        etc.
        '''
        if not name:
            name = os.path.basename(path)

        info = self.check_output([path, '--version'],
                                 env=env,
                                 stderr=subprocess.STDOUT)
        match = re.search(name + ' ([a-z0-9\.]+)', info)
        if not match:
            print('ERROR! Unable to identify %s version.' % name)
            return None

        return LooseVersion(match.group(1))

    def _hgplain_env(self):
        """ Returns a copy of the current environment updated with the HGPLAIN
        environment variable.

        HGPLAIN prevents Mercurial from applying locale variations to the output
        making it suitable for use in scripts.
        """
        env = os.environ.copy()
        env[b'HGPLAIN'] = b'1'

        return env

    def is_mercurial_modern(self):
        hg = self.which('hg')
        if not hg:
            print(NO_MERCURIAL)
            return False, False, None

        our = self._parse_version(hg, 'version', self._hgplain_env())
        if not our:
            return True, False, None

        return True, our >= MODERN_MERCURIAL_VERSION, our

    def ensure_mercurial_modern(self):
        installed, modern, version = self.is_mercurial_modern()

        if modern:
            print('Your version of Mercurial (%s) is sufficiently modern.' %
                  version)
            return installed, modern

        self._ensure_package_manager_updated()

        if installed:
            print('Your version of Mercurial (%s) is not modern enough.' %
                  version)
            print('(Older versions of Mercurial have known security vulnerabilities. '
                  'Unless you are running a patched Mercurial version, you may be '
                  'vulnerable.')
        else:
            print('You do not have Mercurial installed')

        if self.upgrade_mercurial(version) is False:
            return installed, modern

        installed, modern, after = self.is_mercurial_modern()

        if installed and not modern:
            print(MERCURIAL_UPGRADE_FAILED % (MODERN_MERCURIAL_VERSION, after))

        return installed, modern

    def upgrade_mercurial(self, current):
        """Upgrade Mercurial.

        Child classes should reimplement this.

        Return False to not perform a version check after the upgrade is
        performed.
        """
        print(MERCURIAL_UNABLE_UPGRADE % (current, MODERN_MERCURIAL_VERSION))

    def is_python_modern(self):
        python = None

        for test in ['python2.7', 'python']:
            python = self.which(test)
            if python:
                break

        assert python

        our = self._parse_version(python, 'Python')
        if not our:
            return False, None

        return our >= MODERN_PYTHON_VERSION, our

    def ensure_python_modern(self):
        modern, version = self.is_python_modern()

        if modern:
            print('Your version of Python (%s) is new enough.' % version)
            return

        print('Your version of Python (%s) is too old. Will try to upgrade.' %
              version)

        self._ensure_package_manager_updated()
        self.upgrade_python(version)

        modern, after = self.is_python_modern()

        if not modern:
            print(PYTHON_UPGRADE_FAILED % (MODERN_PYTHON_VERSION, after))
            sys.exit(1)

    def upgrade_python(self, current):
        """Upgrade Python.

        Child classes should reimplement this.
        """
        print(PYTHON_UNABLE_UPGRADE % (current, MODERN_PYTHON_VERSION))

    def is_rust_modern(self):
        rustc = self.which('rustc')
        if not rustc:
            print('Could not find a Rust compiler.')
            return False, None

        cargo = self.which('cargo')

        our = self._parse_version(rust)
        if not our:
            return False, None

        return our >= MODERN_RUST_VERSION, our

    def ensure_rust_modern(self):
        modern, version = self.is_rust_modern()

        if modern:
            print('Your version of Rust (%s) is new enough.' % version)
            return

        if not version:
            # Rust wasn't in PATH. Try the standard path.
            cargo_home = os.environ.get('CARGO_HOME',
                    os.path.expanduser(os.path.join('~', '.cargo')))
            cargo_bin = os.path.join(cargo_home, 'bin')
            have_rustc = os.path.exists(os.path.join(cargo_bin, 'rustc'))
            have_cargo = os.path.exists(os.path.join(cargo_bin, 'cargo'))
            if have_rustc or have_cargo:
                print(RUST_NOT_IN_PATH % { 'cargo_bin': cargo_bin,
                                           'cargo_home': cargo_home })
                sys.exit(1)

        rustup = self.which('rustup')
        if rustup:
            rustup_version = self._parse_version(rustup)
            if not rustup_version:
                print(RUSTUP_OLD)
                sys.exit(1)
            if not version:
                # We have rustup but no rustc.
                # Try running rustup; maybe it will fix things.
                print('Found rustup. Will try to upgrade.')
            else:
                # We have both rustup and rustc.
                print('Your version of Rust (%s) is too old. Will try to upgrade.' %
                  version)
            self.upgrade_rust(rustup)
        else:
            # No rustc or rustup.
            print('Will try to install Rust.')
            self.install_rust()

        modern, after = self.is_rust_modern()

        if not modern:
            print(RUST_UPGRADE_FAILED % (MODERN_RUST_VERSION, after))
            sys.exit(1)

    def upgrade_rust(self, rustup):
        """Upgrade Rust.

        Invoke rustup from the given path to update the rust install."""
        subprocess.check_call([rustup, 'update'])

    def install_rust(self):
        """Download and run the rustup installer."""
        import errno
        import stat
        import tempfile
        platform = rust.platform()
        url = rust.rustup_url(platform)
        checksum = rust.rustup_hash(platform)
        if not url or not checksum:
            print('ERROR: Could not download installer.')
            sys.exit(1)
        print('Downloading rustup-init... ', end='')
        fd, rustup_init = tempfile.mkstemp(prefix=os.path.basename(url))
        os.close(fd)
        try:
            self.http_download_and_save(url, rustup_init, checksum)
            mode = os.stat(rustup_init).st_mode
            os.chmod(rustup_init, mode | stat.S_IRWXU)
            print('Ok')
            print('Running rustup-init...')
            subprocess.check_call([rustup_init, '-y',
                '--default-toolchain', 'stable',
                '--default-host', platform,
            ])
            print('Rust installation complete.')
        finally:
            try:
                os.remove(rustup_init)
            except OSError as e:
                if e.errno != errno.ENOENT:
                    raise

    def http_download_and_save(self, url, dest, sha256hexhash):
        f = urllib2.urlopen(url)
        h = hashlib.sha256()
        with open(dest, 'wb') as out:
            while True:
                data = f.read(4096)
                if data:
                    out.write(data)
                    h.update(data)
                else:
                    break
        if h.hexdigest() != sha256hexhash:
            os.remove(dest)
            raise ValueError('Hash of downloaded file does not match expected hash')
