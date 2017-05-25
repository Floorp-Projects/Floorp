#!/usr/bin/env python
# Copyright (c) 2002-2005 ActiveState
# See LICENSE.txt for license details.

"""
    which.py dev build script

    Usage:
        python build.py [<options>...] [<targets>...]

    Options:
        --help, -h      Print this help and exit.
        --targets, -t   List all available targets.

    This is the primary build script for the which.py project. It exists
    to assist in building, maintaining, and distributing this project.
    
    It is intended to have Makefile semantics. I.e. 'python build.py'
    will build execute the default target, 'python build.py foo' will
    build target foo, etc. However, there is no intelligent target
    interdependency tracking (I suppose I could do that with function
    attributes).
"""

import os
from os.path import basename, dirname, splitext, isfile, isdir, exists, \
                    join, abspath, normpath
import sys
import getopt
import types
import getpass
import shutil
import glob
import logging
import re



#---- exceptions

class Error(Exception):
    pass



#---- globals

log = logging.getLogger("build")




#---- globals

_project_name_ = "which"



#---- internal support routines

def _get_trentm_com_dir():
    """Return the path to the local trentm.com source tree."""
    d = normpath(join(dirname(__file__), os.pardir, "trentm.com"))
    if not isdir(d):
        raise Error("could not find 'trentm.com' src dir at '%s'" % d)
    return d

def _get_local_bits_dir():
    import imp
    info = imp.find_module("tmconfig", [_get_trentm_com_dir()])
    tmconfig = imp.load_module("tmconfig", *info)
    return tmconfig.bitsDir

def _get_project_bits_dir():
    d = normpath(join(dirname(__file__), "bits"))
    return d

def _get_project_version():
    import imp, os
    data = imp.find_module(_project_name_, [os.path.dirname(__file__)])
    mod = imp.load_module(_project_name_, *data)
    return mod.__version__


# Recipe: run (0.5.1) in /Users/trentm/tm/recipes/cookbook
_RUN_DEFAULT_LOGSTREAM = ("RUN", "DEFAULT", "LOGSTREAM")
def __run_log(logstream, msg, *args, **kwargs):
    if not logstream:
        pass
    elif logstream is _RUN_DEFAULT_LOGSTREAM:
        try:
            log.debug(msg, *args, **kwargs)
        except NameError:
            pass
    else:
        logstream(msg, *args, **kwargs)

def _run(cmd, logstream=_RUN_DEFAULT_LOGSTREAM):
    """Run the given command.

        "cmd" is the command to run
        "logstream" is an optional logging stream on which to log the command.
            If None, no logging is done. If unspecifed, this looks for a Logger
            instance named 'log' and logs the command on log.debug().

    Raises OSError is the command returns a non-zero exit status.
    """
    __run_log(logstream, "running '%s'", cmd)
    retval = os.system(cmd)
    if hasattr(os, "WEXITSTATUS"):
        status = os.WEXITSTATUS(retval)
    else:
        status = retval
    if status:
        #TODO: add std OSError attributes or pick more approp. exception
        raise OSError("error running '%s': %r" % (cmd, status))

def _run_in_dir(cmd, cwd, logstream=_RUN_DEFAULT_LOGSTREAM):
    old_dir = os.getcwd()
    try:
        os.chdir(cwd)
        __run_log(logstream, "running '%s' in '%s'", cmd, cwd)
        _run(cmd, logstream=None)
    finally:
        os.chdir(old_dir)


# Recipe: rmtree (0.5) in /Users/trentm/tm/recipes/cookbook
def _rmtree_OnError(rmFunction, filePath, excInfo):
    if excInfo[0] == OSError:
        # presuming because file is read-only
        os.chmod(filePath, 0777)
        rmFunction(filePath)
def _rmtree(dirname):
    import shutil
    shutil.rmtree(dirname, 0, _rmtree_OnError)


# Recipe: pretty_logging (0.1) in /Users/trentm/tm/recipes/cookbook
class _PerLevelFormatter(logging.Formatter):
    """Allow multiple format string -- depending on the log level.
    
    A "fmtFromLevel" optional arg is added to the constructor. It can be
    a dictionary mapping a log record level to a format string. The
    usual "fmt" argument acts as the default.
    """
    def __init__(self, fmt=None, datefmt=None, fmtFromLevel=None):
        logging.Formatter.__init__(self, fmt, datefmt)
        if fmtFromLevel is None:
            self.fmtFromLevel = {}
        else:
            self.fmtFromLevel = fmtFromLevel
    def format(self, record):
        record.levelname = record.levelname.lower()
        if record.levelno in self.fmtFromLevel:
            #XXX This is a non-threadsafe HACK. Really the base Formatter
            #    class should provide a hook accessor for the _fmt
            #    attribute. *Could* add a lock guard here (overkill?).
            _saved_fmt = self._fmt
            self._fmt = self.fmtFromLevel[record.levelno]
            try:
                return logging.Formatter.format(self, record)
            finally:
                self._fmt = _saved_fmt
        else:
            return logging.Formatter.format(self, record)

