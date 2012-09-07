# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import with_statement
__all__ = ['check_for_crashes']

import os, sys, glob, urllib2, tempfile, subprocess, shutil, urlparse, zipfile
import mozlog

def is_url(thing):
  """
  Return True if thing looks like a URL.
  """
  # We want to download URLs like http://... but not Windows paths like c:\...
  return len(urlparse.urlparse(thing).scheme) >= 2

def extractall(zip, path = None):
    """
    Compatibility shim for Python 2.6's ZipFile.extractall.
    """
    if hasattr(zip, "extractall"):
        return zip.extractall(path)

    if path is None:
        path = os.curdir

    for name in self._zipfile.namelist():
        filename = os.path.normpath(os.path.join(path, name))
        if name.endswith("/"):
            os.makedirs(filename)
        else:
            path = os.path.split(filename)[0]
            if not os.path.isdir(path):
                os.makedirs(path)
        with open(filename, "wb") as dest:
            dest.write(zip.read(name))

def check_for_crashes(dump_directory, symbols_path,
                      stackwalk_binary=None,
                      dump_save_path=None,
                      test_name=None):
    """
    Print a stack trace for minidumps left behind by a crashing program.

    Arguments:
    dump_directory: The directory in which to look for minidumps.
    symbols_path: The path to symbols to use for dump processing.
                  This can either be a path to a directory
                  containing Breakpad-format symbols, or a URL
                  to a zip file containing a set of symbols.
    stackwalk_binary: The path to the minidump_stackwalk binary.
                      If not set, the environment variable
                      MINIDUMP_STACKWALK will be checked.
    dump_save_path: A directory in which to copy minidump files
                    for safekeeping. If not set, the environment
                    variable MINIDUMP_SAVE_PATH will be checked.
    test_name: The test name to be used in log output.

    Returns True if any minidumps were found, False otherwise.
    """
    log = mozlog.getLogger('mozcrash')
    if stackwalk_binary is None:
        stackwalk_binary = os.environ.get('MINIDUMP_STACKWALK', None)

    # try to get the caller's filename if no test name is given
    if test_name is None:
        try:
            test_name = os.path.basename(sys._getframe(1).f_code.co_filename)
        except:
            test_name = "unknown"

    # Check preconditions
    dumps = glob.glob(os.path.join(dump_directory, '*.dmp'))
    if len(dumps) == 0:
        return False

    found_crash = False
    remove_symbols = False 
    # If our symbols are at a remote URL, download them now
    if is_url(symbols_path):
        log.info("Downloading symbols from: %s", symbols_path)
        remove_symbols = True
        # Get the symbols and write them to a temporary zipfile
        data = urllib2.urlopen(symbols_path)
        symbols_file = tempfile.TemporaryFile()
        symbols_file.write(data.read())
        # extract symbols to a temporary directory (which we'll delete after
        # processing all crashes)
        symbols_path = tempfile.mkdtemp()
        zfile = zipfile.ZipFile(symbols_file, 'r')
        extractall(zfile, symbols_path)
        zfile.close()

    try:
        for d in dumps:
            log.info("PROCESS-CRASH | %s | application crashed (minidump found)", test_name)
            log.info("Crash dump filename: %s", d)
            if symbols_path and stackwalk_binary and os.path.exists(stackwalk_binary):
                # run minidump_stackwalk
                p = subprocess.Popen([stackwalk_binary, d, symbols_path],
                                     stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE)
                (out, err) = p.communicate()
                if len(out) > 3:
                    # minidump_stackwalk is chatty,
                    # so ignore stderr when it succeeds.
                    print out
                else:
                    print "stderr from minidump_stackwalk:"
                    print err
                if p.returncode != 0:
                    log.error("minidump_stackwalk exited with return code %d", p.returncode)
            else:
                if not symbols_path:
                    log.warn("No symbols path given, can't process dump.")
                if not stackwalk_binary:
                    log.warn("MINIDUMP_STACKWALK not set, can't process dump.")
                elif stackwalk_binary and not os.path.exists(stackwalk_binary):
                    log.warn("MINIDUMP_STACKWALK binary not found: %s", stackwalk_binary)
            if dump_save_path is None:
                dump_save_path = os.environ.get('MINIDUMP_SAVE_PATH', None)
            if dump_save_path:
                shutil.move(d, dump_save_path)
                log.info("Saved dump as %s", os.path.join(dump_save_path,
                                                          os.path.basename(d)))
            else:
                os.remove(d)
            extra = os.path.splitext(d)[0] + ".extra"
            if os.path.exists(extra):
                os.remove(extra)
            found_crash = True
    finally:
        if remove_symbols:
            shutil.rmtree(symbols_path)

    return found_crash
