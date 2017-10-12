#!/usr/bin/env python
"""Create a "virtual" Python installation"""

import os
import sys

# If we are running in a new interpreter to create a virtualenv,
# we do NOT want paths from our existing location interfering with anything,
# So we remove this file's directory from sys.path - most likely to be
# the previous interpreter's site-packages. Solves #705, #763, #779
if os.environ.get('VIRTUALENV_INTERPRETER_RUNNING'):
    for path in sys.path[:]:
        if os.path.realpath(os.path.dirname(__file__)) == os.path.realpath(path):
            sys.path.remove(path)

import base64
import codecs
import optparse
import re
import shutil
import logging
import zlib
import errno
import glob
import distutils.sysconfig
import struct
import subprocess
import pkgutil
import tempfile
import textwrap
from distutils.util import strtobool
from os.path import join

try:
    import ConfigParser
except ImportError:
    import configparser as ConfigParser

__version__ = "15.0.1"
virtualenv_version = __version__  # legacy

if sys.version_info < (2, 6):
    print('ERROR: %s' % sys.exc_info()[1])
    print('ERROR: this script requires Python 2.6 or greater.')
    sys.exit(101)

try:
    basestring
except NameError:
    basestring = str

py_version = 'python%s.%s' % (sys.version_info[0], sys.version_info[1])

is_jython = sys.platform.startswith('java')
is_pypy = hasattr(sys, 'pypy_version_info')
is_win = (sys.platform == 'win32' and os.sep == '\\')
is_cygwin = (sys.platform == 'cygwin')
is_msys2 = (sys.platform == 'win32' and os.sep == '/')
is_darwin = (sys.platform == 'darwin')
abiflags = getattr(sys, 'abiflags', '')

user_dir = os.path.expanduser('~')
if is_win:
    default_storage_dir = os.path.join(user_dir, 'virtualenv')
else:
    default_storage_dir = os.path.join(user_dir, '.virtualenv')
default_config_file = os.path.join(default_storage_dir, 'virtualenv.ini')

if is_pypy:
    expected_exe = 'pypy'
elif is_jython:
    expected_exe = 'jython'
else:
    expected_exe = 'python'

# Return a mapping of version -> Python executable
# Only provided for Windows, where the information in the registry is used
if not is_win:
    def get_installed_pythons():
        return {}
else:
    try:
        import winreg
    except ImportError:
        import _winreg as winreg

    def get_installed_pythons():
        try:
            python_core = winreg.CreateKey(winreg.HKEY_LOCAL_MACHINE,
                                           "Software\\Python\\PythonCore")
        except WindowsError:
            # No registered Python installations
            return {}
        i = 0
        versions = []
        while True:
            try:
                versions.append(winreg.EnumKey(python_core, i))
                i = i + 1
            except WindowsError:
                break
        exes = dict()
        for ver in versions:
            try:
                path = winreg.QueryValue(python_core, "%s\\InstallPath" % ver)
            except WindowsError:
                continue
            exes[ver] = join(path, "python.exe")

        winreg.CloseKey(python_core)

        # Add the major versions
        # Sort the keys, then repeatedly update the major version entry
        # Last executable (i.e., highest version) wins with this approach
        for ver in sorted(exes):
            exes[ver[0]] = exes[ver]

        return exes

REQUIRED_MODULES = ['os', 'posix', 'posixpath', 'nt', 'ntpath', 'genericpath',
                    'fnmatch', 'locale', 'encodings', 'codecs',
                    'stat', 'UserDict', 'readline', 'copy_reg', 'types',
                    're', 'sre', 'sre_parse', 'sre_constants', 'sre_compile',
                    'zlib']

REQUIRED_FILES = ['lib-dynload', 'config']

majver, minver = sys.version_info[:2]
if majver == 2:
    if minver >= 6:
        REQUIRED_MODULES.extend(['warnings', 'linecache', '_abcoll', 'abc'])
    if minver >= 7:
        REQUIRED_MODULES.extend(['_weakrefset'])
    if is_msys2:
        REQUIRED_MODULES.extend(['functools'])
elif majver == 3:
    # Some extra modules are needed for Python 3, but different ones
    # for different versions.
    REQUIRED_MODULES.extend([
    	'_abcoll', 'warnings', 'linecache', 'abc', 'io', '_weakrefset',
    	'copyreg', 'tempfile', 'random', '__future__', 'collections',
    	'keyword', 'tarfile', 'shutil', 'struct', 'copy', 'tokenize',
    	'token', 'functools', 'heapq', 'bisect', 'weakref', 'reprlib'
    ])
    if minver >= 2:
        REQUIRED_FILES[-1] = 'config-%s' % majver
    if minver >= 3:
        import sysconfig
        platdir = sysconfig.get_config_var('PLATDIR')
        REQUIRED_FILES.append(platdir)
        REQUIRED_MODULES.extend([
        	'base64', '_dummy_thread', 'hashlib', 'hmac',
        	'imp', 'importlib', 'rlcompleter'
        ])
    if minver >= 4:
        REQUIRED_MODULES.extend([
            'operator',
            '_collections_abc',
            '_bootlocale',
        ])

if is_pypy:
    # these are needed to correctly display the exceptions that may happen
    # during the bootstrap
    REQUIRED_MODULES.extend(['traceback', 'linecache'])


class Logger(object):

    """
    Logging object for use in command-line script.  Allows ranges of
    levels, to avoid some redundancy of displayed information.
    """

    DEBUG = logging.DEBUG
    INFO = logging.INFO
    NOTIFY = (logging.INFO+logging.WARN)/2
    WARN = WARNING = logging.WARN
    ERROR = logging.ERROR
    FATAL = logging.FATAL

    LEVELS = [DEBUG, INFO, NOTIFY, WARN, ERROR, FATAL]

    def __init__(self, consumers):
        self.consumers = consumers
        self.indent = 0
        self.in_progress = None
        self.in_progress_hanging = False

    def debug(self, msg, *args, **kw):
        self.log(self.DEBUG, msg, *args, **kw)

    def info(self, msg, *args, **kw):
        self.log(self.INFO, msg, *args, **kw)

    def notify(self, msg, *args, **kw):
        self.log(self.NOTIFY, msg, *args, **kw)

    def warn(self, msg, *args, **kw):
        self.log(self.WARN, msg, *args, **kw)

    def error(self, msg, *args, **kw):
        self.log(self.ERROR, msg, *args, **kw)

    def fatal(self, msg, *args, **kw):
        self.log(self.FATAL, msg, *args, **kw)

    def log(self, level, msg, *args, **kw):
        if args:
            if kw:
                raise TypeError(
                    "You may give positional or keyword arguments, not both")
        args = args or kw
        rendered = None
        for consumer_level, consumer in self.consumers:
            if self.level_matches(level, consumer_level):
                if (self.in_progress_hanging
                    and consumer in (sys.stdout, sys.stderr)):
                    self.in_progress_hanging = False
                    sys.stdout.write('\n')
                    sys.stdout.flush()
                if rendered is None:
                    if args:
                        rendered = msg % args
                    else:
                        rendered = msg
                    rendered = ' '*self.indent + rendered
                if hasattr(consumer, 'write'):
                    consumer.write(rendered+'\n')
                else:
                    consumer(rendered)

    def start_progress(self, msg):
        assert not self.in_progress, (
            "Tried to start_progress(%r) while in_progress %r"
            % (msg, self.in_progress))
        if self.level_matches(self.NOTIFY, self._stdout_level()):
            sys.stdout.write(msg)
            sys.stdout.flush()
            self.in_progress_hanging = True
        else:
            self.in_progress_hanging = False
        self.in_progress = msg

    def end_progress(self, msg='done.'):
        assert self.in_progress, (
            "Tried to end_progress without start_progress")
        if self.stdout_level_matches(self.NOTIFY):
            if not self.in_progress_hanging:
                # Some message has been printed out since start_progress
                sys.stdout.write('...' + self.in_progress + msg + '\n')
                sys.stdout.flush()
            else:
                sys.stdout.write(msg + '\n')
                sys.stdout.flush()
        self.in_progress = None
        self.in_progress_hanging = False

    def show_progress(self):
        """If we are in a progress scope, and no log messages have been
        shown, write out another '.'"""
        if self.in_progress_hanging:
            sys.stdout.write('.')
            sys.stdout.flush()

    def stdout_level_matches(self, level):
        """Returns true if a message at this level will go to stdout"""
        return self.level_matches(level, self._stdout_level())

    def _stdout_level(self):
        """Returns the level that stdout runs at"""
        for level, consumer in self.consumers:
            if consumer is sys.stdout:
                return level
        return self.FATAL

    def level_matches(self, level, consumer_level):
        """
        >>> l = Logger([])
        >>> l.level_matches(3, 4)
        False
        >>> l.level_matches(3, 2)
        True
        >>> l.level_matches(slice(None, 3), 3)
        False
        >>> l.level_matches(slice(None, 3), 2)
        True
        >>> l.level_matches(slice(1, 3), 1)
        True
        >>> l.level_matches(slice(2, 3), 1)
        False
        """
        if isinstance(level, slice):
            start, stop = level.start, level.stop
            if start is not None and start > consumer_level:
                return False
            if stop is not None and stop <= consumer_level:
                return False
            return True
        else:
            return level >= consumer_level

    #@classmethod
    def level_for_integer(cls, level):
        levels = cls.LEVELS
        if level < 0:
            return levels[0]
        if level >= len(levels):
            return levels[-1]
        return levels[level]

    level_for_integer = classmethod(level_for_integer)

# create a silent logger just to prevent this from being undefined
# will be overridden with requested verbosity main() is called.
logger = Logger([(Logger.LEVELS[-1], sys.stdout)])

def mkdir(path):
    if not os.path.exists(path):
        logger.info('Creating %s', path)
        os.makedirs(path)
    else:
        logger.info('Directory %s already exists', path)

def copyfileordir(src, dest, symlink=True):
    if os.path.isdir(src):
        shutil.copytree(src, dest, symlink)
    else:
        shutil.copy2(src, dest)

def copyfile(src, dest, symlink=True):
    if not os.path.exists(src):
        # Some bad symlink in the src
        logger.warn('Cannot find file %s (bad symlink)', src)
        return
    if os.path.exists(dest):
        logger.debug('File %s already exists', dest)
        return
    if not os.path.exists(os.path.dirname(dest)):
        logger.info('Creating parent directories for %s', os.path.dirname(dest))
        os.makedirs(os.path.dirname(dest))
    if not os.path.islink(src):
        srcpath = os.path.abspath(src)
    else:
        srcpath = os.readlink(src)
    if symlink and hasattr(os, 'symlink') and not is_win:
        logger.info('Symlinking %s', dest)
        try:
            os.symlink(srcpath, dest)
        except (OSError, NotImplementedError):
            logger.info('Symlinking failed, copying to %s', dest)
            copyfileordir(src, dest, symlink)
    else:
        logger.info('Copying to %s', dest)
        copyfileordir(src, dest, symlink)

