#!/usr/bin/env python
"""Create a "virtual" Python installation
"""

virtualenv_version = "1.4.8"

import sys
import os
import optparse
import re
import shutil
import logging
import distutils.sysconfig
try:
    import subprocess
except ImportError, e:
    if sys.version_info <= (2, 3):
        print 'ERROR: %s' % e
        print 'ERROR: this script requires Python 2.4 or greater; or at least the subprocess module.'
        print 'If you copy subprocess.py from a newer version of Python this script will probably work'
        sys.exit(101)
    else:
        raise
try:
    set
except NameError:
    from sets import Set as set

join = os.path.join
py_version = 'python%s.%s' % (sys.version_info[0], sys.version_info[1])
is_jython = sys.platform.startswith('java')
expected_exe = is_jython and 'jython' or 'python'

REQUIRED_MODULES = ['os', 'posix', 'posixpath', 'nt', 'ntpath', 'genericpath',
                    'fnmatch', 'locale', 'encodings', 'codecs',
                    'stat', 'UserDict', 'readline', 'copy_reg', 'types',
                    're', 'sre', 'sre_parse', 'sre_constants', 'sre_compile',
                    'lib-dynload', 'config', 'zlib']

if sys.version_info[:2] >= (2, 6):
    REQUIRED_MODULES.extend(['warnings', 'linecache', '_abcoll', 'abc'])
if sys.version_info[:2] <= (2, 3):
    REQUIRED_MODULES.extend(['sets', '__future__'])

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
        self.log(self.WARN, msg, *args, **kw)
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
        >>> l = Logger()
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
            if stop is not None or stop <= consumer_level:
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

def mkdir(path):
    if not os.path.exists(path):
        logger.info('Creating %s', path)
        os.makedirs(path)
    else:
        logger.info('Directory %s already exists', path)

def copyfile(src, dest, symlink=True):
    if not os.path.exists(src):
        # Some bad symlink in the src
        logger.warn('Cannot find file %s (bad symlink)', src)
        return
    if os.path.exists(dest):
        logger.debug('File %s already exists', dest)
        return
    if not os.path.exists(os.path.dirname(dest)):
        logger.info('Creating parent directories for %s' % os.path.dirname(dest))
        os.makedirs(os.path.dirname(dest))
    if symlink and hasattr(os, 'symlink'):
        logger.info('Symlinking %s', dest)
        os.symlink(os.path.abspath(src), dest)
    else:
        logger.info('Copying to %s', dest)
        if os.path.isdir(src):
            shutil.copytree(src, dest, True)
        else:
            shutil.copy2(src, dest)

def writefile(dest, content, overwrite=True):
    if not os.path.exists(dest):
        logger.info('Writing %s', dest)
        f = open(dest, 'wb')
        f.write(content)
        f.close()
        return
    else:
        f = open(dest, 'rb')
        c = f.read()
        f.close()
        if c != content:
            if not overwrite:
                logger.notify('File %s exists with different content; not overwriting', dest)
                return
            logger.notify('Overwriting %s with new content', dest)
            f = open(dest, 'wb')
            f.write(content)
            f.close()
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
        oldmode = os.stat(fn).st_mode & 07777
        newmode = (oldmode | 0555) & 07777
        os.chmod(fn, newmode)
        logger.info('Changed mode of %s to %s', fn, oct(newmode))

def _find_file(filename, dirs):
    for dir in dirs:
        if os.path.exists(join(dir, filename)):
            return join(dir, filename)
    return filename

def _install_req(py_executable, unzip=False, distribute=False):
    if not distribute:
        setup_fn = 'setuptools-0.6c11-py%s.egg' % sys.version[:3]
        project_name = 'setuptools'
        bootstrap_script = EZ_SETUP_PY
        source = None
    else:
        setup_fn = None
        source = 'distribute-0.6.8.tar.gz'
        project_name = 'distribute'
        bootstrap_script = DISTRIBUTE_SETUP_PY
        try:
            # check if the global Python has distribute installed or plain
            # setuptools
            import pkg_resources
            if not hasattr(pkg_resources, '_distribute'):
                location = os.path.dirname(pkg_resources.__file__)
                logger.notify("A globally installed setuptools was found (in %s)" % location)
                logger.notify("Use the --no-site-packages option to use distribute in "
                              "the virtualenv.")
        except ImportError:
            pass

    search_dirs = file_search_dirs()

    if setup_fn is not None:
        setup_fn = _find_file(setup_fn, search_dirs)

    if source is not None:
        source = _find_file(source, search_dirs)

    if is_jython and os._name == 'nt':
        # Jython's .bat sys.executable can't handle a command line
        # argument with newlines
        import tempfile
        fd, ez_setup = tempfile.mkstemp('.py')
        os.write(fd, bootstrap_script)
        os.close(fd)
        cmd = [py_executable, ez_setup]
    else:
        cmd = [py_executable, '-c', bootstrap_script]
    if unzip:
        cmd.append('--always-unzip')
    env = {}
    if logger.stdout_level_matches(logger.DEBUG):
        cmd.append('-v')

    old_chdir = os.getcwd()
    if setup_fn is not None and os.path.exists(setup_fn):
        logger.info('Using existing %s egg: %s' % (project_name, setup_fn))
        cmd.append(setup_fn)
        if os.environ.get('PYTHONPATH'):
            env['PYTHONPATH'] = setup_fn + os.path.pathsep + os.environ['PYTHONPATH']
        else:
            env['PYTHONPATH'] = setup_fn
    else:
        # the source is found, let's chdir
        if source is not None and os.path.exists(source):
            os.chdir(os.path.dirname(source))
        else:
            logger.info('No %s egg found; downloading' % project_name)
        cmd.extend(['--always-copy', '-U', project_name])
    logger.start_progress('Installing %s...' % project_name)
    logger.indent += 2
    cwd = None
    if project_name == 'distribute':
        env['DONT_PATCH_SETUPTOOLS'] = 'true'

    def _filter_ez_setup(line):
        return filter_ez_setup(line, project_name)

    if not os.access(os.getcwd(), os.W_OK):
        cwd = '/tmp'
        if source is not None and os.path.exists(source):
            # the current working dir is hostile, let's copy the
            # tarball to /tmp
            target = os.path.join(cwd, os.path.split(source)[-1])
            shutil.copy(source, target)
    try:
        call_subprocess(cmd, show_stdout=False,
                        filter_stdout=_filter_ez_setup,
                        extra_env=env,
                        cwd=cwd)
    finally:
        logger.indent -= 2
        logger.end_progress()
        if os.getcwd() != old_chdir:
            os.chdir(old_chdir)
        if is_jython and os._name == 'nt':
            os.remove(ez_setup)

