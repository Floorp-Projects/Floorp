# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

__all__ = ['check_for_crashes',
           'check_for_java_exception']

import glob
import mozlog
import os
import re
import shutil
import subprocess
import sys
import tempfile
import urllib2
import zipfile

from mozfile import extract_zip
from mozfile import is_url


def check_for_crashes(dump_directory, symbols_path,
                      stackwalk_binary=None,
                      dump_save_path=None,
                      test_name=None,
                      quiet=False):
    """
    Print a stack trace for minidump files left behind by a crashing program.

    `dump_directory` will be searched for minidump files. Any minidump files found will
    have `stackwalk_binary` executed on them, with `symbols_path` passed as an extra
    argument.

    `stackwalk_binary` should be a path to the minidump_stackwalk binary.
    If `stackwalk_binary` is not set, the MINIDUMP_STACKWALK environment variable
    will be checked and its value used if it is not empty.

    `symbols_path` should be a path to a directory containing symbols to use for
    dump processing. This can either be a path to a directory containing Breakpad-format
    symbols, or a URL to a zip file containing a set of symbols.

    If `dump_save_path` is set, it should be a path to a directory in which to copy minidump
    files for safekeeping after a stack trace has been printed. If not set, the environment
    variable MINIDUMP_SAVE_PATH will be checked and its value used if it is not empty.

    If `test_name` is set it will be used as the test name in log output. If not set the
    filename of the calling function will be used.

    If `quiet` is set, no PROCESS-CRASH message will be printed to stdout if a
    crash is detected.

    Returns True if any minidumps were found, False otherwise.
    """
    dumps = glob.glob(os.path.join(dump_directory, '*.dmp'))
    if not dumps:
        return False

    if stackwalk_binary is None:
        stackwalk_binary = os.environ.get('MINIDUMP_STACKWALK', None)

    # try to get the caller's filename if no test name is given
    if test_name is None:
        try:
            test_name = os.path.basename(sys._getframe(1).f_code.co_filename)
        except:
            test_name = "unknown"

    try:
        log = mozlog.getLogger('mozcrash')
        remove_symbols = False
        # If our symbols are at a remote URL, download them now
        # We want to download URLs like http://... but not Windows paths like c:\...
        if symbols_path and is_url(symbols_path):
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
            extract_zip(zfile, symbols_path)
            zfile.close()

        for d in dumps:
            stackwalk_output = []
            stackwalk_output.append("Crash dump filename: " + d)
            top_frame = None
            if symbols_path and stackwalk_binary and os.path.exists(stackwalk_binary):
                # run minidump_stackwalk
                p = subprocess.Popen([stackwalk_binary, d, symbols_path],
                                     stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE)
                (out, err) = p.communicate()
                if len(out) > 3:
                    # minidump_stackwalk is chatty,
                    # so ignore stderr when it succeeds.
                    stackwalk_output.append(out)
                    # The top frame of the crash is always the line after "Thread N (crashed)"
                    # Examples:
                    #  0  libc.so + 0xa888
                    #  0  libnss3.so!nssCertificate_Destroy [certificate.c : 102 + 0x0]
                    #  0  mozjs.dll!js::GlobalObject::getDebuggers() [GlobalObject.cpp:89df18f9b6da : 580 + 0x0]
                    #  0  libxul.so!void js::gc::MarkInternal<JSObject>(JSTracer*, JSObject**) [Marking.cpp : 92 + 0x28]
                    lines = out.splitlines()
                    for i, line in enumerate(lines):
                        if "(crashed)" in line:
                            match = re.search(r"^ 0  (?:.*!)?(?:void )?([^\[]+)", lines[i+1])
                            if match:
                                top_frame = "@ %s" % match.group(1).strip()
                            break
                else:
                    stackwalk_output.append("stderr from minidump_stackwalk:")
                    stackwalk_output.append(err)
                if p.returncode != 0:
                    stackwalk_output.append("minidump_stackwalk exited with return code %d" % p.returncode)
            else:
                if not symbols_path:
                    stackwalk_output.append("No symbols path given, can't process dump.")
                if not stackwalk_binary:
                    stackwalk_output.append("MINIDUMP_STACKWALK not set, can't process dump.")
                elif stackwalk_binary and not os.path.exists(stackwalk_binary):
                    stackwalk_output.append("MINIDUMP_STACKWALK binary not found: %s" % stackwalk_binary)
            if not top_frame:
                top_frame = "Unknown top frame"
            if not quiet:
                print "PROCESS-CRASH | %s | application crashed [%s]" % (test_name, top_frame)
                print '\n'.join(stackwalk_output)
            if dump_save_path is None:
                dump_save_path = os.environ.get('MINIDUMP_SAVE_PATH', None)
            if dump_save_path:
                # This code did not previously create the directory,
                # so there may be a file hanging out with its name.
                if os.path.isfile(dump_save_path):
                    os.unlink(dump_save_path)
                if not os.path.isdir(dump_save_path):
                    try:
                        os.makedirs(dump_save_path)
                    except OSError:
                        pass
                shutil.move(d, dump_save_path)
                log.info("Saved dump as %s", os.path.join(dump_save_path,
                                                          os.path.basename(d)))
            else:
                os.remove(d)
            extra = os.path.splitext(d)[0] + ".extra"
            if os.path.exists(extra):
                os.remove(extra)
    finally:
        if remove_symbols:
            shutil.rmtree(symbols_path)

    return True


def check_for_java_exception(logcat):
    """
    Print a summary of a fatal Java exception, if present in the provided
    logcat output.

    Example:
    PROCESS-CRASH | java-exception | java.lang.NullPointerException at org.mozilla.gecko.GeckoApp$21.run(GeckoApp.java:1833)

    `logcat` should be a list of strings.

    Returns True if a fatal Java exception was found, False otherwise.
    """
    found_exception = False

    for i, line in enumerate(logcat):
        # Logs will be of form:
        #
        # 01-30 20:15:41.937 E/GeckoAppShell( 1703): >>> REPORTING UNCAUGHT EXCEPTION FROM THREAD 9 ("GeckoBackgroundThread")
        # 01-30 20:15:41.937 E/GeckoAppShell( 1703): java.lang.NullPointerException
        # 01-30 20:15:41.937 E/GeckoAppShell( 1703): 	at org.mozilla.gecko.GeckoApp$21.run(GeckoApp.java:1833)
        # 01-30 20:15:41.937 E/GeckoAppShell( 1703): 	at android.os.Handler.handleCallback(Handler.java:587)
        if "REPORTING UNCAUGHT EXCEPTION" in line or "FATAL EXCEPTION" in line:
            # Strip away the date, time, logcat tag and pid from the next two lines and
            # concatenate the remainder to form a concise summary of the exception.
            found_exception = True
            if len(logcat) >= i + 3:
                logre = re.compile(r".*\): \t?(.*)")
                m = logre.search(logcat[i+1])
                if m and m.group(1):
                    exception_type = m.group(1)
                m = logre.search(logcat[i+2])
                if m and m.group(1):
                    exception_location = m.group(1)
                print "PROCESS-CRASH | java-exception | %s %s" % (exception_type, exception_location)
            else:
                print "Automation Error: Logcat is truncated!"
            break

    return found_exception