def _setup_logging():
    hdlr = logging.StreamHandler()
    defaultFmt = "%(name)s: %(levelname)s: %(message)s"
    infoFmt = "%(name)s: %(message)s"
    fmtr = _PerLevelFormatter(fmt=defaultFmt,
                              fmtFromLevel={logging.INFO: infoFmt})
    hdlr.setFormatter(fmtr)
    logging.root.addHandler(hdlr)
    log.setLevel(logging.INFO)


def _getTargets():
    """Find all targets and return a dict of targetName:targetFunc items."""
    targets = {}
    for name, attr in sys.modules[__name__].__dict__.items():
        if name.startswith('target_'):
            targets[ name[len('target_'):] ] = attr
    return targets

def _listTargets(targets):
    """Pretty print a list of targets."""
    width = 77
    nameWidth = 15 # min width
    for name in targets.keys():
        nameWidth = max(nameWidth, len(name))
    nameWidth += 2  # space btwn name and doc
    format = "%%-%ds%%s" % nameWidth
    print format % ("TARGET", "DESCRIPTION")
    for name, func in sorted(targets.items()):
        doc = _first_paragraph(func.__doc__ or "", True)
        if len(doc) > (width - nameWidth):
            doc = doc[:(width-nameWidth-3)] + "..."
        print format % (name, doc)


# Recipe: first_paragraph (1.0.1) in /Users/trentm/tm/recipes/cookbook
def _first_paragraph(text, join_lines=False):
    """Return the first paragraph of the given text."""
    para = text.lstrip().split('\n\n', 1)[0]
    if join_lines:
        lines = [line.strip() for line in  para.splitlines(0)]
        para = ' '.join(lines)
    return para



#---- build targets

def target_default():
    target_all()

def target_all():
    """Build all release packages."""
    log.info("target: default")
    if sys.platform == "win32":
        target_launcher()
    target_sdist()
    target_webdist()


def target_clean():
    """remove all build/generated bits"""
    log.info("target: clean")
    if sys.platform == "win32":
        _run("nmake -f Makefile.win clean")

    ver = _get_project_version()
    dirs = ["dist", "build", "%s-%s" % (_project_name_, ver)]
    for d in dirs:
        print "removing '%s'" % d
        if os.path.isdir(d): _rmtree(d)

    patterns = ["*.pyc", "*~", "MANIFEST",
                os.path.join("test", "*~"),
                os.path.join("test", "*.pyc"),
               ]
    for pattern in patterns:
        for file in glob.glob(pattern):
            print "removing '%s'" % file
            os.unlink(file)


def target_launcher():
    """Build the Windows launcher executable."""
    log.info("target: launcher")
    assert sys.platform == "win32", "'launcher' target only supported on Windows"
    _run("nmake -f Makefile.win")


def target_docs():
    """Regenerate some doc bits from project-info.xml."""
    log.info("target: docs")
    _run("projinfo -f project-info.xml -R -o README.txt --force")
    _run("projinfo -f project-info.xml --index-markdown -o index.markdown --force")


def target_sdist():
    """Build a source distribution."""
    log.info("target: sdist")
    target_docs()
    bitsDir = _get_project_bits_dir()
    _run("python setup.py sdist -f --formats zip -d %s" % bitsDir,
         log.info)