def file_search_dirs():
    here = os.path.dirname(os.path.abspath(__file__))
    dirs = ['.', here,
            join(here, 'virtualenv_support')]
    if os.path.splitext(os.path.dirname(__file__))[0] != 'virtualenv':
        # Probably some boot script; just in case virtualenv is installed...
        try:
            import virtualenv
        except ImportError:
            pass
        else:
            dirs.append(os.path.join(os.path.dirname(virtualenv.__file__), 'virtualenv_support'))
    return [d for d in dirs if os.path.isdir(d)]

def install_setuptools(py_executable, unzip=False):
    _install_req(py_executable, unzip)

def install_distribute(py_executable, unzip=False):
    _install_req(py_executable, unzip, distribute=True)

_pip_re = re.compile(r'^pip-.*(zip|tar.gz|tar.bz2|tgz|tbz)$', re.I)
def install_pip(py_executable):
    filenames = []
    for dir in file_search_dirs():
        filenames.extend([join(dir, fn) for fn in os.listdir(dir)
                          if _pip_re.search(fn)])
    filenames.sort(key=lambda x: os.path.basename(x).lower())
    if not filenames:
        filename = 'pip'
    else:
        filename = filenames[-1]
    easy_install_script = 'easy_install'
    if sys.platform == 'win32':
        easy_install_script = 'easy_install-script.py'
    cmd = [py_executable, join(os.path.dirname(py_executable), easy_install_script), filename]
    if filename == 'pip':
        logger.info('Installing pip from network...')
    else:
        logger.info('Installing %s' % os.path.basename(filename))
    logger.indent += 2
    def _filter_setup(line):
        return filter_ez_setup(line, 'pip')
    try:
        call_subprocess(cmd, show_stdout=False,
                        filter_stdout=_filter_setup)
    finally:
        logger.indent -= 2

def filter_ez_setup(line, project_name='setuptools'):
    if not line.strip():
        return Logger.DEBUG
    if project_name == 'distribute':
        for prefix in ('Extracting', 'Now working', 'Installing', 'Before',
                       'Scanning', 'Setuptools', 'Egg', 'Already',
                       'running', 'writing', 'reading', 'installing',
                       'creating', 'copying', 'byte-compiling', 'removing',
                       'Processing'):
            if line.startswith(prefix):
                return Logger.DEBUG
        return Logger.DEBUG
    for prefix in ['Reading ', 'Best match', 'Processing setuptools',
                   'Copying setuptools', 'Adding setuptools',
                   'Installing ', 'Installed ']:
        if line.startswith(prefix):
            return Logger.DEBUG
    return Logger.INFO

def main():
    parser = optparse.OptionParser(
        version=virtualenv_version,
        usage="%prog [OPTIONS] DEST_DIR")

    parser.add_option(
        '-v', '--verbose',
        action='count',
        dest='verbose',
        default=0,
        help="Increase verbosity")

    parser.add_option(
        '-q', '--quiet',
        action='count',
        dest='quiet',
        default=0,
        help='Decrease verbosity')

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
        help="Clear out the non-root install and start from scratch")

    parser.add_option(
        '--no-site-packages',
        dest='no_site_packages',
        action='store_true',
        help="Don't give access to the global site-packages dir to the "
             "virtual environment")

    parser.add_option(
        '--unzip-setuptools',
        dest='unzip_setuptools',
        action='store_true',
        help="Unzip Setuptools or Distribute when installing it")

    parser.add_option(
        '--relocatable',
        dest='relocatable',
        action='store_true',
        help='Make an EXISTING virtualenv environment relocatable.  '
        'This fixes up scripts and makes all .pth files relative')

    parser.add_option(
        '--distribute',
        dest='use_distribute',
        action='store_true',
        help='Use Distribute instead of Setuptools. Set environ variable'
        'VIRTUALENV_USE_DISTRIBUTE to make it the default ')

    if 'extend_parser' in globals():
        extend_parser(parser)

    options, args = parser.parse_args()

    global logger

    if 'adjust_options' in globals():
        adjust_options(options, args)

    verbosity = options.verbose - options.quiet
    logger = Logger([(Logger.level_for_integer(2-verbosity), sys.stdout)])

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
            os.execvpe(interpreter, [interpreter, file] + sys.argv[1:], env)

    if not args:
        print 'You must provide a DEST_DIR'
        parser.print_help()
        sys.exit(2)
    if len(args) > 1:
        print 'There must be only one argument: DEST_DIR (you gave %s)' % (
            ' '.join(args))
        parser.print_help()
        sys.exit(2)

    home_dir = args[0]

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

    create_environment(home_dir, site_packages=not options.no_site_packages, clear=options.clear,
                       unzip_setuptools=options.unzip_setuptools,
                       use_distribute=options.use_distribute)
    if 'after_install' in globals():
        after_install(options, home_dir)

def call_subprocess(cmd, show_stdout=True,
                    filter_stdout=None, cwd=None,
                    raise_on_returncode=True, extra_env=None):
    cmd_parts = []
    for part in cmd:
        if len(part) > 40:
            part = part[:30]+"..."+part[-5:]
        if ' ' in part or '\n' in part or '"' in part or "'" in part:
            part = '"%s"' % part.replace('"', '\\"')
        cmd_parts.append(part)
    cmd_desc = ' '.join(cmd_parts)
    if show_stdout:
        stdout = None
    else:
        stdout = subprocess.PIPE
    logger.debug("Running command %s" % cmd_desc)
    if extra_env:
        env = os.environ.copy()
        env.update(extra_env)
    else:
        env = None
    try:
        proc = subprocess.Popen(
            cmd, stderr=subprocess.STDOUT, stdin=None, stdout=stdout,
            cwd=cwd, env=env)
    except Exception, e:
        logger.fatal(
            "Error %s while executing command %s" % (e, cmd_desc))
        raise
    all_output = []
    if stdout is not None:
        stdout = proc.stdout
        while 1:
            line = stdout.readline()
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
        proc.communicate()
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


def create_environment(home_dir, site_packages=True, clear=False,
                       unzip_setuptools=False, use_distribute=False):
    """
    Creates a new environment in ``home_dir``.

    If ``site_packages`` is true (the default) then the global
    ``site-packages/`` directory will be on the path.

    If ``clear`` is true (default False) then the environment will
    first be cleared.
    """
    home_dir, lib_dir, inc_dir, bin_dir = path_locations(home_dir)

    py_executable = os.path.abspath(install_python(
        home_dir, lib_dir, inc_dir, bin_dir,
        site_packages=site_packages, clear=clear))

    install_distutils(lib_dir, home_dir)

    if use_distribute or os.environ.get('VIRTUALENV_USE_DISTRIBUTE'):
        install_distribute(py_executable, unzip=unzip_setuptools)
    else:
        install_setuptools(py_executable, unzip=unzip_setuptools)

    install_pip(py_executable)

    install_activate(home_dir, bin_dir)