def writefile(dest, content, overwrite=True):
    if not os.path.exists(dest):
        logger.info('Writing %s', dest)
        with open(dest, 'wb') as f:
            f.write(content.encode('utf-8'))
        return
    else:
        with open(dest, 'rb') as f:
            c = f.read()
        if c != content.encode("utf-8"):
            if not overwrite:
                logger.notify('File %s exists with different content; not overwriting', dest)
                return
            logger.notify('Overwriting %s with new content', dest)
            with open(dest, 'wb') as f:
                f.write(content.encode('utf-8'))
        else:
            logger.info('Content %s already in place', dest)

def rmtree(dir):
    if os.path.exists(dir):
        logger.notify('Deleting tree %s', dir)
        shutil.rmtree(dir)
    else:
        logger.info('Do not need to delete %s; already gone', dir)

def make_exe(fn):
    if hasattr(os, 'chmod'):
        oldmode = os.stat(fn).st_mode & 0xFFF # 0o7777
        newmode = (oldmode | 0x16D) & 0xFFF # 0o555, 0o7777
        os.chmod(fn, newmode)
        logger.info('Changed mode of %s to %s', fn, oct(newmode))

def _find_file(filename, dirs):
    for dir in reversed(dirs):
        files = glob.glob(os.path.join(dir, filename))
        if files and os.path.isfile(files[0]):
            return True, files[0]
    return False, filename

def file_search_dirs():
    here = os.path.dirname(os.path.abspath(__file__))
    dirs = [here, join(here, 'virtualenv_support')]
    if os.path.splitext(os.path.dirname(__file__))[0] != 'virtualenv':
        # Probably some boot script; just in case virtualenv is installed...
        try:
            import virtualenv
        except ImportError:
            pass
        else:
            dirs.append(os.path.join(
                os.path.dirname(virtualenv.__file__), 'virtualenv_support'))
    return [d for d in dirs if os.path.isdir(d)]


class UpdatingDefaultsHelpFormatter(optparse.IndentedHelpFormatter):
    """
    Custom help formatter for use in ConfigOptionParser that updates
    the defaults before expanding them, allowing them to show up correctly
    in the help listing
    """
    def expand_default(self, option):
        if self.parser is not None:
            self.parser.update_defaults(self.parser.defaults)
        return optparse.IndentedHelpFormatter.expand_default(self, option)


class ConfigOptionParser(optparse.OptionParser):
    """
    Custom option parser which updates its defaults by checking the
    configuration files and environmental variables
    """
    def __init__(self, *args, **kwargs):
        self.config = ConfigParser.RawConfigParser()
        self.files = self.get_config_files()
        self.config.read(self.files)
        optparse.OptionParser.__init__(self, *args, **kwargs)

    def get_config_files(self):
        config_file = os.environ.get('VIRTUALENV_CONFIG_FILE', False)
        if config_file and os.path.exists(config_file):
            return [config_file]
        return [default_config_file]

    def update_defaults(self, defaults):
        """
        Updates the given defaults with values from the config files and
        the environ. Does a little special handling for certain types of
        options (lists).
        """
        # Then go and look for the other sources of configuration:
        config = {}
        # 1. config files
        config.update(dict(self.get_config_section('virtualenv')))
        # 2. environmental variables
        config.update(dict(self.get_environ_vars()))
        # Then set the options with those values
        for key, val in config.items():
            key = key.replace('_', '-')
            if not key.startswith('--'):
                key = '--%s' % key  # only prefer long opts
            option = self.get_option(key)
            if option is not None:
                # ignore empty values
                if not val:
                    continue
                # handle multiline configs
                if option.action == 'append':
                    val = val.split()
                else:
                    option.nargs = 1
                if option.action == 'store_false':
                    val = not strtobool(val)
                elif option.action in ('store_true', 'count'):
                    val = strtobool(val)
                try:
                    val = option.convert_value(key, val)
                except optparse.OptionValueError:
                    e = sys.exc_info()[1]
                    print("An error occurred during configuration: %s" % e)
                    sys.exit(3)
                defaults[option.dest] = val
        return defaults

    def get_config_section(self, name):
        """
        Get a section of a configuration
        """
        if self.config.has_section(name):
            return self.config.items(name)
        return []

    def get_environ_vars(self, prefix='VIRTUALENV_'):
        """
        Returns a generator with all environmental vars with prefix VIRTUALENV
        """
        for key, val in os.environ.items():
            if key.startswith(prefix):
                yield (key.replace(prefix, '').lower(), val)

    def get_default_values(self):
        """
        Overridding to make updating the defaults after instantiation of
        the option parser possible, update_defaults() does the dirty work.
        """
        if not self.process_default_values:
            # Old, pre-Optik 1.5 behaviour.
            return optparse.Values(self.defaults)

        defaults = self.update_defaults(self.defaults.copy())  # ours
        for option in self._get_all_options():
            default = defaults.get(option.dest)
            if isinstance(default, basestring):
                opt_str = option.get_opt_string()
                defaults[option.dest] = option.check_value(opt_str, default)
        return optparse.Values(defaults)


def main():
    parser = ConfigOptionParser(
        version=virtualenv_version,
        usage="%prog [OPTIONS] DEST_DIR",
        formatter=UpdatingDefaultsHelpFormatter())

    parser.add_option(
        '-v', '--verbose',
        action='count',
        dest='verbose',
        default=0,
        help="Increase verbosity.")

    parser.add_option(
        '-q', '--quiet',
        action='count',
        dest='quiet',
        default=0,
        help='Decrease verbosity.')

    parser.add_option(
        '-p', '--python',
        dest='python',
        metavar='PYTHON_EXE',
        help='The Python interpreter to use, e.g., --python=python2.5 will use the python2.5 '
        'interpreter to create the new environment.  The default is the interpreter that '
        'virtualenv was installed with (%s)' % sys.executable)

    parser.add_option(
        '--clear',
        dest='clear',
        action='store_true',
        help="Clear out the non-root install and start from scratch.")

    parser.set_defaults(system_site_packages=False)
    parser.add_option(
        '--no-site-packages',
        dest='system_site_packages',
        action='store_false',
        help="DEPRECATED. Retained only for backward compatibility. "
             "Not having access to global site-packages is now the default behavior.")

    parser.add_option(
        '--system-site-packages',
        dest='system_site_packages',
        action='store_true',
        help="Give the virtual environment access to the global site-packages.")

    parser.add_option(
        '--always-copy',
        dest='symlink',
        action='store_false',
        default=True,
        help="Always copy files rather than symlinking.")

    parser.add_option(
        '--unzip-setuptools',
        dest='unzip_setuptools',
        action='store_true',
        help="Unzip Setuptools when installing it.")

    parser.add_option(
        '--relocatable',
        dest='relocatable',
        action='store_true',
        help='Make an EXISTING virtualenv environment relocatable. '
             'This fixes up scripts and makes all .pth files relative.')

    parser.add_option(
        '--no-setuptools',
        dest='no_setuptools',
        action='store_true',
        help='Do not install setuptools in the new virtualenv.')

    parser.add_option(
        '--no-pip',
        dest='no_pip',
        action='store_true',
        help='Do not install pip in the new virtualenv.')

    parser.add_option(
        '--no-wheel',
        dest='no_wheel',
        action='store_true',
        help='Do not install wheel in the new virtualenv.')

    default_search_dirs = file_search_dirs()
    parser.add_option(
        '--extra-search-dir',
        dest="search_dirs",
        action="append",
        metavar='DIR',
        default=default_search_dirs,
        help="Directory to look for setuptools/pip distributions in. "
              "This option can be used multiple times.")

    parser.add_option(
        "--download",
        dest="download",
        default=True,
        action="store_true",
        help="Download preinstalled packages from PyPI.",
    )

    parser.add_option(
        "--no-download",
        '--never-download',
        dest="download",
        action="store_false",
        help="Do not download preinstalled packages from PyPI.",
    )

    parser.add_option(
        '--prompt',
        dest='prompt',
        help='Provides an alternative prompt prefix for this environment.')

    parser.add_option(
        '--setuptools',
        dest='setuptools',
        action='store_true',
        help="DEPRECATED. Retained only for backward compatibility. This option has no effect.")

    parser.add_option(
        '--distribute',
        dest='distribute',
        action='store_true',
        help="DEPRECATED. Retained only for backward compatibility. This option has no effect.")

    if 'extend_parser' in globals():
        extend_parser(parser)

    options, args = parser.parse_args()

    global logger

    if 'adjust_options' in globals():
        adjust_options(options, args)

    verbosity = options.verbose - options.quiet
    logger = Logger([(Logger.level_for_integer(2 - verbosity), sys.stdout)])

    if options.python and not os.environ.get('VIRTUALENV_INTERPRETER_RUNNING'):
        env = os.environ.copy()
        interpreter = resolve_interpreter(options.python)
        if interpreter == sys.executable:
            logger.warn('Already using interpreter %s' % interpreter)
        else:
            logger.notify('Running virtualenv with interpreter %s' % interpreter)
            env['VIRTUALENV_INTERPRETER_RUNNING'] = 'true'
            file = __file__
            if file.endswith('.pyc'):
                file = file[:-1]
            popen = subprocess.Popen([interpreter, file] + sys.argv[1:], env=env)
            raise SystemExit(popen.wait())

    if not args:
        print('You must provide a DEST_DIR')
        parser.print_help()
        sys.exit(2)
    if len(args) > 1:
        print('There must be only one argument: DEST_DIR (you gave %s)' % (
            ' '.join(args)))
        parser.print_help()
        sys.exit(2)

    home_dir = args[0]

    if os.path.exists(home_dir) and os.path.isfile(home_dir):
        logger.fatal('ERROR: File already exists and is not a directory.')
        logger.fatal('Please provide a different path or delete the file.')
        sys.exit(3)

    if os.environ.get('WORKING_ENV'):
        logger.fatal('ERROR: you cannot run virtualenv while in a workingenv')
        logger.fatal('Please deactivate your workingenv, then re-run this script')
        sys.exit(3)

    if 'PYTHONHOME' in os.environ:
        logger.warn('PYTHONHOME is set.  You *must* activate the virtualenv before using it')
        del os.environ['PYTHONHOME']

    if options.relocatable:
        make_environment_relocatable(home_dir)
        return

    create_environment(home_dir,
                       site_packages=options.system_site_packages,
                       clear=options.clear,
                       unzip_setuptools=options.unzip_setuptools,
                       prompt=options.prompt,
                       search_dirs=options.search_dirs,
                       download=options.download,
                       no_setuptools=options.no_setuptools,
                       no_pip=options.no_pip,
                       no_wheel=options.no_wheel,
                       symlink=options.symlink and hasattr(os, 'symlink')) # MOZ: Make sure we don't use symlink when we don't have it
    if 'after_install' in globals():
        after_install(options, home_dir)