def target_webdist():
    """Build a web dist package.
    
    "Web dist" packages are zip files with '.web' package. All files in
    the zip must be under a dir named after the project. There must be a
    webinfo.xml file at <projname>/webinfo.xml. This file is "defined"
    by the parsing in trentm.com/build.py.
    """ 
    assert sys.platform != "win32", "'webdist' not implemented for win32"
    log.info("target: webdist")
    bitsDir = _get_project_bits_dir()
    buildDir = join("build", "webdist")
    distDir = join(buildDir, _project_name_)
    if exists(buildDir):
        _rmtree(buildDir)
    os.makedirs(distDir)

    target_docs()
    
    # Copy the webdist bits to the build tree.
    manifest = [
        "project-info.xml",
        "index.markdown",
        "LICENSE.txt",
        "which.py",
        "logo.jpg",
    ]
    for src in manifest:
        if dirname(src):
            dst = join(distDir, dirname(src))
            os.makedirs(dst)
        else:
            dst = distDir
        _run("cp %s %s" % (src, dst))

    # Zip up the webdist contents.
    ver = _get_project_version()
    bit = abspath(join(bitsDir, "%s-%s.web" % (_project_name_, ver)))
    if exists(bit):
        os.remove(bit)
    _run_in_dir("zip -r %s %s" % (bit, _project_name_), buildDir, log.info)


def target_install():
    """Use the setup.py script to install."""
    log.info("target: install")
    _run("python setup.py install")


def target_upload_local():
    """Update release bits to *local* trentm.com bits-dir location.
    
    This is different from the "upload" target, which uploads release
    bits remotely to trentm.com.
    """
    log.info("target: upload_local")
    assert sys.platform != "win32", "'upload_local' not implemented for win32"

    ver = _get_project_version()
    localBitsDir = _get_local_bits_dir()
    uploadDir = join(localBitsDir, _project_name_, ver)

    bitsPattern = join(_get_project_bits_dir(),
                       "%s-*%s*" % (_project_name_, ver))
    bits = glob.glob(bitsPattern)
    if not bits:
        log.info("no bits matching '%s' to upload", bitsPattern)
    else:
        if not exists(uploadDir):
            os.makedirs(uploadDir)
        for bit in bits:
            _run("cp %s %s" % (bit, uploadDir), log.info)


def target_upload():
    """Upload binary and source distribution to trentm.com bits
    directory.
    """
    log.info("target: upload")

    ver = _get_project_version()
    bitsDir = _get_project_bits_dir()
    bitsPattern = join(bitsDir, "%s-*%s*" % (_project_name_, ver))
    bits = glob.glob(bitsPattern)
    if not bits:
        log.info("no bits matching '%s' to upload", bitsPattern)
        return

    # Ensure have all the expected bits.
    expectedBits = [
        re.compile("%s-.*\.zip$" % _project_name_),
        re.compile("%s-.*\.web$" % _project_name_)
    ]
    for expectedBit in expectedBits:
        for bit in bits:
            if expectedBit.search(bit):
                break
        else:
            raise Error("can't find expected bit matching '%s' in '%s' dir"
                        % (expectedBit.pattern, bitsDir))

    # Upload the bits.
    user = "trentm"
    host = "trentm.com"
    remoteBitsBaseDir = "~/data/bits"
    remoteBitsDir = join(remoteBitsBaseDir, _project_name_, ver)
    if sys.platform == "win32":
        ssh = "plink"
        scp = "pscp -unsafe"
    else:
        ssh = "ssh"
        scp = "scp"
    _run("%s %s@%s 'mkdir -p %s'" % (ssh, user, host, remoteBitsDir), log.info)
    for bit in bits:
        _run("%s %s %s@%s:%s" % (scp, bit, user, host, remoteBitsDir),
             log.info)


def target_check_version():
    """grep for version strings in source code
    
    List all things that look like version strings in the source code.
    Used for checking that versioning is updated across the board.  
    """
    sources = [
        "which.py",
        "project-info.xml",
    ]
    pattern = r'[0-9]\+\(\.\|, \)[0-9]\+\(\.\|, \)[0-9]\+'
    _run('grep -n "%s" %s' % (pattern, ' '.join(sources)), None)



#---- mainline

def build(targets=[]):
    log.debug("build(targets=%r)" % targets)
    available = _getTargets()
    if not targets:
        if available.has_key('default'):
            return available['default']()
        else:   
            log.warn("No default target available. Doing nothing.")
    else:
        for target in targets:
            if available.has_key(target):
                retval = available[target]()
                if retval:
                    raise Error("Error running '%s' target: retval=%s"\
                                % (target, retval))
            else:
                raise Error("Unknown target: '%s'" % target)

def main(argv):
    _setup_logging()

    # Process options.
    optlist, targets = getopt.getopt(argv[1:], 'ht', ['help', 'targets'])
    for opt, optarg in optlist:
        if opt in ('-h', '--help'):
            sys.stdout.write(__doc__ + '\n')
            return 0
        elif opt in ('-t', '--targets'):
            return _listTargets(_getTargets())

    return build(targets)

if __name__ == "__main__":
    sys.exit( main(sys.argv) )