def path_locations(home_dir):
    """Return the path locations for the environment (where libraries are,
    where scripts go, etc)"""
    # XXX: We'd use distutils.sysconfig.get_python_inc/lib but its
    # prefix arg is broken: http://bugs.python.org/issue3386
    if sys.platform == 'win32':
        # Windows has lots of problems with executables with spaces in
        # the name; this function will remove them (using the ~1
        # format):
        mkdir(home_dir)
        if ' ' in home_dir:
            try:
                import win32api
            except ImportError:
                print 'Error: the path "%s" has a space in it' % home_dir
                print 'To handle these kinds of paths, the win32api module must be installed:'
                print '  http://sourceforge.net/projects/pywin32/'
                sys.exit(3)
            home_dir = win32api.GetShortPathName(home_dir)
        lib_dir = join(home_dir, 'Lib')
        inc_dir = join(home_dir, 'Include')
        bin_dir = join(home_dir, 'Scripts')
    elif is_jython:
        lib_dir = join(home_dir, 'Lib')
        inc_dir = join(home_dir, 'Include')
        bin_dir = join(home_dir, 'bin')
    else:
        lib_dir = join(home_dir, 'lib', py_version)
        inc_dir = join(home_dir, 'include', py_version)
        bin_dir = join(home_dir, 'bin')
    return home_dir, lib_dir, inc_dir, bin_dir

def install_python(home_dir, lib_dir, inc_dir, bin_dir, site_packages, clear):
    """Install just the base environment, no distutils patches etc"""
    if sys.executable.startswith(bin_dir):
        print 'Please use the *system* python to run this script'
        return

    if clear:
        rmtree(lib_dir)
        ## FIXME: why not delete it?
        ## Maybe it should delete everything with #!/path/to/venv/python in it
        logger.notify('Not deleting %s', bin_dir)

    if hasattr(sys, 'real_prefix'):
        logger.notify('Using real prefix %r' % sys.real_prefix)
        prefix = sys.real_prefix
    else:
        prefix = sys.prefix
    mkdir(lib_dir)
    fix_lib64(lib_dir)
    stdlib_dirs = [os.path.dirname(os.__file__)]
    if sys.platform == 'win32':
        stdlib_dirs.append(join(os.path.dirname(stdlib_dirs[0]), 'DLLs'))
    elif sys.platform == 'darwin':
        stdlib_dirs.append(join(stdlib_dirs[0], 'site-packages'))
    for stdlib_dir in stdlib_dirs:
        if not os.path.isdir(stdlib_dir):
            continue
        if hasattr(os, 'symlink'):
            logger.info('Symlinking Python bootstrap modules')
        else:
            logger.info('Copying Python bootstrap modules')
        logger.indent += 2
        try:
            for fn in os.listdir(stdlib_dir):
                if fn != 'site-packages' and os.path.splitext(fn)[0] in REQUIRED_MODULES:
                    copyfile(join(stdlib_dir, fn), join(lib_dir, fn))
        finally:
            logger.indent -= 2
    mkdir(join(lib_dir, 'site-packages'))
    writefile(join(lib_dir, 'site.py'), SITE_PY)
    writefile(join(lib_dir, 'orig-prefix.txt'), prefix)
    site_packages_filename = join(lib_dir, 'no-global-site-packages.txt')
    if not site_packages:
        writefile(site_packages_filename, '')
    else:
        if os.path.exists(site_packages_filename):
            logger.info('Deleting %s' % site_packages_filename)
            os.unlink(site_packages_filename)

    stdinc_dir = join(prefix, 'include', py_version)
    if os.path.exists(stdinc_dir):
        copyfile(stdinc_dir, inc_dir)
    else:
        logger.debug('No include dir %s' % stdinc_dir)

    if sys.exec_prefix != prefix:
        if sys.platform == 'win32':
            exec_dir = join(sys.exec_prefix, 'lib')
        elif is_jython:
            exec_dir = join(sys.exec_prefix, 'Lib')
        else:
            exec_dir = join(sys.exec_prefix, 'lib', py_version)
        for fn in os.listdir(exec_dir):
            copyfile(join(exec_dir, fn), join(lib_dir, fn))

    if is_jython:
        # Jython has either jython-dev.jar and javalib/ dir, or just
        # jython.jar
        for name in 'jython-dev.jar', 'javalib', 'jython.jar':
            src = join(prefix, name)
            if os.path.exists(src):
                copyfile(src, join(home_dir, name))
        # XXX: registry should always exist after Jython 2.5rc1
        src = join(prefix, 'registry')
        if os.path.exists(src):
            copyfile(src, join(home_dir, 'registry'), symlink=False)
        copyfile(join(prefix, 'cachedir'), join(home_dir, 'cachedir'),
                 symlink=False)

    mkdir(bin_dir)
    py_executable = join(bin_dir, os.path.basename(sys.executable))
    if 'Python.framework' in prefix:
        if re.search(r'/Python(?:-32|-64)*$', py_executable):
            # The name of the python executable is not quite what
            # we want, rename it.
            py_executable = os.path.join(
                    os.path.dirname(py_executable), 'python')

    logger.notify('New %s executable in %s', expected_exe, py_executable)
    if sys.executable != py_executable:
        ## FIXME: could I just hard link?
        executable = sys.executable
        if sys.platform == 'cygwin' and os.path.exists(executable + '.exe'):
            # Cygwin misreports sys.executable sometimes
            executable += '.exe'
            py_executable += '.exe'
            logger.info('Executable actually exists in %s' % executable)
        shutil.copyfile(executable, py_executable)
        make_exe(py_executable)
        if sys.platform == 'win32' or sys.platform == 'cygwin':
            pythonw = os.path.join(os.path.dirname(sys.executable), 'pythonw.exe')
            if os.path.exists(pythonw):
                logger.info('Also created pythonw.exe')
                shutil.copyfile(pythonw, os.path.join(os.path.dirname(py_executable), 'pythonw.exe'))

    if os.path.splitext(os.path.basename(py_executable))[0] != expected_exe:
        secondary_exe = os.path.join(os.path.dirname(py_executable),
                                     expected_exe)
        py_executable_ext = os.path.splitext(py_executable)[1]
        if py_executable_ext == '.exe':
            # python2.4 gives an extension of '.4' :P
            secondary_exe += py_executable_ext
        if os.path.exists(secondary_exe):
            logger.warn('Not overwriting existing %s script %s (you must use %s)'
                        % (expected_exe, secondary_exe, py_executable))
        else:
            logger.notify('Also creating executable in %s' % secondary_exe)
            shutil.copyfile(sys.executable, secondary_exe)
            make_exe(secondary_exe)

    if 'Python.framework' in prefix:
        logger.debug('MacOSX Python framework detected')

        # Make sure we use the the embedded interpreter inside
        # the framework, even if sys.executable points to
        # the stub executable in ${sys.prefix}/bin
        # See http://groups.google.com/group/python-virtualenv/
        #                              browse_thread/thread/17cab2f85da75951
        shutil.copy(
                os.path.join(
                    prefix, 'Resources/Python.app/Contents/MacOS/%s' % os.path.basename(sys.executable)),
                py_executable)

        # Copy the framework's dylib into the virtual
        # environment
        virtual_lib = os.path.join(home_dir, '.Python')

        if os.path.exists(virtual_lib):
            os.unlink(virtual_lib)
        copyfile(
            os.path.join(prefix, 'Python'),
            virtual_lib)

        # And then change the install_name of the copied python executable
        try:
            call_subprocess(
                ["install_name_tool", "-change",
                 os.path.join(prefix, 'Python'),
                 '@executable_path/../.Python',
                 py_executable])
        except:
            logger.fatal(
                "Could not call install_name_tool -- you must have Apple's development tools installed")
            raise

        # Some tools depend on pythonX.Y being present
        py_executable_version = '%s.%s' % (
            sys.version_info[0], sys.version_info[1])
        if not py_executable.endswith(py_executable_version):
            # symlinking pythonX.Y > python
            pth = py_executable + '%s.%s' % (
                    sys.version_info[0], sys.version_info[1])
            if os.path.exists(pth):
                os.unlink(pth)
            os.symlink('python', pth)
        else:
            # reverse symlinking python -> pythonX.Y (with --python)
            pth = join(bin_dir, 'python')
            if os.path.exists(pth):
                os.unlink(pth)
            os.symlink(os.path.basename(py_executable), pth)

    if sys.platform == 'win32' and ' ' in py_executable:
        # There's a bug with subprocess on Windows when using a first
        # argument that has a space in it.  Instead we have to quote
        # the value:
        py_executable = '"%s"' % py_executable
    cmd = [py_executable, '-c', 'import sys; print sys.prefix']
    logger.info('Testing executable with %s %s "%s"' % tuple(cmd))
    proc = subprocess.Popen(cmd,
                            stdout=subprocess.PIPE)
    proc_stdout, proc_stderr = proc.communicate()
    proc_stdout = os.path.normcase(os.path.abspath(proc_stdout.strip()))
    if proc_stdout != os.path.normcase(os.path.abspath(home_dir)):
        logger.fatal(
            'ERROR: The executable %s is not functioning' % py_executable)
        logger.fatal(
            'ERROR: It thinks sys.prefix is %r (should be %r)'
            % (proc_stdout, os.path.normcase(os.path.abspath(home_dir))))
        logger.fatal(
            'ERROR: virtualenv is not compatible with this system or executable')
        if sys.platform == 'win32':
            logger.fatal(
                'Note: some Windows users have reported this error when they installed Python for "Only this user".  The problem may be resolvable if you install Python "For all users".  (See https://bugs.launchpad.net/virtualenv/+bug/352844)')
        sys.exit(100)
    else:
        logger.info('Got sys.prefix result: %r' % proc_stdout)

    pydistutils = os.path.expanduser('~/.pydistutils.cfg')
    if os.path.exists(pydistutils):
        logger.notify('Please make sure you remove any previous custom paths from '
                      'your %s file.' % pydistutils)
    ## FIXME: really this should be calculated earlier
    return py_executable