def call_subprocess(cmd, show_stdout=True,
                    filter_stdout=None, cwd=None,
                    raise_on_returncode=True, extra_env=None,
                    remove_from_env=None, stdin=None):
    cmd_parts = []
    for part in cmd:
        if len(part) > 45:
            part = part[:20]+"..."+part[-20:]
        if ' ' in part or '\n' in part or '"' in part or "'" in part:
            part = '"%s"' % part.replace('"', '\\"')
        if hasattr(part, 'decode'):
            try:
                part = part.decode(sys.getdefaultencoding())
            except UnicodeDecodeError:
                part = part.decode(sys.getfilesystemencoding())
        cmd_parts.append(part)
    cmd_desc = ' '.join(cmd_parts)
    if show_stdout:
        stdout = None
    else:
        stdout = subprocess.PIPE
    logger.debug("Running command %s" % cmd_desc)
    if extra_env or remove_from_env:
        env = os.environ.copy()
        if extra_env:
            env.update(extra_env)
        if remove_from_env:
            for varname in remove_from_env:
                env.pop(varname, None)
    else:
        env = None
    try:
        proc = subprocess.Popen(
            cmd, stderr=subprocess.STDOUT,
            stdin=None if stdin is None else subprocess.PIPE,
            stdout=stdout,
            cwd=cwd, env=env)
    except Exception:
        e = sys.exc_info()[1]
        logger.fatal(
            "Error %s while executing command %s" % (e, cmd_desc))
        raise
    all_output = []
    if stdout is not None:
        if stdin is not None:
            proc.stdin.write(stdin)
            proc.stdin.close()

        stdout = proc.stdout
        encoding = sys.getdefaultencoding()
        fs_encoding = sys.getfilesystemencoding()
        while 1:
            line = stdout.readline()
            try:
                line = line.decode(encoding)
            except UnicodeDecodeError:
                line = line.decode(fs_encoding)
            if not line:
                break
            line = line.rstrip()
            all_output.append(line)
            if filter_stdout:
                level = filter_stdout(line)
                if isinstance(level, tuple):
                    level, line = level
                logger.log(level, line)
                if not logger.stdout_level_matches(level):
                    logger.show_progress()
            else:
                logger.info(line)
    else:
        proc.communicate(stdin)
    proc.wait()
    if proc.returncode:
        if raise_on_returncode:
            if all_output:
                logger.notify('Complete output from command %s:' % cmd_desc)
                logger.notify('\n'.join(all_output) + '\n----------------------------------------')
            raise OSError(
                "Command %s failed with error code %s"
                % (cmd_desc, proc.returncode))
        else:
            logger.warn(
                "Command %s had error code %s"
                % (cmd_desc, proc.returncode))

def filter_install_output(line):
    if line.strip().startswith('running'):
        return Logger.INFO
    return Logger.DEBUG

def find_wheels(projects, search_dirs):
    """Find wheels from which we can import PROJECTS.

    Scan through SEARCH_DIRS for a wheel for each PROJECT in turn. Return
    a list of the first wheel found for each PROJECT
    """

    wheels = []

    # Look through SEARCH_DIRS for the first suitable wheel. Don't bother
    # about version checking here, as this is simply to get something we can
    # then use to install the correct version.
    for project in projects:
        for dirname in search_dirs:
            # This relies on only having "universal" wheels available.
            # The pattern could be tightened to require -py2.py3-none-any.whl.
            files = glob.glob(os.path.join(dirname, project + '-*.whl'))
            if files:
                wheels.append(os.path.abspath(files[0]))
                break
        else:
            # We're out of luck, so quit with a suitable error
            logger.fatal('Cannot find a wheel for %s' % (project,))

    return wheels

def install_wheel(project_names, py_executable, search_dirs=None,
                  download=False):
    if search_dirs is None:
        search_dirs = file_search_dirs()

    wheels = find_wheels(['setuptools', 'pip'], search_dirs)
    pythonpath = os.pathsep.join(wheels)

    # PIP_FIND_LINKS uses space as the path separator and thus cannot have paths
    # with spaces in them. Convert any of those to local file:// URL form.
    try:
        from urlparse import urljoin
        from urllib import pathname2url
    except ImportError:
        from urllib.parse import urljoin
        from urllib.request import pathname2url
    def space_path2url(p):
        if ' ' not in p:
            return p
        return urljoin('file:', pathname2url(os.path.abspath(p)))
    findlinks = ' '.join(space_path2url(d) for d in search_dirs)

    SCRIPT = textwrap.dedent("""
        import sys
        import pkgutil
        import tempfile
        import os

        import pip

        cert_data = pkgutil.get_data("pip._vendor.requests", "cacert.pem")
        if cert_data is not None:
            cert_file = tempfile.NamedTemporaryFile(delete=False)
            cert_file.write(cert_data)
            cert_file.close()
        else:
            cert_file = None

        try:
            args = ["install", "--ignore-installed"]
            if cert_file is not None:
                args += ["--cert", cert_file.name]
            args += sys.argv[1:]

            sys.exit(pip.main(args))
        finally:
            if cert_file is not None:
                os.remove(cert_file.name)
    """).encode("utf8")

    cmd = [py_executable, '-'] + project_names
    logger.start_progress('Installing %s...' % (', '.join(project_names)))
    logger.indent += 2

    env = {
        "PYTHONPATH": pythonpath,
        "JYTHONPATH": pythonpath,  # for Jython < 3.x
        "PIP_FIND_LINKS": findlinks,
        "PIP_USE_WHEEL": "1",
        "PIP_ONLY_BINARY": ":all:",
        "PIP_PRE": "1",
        "PIP_USER": "0",
    }

    if not download:
        env["PIP_NO_INDEX"] = "1"

    try:
        call_subprocess(cmd, show_stdout=False, extra_env=env, stdin=SCRIPT)
    finally:
        logger.indent -= 2
        logger.end_progress()


def create_environment(home_dir, site_packages=False, clear=False,
                       unzip_setuptools=False,
                       prompt=None, search_dirs=None, download=False,
                       no_setuptools=False, no_pip=False, no_wheel=False,
                       symlink=True):
    """
    Creates a new environment in ``home_dir``.

    If ``site_packages`` is true, then the global ``site-packages/``
    directory will be on the path.

    If ``clear`` is true (default False) then the environment will
    first be cleared.
    """
    home_dir, lib_dir, inc_dir, bin_dir = path_locations(home_dir)

    py_executable = os.path.abspath(install_python(
        home_dir, lib_dir, inc_dir, bin_dir,
        site_packages=site_packages, clear=clear, symlink=symlink))

    install_distutils(home_dir)

    to_install = []

    if not no_setuptools:
        to_install.append('setuptools')

    if not no_pip:
        to_install.append('pip')

    if not no_wheel:
        to_install.append('wheel')

    if to_install:
        install_wheel(
            to_install,
            py_executable,
            search_dirs,
            download=download,
        )

    install_activate(home_dir, bin_dir, prompt)

    install_python_config(home_dir, bin_dir, prompt)

def is_executable_file(fpath):
    return os.path.isfile(fpath) and os.access(fpath, os.X_OK)

def path_locations(home_dir):
    """Return the path locations for the environment (where libraries are,
    where scripts go, etc)"""
    home_dir = os.path.abspath(home_dir)
    # XXX: We'd use distutils.sysconfig.get_python_inc/lib but its
    # prefix arg is broken: http://bugs.python.org/issue3386
    if is_win:
        # Windows has lots of problems with executables with spaces in
        # the name; this function will remove them (using the ~1
        # format):
        mkdir(home_dir)
        if ' ' in home_dir:
            import ctypes
            GetShortPathName = ctypes.windll.kernel32.GetShortPathNameW
            size = max(len(home_dir)+1, 256)
            buf = ctypes.create_unicode_buffer(size)
            try:
                u = unicode
            except NameError:
                u = str
            ret = GetShortPathName(u(home_dir), buf, size)
            if not ret:
                print('Error: the path "%s" has a space in it' % home_dir)
                print('We could not determine the short pathname for it.')
                print('Exiting.')
                sys.exit(3)
            home_dir = str(buf.value)
        lib_dir = join(home_dir, 'Lib')
        inc_dir = join(home_dir, 'Include')
        bin_dir = join(home_dir, 'Scripts')
    if is_jython:
        lib_dir = join(home_dir, 'Lib')
        inc_dir = join(home_dir, 'Include')
        bin_dir = join(home_dir, 'bin')
    elif is_pypy:
        lib_dir = home_dir
        inc_dir = join(home_dir, 'include')
        bin_dir = join(home_dir, 'bin')
    elif not is_win:
        lib_dir = join(home_dir, 'lib', py_version)
        inc_dir = join(home_dir, 'include', py_version + abiflags)
        bin_dir = join(home_dir, 'bin')
    return home_dir, lib_dir, inc_dir, bin_dir


def change_prefix(filename, dst_prefix):
    prefixes = [sys.prefix]

    if is_darwin:
        prefixes.extend((
            os.path.join("/Library/Python", sys.version[:3], "site-packages"),
            os.path.join(sys.prefix, "Extras", "lib", "python"),
            os.path.join("~", "Library", "Python", sys.version[:3], "site-packages"),
            # Python 2.6 no-frameworks
            os.path.join("~", ".local", "lib","python", sys.version[:3], "site-packages"),
            # System Python 2.7 on OSX Mountain Lion
            os.path.join("~", "Library", "Python", sys.version[:3], "lib", "python", "site-packages")))

    if hasattr(sys, 'real_prefix'):
        prefixes.append(sys.real_prefix)
    if hasattr(sys, 'base_prefix'):
        prefixes.append(sys.base_prefix)
    prefixes = list(map(os.path.expanduser, prefixes))
    prefixes = list(map(os.path.abspath, prefixes))
    # Check longer prefixes first so we don't split in the middle of a filename
    prefixes = sorted(prefixes, key=len, reverse=True)
    filename = os.path.abspath(filename)
    # On Windows, make sure drive letter is uppercase
    if is_win and filename[0] in 'abcdefghijklmnopqrstuvwxyz':
        filename = filename[0].upper() + filename[1:]
    for i, prefix in enumerate(prefixes):
        if is_win and prefix[0] in 'abcdefghijklmnopqrstuvwxyz':
            prefixes[i] = prefix[0].upper() + prefix[1:]
    for src_prefix in prefixes:
        if filename.startswith(src_prefix):
            _, relpath = filename.split(src_prefix, 1)
            if src_prefix != os.sep: # sys.prefix == "/"
                assert relpath[0] == os.sep
                relpath = relpath[1:]
            return join(dst_prefix, relpath)
    assert False, "Filename %s does not start with any of these prefixes: %s" % \
        (filename, prefixes)

