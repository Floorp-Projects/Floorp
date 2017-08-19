"""Main entry point"""

import optparse
import os
import shlex
import shutil
import sys
import tempfile

try:
    import configparser
except ImportError: # pragma: nocover
    import ConfigParser as configparser

from cram._cli import runcli
from cram._encoding import b, fsencode, stderrb, stdoutb
from cram._run import runtests
from cram._xunit import runxunit

def _which(cmd):
    """Return the path to cmd or None if not found"""
    cmd = fsencode(cmd)
    for p in os.environ['PATH'].split(os.pathsep):
        path = os.path.join(fsencode(p), cmd)
        if os.path.isfile(path) and os.access(path, os.X_OK):
            return os.path.abspath(path)
    return None

def _expandpath(path):
    """Expands ~ and environment variables in path"""
    return os.path.expanduser(os.path.expandvars(path))

class _OptionParser(optparse.OptionParser):
    """Like optparse.OptionParser, but supports setting values through
    CRAM= and .cramrc."""

    def __init__(self, *args, **kwargs):
        self._config_opts = {}
        optparse.OptionParser.__init__(self, *args, **kwargs)

    def add_option(self, *args, **kwargs):
        option = optparse.OptionParser.add_option(self, *args, **kwargs)
        if option.dest and option.dest != 'version':
            key = option.dest.replace('_', '-')
            self._config_opts[key] = option.action == 'store_true'
        return option

    def parse_args(self, args=None, values=None):
        config = configparser.RawConfigParser()
        config.read(_expandpath(os.environ.get('CRAMRC', '.cramrc')))
        defaults = {}
        for key, isbool in self._config_opts.items():
            try:
                if isbool:
                    try:
                        value = config.getboolean('cram', key)
                    except ValueError:
                        value = config.get('cram', key)
                        self.error('--%s: invalid boolean value: %r'
                                   % (key, value))
                else:
                    value = config.get('cram', key)
            except (configparser.NoSectionError, configparser.NoOptionError):
                pass
            else:
                defaults[key] = value
        self.set_defaults(**defaults)

        eargs = os.environ.get('CRAM', '').strip()
        if eargs:
            args = args or []
            args += shlex.split(eargs)

        try:
            return optparse.OptionParser.parse_args(self, args, values)
        except optparse.OptionValueError:
            self.error(str(sys.exc_info()[1]))

def _parseopts(args):
    """Parse command line arguments"""
    p = _OptionParser(usage='cram [OPTIONS] TESTS...', prog='cram')
    p.add_option('-V', '--version', action='store_true',
                 help='show version information and exit')
    p.add_option('-q', '--quiet', action='store_true',
                 help="don't print diffs")
    p.add_option('-v', '--verbose', action='store_true',
                 help='show filenames and test status')
    p.add_option('-i', '--interactive', action='store_true',
                 help='interactively merge changed test output')
    p.add_option('-d', '--debug', action='store_true',
                 help='write script output directly to the terminal')
    p.add_option('-y', '--yes', action='store_true',
                 help='answer yes to all questions')
    p.add_option('-n', '--no', action='store_true',
                 help='answer no to all questions')
    p.add_option('-E', '--preserve-env', action='store_true',
                 help="don't reset common environment variables")
    p.add_option('--keep-tmpdir', action='store_true',
                 help='keep temporary directories')
    p.add_option('--shell', action='store', default='/bin/sh', metavar='PATH',
                 help='shell to use for running tests (default: %default)')
    p.add_option('--shell-opts', action='store', metavar='OPTS',
                 help='arguments to invoke shell with')
    p.add_option('--indent', action='store', default=2, metavar='NUM',
                 type='int', help=('number of spaces to use for indentation '
                                   '(default: %default)'))
    p.add_option('--xunit-file', action='store', metavar='PATH',
                 help='path to write xUnit XML output')
    opts, paths = p.parse_args(args)
    paths = [fsencode(path) for path in paths]
    return opts, paths, p.get_usage

def main(args):
    """Main entry point.

    If you're thinking of using Cram in other Python code (e.g., unit tests),
    consider using the test() or testfile() functions instead.

    :param args: Script arguments (excluding script name)
    :type args: str
    :return: Exit code (non-zero on failure)
    :rtype: int
    """
    opts, paths, getusage = _parseopts(args)
    if opts.version:
        sys.stdout.write("""Cram CLI testing framework (version 0.7)

Copyright (C) 2010-2016 Brodie Rao <brodie@bitheap.org> and others
This is free software; see the source for copying conditions. There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
""")
        return

    conflicts = [('--yes', opts.yes, '--no', opts.no),
                 ('--quiet', opts.quiet, '--interactive', opts.interactive),
                 ('--debug', opts.debug, '--quiet', opts.quiet),
                 ('--debug', opts.debug, '--interactive', opts.interactive),
                 ('--debug', opts.debug, '--verbose', opts.verbose),
                 ('--debug', opts.debug, '--xunit-file', opts.xunit_file)]
    for s1, o1, s2, o2 in conflicts:
        if o1 and o2:
            sys.stderr.write('options %s and %s are mutually exclusive\n'
                             % (s1, s2))
            return 2

    shellcmd = _which(opts.shell)
    if not shellcmd:
        stderrb.write(b('shell not found: ') + fsencode(opts.shell) + b('\n'))
        return 2
    shell = [shellcmd]
    if opts.shell_opts:
        shell += shlex.split(opts.shell_opts)

    patchcmd = None
    if opts.interactive:
        patchcmd = _which('patch')
        if not patchcmd:
            sys.stderr.write('patch(1) required for -i\n')
            return 2

    if not paths:
        sys.stdout.write(getusage())
        return 2

    badpaths = [path for path in paths if not os.path.exists(path)]
    if badpaths:
        stderrb.write(b('no such file: ') + badpaths[0] + b('\n'))
        return 2

    if opts.yes:
        answer = 'y'
    elif opts.no:
        answer = 'n'
    else:
        answer = None

    tmpdir = os.environ['CRAMTMP'] = tempfile.mkdtemp('', 'cramtests-')
    tmpdirb = fsencode(tmpdir)
    proctmp = os.path.join(tmpdir, 'tmp')
    for s in ('TMPDIR', 'TEMP', 'TMP'):
        os.environ[s] = proctmp

    os.mkdir(proctmp)
    try:
        tests = runtests(paths, tmpdirb, shell, indent=opts.indent,
                         cleanenv=not opts.preserve_env, debug=opts.debug)
        if not opts.debug:
            tests = runcli(tests, quiet=opts.quiet, verbose=opts.verbose,
                           patchcmd=patchcmd, answer=answer)
            if opts.xunit_file is not None:
                tests = runxunit(tests, opts.xunit_file)

        hastests = False
        failed = False
        for path, test in tests:
            hastests = True
            refout, postout, diff = test()
            if diff:
                failed = True

        if not hastests:
            sys.stderr.write('no tests found\n')
            return 2

        return int(failed)
    finally:
        if opts.keep_tmpdir:
            stdoutb.write(b('# Kept temporary directory: ') + tmpdirb +
                          b('\n'))
        else:
            shutil.rmtree(tmpdir)