def install_activate(home_dir, bin_dir):
    if sys.platform == 'win32' or is_jython and os._name == 'nt':
        files = {'activate.bat': ACTIVATE_BAT,
                 'deactivate.bat': DEACTIVATE_BAT}
        if os.environ.get('OS') == 'Windows_NT' and os.environ.get('OSTYPE') == 'cygwin':
            files['activate'] = ACTIVATE_SH
    else:
        files = {'activate': ACTIVATE_SH}
    files['activate_this.py'] = ACTIVATE_THIS
    for name, content in files.items():
        content = content.replace('__VIRTUAL_ENV__', os.path.abspath(home_dir))
        content = content.replace('__VIRTUAL_NAME__', os.path.basename(os.path.abspath(home_dir)))
        content = content.replace('__BIN_NAME__', os.path.basename(bin_dir))
        writefile(os.path.join(bin_dir, name), content)

def install_distutils(lib_dir, home_dir):
    distutils_path = os.path.join(lib_dir, 'distutils')
    mkdir(distutils_path)
    ## FIXME: maybe this prefix setting should only be put in place if
    ## there's a local distutils.cfg with a prefix setting?
    home_dir = os.path.abspath(home_dir)
    ## FIXME: this is breaking things, removing for now:
    #distutils_cfg = DISTUTILS_CFG + "\n[install]\nprefix=%s\n" % home_dir
    writefile(os.path.join(distutils_path, '__init__.py'), DISTUTILS_INIT)
    writefile(os.path.join(distutils_path, 'distutils.cfg'), DISTUTILS_CFG, overwrite=False)

def fix_lib64(lib_dir):
    """
    Some platforms (particularly Gentoo on x64) put things in lib64/pythonX.Y
    instead of lib/pythonX.Y.  If this is such a platform we'll just create a
    symlink so lib64 points to lib
    """
    if [p for p in distutils.sysconfig.get_config_vars().values()
        if isinstance(p, basestring) and 'lib64' in p]:
        logger.debug('This system uses lib64; symlinking lib64 to lib')
        assert os.path.basename(lib_dir) == 'python%s' % sys.version[:3], (
            "Unexpected python lib dir: %r" % lib_dir)
        lib_parent = os.path.dirname(lib_dir)
        assert os.path.basename(lib_parent) == 'lib', (
            "Unexpected parent dir: %r" % lib_parent)
        copyfile(lib_parent, os.path.join(os.path.dirname(lib_parent), 'lib64'))

def resolve_interpreter(exe):
    """
    If the executable given isn't an absolute path, search $PATH for the interpreter
    """
    if os.path.abspath(exe) != exe:
        paths = os.environ.get('PATH', '').split(os.pathsep)
        for path in paths:
            if os.path.exists(os.path.join(path, exe)):
                exe = os.path.join(path, exe)
                break
    if not os.path.exists(exe):
        logger.fatal('The executable %s (from --python=%s) does not exist' % (exe, exe))
        sys.exit(3)
    return exe

############################################################
## Relocating the environment:

def make_environment_relocatable(home_dir):
    """
    Makes the already-existing environment use relative paths, and takes out
    the #!-based environment selection in scripts.
    """
    activate_this = os.path.join(home_dir, 'bin', 'activate_this.py')
    if not os.path.exists(activate_this):
        logger.fatal(
            'The environment doesn\'t have a file %s -- please re-run virtualenv '
            'on this environment to update it' % activate_this)
    fixup_scripts(home_dir)
    fixup_pth_and_egg_link(home_dir)
    ## FIXME: need to fix up distutils.cfg