def copy_required_modules(dst_prefix, symlink):
    import imp

    for modname in REQUIRED_MODULES:
        if modname in sys.builtin_module_names:
            logger.info("Ignoring built-in bootstrap module: %s" % modname)
            continue
        try:
            f, filename, _ = imp.find_module(modname)
        except ImportError:
            logger.info("Cannot import bootstrap module: %s" % modname)
        else:
            if f is not None:
                f.close()
            # special-case custom readline.so on OS X, but not for pypy:
            if modname == 'readline' and sys.platform == 'darwin' and not (
                    is_pypy or filename.endswith(join('lib-dynload', 'readline.so'))):
                dst_filename = join(dst_prefix, 'lib', 'python%s' % sys.version[:3], 'readline.so')
            elif modname == 'readline' and sys.platform == 'win32':
                # special-case for Windows, where readline is not a
                # standard module, though it may have been installed in
                # site-packages by a third-party package
                pass
            else:
                dst_filename = change_prefix(filename, dst_prefix)
            copyfile(filename, dst_filename, symlink)
            if filename.endswith('.pyc'):
                pyfile = filename[:-1]
                if os.path.exists(pyfile):
                    copyfile(pyfile, dst_filename[:-1], symlink)


def subst_path(prefix_path, prefix, home_dir):
    prefix_path = os.path.normpath(prefix_path)
    prefix = os.path.normpath(prefix)
    home_dir = os.path.normpath(home_dir)
    if not prefix_path.startswith(prefix):
        logger.warn('Path not in prefix %r %r', prefix_path, prefix)
        return
    return prefix_path.replace(prefix, home_dir, 1)


def install_python(home_dir, lib_dir, inc_dir, bin_dir, site_packages, clear, symlink=True):
    """Install just the base environment, no distutils patches etc"""
    if sys.executable.startswith(bin_dir):
        print('Please use the *system* python to run this script')
        return

    if clear:
        rmtree(lib_dir)
        ## FIXME: why not delete it?
        ## Maybe it should delete everything with #!/path/to/venv/python in it
        logger.notify('Not deleting %s', bin_dir)

    if hasattr(sys, 'real_prefix'):
        logger.notify('Using real prefix %r' % sys.real_prefix)
        prefix = sys.real_prefix
    elif hasattr(sys, 'base_prefix'):
        logger.notify('Using base prefix %r' % sys.base_prefix)
        prefix = sys.base_prefix
    else:
        prefix = sys.prefix
    mkdir(lib_dir)
    fix_lib64(lib_dir, symlink)
    stdlib_dirs = [os.path.dirname(os.__file__)]
    if is_win:
        stdlib_dirs.append(join(os.path.dirname(stdlib_dirs[0]), 'DLLs'))
    elif is_darwin:
        stdlib_dirs.append(join(stdlib_dirs[0], 'site-packages'))
    if hasattr(os, 'symlink'):
        logger.info('Symlinking Python bootstrap modules')
    else:
        logger.info('Copying Python bootstrap modules')
    logger.indent += 2
    try:
        # copy required files...
        for stdlib_dir in stdlib_dirs:
            if not os.path.isdir(stdlib_dir):
                continue
            for fn in os.listdir(stdlib_dir):
                bn = os.path.splitext(fn)[0]
                if fn != 'site-packages' and bn in REQUIRED_FILES:
                    copyfile(join(stdlib_dir, fn), join(lib_dir, fn), symlink)
        # ...and modules
        copy_required_modules(home_dir, symlink)
    finally:
        logger.indent -= 2
    mkdir(join(lib_dir, 'site-packages'))
    import site
    site_filename = site.__file__
    if site_filename.endswith('.pyc') or site_filename.endswith('.pyo'):
        site_filename = site_filename[:-1]
    elif site_filename.endswith('$py.class'):
        site_filename = site_filename.replace('$py.class', '.py')
    site_filename_dst = change_prefix(site_filename, home_dir)
    site_dir = os.path.dirname(site_filename_dst)
    # MOZ: Copies a site.py if it exists instead of using the one hex encoded in
    # this file. Necessary for some site.py fixes for MinGW64 version of python
    site_py_src_path = os.path.join(os.path.dirname(__file__), 'site.py')
    if os.path.isfile(site_py_src_path):
        shutil.copy(site_py_src_path, site_filename_dst)
    else:
        writefile(site_filename_dst, SITE_PY)
    writefile(join(site_dir, 'orig-prefix.txt'), prefix)
    site_packages_filename = join(site_dir, 'no-global-site-packages.txt')
    if not site_packages:
        writefile(site_packages_filename, '')

    if is_pypy or is_win:
        stdinc_dir = join(prefix, 'include')
    else:
        stdinc_dir = join(prefix, 'include', py_version + abiflags)
    if os.path.exists(stdinc_dir):
        copyfile(stdinc_dir, inc_dir, symlink)
    else:
        logger.debug('No include dir %s' % stdinc_dir)

    platinc_dir = distutils.sysconfig.get_python_inc(plat_specific=1)
    if platinc_dir != stdinc_dir:
        platinc_dest = distutils.sysconfig.get_python_inc(
            plat_specific=1, prefix=home_dir)
        if platinc_dir == platinc_dest:
            # Do platinc_dest manually due to a CPython bug;
            # not http://bugs.python.org/issue3386 but a close cousin
            platinc_dest = subst_path(platinc_dir, prefix, home_dir)
        if platinc_dest:
            # PyPy's stdinc_dir and prefix are relative to the original binary
            # (traversing virtualenvs), whereas the platinc_dir is relative to
            # the inner virtualenv and ignores the prefix argument.
            # This seems more evolved than designed.
            copyfile(platinc_dir, platinc_dest, symlink)

    # pypy never uses exec_prefix, just ignore it
    if sys.exec_prefix != prefix and not is_pypy:
        if is_win:
            exec_dir = join(sys.exec_prefix, 'lib')
        elif is_jython:
            exec_dir = join(sys.exec_prefix, 'Lib')
        else:
            exec_dir = join(sys.exec_prefix, 'lib', py_version)
        for fn in os.listdir(exec_dir):
            copyfile(join(exec_dir, fn), join(lib_dir, fn), symlink)

    if is_jython:
        # Jython has either jython-dev.jar and javalib/ dir, or just
        # jython.jar
        for name in 'jython-dev.jar', 'javalib', 'jython.jar':
            src = join(prefix, name)
            if os.path.exists(src):
                copyfile(src, join(home_dir, name), symlink)
        # XXX: registry should always exist after Jython 2.5rc1
        src = join(prefix, 'registry')
        if os.path.exists(src):
            copyfile(src, join(home_dir, 'registry'), symlink=False)
        copyfile(join(prefix, 'cachedir'), join(home_dir, 'cachedir'),
                 symlink=False)

    mkdir(bin_dir)
    py_executable = join(bin_dir, os.path.basename(sys.executable))
    if 'Python.framework' in prefix:
        # OS X framework builds cause validation to break
        # https://github.com/pypa/virtualenv/issues/322
        if os.environ.get('__PYVENV_LAUNCHER__'):
            del os.environ["__PYVENV_LAUNCHER__"]
        if re.search(r'/Python(?:-32|-64)*$', py_executable):
            # The name of the python executable is not quite what
            # we want, rename it.
            py_executable = os.path.join(
                    os.path.dirname(py_executable), 'python')

    logger.notify('New %s executable in %s', expected_exe, py_executable)
    pcbuild_dir = os.path.dirname(sys.executable)
    pyd_pth = os.path.join(lib_dir, 'site-packages', 'virtualenv_builddir_pyd.pth')
    if is_win and os.path.exists(os.path.join(pcbuild_dir, 'build.bat')):
        logger.notify('Detected python running from build directory %s', pcbuild_dir)
        logger.notify('Writing .pth file linking to build directory for *.pyd files')
        writefile(pyd_pth, pcbuild_dir)
    else:
        pcbuild_dir = None
        if os.path.exists(pyd_pth):
            logger.info('Deleting %s (not Windows env or not build directory python)' % pyd_pth)
            os.unlink(pyd_pth)

    if sys.executable != py_executable:
        ## FIXME: could I just hard link?
        executable = sys.executable
        shutil.copyfile(executable, py_executable)
        make_exe(py_executable)
        if is_win or is_cygwin:
            pythonw = os.path.join(os.path.dirname(sys.executable), 'pythonw.exe')
            if os.path.exists(pythonw):
                logger.info('Also created pythonw.exe')
                shutil.copyfile(pythonw, os.path.join(os.path.dirname(py_executable), 'pythonw.exe'))
            python_d = os.path.join(os.path.dirname(sys.executable), 'python_d.exe')
            python_d_dest = os.path.join(os.path.dirname(py_executable), 'python_d.exe')
            if os.path.exists(python_d):
                logger.info('Also created python_d.exe')
                shutil.copyfile(python_d, python_d_dest)
            elif os.path.exists(python_d_dest):
                logger.info('Removed python_d.exe as it is no longer at the source')
                os.unlink(python_d_dest)
            # we need to copy the DLL to enforce that windows will load the correct one.
            # may not exist if we are cygwin.
            py_executable_dll = 'python%s%s.dll' % (
                sys.version_info[0], sys.version_info[1])
            py_executable_dll_d = 'python%s%s_d.dll' % (
                sys.version_info[0], sys.version_info[1])
            pythondll = os.path.join(os.path.dirname(sys.executable), py_executable_dll)
            pythondll_d = os.path.join(os.path.dirname(sys.executable), py_executable_dll_d)
            pythondll_d_dest = os.path.join(os.path.dirname(py_executable), py_executable_dll_d)
            if os.path.exists(pythondll):
                logger.info('Also created %s' % py_executable_dll)
                shutil.copyfile(pythondll, os.path.join(os.path.dirname(py_executable), py_executable_dll))
            if os.path.exists(pythondll_d):
                logger.info('Also created %s' % py_executable_dll_d)
                shutil.copyfile(pythondll_d, pythondll_d_dest)
            elif os.path.exists(pythondll_d_dest):
                logger.info('Removed %s as the source does not exist' % pythondll_d_dest)
                os.unlink(pythondll_d_dest)
        if is_pypy:
            # make a symlink python --> pypy-c
            python_executable = os.path.join(os.path.dirname(py_executable), 'python')
            if sys.platform in ('win32', 'cygwin'):
                python_executable += '.exe'
            logger.info('Also created executable %s' % python_executable)
            copyfile(py_executable, python_executable, symlink)

            if is_win:
                for name in ['libexpat.dll', 'libpypy.dll', 'libpypy-c.dll',
                            'libeay32.dll', 'ssleay32.dll', 'sqlite3.dll',
                            'tcl85.dll', 'tk85.dll']:
                    src = join(prefix, name)
                    if os.path.exists(src):
                        copyfile(src, join(bin_dir, name), symlink)

                for d in sys.path:
                    if d.endswith('lib_pypy'):
                        break
                else:
                    logger.fatal('Could not find lib_pypy in sys.path')
                    raise SystemExit(3)
                logger.info('Copying lib_pypy')
                copyfile(d, os.path.join(home_dir, 'lib_pypy'), symlink)

    if os.path.splitext(os.path.basename(py_executable))[0] != expected_exe:
        secondary_exe = os.path.join(os.path.dirname(py_executable),
                                     expected_exe)
        py_executable_ext = os.path.splitext(py_executable)[1]
        if py_executable_ext.lower() == '.exe':
            # python2.4 gives an extension of '.4' :P
            secondary_exe += py_executable_ext
        if os.path.exists(secondary_exe):
            logger.warn('Not overwriting existing %s script %s (you must use %s)'
                        % (expected_exe, secondary_exe, py_executable))
        else:
            logger.notify('Also creating executable in %s' % secondary_exe)
            shutil.copyfile(sys.executable, secondary_exe)
            make_exe(secondary_exe)

    if '.framework' in prefix:
        if 'Python.framework' in prefix:
            logger.debug('MacOSX Python framework detected')
            # Make sure we use the embedded interpreter inside
            # the framework, even if sys.executable points to
            # the stub executable in ${sys.prefix}/bin
            # See http://groups.google.com/group/python-virtualenv/
            #                              browse_thread/thread/17cab2f85da75951
            original_python = os.path.join(
                prefix, 'Resources/Python.app/Contents/MacOS/Python')
        if 'EPD' in prefix:
            logger.debug('EPD framework detected')
            original_python = os.path.join(prefix, 'bin/python')
        shutil.copy(original_python, py_executable)

        # Copy the framework's dylib into the virtual
        # environment
        virtual_lib = os.path.join(home_dir, '.Python')

        if os.path.exists(virtual_lib):
            os.unlink(virtual_lib)
        copyfile(
            os.path.join(prefix, 'Python'),
            virtual_lib,
            symlink)

        # And then change the install_name of the copied python executable
        try:
            mach_o_change(py_executable,
                          os.path.join(prefix, 'Python'),
                          '@executable_path/../.Python')
        except:
            e = sys.exc_info()[1]
            logger.warn("Could not call mach_o_change: %s. "
                        "Trying to call install_name_tool instead." % e)
            try:
                call_subprocess(
                    ["install_name_tool", "-change",
                     os.path.join(prefix, 'Python'),
                     '@executable_path/../.Python',
                     py_executable])
            except:
                logger.fatal("Could not call install_name_tool -- you must "
                             "have Apple's development tools installed")
                raise

    if not is_win:
        # Ensure that 'python', 'pythonX' and 'pythonX.Y' all exist
        py_exe_version_major = 'python%s' % sys.version_info[0]
        py_exe_version_major_minor = 'python%s.%s' % (
            sys.version_info[0], sys.version_info[1])
        py_exe_no_version = 'python'
        required_symlinks = [ py_exe_no_version, py_exe_version_major,
                         py_exe_version_major_minor ]

        py_executable_base = os.path.basename(py_executable)

        if py_executable_base in required_symlinks:
            # Don't try to symlink to yourself.
            required_symlinks.remove(py_executable_base)

        for pth in required_symlinks:
            full_pth = join(bin_dir, pth)
            if os.path.exists(full_pth):
                os.unlink(full_pth)
            if symlink:
                os.symlink(py_executable_base, full_pth)
            else:
                copyfile(py_executable, full_pth, symlink)

    if is_win and ' ' in py_executable:
        # There's a bug with subprocess on Windows when using a first
        # argument that has a space in it.  Instead we have to quote
        # the value:
        py_executable = '"%s"' % py_executable
    # NOTE: keep this check as one line, cmd.exe doesn't cope with line breaks
    cmd = [py_executable, '-c', 'import sys;out=sys.stdout;'
        'getattr(out, "buffer", out).write(sys.prefix.encode("utf-8"))']
    logger.info('Testing executable with %s %s "%s"' % tuple(cmd))
    try:
        proc = subprocess.Popen(cmd,
                            stdout=subprocess.PIPE)
        proc_stdout, proc_stderr = proc.communicate()
    except OSError:
        e = sys.exc_info()[1]
        if e.errno == errno.EACCES:
            logger.fatal('ERROR: The executable %s could not be run: %s' % (py_executable, e))
            sys.exit(100)
        else:
            raise e

    proc_stdout = proc_stdout.strip().decode("utf-8")
    proc_stdout = os.path.normcase(os.path.abspath(proc_stdout))
    norm_home_dir = os.path.normcase(os.path.abspath(home_dir))
    if hasattr(norm_home_dir, 'decode'):
        norm_home_dir = norm_home_dir.decode(sys.getfilesystemencoding())
    if proc_stdout != norm_home_dir:
        logger.fatal(
            'ERROR: The executable %s is not functioning' % py_executable)
        logger.fatal(
            'ERROR: It thinks sys.prefix is %r (should be %r)'
            % (proc_stdout, norm_home_dir))
        logger.fatal(
            'ERROR: virtualenv is not compatible with this system or executable')
        if is_win:
            logger.fatal(
                'Note: some Windows users have reported this error when they '
                'installed Python for "Only this user" or have multiple '
                'versions of Python installed. Copying the appropriate '
                'PythonXX.dll to the virtualenv Scripts/ directory may fix '
                'this problem.')
        sys.exit(100)
    else:
        logger.info('Got sys.prefix result: %r' % proc_stdout)

    pydistutils = os.path.expanduser('~/.pydistutils.cfg')
    if os.path.exists(pydistutils):
        logger.notify('Please make sure you remove any previous custom paths from '
                      'your %s file.' % pydistutils)
    ## FIXME: really this should be calculated earlier

    fix_local_scheme(home_dir, symlink)

    if site_packages:
        if os.path.exists(site_packages_filename):
            logger.info('Deleting %s' % site_packages_filename)
            os.unlink(site_packages_filename)

    return py_executable


