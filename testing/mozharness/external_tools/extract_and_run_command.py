#!/usr/bin/env python
"""\
Usage: extract_and_run_command.py [-j N] [command to run] -- [files and/or directories]
    -j is the number of workers to start, defaulting to 1.
    [command to run] must be a command that can accept one or many files
    to process as arguments.

WARNING: This script does NOT respond to SIGINT. You must use SIGQUIT or SIGKILL to
         terminate it early.
 """

### The canonical location for this file is
###   https://hg.mozilla.org/build/tools/file/default/stage/extract_and_run_command.py
###
### Please update the copy in puppet to deploy new changes to
### stage.mozilla.org, see
# https://wiki.mozilla.org/ReleaseEngineering/How_To/Modify_scripts_on_stage

import logging
import os
from os import path
import sys
from Queue import Queue
import shutil
import subprocess
import tempfile
from threading import Thread
import time

logging.basicConfig(
    stream=sys.stdout, level=logging.INFO, format="%(message)s")
log = logging.getLogger(__name__)

try:
    # the future - https://github.com/mozilla/build-mar via a venv
    from mardor.marfile import BZ2MarFile
except:
    # the past - http://hg.mozilla.org/build/tools/file/default/buildfarm/utils/mar.py
    sys.path.append(
        path.join(path.dirname(path.realpath(__file__)), "../buildfarm/utils"))
    from mar import BZ2MarFile

SEVENZIP = "7za"


def extractMar(filename, tempdir):
    m = BZ2MarFile(filename)
    m.extractall(path=tempdir)


def extractExe(filename, tempdir):
    try:
        # We don't actually care about output, put we redirect to a tempfile
        # to avoid deadlocking in wait() when stdout=PIPE
        fd = tempfile.TemporaryFile()
        proc = subprocess.Popen([SEVENZIP, 'x', '-o%s' % tempdir, filename],
                                stdout=fd, stderr=subprocess.STDOUT)
        proc.wait()
    except subprocess.CalledProcessError:
        # Not all EXEs are 7-zip files, so we have to ignore extraction errors
        pass

# The keys here are matched against the last 3 characters of filenames.
# The values are callables that accept two string arguments.
EXTRACTORS = {
    '.mar': extractMar,
    '.exe': extractExe,
}


def find_files(d):
    """yields all of the files in `d'"""
    for root, dirs, files in os.walk(d):
        for f in files:
            yield path.abspath(path.join(root, f))


def rchmod(d, mode=0755):
    """chmods everything in `d' to `mode', including `d' itself"""
    os.chmod(d, mode)
    for root, dirs, files in os.walk(d):
        for item in dirs:
            os.chmod(path.join(root, item), mode)
        for item in files:
            os.chmod(path.join(root, item), mode)


def maybe_extract(filename):
    """If an extractor is found for `filename', extracts it to a temporary
       directory and chmods it. The consumer is responsible for removing
       the extracted files, if desired."""
    ext = path.splitext(filename)[1]
    if ext not in EXTRACTORS.keys():
        return None
    # Append the full filepath to the tempdir
    tempdir_root = tempfile.mkdtemp()
    tempdir = path.join(tempdir_root, filename.lstrip('/'))
    os.makedirs(tempdir)
    EXTRACTORS[ext](filename, tempdir)
    rchmod(tempdir_root)
    return tempdir_root


def process(item, command):
    def format_time(t):
        return time.strftime("%H:%M:%S", time.localtime(t))
    # Buffer output to avoid interleaving of multiple workers'
    logs = []
    args = [item]
    proc = None
    start = time.time()
    logs.append("START %s: %s" % (format_time(start), item))
    # If the file was extracted, we need to process all of its files, too.
    tempdir = maybe_extract(item)
    if tempdir:
        for f in find_files(tempdir):
            args.append(f)

    try:
        fd = tempfile.TemporaryFile()
        proc = subprocess.Popen(command + args, stdout=fd)
        proc.wait()
        if proc.returncode != 0:
            raise Exception("returned %s" % proc.returncode)
    finally:
        if tempdir:
            shutil.rmtree(tempdir)
        fd.seek(0)
        # rstrip() here to avoid an unnecessary newline, if it exists.
        logs.append(fd.read().rstrip())
        end = time.time()
        elapsed = end - start
        logs.append("END %s (%d seconds elapsed): %s\n" % (
            format_time(end), elapsed, item))
        # Now that we've got all of our output, print it. It's important that
        # the logging module is used for this, because "print" is not
        # thread-safe.
        log.info("\n".join(logs))


def worker(command, errors):
    item = q.get()
    while item != None:
        try:
            process(item, command)
        except:
            errors.put(item)
        item = q.get()

if __name__ == '__main__':
    # getopt is used in favour of optparse to enable "--" as a separator
    # between the command and list of files. optparse doesn't allow that.
    from getopt import getopt
    options, args = getopt(sys.argv[1:], 'j:h', ['help'])

    concurrency = 1
    for o, a in options:
        if o == '-j':
            concurrency = int(a)
        elif o in ('-h', '--help'):
            log.info(__doc__)
            sys.exit(0)

    if len(args) < 3 or '--' not in args:
        log.error(__doc__)
        sys.exit(1)

    command = []
    while args[0] != "--":
        command.append(args.pop(0))
    args.pop(0)

    q = Queue()
    errors = Queue()
    threads = []
    for i in range(concurrency):
        t = Thread(target=worker, args=(command, errors))
        t.start()
        threads.append(t)

    # find_files is a generator, so work will begin prior to it finding
    # all of the files
    for arg in args:
        if path.isfile(arg):
            q.put(arg)
        else:
            for f in find_files(arg):
                q.put(f)
    # Because the workers are started before we start populating the q
    # they can't use .empty() to determine whether or not their done.
    # We also can't use q.join() or j.task_done(), because we need to
    # support Python 2.4. We know that find_files won't yield None,
    # so we can detect doneness by having workers die when they get None
    # as an item.
    for i in range(concurrency):
        q.put(None)

    for t in threads:
        t.join()

    if not errors.empty():
        log.error("Command failed for the following files:")
        while not errors.empty():
            log.error("  %s" % errors.get())
        sys.exit(1)