OK_ABS_SCRIPTS = ['python', 'python%s' % sys.version[:3],
                  'activate', 'activate.bat', 'activate_this.py']

def fixup_scripts(home_dir):
    # This is what we expect at the top of scripts:
    shebang = '#!%s/bin/python' % os.path.normcase(os.path.abspath(home_dir))
    # This is what we'll put:
    new_shebang = '#!/usr/bin/env python%s' % sys.version[:3]
    activate = "import os; activate_this=os.path.join(os.path.dirname(__file__), 'activate_this.py'); execfile(activate_this, dict(__file__=activate_this)); del os, activate_this"
    bin_dir = os.path.join(home_dir, 'bin')
    for filename in os.listdir(bin_dir):
        filename = os.path.join(bin_dir, filename)
        if not os.path.isfile(filename):
            # ignore subdirs, e.g. .svn ones.
            continue
        f = open(filename, 'rb')
        lines = f.readlines()
        f.close()
        if not lines:
            logger.warn('Script %s is an empty file' % filename)
            continue
        if not lines[0].strip().startswith(shebang):
            if os.path.basename(filename) in OK_ABS_SCRIPTS:
                logger.debug('Cannot make script %s relative' % filename)
            elif lines[0].strip() == new_shebang:
                logger.info('Script %s has already been made relative' % filename)
            else:
                logger.warn('Script %s cannot be made relative (it\'s not a normal script that starts with %s)'
                            % (filename, shebang))
            continue
        logger.notify('Making script %s relative' % filename)
        lines = [new_shebang+'\n', activate+'\n'] + lines[1:]
        f = open(filename, 'wb')
        f.writelines(lines)
        f.close()

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
    f = open(filename)
    prev_lines = f.readlines()
    f.close()
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
    f = open(filename, 'w')
    f.write('\n'.join(lines) + '\n')
    f.close()