def install_activate(home_dir, bin_dir, prompt=None):
    if is_win or is_jython and os._name == 'nt':
        files = {
            'activate.bat': ACTIVATE_BAT,
            'deactivate.bat': DEACTIVATE_BAT,
            'activate.ps1': ACTIVATE_PS,
        }

        # MSYS needs paths of the form /c/path/to/file
        drive, tail = os.path.splitdrive(home_dir.replace(os.sep, '/'))
        home_dir_msys = (drive and "/%s%s" or "%s%s") % (drive[:1], tail)

        # Run-time conditional enables (basic) Cygwin compatibility
        home_dir_sh = ("""$(if [ "$OSTYPE" "==" "cygwin" ]; then cygpath -u '%s'; else echo '%s'; fi;)""" %
                       (home_dir, home_dir_msys))
        files['activate'] = ACTIVATE_SH.replace('__VIRTUAL_ENV__', home_dir_sh)

    else:
        files = {'activate': ACTIVATE_SH}

        # suppling activate.fish in addition to, not instead of, the
        # bash script support.
        files['activate.fish'] = ACTIVATE_FISH

        # same for csh/tcsh support...
        files['activate.csh'] = ACTIVATE_CSH

    files['activate_this.py'] = ACTIVATE_THIS

    install_files(home_dir, bin_dir, prompt, files)

def install_files(home_dir, bin_dir, prompt, files):
    if hasattr(home_dir, 'decode'):
        home_dir = home_dir.decode(sys.getfilesystemencoding())
    vname = os.path.basename(home_dir)
    for name, content in files.items():
        content = content.replace('__VIRTUAL_PROMPT__', prompt or '')
        content = content.replace('__VIRTUAL_WINPROMPT__', prompt or '(%s)' % vname)
        content = content.replace('__VIRTUAL_ENV__', home_dir)
        content = content.replace('__VIRTUAL_NAME__', vname)
        content = content.replace('__BIN_NAME__', os.path.basename(bin_dir))
        writefile(os.path.join(bin_dir, name), content)

def install_python_config(home_dir, bin_dir, prompt=None):
    if sys.platform == 'win32' or is_jython and os._name == 'nt':
        files = {}
    else:
        files = {'python-config': PYTHON_CONFIG}
    install_files(home_dir, bin_dir, prompt, files)
    for name, content in files.items():
        make_exe(os.path.join(bin_dir, name))

def install_distutils(home_dir):
    distutils_path = change_prefix(distutils.__path__[0], home_dir)
    mkdir(distutils_path)
    ## FIXME: maybe this prefix setting should only be put in place if
    ## there's a local distutils.cfg with a prefix setting?
    home_dir = os.path.abspath(home_dir)
    ## FIXME: this is breaking things, removing for now:
    #distutils_cfg = DISTUTILS_CFG + "\n[install]\nprefix=%s\n" % home_dir
    writefile(os.path.join(distutils_path, '__init__.py'), DISTUTILS_INIT)
    writefile(os.path.join(distutils_path, 'distutils.cfg'), DISTUTILS_CFG, overwrite=False)

def fix_local_scheme(home_dir, symlink=True):
    """
    Platforms that use the "posix_local" install scheme (like Ubuntu with
    Python 2.7) need to be given an additional "local" location, sigh.
    """
    try:
        import sysconfig
    except ImportError:
        pass
    else:
        if sysconfig._get_default_scheme() == 'posix_local':
            local_path = os.path.join(home_dir, 'local')
            if not os.path.exists(local_path):
                os.mkdir(local_path)
                for subdir_name in os.listdir(home_dir):
                    if subdir_name == 'local':
                        continue
                    copyfile(os.path.abspath(os.path.join(home_dir, subdir_name)), \
                                                            os.path.join(local_path, subdir_name), symlink)

def fix_lib64(lib_dir, symlink=True):
    """
    Some platforms (particularly Gentoo on x64) put things in lib64/pythonX.Y
    instead of lib/pythonX.Y.  If this is such a platform we'll just create a
    symlink so lib64 points to lib
    """
    # PyPy's library path scheme is not affected by this.
    # Return early or we will die on the following assert.
    if is_pypy:
        logger.debug('PyPy detected, skipping lib64 symlinking')
        return
    # Check we have a lib64 library path
    if not [p for p in distutils.sysconfig.get_config_vars().values()
            if isinstance(p, basestring) and 'lib64' in p]:
        return

    logger.debug('This system uses lib64; symlinking lib64 to lib')

    assert os.path.basename(lib_dir) == 'python%s' % sys.version[:3], (
        "Unexpected python lib dir: %r" % lib_dir)
    lib_parent = os.path.dirname(lib_dir)
    top_level = os.path.dirname(lib_parent)
    lib_dir = os.path.join(top_level, 'lib')
    lib64_link = os.path.join(top_level, 'lib64')
    assert os.path.basename(lib_parent) == 'lib', (
        "Unexpected parent dir: %r" % lib_parent)
    if os.path.lexists(lib64_link):
        return
    if symlink:
        os.symlink('lib', lib64_link)
    else:
        copyfile('lib', lib64_link)

