# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

__all__ = ['check_for_crashes',
           'check_for_java_exception']

import glob
import os
import re
import shutil
import subprocess
import sys
import tempfile
import urllib2
import zipfile
from collections import namedtuple

import mozfile
import mozlog
from mozlog.structured import structuredlog


StackInfo = namedtuple("StackInfo",
                       ["minidump_path",
                        "signature",
                        "stackwalk_stdout",
                        "stackwalk_stderr",
                        "stackwalk_retcode",
                        "stackwalk_errors",
                        "extra"])


def get_logger():
    structured_logger = structuredlog.get_default_logger("mozcrash")
    if structured_logger is None:
        return mozlog.getLogger('mozcrash')
    return structured_logger


def check_for_crashes(dump_directory,
                      symbols_path,
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

    # try to get the caller's filename if no test name is given
    if test_name is None:
        try:
            test_name = os.path.basename(sys._getframe(1).f_code.co_filename)
        except:
            test_name = "unknown"

    crash_info = CrashInfo(dump_directory, symbols_path, dump_save_path=dump_save_path,
                           stackwalk_binary=stackwalk_binary)

    if not crash_info.has_dumps:
        return False

    for info in crash_info:
        if not quiet:
            stackwalk_output = ["Crash dump filename: %s" % info.minidump_path]
            if info.stackwalk_stderr:
                stackwalk_output.append("stderr from minidump_stackwalk:")
                stackwalk_output.append(info.stackwalk_stderr)
            elif info.stackwalk_stdout is not None:
                stackwalk_output.append(info.stackwalk_stdout)
            if info.stackwalk_retcode is not None and info.stackwalk_retcode != 0:
                stackwalk_output.append("minidump_stackwalk exited with return code %d" %
                                        info.stackwalk_retcode)
            signature = info.signature if info.signature else "unknown top frame"
            print "PROCESS-CRASH | %s | application crashed [%s]" % (test_name,
                                                                     signature)
            print '\n'.join(stackwalk_output)
            print '\n'.join(info.stackwalk_errors)

    return True


def log_crashes(logger,
                dump_directory,
                symbols_path,
                process=None,
                test=None,
                stackwalk_binary=None,
                dump_save_path=None):
    """Log crashes using a structured logger"""
    for info in CrashInfo(dump_directory, symbols_path, dump_save_path=dump_save_path,
                          stackwalk_binary=stackwalk_binary):
        kwargs = info._asdict()
        kwargs.pop("extra")
        logger.crash(process=process, test=test, **kwargs)


class CrashInfo(object):
    """Get information about a crash based on dump files.

    Typical usage is to iterate over the CrashInfo object. This returns StackInfo
    objects, one for each crash dump file that is found in the dump_directory.

    :param dump_directory: Path to search for minidump files
    :param symbols_path: Path to a path to a directory containing symbols to use for
                         dump processing. This can either be a path to a directory
                         containing Breakpad-format symbols, or a URL to a zip file
                         containing a set of symbols.
    :param dump_save_path: Path to which to save the dump files. If this is None,
                           the MINIDUMP_SAVE_PATH environment variable will be used.
    :param stackwalk_binary: Path to the minidump_stackwalk binary. If this is None,
                             the MINIDUMP_STACKWALK environment variable will be used
                             as the path to the minidump binary."""

    def __init__(self, dump_directory, symbols_path, dump_save_path=None,
                 stackwalk_binary=None):
        self.dump_directory = dump_directory
        self.symbols_path = symbols_path
        self.remove_symbols = False

        if dump_save_path is None:
            dump_save_path = os.environ.get('MINIDUMP_SAVE_PATH', None)
        self.dump_save_path = dump_save_path

        if stackwalk_binary is None:
            stackwalk_binary = os.environ.get('MINIDUMP_STACKWALK', None)
        self.stackwalk_binary = stackwalk_binary

        self.logger = get_logger()
        self._dump_files = None

    def _get_symbols(self):
        # This updates self.symbols_path so we only download once
        if self.symbols_path and mozfile.is_url(self.symbols_path):
            self.remove_symbols = True
            self.logger.info("Downloading symbols from: %s" % self.symbols_path)
            # Get the symbols and write them to a temporary zipfile
            data = urllib2.urlopen(self.symbols_path)
            with tempfile.TemporaryFile() as symbols_file:
                symbols_file.write(data.read())
                # extract symbols to a temporary directory (which we'll delete after
                # processing all crashes)
                self.symbols_path = tempfile.mkdtemp()
                with zipfile.ZipFile(symbols_file, 'r') as zfile:
                    mozfile.extract_zip(zfile, self.symbols_path)

    @property
    def dump_files(self):
        """List of tuple (path_to_dump_file, path_to_extra_file) for each dump
           file in self.dump_directory. The extra files may not exist."""
        if self._dump_files is None:
            self._dump_files = [(path, os.path.splitext(path)[0] + '.extra') for path in
                                glob.glob(os.path.join(self.dump_directory, '*.dmp'))]
        return self._dump_files

    @property
    def has_dumps(self):
        """Boolean indicating whether any crash dump files were found in the
        current directory"""
        return len(self.dump_files) > 0

    def __iter__(self):
        for path, extra in self.dump_files:
            rv = self._process_dump_file(path, extra)
            yield rv

        if self.remove_symbols:
            mozfile.remove(self.symbols_path)

    def _process_dump_file(self, path, extra):
        """Process a single dump file using self.stackwalk_binary, and return a
        tuple containing properties of the crash dump.

        :param path: Path to the minidump file to analyse
        :return: A StackInfo tuple with the fields::
                   minidump_path: Path of the dump file
                   signature: The top frame of the stack trace, or None if it
                              could not be determined.
                   stackwalk_stdout: String of stdout data from stackwalk
                   stackwalk_stderr: String of stderr data from stackwalk or
                                     None if it succeeded
                   stackwalk_retcode: Return code from stackwalk
                   stackwalk_errors: List of errors in human-readable form that prevented
                                     stackwalk being launched.
        """
        self._get_symbols()

        errors = []
        signature = None
        include_stderr = False
        out = None
        err = None
        retcode = None
        if (self.symbols_path and self.stackwalk_binary and
            os.path.exists(self.stackwalk_binary)):
            # run minidump_stackwalk
            p = subprocess.Popen([self.stackwalk_binary, path, self.symbols_path],
                                 stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE)
            (out, err) = p.communicate()
            retcode = p.returncode
            if len(out) > 3:
                # minidump_stackwalk is chatty,
                # so ignore stderr when it succeeds.
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
                            signature = "@ %s" % match.group(1).strip()
                        break
            else:
                include_stderr = True
        else:
            if not self.symbols_path:
                errors.append("No symbols path given, can't process dump.")
            if not self.stackwalk_binary:
                errors.append("MINIDUMP_STACKWALK not set, can't process dump.")
            elif self.stackwalk_binary and not os.path.exists(self.stackwalk_binary):
                errors.append("MINIDUMP_STACKWALK binary not found: %s" % self.stackwalk_binary)

        if self.dump_save_path:
            self._save_dump_file(path, extra)

        if os.path.exists(path):
            mozfile.remove(path)
        if os.path.exists(extra):
            mozfile.remove(extra)

        return StackInfo(path,
                         signature,
                         out,
                         err if include_stderr else None,
                         retcode,
                         errors,
                         extra)

    def _save_dump_file(self, path, extra):
        if os.path.isfile(self.dump_save_path):
            os.unlink(self.dump_save_path)
        if not os.path.isdir(self.dump_save_path):
            try:
                os.makedirs(self.dump_save_path)
            except OSError:
                pass

        shutil.move(path, self.dump_save_path)
        self.logger.info("Saved minidump as %s" %
                         os.path.join(self.dump_save_path, os.path.basename(path)))

        if os.path.isfile(extra):
            shutil.move(extra, self.dump_save_path)
            self.logger.info("Saved app info as %s" %
                             os.path.join(self.dump_save_path, os.path.basename(extra)))


def check_for_java_exception(logcat, quiet=False):
    """
    Print a summary of a fatal Java exception, if present in the provided
    logcat output.

    Example:
    PROCESS-CRASH | java-exception | java.lang.NullPointerException at org.mozilla.gecko.GeckoApp$21.run(GeckoApp.java:1833)

    `logcat` should be a list of strings.

    If `quiet` is set, no PROCESS-CRASH message will be printed to stdout if a
    crash is detected.

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
                if not quiet:
                    print "PROCESS-CRASH | java-exception | %s %s" % (exception_type, exception_location)
            else:
                print "Automation Error: Logcat is truncated!"
            break

    return found_exception