def fixup_egg_link(filename):
    f = open(filename)
    link = f.read().strip()
    f.close()
    if os.path.abspath(link) != link:
        logger.debug('Link in %s already relative' % filename)
        return
    new_link = make_relative_path(filename, link)
    logger.notify('Rewriting link %s in %s as %s' % (link, filename, new_link))
    f = open(filename, 'w')
    f.write(new_link)
    f.close()

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

    If you provide something like ``python_version='2.4'`` then the
    script will start with ``#!/usr/bin/env python2.4`` instead of
    ``#!/usr/bin/env python``.  You can use this when the script must
    be run with a particular Python version.
    """
    filename = __file__
    if filename.endswith('.pyc'):
        filename = filename[:-1]
    f = open(filename, 'rb')
    content = f.read()
    f.close()
    py_exe = 'python%s' % python_version
    content = (('#!/usr/bin/env %s\n' % py_exe)
               + '## WARNING: This file is generated\n'
               + content)
    return content.replace('##EXT' 'END##', extra_text)

##EXTEND##

##file site.py
SITE_PY = """
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
yBXo8FFND74JkgILcmKn2vJbYxD8H2/QG9E=
""".decode("base64").decode("zlib")

##file ez_setup.py
EZ_SETUP_PY = """
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
178BYnMUkw==
""".decode("base64").decode("zlib")

##file distribute_setup.py
DISTRIBUTE_SETUP_PY = """
eJztG2tz2zbyO38FTh4PqYSm7bT3GM+pc2nj9DzNJZnYaT8kGRoiIYk1X+XDsvrrb3cBkCAJyUnb
u5mbOd3VoYjFYrHvXUBHfyp3zabIndls9m1RNHVT8ZLFCfybLNtGsCSvG56mvEkAyLlasV3Rsi3P
G9YUrK0Fq0XTlk1RpDXA4mjFSh7d8bVwazkYlDuf/dzWDQBEaRsL1myS2lklKaKHL4CEZwJWrUTU
FNWObZNmw5LGZzyPGY9jmoALImxTlKxYyZU0/osLx2HwWVVFZlAf0jhLsrKoGqQ27Kkl+OErbz7Z
YSV+aYEsxlldiihZJRG7F1UNzEAa+qk+PgNUXGzztOCxkyVVVVQ+KyriEs8ZTxtR5Rx4qoH6Hfu0
aARQccHqgi13rG7LMt0l+drBTfOyrIqySnB6UaIwiB+3t+Md3N4GjnOD7CL+RrQwYhSsauG5xq1E
VVLS9pR0icpyXfHYlGeASuEo5hW1fqp33WOTZEI/r/KMN9GmGxJZiRR033lFXzsJtU2CKiNH02Lt
OE21u+ilWCeofXL4/fXlu/D66ubSEQ+RANKv6P0lslhO6SDYgr0ucmFg02S3S2BhJOpaqkosViyU
yh9GWew94dW6nssp+MGvgMyD7QbiQURtw5ep8OfsKQ11cBXwq8oN9EEEHPUIG1ss2Jmzl+gjUHRg
PogGpBizFUhBEsSeBV/9oUQesV/aogFlwtdtJvIGWL+C5XPQxR4MXiGmEswdiMmQfBdgvnrm9ktq
shChwG3Oh2MKjwv/A+OG8emwwTZ3dlzPXHaMgBM4BTMeUpv+0FNArIMHtWL9aSydog7qkoPVefD0
Nvzp+dWNz0ZMY09Mmb24fPn8/aub8MfLd9dXb17DerOz4C/B+dmsG3r/7hW+3jRNeXF6Wu7KJJCi
CopqfaqcYH1ag6OKxGl82vul05lzfXnz/u3NmzevrsOXz3+4fDFaKDo/nzkm0Nsfvg+vXr98g+Oz
2UfnX6LhMW/4yY/SHV2w8+DMeQ1+9MIwYacbPa6d6zbLOFgFe4CP888iEyclUEjfnectUF6Zzyci
40kq37xKIpHXCvSFkA6E8OILIAgkuG9HjuOQGitf44EnWMK/c20D4gFiTkTKSe5dDtNgk5XgImHL
2psE2V2Mz+CpcRzcRrDlVe65lz0S0IHj2vXVZAlYpHG4jQERiH8tmmgbKwydlyAosN0NzPHMqQTF
iQjpwoKiFHm3iw4mVPtQWxxMDqK0qAWGl94g14UiFjfdBYIOAPyJ3DoQVfJmE/wM8IowH1+moE0G
rR/OPs2nG5FY+oGeYa+LLdsW1Z3JMQ1tUKmEhmFoiuOqG2QvOt1256Y7yYtm4MBcHbFhOVchd0ce
pF/gGnQUQj/g34LLYtuqgMe4rbSumMlJYCw8wiIEQQv0vCwDFw1az/iyuBd60irJAY9NFaTmzLUS
L9sEXoj12oP/fK2s8FCEyLr/6/T/gE6TDCkW5gykaEH0bQdhKDbC9oKQ8u45tU/HT37Bv0v0/ag2
9OoEv8GfykD0mWoodyCjmtauStRt2gyVB5aSwMoGNcfFAyxd03C/SsUTSFGv3lBq4rnfFW0a0yzi
lLSd9RptRVlBDESrHNZT6bDfZbXhktdCb8x4HYuU79SqyMqxGih4tw+TJ8f1Sbk7jgP4P/LOmkjA
55j1VGBQV18g4qwK0CHLy/NP889njzILILjbi5Fx79n/PlpHnz1c6vXqEYdDgJSzIfngD0XVeGc+
6+Wvst9h3WMk+Utd9ekAHVL6vSDTkPIe1Rhqx4tRijTiwMJIk6zckDtYoIq3lYUJi/M/+yCccMXv
xOKmakXnXTNOJl63UJhtKXkmHeXLukjRUJEXTr+EoWkAgv96Jve2vA4llwR6U7e8W4dgUpS11ZTE
In+zIm5TUWOl9LHbjdtzZQw49cSDL4ZoBusNAaRybnjNm6byBoBgKGFsBF1rEo6zFQftWTgNDSvg
MYhyDn3t0kHsK2u6mTL3/j3eYj/zBswIVJnuzXqWfLOYPVWrzS1kjXcxxKfS5u+KfJUmUTNcWoCW
yNohIm/izcGfjAVnatWU9zgdQh1kJMG2gkLXm0DMbsiz07Zis+dg9Ga8bxbHULBArY+C5veQrlMl
8zGfTfFhKyXiudtgvalMHTBvN9gmoP6KagvAU9XmGF0C9jYVIB4rPt064CwrKiQ1whRNE7pKqrrx
wTQBjXW6C4h32uWwk/fGvtzAAv8x/5h737VVBaukO4mYHVdzQD7w/yLAKg4zh6kqS6EljfdsOCbS
2mIfoIFsZHKGfX8Y+YlPOAUjMzV2irt9xeyXWMNnxZB9FmPV6y6bgVVfF83Los3j3220j5JpI3GS
6hxyV2FUCd6IsbcKcXNkgV0WheHqQJT+vTGLPpbApeKV8sJQD7/oW3yduVJc7RqJYHtpEVHpQm1O
xfikkZ27HCp5mRTeKtpvWb2hzGyJ7ch7niYD7Nry8jZbigosmpMpd16BcGH7j5Je6ph0fUjQApoi
2O2AH7cMexwe+Ihoo1cXeSzDJvZoOXNP3XnAbiVPbnHFQe4P/kVUQqeQXb9LryLiQO6RONhNV3ug
DmtU5DH1OkuOgX4pVuhusK0ZNS1P+44r7a/BSqoJtBj+IwnDIBaRUNsKquAlRSGBbW7Vb65SLKsc
wxqtsdJA8cw2t1n/GqI6YOtnkBwHWIatf0UHqKQvm9rVIFdFQbKnHRaZ//F7ASzdk4JrUJVdVhGi
g32p1qphraO8WaKdXyDPn98XCWp1iZYbd+T0Gc4kpHfFS2c95OPrmY9bGrpsSZTikjcZPmLvBI9P
KbYyDDCQnAHpbAkmd+djh32LSojRULoW0OSoqCpwF2R9I2SwW9JqbS8JnnU0guC1CusPNuUwQagi
0AcejzIqyUYiWjLLZ7PtcjYBUmkBIuvHJj5TSQLWsqQYQIAu0UfwgN8S7mBRE77vnJKEYS8pWYKS
sS4FS2z6h8gzD4d9YCNwJm96V/gT2TyP7tqSuLiSCYfIGc0Fj6cNlbQIZB4qHJpTiHhuchP2MIVd
6KX7vR2B7HHaTi4lYkut/3wIYbaRFAtecsgPRr2ZtwiNKVKgJ0CURZsJiUlEsYxz5iYgad+6Niei
xK15Z4+QK5t8sDDSssBTNM0PqzS0TMdMNZinUEEYriEqLYsHb9XmEUYphYOGzXFqm/vsyZO77fxA
tSMPdfq6U03XDu+FjhjX8v3QIGDN+6SQjb7JIYj+lLwe1k9jnEFYpFjiTd93yB+Z38EBFvscpUYw
TpLRrx+rlfppUtv281HJUEtlwP5HPYVaZsq7w1u1MtKaMNshTeUzdcdx/mF+I9WamJEkNhdbHQTx
LQQ0N3jz6kVwXOPpER5EBvhn0kR9h+hkHEGfXcj2nTQOjVP1U7GMxK+ebVRRr186mtisuIe8FDgV
ms1or0x5JDawd6GbwqOImdTY1puCDal/n99BzBn0uSHHUXsw5u53WStM8Tu1km8qps/ejZ6rnRSg
Wh3sBupfD+f6ZuvjCTbnTjAPH7ch9OIDU8DPEvzOncmW1bAS6TnQNyMpWzbPp811RwxwJloAckIt
EKmQp59F22B+iQFpy3e9G9clxTg3MtjjE/u6SDSSqJpvcKK3bRUtgexwACuj36AKnUySIVbN8Jnl
aFA1kRVHJ6becwNMgY+jns+G1FiV6Qgwb1kqGrdmqPhdPB/zs1M0xW/UNc/slvmjPpvqluOhPz4a
3NMYDslDwQxOnsYtXQUyKixNbzPBMu0L2PQSfK3skQNbNbGKE3s61u51f2cmNipyd7QTS4jnK0g7
u6NUnKx2ZCQ0CNLd7Ojau52C94zDtB4w4OkRpA1ZBm44LJY/e/3BXKB7wiWUTlCfyEznsWp84Jks
Lv5L5g+cp0k7KJelAnnMoVrEpjmlq/GpMyG27e6JYWA8KuZ4n33UIMuofqPkfRemC1UnHXXv0WCB
jwPt8fadr/uSti9wXyNSJp5M83Lqyqw+RIIf8CBjb/wdyl/G5MmsPl/uXN3hnNnqCAlgf/4sWdVs
tCT2s8qQUQAT3HF6MdqKQjneinr92FYGZBjtpbG8Ht+fUZp1wabPpY6UCwfPH92h4BP8ZiuV9qqT
LGYuv//+BBmOrhuYL5+/QJ2SSdFyML7t88WfG88Mn9rHtD11GxCf3XV8G746yIr5I4b4KOf+KxZg
sMIML7K71sWXSWz5Vnbf9gYXy3mSwkwtxrCsxCp58LSr7b17F3LIN6ujNKhs7o1TaoNc/K6ugWnA
D/oBYlYsHowg9vT84lOXkNCgry+LibzNRMXlNTKzpkRQec9Spi4nJxXsVZ7ey02Mc13YBOAIYM2q
qbE5inq5QD8u8VgK1qYoVbuRZpZp0ngurrNw5x9ORmdKBgs0+8zFFK7xwYakCut7SYX1mDAFZZN3
376R/LEfFg7IrT8Q5FMLlb+ZUsVwvHV4ctLWonKpM97f7VQnXdiFnJJ4YMkOw17Fn+jtWPOvI05n
YsbRmb7hZ7PNvWe7hxoBR2wrXDCvCEiwhFwjawTtNC6mxIWQjKmFyLBVbp7wTRta9HWLtjNMwdXV
GWTDdENGDMKcESZv6wBzqOGxdPBOHlliEgterwJnM0j77QnxSI4UgRHDgty08qiKcze7Ukz4hn0d
4yzk+durP5jweV9cjRGCUg4V0ryQZF6PN1N9WfDaRXPEYtEIdfELgzMeJncRDjU1HmeU3UnSYkxe
oIfG+mxe2ze6C3Jp0G7dZrCsonhBfXHpGFEhyTEmD0RsWUG5HYtY3uBPVgre/K1AbRT1sbozlvl9
X143h838fxhFbJTZpaCwAUP9McGASLbzbVcZp9oqLzUDLRuoBvZXDIM0C6xSyrE2b5ypLVk2EYg8
VhGErj3t2VR+Ii+k9cIb0IH2vb8/ZZWqnqxIAxy21qOlWWHcWdxP0r6MyELK4QRJkejtyy9R54ZV
/hfkmHuTzAPnBCPeDOdNTwpM3ehOn9Cs6YhUuj86rjT8fS7Goh1m979XniN66cAuF8bZRsrbPNr0
+Vz/Zhwp36mRwZ4xtLENx5YR/qhGQlD5rX+UgVD6Zv/wZv4n9rTL8qTj0/c4rD+66Eg0Lq/WIl3J
ru9iFsx8lgk8YK4X6Lj7kyp14ZYODBWEPLagw+IKtiTpx6+RvIqi75tqvvYH3+j48DdBxTbHQjIr
Yvz1kHSy2KkmgFJUWVLX9HOe/iBBI0lA0tTwAcbGdcBucQNud4EAf8oDSFeCCJlctwVCFQfgESar
Hbno7mSmxVMiIsOfZtGlAuAnkUzdK40HG8RKVUAtlju2Fo3C5c2HJ+0q64mKcmd+h2oGcmx1c0wy
VF471gCK8f31MpMDoA+fuuCrxTIJunoAA2C6crp8H1YipwNuW4EMyk81rJq3I+M/0oQN6FEXH2q+
EihVMTr+7SEDXkIZF3tqjaG/0HQtiFsB/jkIiPeOsFXx9dd/owQhSjIQH5UpQN/ZX8/OjIwnXQVK
9BqnVP4ucL8T2KMSrEbumyR3Sc6ojcX+zrxnPvva4BDaGM4XlQcYzn3E82xu8zAsykqCCbDSloBB
f7QyZhsi9SRmO0AlqfdsffMJojuxW2gFDPAeJagv0uwiAe7cZwqbvGKqGQTpEV0IAFydBXdWi6pL
4sB8acy8kdIZ4wMi6RDL2hvQAh8yaHIOSFKONkBcL2OFdz4FbOlw7DMAow3s7ACgysJNi/0NtyOl
iuLkFLifQt15bino8ObpqEq0XdQjZGG8XHughDPlWvAXT3gxRuhwkPGEqtx7n+25DNYHgqtDP4sk
Fbjk9U5Baed3+Jq4CqTjH0EBcQmdp2OGElLpG4ZIahiq39wR3V2T4/zi09z5N4dES24=
""".decode("base64").decode("zlib")

##file activate.sh
ACTIVATE_SH = """
eJytVFFv2jAQfs+vuIU+QDWK+tqKB6oigdRC1bBOW1sZk1yIpWAj2yGj0/77ziFAUijStPIA2Hc+
f/7u+64Bk0QYiEWKsMiMhRlCZjCCXNgEfKMyHSLMhOzw0IoVt+jDeazVAmbcJOdeA9Yqg5BLqSzo
TIKwEAmNoU3Xnhfh9hQ0W/DbA/o0QKNBCyqNAOVKaCUXKC2suBZ8lqIpskQMz9CW4J+x8d0texo+
Tr717thDbzLw4RWuwSYoi0z3cdvdY6m7DPy1VNoWibu9TDocB4eKeCxOwvgxGYxHg/F9/xiYXfAA
0v7YAbBd6CS8ehaBLCktmmgSlRGpEVqiv+gPcBnBm0m+Qp6IMIGErxA4/VAoVIuFC9uE26L1ZSkS
QMjTlCRgFcwJAXWU/sVKu8WSk0bKo+YC4DvJRGW2DFsh52WZWqIjCM4cuRAmXM7RQE5645H7WoPT
Dl1LulgScozeUX/TC6jpbbVZ/QwG7Kn/GAzHoyPkF09r6xo9HzUxuDzWveDyoG2UeNCv4PJko8rw
FsImZRvtj572wL4QLgLSBV8qGaGxOnOewXfYGhBgGsM24cu729sutDXb9uo/HvlzExdaY0rdrxmt
Ys/63Z5Xgdr1GassGfO9koTqe7wDHxGNGw+Wi0p2h7Gb4YiNevd9xq7KtKpFd7j3inds0Q5FrBN7
LtIUYi5St1/NMi7LKdZpDhdLuwZ6FwkTmhsTUMaMR2SNdc7XLaoXFrahqQdTqtUs6Myu4YoUu6vb
guspCFm4ytsL6sNB8IFtu7UjFWlUnO00s7nhDWqssdth0Lu567OHx/H9w+TkjYWKd8ItyvlTAo+S
LxBeanVf/GmhP+rsoR8a4EwpeEpTgRgin0OPdiQZdy7CctYrLcq5XR5BhMTa5VWnk+f5xRtasvrq
gsZBx6jY5lxjh7sqnbrvnisQp1T6KNiX6fQV9m/D1GC9SvPEQ1v7g+WIrxjaMf9Js/QT5uh/ztB/
n5/b2Uk0/AXm/2MV
""".decode("base64").decode("zlib")

##file activate.bat
ACTIVATE_BAT = """
eJyFUssKgzAQvAfyD3swYH+hItSiVKlGsalQKOyhauvFHOr/U+MzFcWc9jEzO7vkVLw+EmRZUvIt
GsiCVNydED2e2YhahkgJJVUJtWwgL8qqLnJI0jhKBJiUQPsUv6/YRmJcKDkMlBGOcehOmptctgJj
e2IP4cfcjyNvFOwVp/JSdWqMygq+MthmkwHNojmfhjuRh3iAGffncsPYhpl2mm5sbY+9QzjC7ylt
sFy6LTEL3rKRcLsGicrXV++4HVz1jzN4Vta+BnsingM+nMLSiB53KfkBsnmnEA==
""".decode("base64").decode("zlib")

##file deactivate.bat
DEACTIVATE_BAT = """
eJxzSE3OyFfIT0vj4spMU0hJTcvMS01RiPf3cYkP8wwKCXX0iQ8I8vcNCFHQ4FIAguLUEgWIgK0q
FlWqXJpcICVYpGzx2BAZ4uHv5+Hv6wq1BWINXBTdKriEKkI1DhW2QAfhttcxxANiFZCBbglQSJUL
i2dASrm4rFz9XLgAwJNbyQ==
""".decode("base64").decode("zlib")

##file distutils-init.py
DISTUTILS_INIT = """
eJytVl2L6zYQffevGBKK7XavKe3bhVBo78uFSyml0IdlEVpbTtR1JCMpm6S/vjOSY0v+uO1DDbs4
0tF8nJk5sjz32jjQNpPhzd7H1ys3SqqjhcfCL1q18vgbN1YY2Kc/pQWlHXB4l8ZdeCfUO5x1c+nE
E1gNVwE1V3CxAqQDp6GVqgF3EmBd08nXLGukUfws4IDBVD13p2pYoS3rLk52ltF6hPhLS1XM4EUc
VsVYKzvBWPkE+WgmLzPZjkaUNmd6KVI3JRwWoRSLM6P98mMG+Dw4q+il8Ev07P7ATCNmRlfQ8/qN
HwVwB99Y4H0vMHAi6BWZUoEhoqXTNXdSK+A2LN6tE+fJ0E+7MhOdFSEM5lNgrJIKWXDF908wy87D
xE3UoHsxkegZTaHIHGNSSYfm+ntelpURvCnK7NEWBI/ap/b8Z1m232N2rj7B60V2DRM3B5NpaLSw
KnfwpvQVTviHOR+F88lhQyBAGlE7be6DoRNg9ldsG3218IHa6MRNU+tGBEYIggwafRk6yzsXDcVU
9Ua08kYxt+F3x12LRaQi52j0xx/ywFxrdMRqVevzmaummlIYEp0WsCAaX8cFb6buuLUTqEgQQ6/Q
04iWRoF38m/BdE8VtlBY0bURiB6KG1crpMZwc2fIjqWh+1UrkSLpWUIP8PySwLKv4qPGSVqDuMPy
dywQ+gS7L1irXVkm5pJsq3l+Ib1lMOvUrxI+/mBBY4KB+WpUtcO06RtzckNvQ6vYj1lGoZM2sdDG
fryJPYJVn/Cfka8XSqNaoLKhmOlqXMzW9+YBVp1EtIThZtOwzCRvMaARa+0xD0b2kcaJGwJsMbc7
hLUfY4vKvsCOBdvDnyfuRbzmXRdGTZgPF7oGQkJACWVD22IMQdhx0npt5S2f+pXO+OwH6d+hwiS5
7IJOjcK2emj1zBy1aONHByfAMoraw6WlrSIFTbGghqASoRCjVncYROFpXM4uYSqhGnuVeGvks4jz
cjnCoR5GnPW7KOh4maVbdFeoplgJ3wh3MSrAsv/QuMjOspnTKRl1fTYqqNisv7uTVnhF1GhoBFbp
lh+OcXN2riA5ZrYXtWxlfcDuC8U5kLoN3CCJYXGpesO6dx6rU0zGMtjU6cNlmW0Fid8Sja4ZG+Z3
fTPbyj+mZnZ2wSQK8RaT9Km0ySRuLpm0DkUUL0ra3WQ2BgGJ7v9I9SKqNKZ/IR4R28RHm+vEz5ic
nZ2IH7bfub8pU1PR3gr10W7xLTfHh6Z6bgZ7K14G7Mj/1z5J6MFo6V5e07H0Ou78dTyeI+mxKOpI
eC2KMSj6HKxd6Uudf/n886fPv+f++x1lbASlmjQuPz8OvGA0j7j2eCu/4bcW6SFeCuNJ0W1GQHI5
iwC9Ey0bjtHd9P4dPA++XxLnZDVuxvFEtlm3lf5a2c02u2LRYXHH/AOs8pIa
""".decode("base64").decode("zlib")

##file distutils.cfg
DISTUTILS_CFG = """
eJxNj00KwkAMhfc9xYNuxe4Ft57AjYiUtDO1wXSmNJnK3N5pdSEEAu8nH6lxHVlRhtDHMPATA4uH
xJ4EFmGbvfJiicSHFRzUSISMY6hq3GLCRLnIvSTnEefN0FIjw5tF0Hkk9Q5dRunBsVoyFi24aaLg
9FDOlL0FPGluf4QjcInLlxd6f6rqkgPu/5nHLg0cXCscXoozRrP51DRT3j9QNl99AP53T2Q=
""".decode("base64").decode("zlib")

##file activate_this.py
ACTIVATE_THIS = """
eJyNUk2L3DAMvftXiCxLEphmSvc2MIcu9NaWHnopwxCcRNlRN7GD7clM/n0lp5mPZQs1JLb8pKcn
WUmSPE9w9GReAM9Yt9RhFg7kSzmtoKE6ZGU0ynJ7AfIcJnuEE3Wd0nWgUQcEQWEkF466QzMCf+Ss
6dGEQqmfgtbaQIWcDxs4HdBElv7og1wBg3gmH0TMjykcrAEyAd3gkP8rMDaocMDbHBWZ9RBdVZIk
SgU3bRTwWjQrPNc4BPiue/zinHUz7DRxws/eowtkTUSyiMhKfi2y3NHMdXX0itcOpYMOh3Ww61g8
luJSDFP6tmH3ftyki2eeJ7mifrAugJ/8crReqUqztC0fC4kuGnKGxWf/snXlZb8kzXMmboW0GDod
Wut62G4hPZF5+pTO5XtiKYOuX/UL+ptcvy2ZTPKvIP1KFdeTiuuHxTXNFXYe/5+km0nmJ3r0KTxG
YSM6z23fbZ7276Tg9x5LdiuFjok7noks1sP2tWscpeRX6KaRnRuT3WnKlQQ51F3JlC2dmSvSRENd
j3wvetUDfLOjDDLPYtPwjDJb7yHYeNXyMPMLtdEQKRtl8HQrdLdX3O4YxZP7RvfcNH6ZCPMsi8td
qZvLAN7yFnoY0DSZhOUXj4WWy+tZ8190ud1tPu5Zzy2N+gOGaVfA
""".decode("base64").decode("zlib")

if __name__ == '__main__':
    main()

## TODO:
## Copy python.exe.manifest
## Monkeypatch distutils.sysconfig