def resolve_interpreter(exe):
    """
    If the executable given isn't an absolute path, search $PATH for the interpreter
    """
    # If the "executable" is a version number, get the installed executable for
    # that version
    python_versions = get_installed_pythons()
    if exe in python_versions:
        exe = python_versions[exe]

    if os.path.abspath(exe) != exe:
        paths = os.environ.get('PATH', '').split(os.pathsep)
        for path in paths:
            if os.path.exists(join(path, exe)):
                exe = join(path, exe)
                break
    if not os.path.exists(exe):
        logger.fatal('The executable %s (from --python=%s) does not exist' % (exe, exe))
        raise SystemExit(3)
    if not is_executable(exe):
        logger.fatal('The executable %s (from --python=%s) is not executable' % (exe, exe))
        raise SystemExit(3)
    return exe

def is_executable(exe):
    """Checks a file is executable"""
    return os.access(exe, os.X_OK)

############################################################
## Relocating the environment:

def make_environment_relocatable(home_dir):
    """
    Makes the already-existing environment use relative paths, and takes out
    the #!-based environment selection in scripts.
    """
    home_dir, lib_dir, inc_dir, bin_dir = path_locations(home_dir)
    activate_this = os.path.join(bin_dir, 'activate_this.py')
    if not os.path.exists(activate_this):
        logger.fatal(
            'The environment doesn\'t have a file %s -- please re-run virtualenv '
            'on this environment to update it' % activate_this)
    fixup_scripts(home_dir, bin_dir)
    fixup_pth_and_egg_link(home_dir)
    ## FIXME: need to fix up distutils.cfg

OK_ABS_SCRIPTS = ['python', 'python%s' % sys.version[:3],
                  'activate', 'activate.bat', 'activate_this.py',
                  'activate.fish', 'activate.csh']

def fixup_scripts(home_dir, bin_dir):
    if is_win:
        new_shebang_args = (
            '%s /c' % os.path.normcase(os.environ.get('COMSPEC', 'cmd.exe')),
            '', '.exe')
    else:
        new_shebang_args = ('/usr/bin/env', sys.version[:3], '')

    # This is what we expect at the top of scripts:
    shebang = '#!%s' % os.path.normcase(os.path.join(
        os.path.abspath(bin_dir), 'python%s' % new_shebang_args[2]))
    # This is what we'll put:
    new_shebang = '#!%s python%s%s' % new_shebang_args

    for filename in os.listdir(bin_dir):
        filename = os.path.join(bin_dir, filename)
        if not os.path.isfile(filename):
            # ignore subdirs, e.g. .svn ones.
            continue
        lines = None
        with open(filename, 'rb') as f:
            try:
                lines = f.read().decode('utf-8').splitlines()
            except UnicodeDecodeError:
                # This is probably a binary program instead
                # of a script, so just ignore it.
                continue
        if not lines:
            logger.warn('Script %s is an empty file' % filename)
            continue

        old_shebang = lines[0].strip()
        old_shebang = old_shebang[0:2] + os.path.normcase(old_shebang[2:])

        if not old_shebang.startswith(shebang):
            if os.path.basename(filename) in OK_ABS_SCRIPTS:
                logger.debug('Cannot make script %s relative' % filename)
            elif lines[0].strip() == new_shebang:
                logger.info('Script %s has already been made relative' % filename)
            else:
                logger.warn('Script %s cannot be made relative (it\'s not a normal script that starts with %s)'
                            % (filename, shebang))
            continue
        logger.notify('Making script %s relative' % filename)
        script = relative_script([new_shebang] + lines[1:])
        with open(filename, 'wb') as f:
            f.write('\n'.join(script).encode('utf-8'))


def relative_script(lines):
    "Return a script that'll work in a relocatable environment."
    activate = "import os; activate_this=os.path.join(os.path.dirname(os.path.realpath(__file__)), 'activate_this.py'); exec(compile(open(activate_this).read(), activate_this, 'exec'), dict(__file__=activate_this)); del os, activate_this"
    # Find the last future statement in the script. If we insert the activation
    # line before a future statement, Python will raise a SyntaxError.
    activate_at = None
    for idx, line in reversed(list(enumerate(lines))):
        if line.split()[:3] == ['from', '__future__', 'import']:
            activate_at = idx + 1
            break
    if activate_at is None:
        # Activate after the shebang.
        activate_at = 1
    return lines[:activate_at] + ['', activate, ''] + lines[activate_at:]

def fixup_pth_and_egg_link(home_dir, sys_path=None):
    """Makes .pth and .egg-link files use relative paths"""
    home_dir = os.path.normcase(os.path.abspath(home_dir))
    if sys_path is None:
        sys_path = sys.path
    for path in sys_path:
        if not path:
            path = '.'
        if not os.path.isdir(path):
            continue
        path = os.path.normcase(os.path.abspath(path))
        if not path.startswith(home_dir):
            logger.debug('Skipping system (non-environment) directory %s' % path)
            continue
        for filename in os.listdir(path):
            filename = os.path.join(path, filename)
            if filename.endswith('.pth'):
                if not os.access(filename, os.W_OK):
                    logger.warn('Cannot write .pth file %s, skipping' % filename)
                else:
                    fixup_pth_file(filename)
            if filename.endswith('.egg-link'):
                if not os.access(filename, os.W_OK):
                    logger.warn('Cannot write .egg-link file %s, skipping' % filename)
                else:
                    fixup_egg_link(filename)

def fixup_pth_file(filename):
    lines = []
    prev_lines = []
    with open(filename) as f:
        prev_lines = f.readlines()
    for line in prev_lines:
        line = line.strip()
        if (not line or line.startswith('#') or line.startswith('import ')
            or os.path.abspath(line) != line):
            lines.append(line)
        else:
            new_value = make_relative_path(filename, line)
            if line != new_value:
                logger.debug('Rewriting path %s as %s (in %s)' % (line, new_value, filename))
            lines.append(new_value)
    if lines == prev_lines:
        logger.info('No changes to .pth file %s' % filename)
        return
    logger.notify('Making paths in .pth file %s relative' % filename)
    with open(filename, 'w') as f:
        f.write('\n'.join(lines) + '\n')

def fixup_egg_link(filename):
    with open(filename) as f:
        link = f.readline().strip()
    if os.path.abspath(link) != link:
        logger.debug('Link in %s already relative' % filename)
        return
    new_link = make_relative_path(filename, link)
    logger.notify('Rewriting link %s in %s as %s' % (link, filename, new_link))
    with open(filename, 'w') as f:
        f.write(new_link)

def make_relative_path(source, dest, dest_is_directory=True):
    """
    Make a filename relative, where the filename is dest, and it is
    being referred to from the filename source.

        >>> make_relative_path('/usr/share/something/a-file.pth',
        ...                    '/usr/share/another-place/src/Directory')
        '../another-place/src/Directory'
        >>> make_relative_path('/usr/share/something/a-file.pth',
        ...                    '/home/user/src/Directory')
        '../../../home/user/src/Directory'
        >>> make_relative_path('/usr/share/a-file.pth', '/usr/share/')
        './'
    """
    source = os.path.dirname(source)
    if not dest_is_directory:
        dest_filename = os.path.basename(dest)
        dest = os.path.dirname(dest)
    dest = os.path.normpath(os.path.abspath(dest))
    source = os.path.normpath(os.path.abspath(source))
    dest_parts = dest.strip(os.path.sep).split(os.path.sep)
    source_parts = source.strip(os.path.sep).split(os.path.sep)
    while dest_parts and source_parts and dest_parts[0] == source_parts[0]:
        dest_parts.pop(0)
        source_parts.pop(0)
    full_parts = ['..']*len(source_parts) + dest_parts
    if not dest_is_directory:
        full_parts.append(dest_filename)
    if not full_parts:
        # Special case for the current directory (otherwise it'd be '')
        return './'
    return os.path.sep.join(full_parts)



############################################################
## Bootstrap script creation:

def create_bootstrap_script(extra_text, python_version=''):
    """
    Creates a bootstrap script, which is like this script but with
    extend_parser, adjust_options, and after_install hooks.

    This returns a string that (written to disk of course) can be used
    as a bootstrap script with your own customizations.  The script
    will be the standard virtualenv.py script, with your extra text
    added (your extra text should be Python code).

    If you include these functions, they will be called:

    ``extend_parser(optparse_parser)``:
        You can add or remove options from the parser here.

    ``adjust_options(options, args)``:
        You can change options here, or change the args (if you accept
        different kinds of arguments, be sure you modify ``args`` so it is
        only ``[DEST_DIR]``).

    ``after_install(options, home_dir)``:

        After everything is installed, this function is called.  This
        is probably the function you are most likely to use.  An
        example would be::

            def after_install(options, home_dir):
                subprocess.call([join(home_dir, 'bin', 'easy_install'),
                                 'MyPackage'])
                subprocess.call([join(home_dir, 'bin', 'my-package-script'),
                                 'setup', home_dir])

        This example immediately installs a package, and runs a setup
        script from that package.

    If you provide something like ``python_version='2.5'`` then the
    script will start with ``#!/usr/bin/env python2.5`` instead of
    ``#!/usr/bin/env python``.  You can use this when the script must
    be run with a particular Python version.
    """
    filename = __file__
    if filename.endswith('.pyc'):
        filename = filename[:-1]
    with codecs.open(filename, 'r', encoding='utf-8') as f:
        content = f.read()
    py_exe = 'python%s' % python_version
    content = (('#!/usr/bin/env %s\n' % py_exe)
               + '## WARNING: This file is generated\n'
               + content)
    return content.replace('##EXT' 'END##', extra_text)

##EXTEND##

def convert(s):
    b = base64.b64decode(s.encode('ascii'))
    return zlib.decompress(b).decode('utf-8')

##file site.py
SITE_PY = convert("""
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
AVijEPwfucjncQ==
""")

##file activate.sh
ACTIVATE_SH = convert("""
eJytVd9v2kAMfs9fYQLq2m4MscdNVKMqEkgtVIQxbeuUHolpTgsXdHehpT/+9/mSEBJS2MOaB0ji
z77P9menDpOAK5jzEGERKw0zhFihD/dcB2CrKJYewoyLFvM0XzGNNpzOZbSAGVPBqVWHdRSDx4SI
NMhYANfgc4meDteW5ePGC45P4MkCumKhUENzDsu1H3lw1vJx1RJxGMKns6O2lWDqINGgotAHFCsu
I7FAoWHFJGezEFWGqsEvaD5C42naHb93X+A3+elYCgVaxgh8DmQAys9HL2SS0mIaWBgm7mTN/O3G
kzu6vHCng/HkW/fSve5O+hTOpnhfQAcoEry5jKVjNypoO0fgwzKSOgHm79KUK06Jfc7/RebHpD8a
9kdXvT2UcnuFWG6p0stNB0mWUUQ1q3uiGRVEMfXHR03dTuQATPjwqIIPcB9wL4CArRAY/ZHJixYL
Y9YBtcAoLQtFevOoI9QaHcEdMSAB0d08kuZhyUiSmav6CPCdVBnFOjNrLu6yMCWgKRA0TInBC5i4
QwX3JG/mm581GKnSsSSxJTFHf9MAKr8w5T/vOv1mUurn5/zlT6fvTntjZzAaNl9rQ5JkU5KIc0GX
inagwU57T2eddqWlTrvaS6d9sImZeUMkhWysveF0m37NcGub9Dpgi0j4qGiOzATjDr06OBjOYQOo
7RBoGtNm9Denv1i0LVI7lxJDXLHSSBeWRflsyyqw7diuW3h0XdvK6lBMyaoMG1UyHdTsoYBuue75
YOgOu1c91/2cwYpznPPeDoQpGL2xSm09NKp7BsvQ2hnT3aMs07lUnskpxewvBk73/LLnXo9HV9eT
ijB3hWBO2ygoiWg/bKuZxqCCQq0DD3vkWIVvI2KosIw+vqW1gIItEG5KJb+xb09g65ktwYKgTc51
uGJ/EFQs0ayEWLCQM5V9N4g+1+8UbXOJzF8bqhKtIqIwicWvzNFROZJlpfD8A7Vc044R0FxkcezG
VzsV75usvTdYef+57v5n1b225qhXfwEmxHEs
""")

##file activate.fish
ACTIVATE_FISH = convert("""
eJyFVVFv0zAQfs+vONJO3RDNxCsSQoMVrdK2Vl03CSHkesllMXLsYDvZivjx2GmTOG0YfWhV+7u7
73z33Y1gnTENKeMIeakNPCKUGhP7xcQTbCJ4ZOKcxoZV1GCUMp1t4O0zMxkTQEGVQjicO4dTyIwp
Ppyfu386Q86jWOZwBhq1ZlK8jYIRXEoQ0jhDYAYSpjA2fBsFQVoKG0UKSLAJB9MEJrMXi6uYMiXl
KCrIZYJARQIKTakEGAkmQ+tU5ZSDRTAlRY7CRJMA7GdkgRoNSJ74t1BRxegjR12jWAoGbfpTAeGY
LK4vycN8tb6/uCbLi/VVWGPcx3maPr2AO4VjYB+HMAxAkQT/i/ptfbW4vVrczAZit3eHDNqL13n0
Ya+w+Tq/uyLL1eJmuSaLh9lqNb/0+IzgznqnAjAvzBa4jG0BNmNXfdJUkxTU2I6xRaKcy+e6VApz
WVmoTGFTgwslrYdN03ONrbbMN1E/FQ7H7gOP0UxRjV67TPRBjF3naCMV1mSkYk9MUN7F8cODZzsE
iIHYviIe6n8WeGQxWKuhl+9Xa49uijq7fehXMRxT9VR9f/8jhDcfYSKkSOyxKp22cNIrIk+nzd2b
Yc7FNpHx8FUn15ZfzXEE98JxZEohx4r6kosCT+R9ZkHQtLmXGYSEeH8JCTvYkcRgXAutp9Rw7Jmf
E/J5fktuL25m1tMe3vLdjDt9bNxr2sMo2P3C9BccqGeYhqfQITz6XurXaqdf99LF1mT2YJrvzqCu
5w7dKvV3PzNyOb+7+Hw923dOuB+AX2SxrZs9Lm0xbCH6kmhjUyuWw+7cC7DX8367H3VzDz6oBtty
tMIeobE21JT6HaRS+TbaoqhbE7rgdGs3xtE4cOF3xo0TfxwsdyRlhUoxuzes18r+Jp88zDx1G+kd
/HTrr1BY2CeuyfnbQtAcu9j+pOw6cy9X0k3IuoyKCZPC5ESf6MkgHE5tLiSW3Oa+W2NnrQfkGv/h
7tR5PNFnMBlw4B9NJTxnzKA9fLTT0aXSb5vw7FUKzcTZPddqYHi2T9/axJmEEN3qHncVCuEPaFmq
uEtpcBj2Z1wjrqGReJBHrY6/go21NA==
""")

##file activate.csh
ACTIVATE_CSH = convert("""
eJx1U2FP2zAQ/e5f8TAV3Soo+0zXbYUiDQkKQgVp2ibjJNfFUuIg22nVf885SVFLO3+I7Lt3fr6X
d8eY58ZjYQpCWfuAhFB7yrAyIYf0Ve1SQmLsuU6DWepAw9TnEoOFq0rwdjAUx/hV1Ui1tVWAqy1M
QGYcpaFYx+yVI67LkKwx1UuTEaYGl4X2Bl+zJpAlP/6V2hTDtCq/DYXQhdEeGW040Q/Eb+t9V/e3
U/V88zh/mtyqh8n8J47G+IKTE3gKZJdoYrK3h5MRU1tGYS83gqNc+3yEgyyP93cP820evHLvr2H8
kaYB/peoyY7aVHzpJnE9e+6I5Z+ji4GMTNJWNuOQq6MA1N25p8pW9HWdVWlfsNpPDbdxjgpaahuw
1M7opCA/FFu1uwxC7L8KUqmto1KyQe3rx0I0Eovdf7BVe67U5c1MzSZ310pddGheZoFPWyytRkzU
aCA/I+RkBXhFXr5aWV0SxjhUI6jwdAj8kmhPzX7nTfJFkM3MImp2VdVFFq1vLHSU5szYQK4Ri+Jd
xlW2JBtOGcyYVW7SnB3v6RS91g3gKapZ0oWxbHVteYIIq3iv7QeuSrUj6KSqQ+yqsxDj1ivNQxKF
YON10Q+NH/ARS95i5Tuqq2Vxfvc23f/FO6zrtXXmJr+ZtMY9/A15ZXFWtmch2rEQ4g1ryVHH
""")

##file activate.bat
ACTIVATE_BAT = convert("""
eJx9Ul9LhEAQfxf8DoOclI/dYyFkaCmcq4gZQTBUrincuZFbff12T133TM+nnd35/Zvxlr7XDFhV
mUZHOVhFlOWP3g4DUriIWoVomYZpNBWUtGpaWgImO191pFkSpzlcmgaI70jVX7n2Qp8tuByg+46O
CMHbMq64T+nmlJt082D1T44muCDk2prgEHF4mdI9RaS/QwSt3zSyIAaftRccvqVTBziD1x/WlPD5
xd729NDBb8Nr4DU9QNMKsJeH9pkhPedhQsIkDuCDCa6A+NF9IevVFAohkqizdHetg/tkWvPoftWJ
MCqnOxv7/x7Np6yv9P2Ker5dmX8yNyCkkWnbZy3N5LarczlqL8htx2EM9rQ/2H5BvIsIEi8OEG8U
+g8CsNTr
""")

##file deactivate.bat
DEACTIVATE_BAT = convert("""
eJyFkN0KgkAUhO8F32EQpHqFQEjQUPAPMaErqVxzId3IrV6/XST/UDx3c86c4WMO5FYysKJQFVVp
CEfqxsnJ9DI7SA25i20fFqs3HO+GYLsDZ7h8GM3xfLHrg1QNvpSX4CWpQGvokZk4uqrQAjXjyElB
a5IjCz0r+2dHcehHCe5MZNmB5R7TdqMqECMptHZh6DN/utb7Zs6Cej8OXYE5J04YOKFvD4GkHuJ0
pilSd1jG6n87tDZ+BUwUOepI6CGSkFMYWf0ihvT33Qj1A+tCkSI=
""")

##file activate.ps1
ACTIVATE_PS = convert("""
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
rKQDxR6N/rffXv+lROXet/9Q+l9I4D1U
""")

##file distutils-init.py
DISTUTILS_INIT = convert("""
eJytV1uL4zYUfvevOE0ottuMW9q3gVDa3aUMXXbLMlDKMBiNrSTqOJKRlMxkf33PkXyRbGe7Dw2E
UXTu37lpxLFV2oIyifAncxmOL0xLIfcG+gv80x9VW6maw7o/CANSWWBwFtqeWMPlGY6qPjV8A0bB
C4eKSTgZ5LRgFeyErMEeOBhbN+Ipgeizhjtnhkn7DdyjuNLPoCS0l/ayQTG0djwZC08cLXozeMss
aG5EzQ0IScpnWtHSTXuxByV/QCmxE7y+eS0uxWeoheaVVfqSJHiU7Mhhi6gULbOHorshkrEnKxpT
0n3A8Y8SMpuwZx6aoix3ouFlmW8gHRSkeSJ2g7hU+kiHLDaQw3bmRDaTGfTnty7gPm0FHbIBg9U9
oh1kZzAFLaue2R6htPCtAda2nGlDSUJ4PZBgCJBGVcwKTAMz/vJiLD+Oin5Z5QlvDPdulC6EsiyE
NFzb7McNTKJzbJqzphx92VKRFY1idenzmq3K0emRcbWBD0ryqc4NZGmKOOOX9Pz5x+/l27tP797c
f/z0d+4NruGNai8uAM0bfsYaw8itFk8ny41jsfpyO+BWlpqfhcG4yxLdi/0tQqoT4a8Vby382mt8
p7XSo7aWGdPBc+b6utaBmCQ7rQKQoWtAuthQCiold2KfJIPTT8xwg9blPumc+YDZC/wYGdAyHpJk
vUbHbHWAp5No6pK/WhhLEWrFjUwtPEv1Agf8YmnsuXUQYkeZoHm8ogP16gt2uHoxcEMdf2C6pmbw
hUMsWGhanboh4IzzmsIpWs134jVPqD/c74bZHdY69UKKSn/+KfVhxLgUlToemayLMYQOqfEC61bh
cbhwaqoGUzIyZRFHPmau5juaWqwRn3mpWmoEA5nhzS5gog/5jbcFQqOZvmBasZtwYlG93k5GEiyw
buHhMWLjDarEGpMGB2LFs5nIJkhp/nUmZneFaRth++lieJtHepIvKgx6PJqIlD9X2j6pG1i9x3pZ
5bHuCPFiirGHeO7McvoXkz786GaKVzC9DSpnOxJdc4xm6NSVq7lNEnKdVlnpu9BNYoKX2Iq3wvgh
gGEUM66kK6j4NiyoneuPLSwaCWDxczgaolEWpiMyDVDb7dNuLAbriL8ig8mmeju31oNvQdpnvEPC
1vAXbWacGRVrGt/uXN/gU0CDDwgooKRrHfTBb1/s9lYZ8ZqOBU0yLvpuP6+K9hLFsvIjeNhBi0KL
MlOuWRn3FRwx5oHXjl0YImUx0+gLzjGchrgzca026ETmYJzPD+IpuKzNi8AFn048Thd63OdD86M6
84zE8yQm0VqXdbbgvub2pKVnS76icBGdeTHHXTKspUmr4NYo/furFLKiMdQzFjHJNcdAnMhltBJK
0/IKX3DVFqvPJ2dLE7bDBkH0l/PJ29074+F0CsGYOxsb7U3myTUncYfXqnLLfa6sJybX4g+hmcjO
kMRBfA1JellfRRKJcyRpxdS4rIl6FdmQCWjo/o9Qz7yKffoP4JHjOvABcRn4CZIT2RH4jnxmfpVG
qgLaAvQBNfuO6X0/Ux02nb4FKx3vgP+XnkX0QW9pLy/NsXgdN24dD3LxO2Nwil7Zlc1dqtP3d7/h
kzp1/+7hGBuY4pk0XD/0Ao/oTe/XGrfyM773aB7iUhgkpy+dwAMalxMP0DrBcsVw/6p25+/hobP9
GBknrWExDhLJ1bwt1NcCNblaFbMKCyvmX0PeRaQ=
""")

##file distutils.cfg
DISTUTILS_CFG = convert("""
eJxNj00KwkAMhfc9xYNuxe4Ft57AjYiUtDO1wXSmNJnK3N5pdSEEAu8nH6lxHVlRhtDHMPATA4uH
xJ4EFmGbvfJiicSHFRzUSISMY6hq3GLCRLnIvSTnEefN0FIjw5tF0Hkk9Q5dRunBsVoyFi24aaLg
9FDOlL0FPGluf4QjcInLlxd6f6rqkgPu/5nHLg0cXCscXoozRrP51DRT3j9QNl99AP53T2Q=
""")

##file activate_this.py
ACTIVATE_THIS = convert("""
eJyNU01v2zAMvetXEB4K21jnDOstQA4dMGCHbeihlyEIDMWmE62yJEiKE//7kXKdpEWLzYBt8evx
kRSzLPs6wiEoswM8YdMpjUXcq1Dz6RZa1cSiTkJdr86GsoTRHuCotBayiWqQEYGtMCgfD1KjGYBe
5a3p0cRKiEe2NtLAFikftnDco0ko/SFEVgEZ8aRCZDIPY9xbA8pE9M4jfW/B2CjiHq9zbJVZuOQq
siwTIvpxKYCembPAU4Muwi/Z4zfvrZ/MXipKeB8C+qisSZYiWfjJfs+0/MFMdWn1hJcO5U7G/SLa
xVx8zU6VG/PXLXvfsyyzUqjeWR8hjGE+2iCE1W1tQ82hsCJN9dzKaoexyB/uH79TnjwvxcW0ntSb
yZ8jq1Z5Q1UXsyy3gf9nbjTEj7NzQMfCJa/YSmrQ+2D/BqfiOi6sclrGzvoeVivIj8rcfcmnIQRF
7XCyeZI7DFe5/lhlCs5PRf5QW66VXT/NrlQ46oD/D6InkOmi3IQcbhKxAX2g4a+Xd5s3UtCtG2py
m8eg6WYWqR6SL5OjKMGfSrYt/6kxxQtOpeAgj1LXBNmpE2ElmCSIy5H0zFd8gJ924HWijWhb2hRC
6wNEm1QdDZtuSZcEprIUBo/XRNcbQe1OUbQ/r3hPTaPJJDNtFLu8KHV5XoNr3Eo6h6YtOKw8e8yw
VF5PnJ+ts3a9/Mz38RpG/AUSzYUW
""")

##file python-config
PYTHON_CONFIG = convert("""
eJyNVV1P2zAUfc+v8ODBiSABxlulTipbO6p1LWqBgVhlhcZpPYUkctzSivHfd6+dpGloGH2Ja/ue
e+65Hz78xNhtf3x90xmw7vCWsRPGLvpDNuz87MKfdKMWSWxZ4ilNpCLZJiuWc66SVFUOZkkcirll
rfxIBAzOMtImDzSVPBRrekwoX/OZu/0r4lm0DHiG60g86u8sjPw5rCyy86NRkB8QuuBRSqfAKESn
3orLTCQxE3GYkC9tYp8fk89OSwNsmXgizrhUtnumeSgeo5GbLUMk49Rv+2nK48Cm/qMwfp333J2/
dVcAGE0CIQHBsgIeEr4Wij0LtWDLzJ9ze5YEvH2WI6CHTAVcSu9ZCsXtgxu81CIvp6/k4eXsdfo7
PvDCRD75yi41QitfzlcPp1OI7i/1/iQitqnr0iMgQ+A6wa+IKwwdxyk9IiXNAzgquTFU8NIxAVjM
osm1Zz526e+shQ4hKRVci69nPC3Kw4NQEmkQ65E7OodxorSvxjvpBjQHDmWFIQ1mlmzlS5vedseT
/mgIEsMJ7Lxz2bLAF9M5xeLEhdbHxpWOw0GdkJApMVBRF1y+a0z3c9WZPAXGFcFrJgCIB+024uad
0CrzmEoRa3Ub4swNIHPGf7QDV+2uj2OiFWsChgCwjKqN6rp5izpbH6Wc1O1TclQTP/XVwi6anTr1
1sbubjZLI1+VptPSdCfwnFBrB1jvebrTA9uUhU2/9gad7xPqeFkaQcnnLbCViZK8d7R1kxzFrIJV
8EaLYmKYpvGVkig+3C5HCXbM1jGCGekiM2pRCVPyRyXYdPf6kcbWEQ36F5V4Gq9N7icNNw+JHwRE
LTgxRXACpvnQv/PuT0xCCAywY/K4hE6Now2qDwaSE5FB+1agsoUveYDepS83qFcF1NufvULD3fTl
g6Hgf7WBt6lzMeiyyWVn3P1WVbwaczHmTzE9A5SyItTVgFYyvs/L/fXlaNgbw8v3azT+0eikVlWD
/vBHbzQumP23uBCjsYdrL9OWARwxs/nuLOzeXbPJTa/Xv6sUmQir5pC1YRLz3eA+CD8Z0XpcW8v9
MZWF36ryyXXf3yBIz6nzqz8Muyz0m5Qj7OexfYo/Ph3LqvkHUg7AuA==
""")

MH_MAGIC = 0xfeedface
MH_CIGAM = 0xcefaedfe
MH_MAGIC_64 = 0xfeedfacf
MH_CIGAM_64 = 0xcffaedfe
FAT_MAGIC = 0xcafebabe
BIG_ENDIAN = '>'
LITTLE_ENDIAN = '<'
LC_LOAD_DYLIB = 0xc
maxint = majver == 3 and getattr(sys, 'maxsize') or getattr(sys, 'maxint')


class fileview(object):
    """
    A proxy for file-like objects that exposes a given view of a file.
    Modified from macholib.
    """

    def __init__(self, fileobj, start=0, size=maxint):
        if isinstance(fileobj, fileview):
            self._fileobj = fileobj._fileobj
        else:
            self._fileobj = fileobj
        self._start = start
        self._end = start + size
        self._pos = 0

    def __repr__(self):
        return '<fileview [%d, %d] %r>' % (
            self._start, self._end, self._fileobj)

    def tell(self):
        return self._pos

    def _checkwindow(self, seekto, op):
        if not (self._start <= seekto <= self._end):
            raise IOError("%s to offset %d is outside window [%d, %d]" % (
                op, seekto, self._start, self._end))

    def seek(self, offset, whence=0):
        seekto = offset
        if whence == os.SEEK_SET:
            seekto += self._start
        elif whence == os.SEEK_CUR:
            seekto += self._start + self._pos
        elif whence == os.SEEK_END:
            seekto += self._end
        else:
            raise IOError("Invalid whence argument to seek: %r" % (whence,))
        self._checkwindow(seekto, 'seek')
        self._fileobj.seek(seekto)
        self._pos = seekto - self._start

    def write(self, bytes):
        here = self._start + self._pos
        self._checkwindow(here, 'write')
        self._checkwindow(here + len(bytes), 'write')
        self._fileobj.seek(here, os.SEEK_SET)
        self._fileobj.write(bytes)
        self._pos += len(bytes)

    def read(self, size=maxint):
        assert size >= 0
        here = self._start + self._pos
        self._checkwindow(here, 'read')
        size = min(size, self._end - here)
        self._fileobj.seek(here, os.SEEK_SET)
        bytes = self._fileobj.read(size)
        self._pos += len(bytes)
        return bytes


def read_data(file, endian, num=1):
    """
    Read a given number of 32-bits unsigned integers from the given file
    with the given endianness.
    """
    res = struct.unpack(endian + 'L' * num, file.read(num * 4))
    if len(res) == 1:
        return res[0]
    return res


def mach_o_change(path, what, value):
    """
    Replace a given name (what) in any LC_LOAD_DYLIB command found in
    the given binary with a new name (value), provided it's shorter.
    """

    def do_macho(file, bits, endian):
        # Read Mach-O header (the magic number is assumed read by the caller)
        cputype, cpusubtype, filetype, ncmds, sizeofcmds, flags = read_data(file, endian, 6)
        # 64-bits header has one more field.
        if bits == 64:
            read_data(file, endian)
        # The header is followed by ncmds commands
        for n in range(ncmds):
            where = file.tell()
            # Read command header
            cmd, cmdsize = read_data(file, endian, 2)
            if cmd == LC_LOAD_DYLIB:
                # The first data field in LC_LOAD_DYLIB commands is the
                # offset of the name, starting from the beginning of the
                # command.
                name_offset = read_data(file, endian)
                file.seek(where + name_offset, os.SEEK_SET)
                # Read the NUL terminated string
                load = file.read(cmdsize - name_offset).decode()
                load = load[:load.index('\0')]
                # If the string is what is being replaced, overwrite it.
                if load == what:
                    file.seek(where + name_offset, os.SEEK_SET)
                    file.write(value.encode() + '\0'.encode())
            # Seek to the next command
            file.seek(where + cmdsize, os.SEEK_SET)

    def do_file(file, offset=0, size=maxint):
        file = fileview(file, offset, size)
        # Read magic number
        magic = read_data(file, BIG_ENDIAN)
        if magic == FAT_MAGIC:
            # Fat binaries contain nfat_arch Mach-O binaries
            nfat_arch = read_data(file, BIG_ENDIAN)
            for n in range(nfat_arch):
                # Read arch header
                cputype, cpusubtype, offset, size, align = read_data(file, BIG_ENDIAN, 5)
                do_file(file, offset, size)
        elif magic == MH_MAGIC:
            do_macho(file, 32, BIG_ENDIAN)
        elif magic == MH_CIGAM:
            do_macho(file, 32, LITTLE_ENDIAN)
        elif magic == MH_MAGIC_64:
            do_macho(file, 64, BIG_ENDIAN)
        elif magic == MH_CIGAM_64:
            do_macho(file, 64, LITTLE_ENDIAN)

    assert(len(what) >= len(value))

    with open(path, 'r+b') as f:
        do_file(f)


if __name__ == '__main__':
    main()

# TODO:
# Copy python.exe.manifest
# Monkeypatch distutils.sysconfig
