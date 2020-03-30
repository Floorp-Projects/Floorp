# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import glob
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import zipfile
from collections import namedtuple
from six import string_types, text_type
from six.moves.urllib.request import urlopen

import mozfile
import mozinfo
import mozlog

__all__ = [
    'check_for_crashes',
    'check_for_java_exception',
    'kill_and_get_minidump',
    'log_crashes',
    'cleanup_pending_crash_reports',
]


StackInfo = namedtuple("StackInfo",
                       ["minidump_path",
                        "signature",
                        "stackwalk_stdout",
                        "stackwalk_stderr",
                        "stackwalk_retcode",
                        "stackwalk_errors",
                        "extra",
                        "reason",
                        "java_stack"])


def get_logger():
    structured_logger = mozlog.get_default_logger("mozcrash")
    if structured_logger is None:
        return mozlog.unstructured.getLogger('mozcrash')
    return structured_logger


def check_for_crashes(dump_directory,
                      symbols_path=None,
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

    Returns number of minidump files found.
    """

    # try to get the caller's filename if no test name is given
    if test_name is None:
        try:
            test_name = os.path.basename(sys._getframe(1).f_code.co_filename)
        except Exception:
            test_name = "unknown"

    if not quiet:
        print("mozcrash checking %s for minidumps..." % dump_directory)

    crash_info = CrashInfo(dump_directory, symbols_path, dump_save_path=dump_save_path,
                           stackwalk_binary=stackwalk_binary)

    crash_count = 0
    for info in crash_info:
        crash_count += 1
        output = None
        if info.java_stack:
            output = u"PROCESS-CRASH | {name} | {stack}".format(
                name=test_name, stack=info.java_stack)
        elif not quiet:
            stackwalk_output = [u"Crash dump filename: {}".format(info.minidump_path)]
            if info.reason:
                stackwalk_output.append("Mozilla crash reason: %s" % info.reason)
            if info.stackwalk_stderr:
                stackwalk_output.append("stderr from minidump_stackwalk:")
                stackwalk_output.append(info.stackwalk_stderr)
            elif info.stackwalk_stdout is not None:
                stackwalk_output.append(info.stackwalk_stdout)
            if info.stackwalk_retcode is not None and info.stackwalk_retcode != 0:
                stackwalk_output.append("minidump_stackwalk exited with return code {}".format(
                                        info.stackwalk_retcode))
            signature = info.signature if info.signature else "unknown top frame"

            output = u"PROCESS-CRASH | {name} | application crashed [{sig}]\n{out}\n{err}".format(
                name=test_name,
                sig=signature,
                out="\n".join(stackwalk_output),
                err="\n".join(info.stackwalk_errors))
        if output is not None:
            if sys.stdout.encoding != 'UTF-8':
                output = output.encode('utf-8')
            print(output)

    return crash_count


def log_crashes(logger,
                dump_directory,
                symbols_path,
                process=None,
                test=None,
                stackwalk_binary=None,
                dump_save_path=None):
    """Log crashes using a structured logger"""
    crash_count = 0
    for info in CrashInfo(dump_directory, symbols_path, dump_save_path=dump_save_path,
                          stackwalk_binary=stackwalk_binary):
        crash_count += 1
        kwargs = info._asdict()
        kwargs.pop("extra")
        logger.crash(process=process, test=test, **kwargs)
    return crash_count


# Function signatures of abort functions which should be ignored when
# determining the appropriate frame for the crash signature.
ABORT_SIGNATURES = (
    "Abort(char const*)",
    "RustMozCrash",
    "NS_DebugBreak",
    # This signature is part of Rust panic stacks on some platforms. On
    # others, it includes a template parameter containing "core::panic::" and
    # is automatically filtered out by that pattern.
    "core::ops::function::Fn::call",
    "gkrust_shared::panic_hook",
    "intentional_panic",
    "mozalloc_abort",
    "mozalloc_abort(char const* const)",
    "static void Abort(const char *)",
)

# Similar to above, but matches if the substring appears anywhere in the
# frame's signature.
ABORT_SUBSTRINGS = (
    # On some platforms, Rust panic frames unfortunately appear without the
    # std::panicking or core::panic namespaces.
    "_panic_",
    "core::panic::",
    "core::result::unwrap_failed",
    "std::panicking::",
)


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
        # If no symbols path has been set create a temporary folder to let the
        # minidump stackwalk download the symbols.
        if not self.symbols_path:
            self.symbols_path = tempfile.mkdtemp()
            self.remove_symbols = True

        # This updates self.symbols_path so we only download once.
        if mozfile.is_url(self.symbols_path):
            self.remove_symbols = True
            self.logger.info("Downloading symbols from: %s" % self.symbols_path)
            # Get the symbols and write them to a temporary zipfile
            data = urlopen(self.symbols_path)
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
                                reversed(sorted(glob.glob(os.path.join(self.dump_directory,
                                                                       '*.dmp'))))]
            max_dumps = 10
            if len(self._dump_files) > max_dumps:
                self.logger.warning("Found %d dump files -- limited to %d!" %
                                    (len(self._dump_files), max_dumps))
                del self._dump_files[max_dumps:]

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
        reason = None
        java_stack = None
        if (self.symbols_path and self.stackwalk_binary and
            os.path.exists(self.stackwalk_binary) and
                os.access(self.stackwalk_binary, os.X_OK)):

            command = [
                self.stackwalk_binary,
                path,
                self.symbols_path
            ]
            self.logger.info(u"Copy/paste: {}".format(' '.join(command)))
            # run minidump_stackwalk
            p = subprocess.Popen(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
            )
            (out, err) = p.communicate()
            retcode = p.returncode

            if len(out) > 3:
                # minidump_stackwalk is chatty,
                # so ignore stderr when it succeeds.
                # The top frame of the crash is always the line after "Thread N (crashed)"
                # Examples:
                #  0  libc.so + 0xa888
                #  0  libnss3.so!nssCertificate_Destroy [certificate.c : 102 + 0x0]
                #  0  mozjs.dll!js::GlobalObject::getDebuggers() [GlobalObject.cpp:89df18f9b6da : 580 + 0x0] # noqa
                # 0  libxul.so!void js::gc::MarkInternal<JSObject>(JSTracer*, JSObject**)
                # [Marking.cpp : 92 + 0x28]
                lines = out.splitlines()
                for i, line in enumerate(lines):
                    if "(crashed)" in line:
                        # Try to find the first frame that isn't an abort
                        # function to use as the signature.
                        for line in lines[i + 1:]:
                            if not line.startswith(" "):
                                break

                            match = re.search(r"^ \d  (?:.*!)?(?:void )?([^\[]+)", line)
                            if match:
                                func = match.group(1).strip()
                                signature = "@ %s" % func

                                if not (func in ABORT_SIGNATURES or
                                        any(pat in func
                                            for pat in ABORT_SUBSTRINGS)):
                                    break
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
            elif not os.access(self.stackwalk_binary, os.X_OK):
                errors.append('This user cannot execute the MINIDUMP_STACKWALK binary.')

        if os.path.exists(extra):
            crash_dict = self._parse_extra_file(extra)
            reason = crash_dict.get("MozCrashReason")
            java_stack = crash_dict.get("JavaStackTrace")

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
                         extra,
                         reason,
                         java_stack)

    def _parse_extra_file(self, path):
        with open(path) as file:
            try:
                return json.load(file)
            except ValueError:
                self.logger.warning(".extra file does not contain proper json")
                return {}

    def _save_dump_file(self, path, extra):
        if os.path.isfile(self.dump_save_path):
            os.unlink(self.dump_save_path)
        if not os.path.isdir(self.dump_save_path):
            try:
                os.makedirs(self.dump_save_path)
            except OSError:
                pass

        shutil.move(path, self.dump_save_path)
        self.logger.info(u"Saved minidump as {}".format(
            os.path.join(self.dump_save_path, os.path.basename(path))
        ))

        if os.path.isfile(extra):
            shutil.move(extra, self.dump_save_path)
            self.logger.info(u"Saved app info as {}".format(
                os.path.join(self.dump_save_path, os.path.basename(extra))
            ))


def check_for_java_exception(logcat, test_name=None, quiet=False):
    """
    Print a summary of a fatal Java exception, if present in the provided
    logcat output.

    Today, exceptions in geckoview are usually noted in the minidump .extra file, allowing
    java exceptions to be reported by the "normal" minidump processing, like log_crashes();
    therefore, this function may be extraneous (but maintained for now, while exception
    handling is evolving).

    Example:
    PROCESS-CRASH | <test-name> | java-exception java.lang.NullPointerException at org.mozilla.gecko.GeckoApp$21.run(GeckoApp.java:1833) # noqa

    `logcat` should be a list of strings.

    If `test_name` is set it will be used as the test name in log output. If not set the
    filename of the calling function will be used.

    If `quiet` is set, no PROCESS-CRASH message will be printed to stdout if a
    crash is detected.

    Returns True if a fatal Java exception was found, False otherwise.
    """

    # try to get the caller's filename if no test name is given
    if test_name is None:
        try:
            test_name = os.path.basename(sys._getframe(1).f_code.co_filename)
        except Exception:
            test_name = "unknown"

    found_exception = False

    for i, line in enumerate(logcat):
        # Logs will be of form:
        #
        # 01-30 20:15:41.937 E/GeckoAppShell( 1703): >>> REPORTING UNCAUGHT EXCEPTION FROM THREAD 9 ("GeckoBackgroundThread") # noqa
        # 01-30 20:15:41.937 E/GeckoAppShell( 1703): java.lang.NullPointerException
        # 01-30 20:15:41.937 E/GeckoAppShell( 1703): at org.mozilla.gecko.GeckoApp$21.run(GeckoApp.java:1833) # noqa
        # 01-30 20:15:41.937 E/GeckoAppShell( 1703): at android.os.Handler.handleCallback(Handler.java:587) # noqa
        if "REPORTING UNCAUGHT EXCEPTION" in line:
            # Strip away the date, time, logcat tag and pid from the next two lines and
            # concatenate the remainder to form a concise summary of the exception.
            found_exception = True
            if len(logcat) >= i + 3:
                logre = re.compile(r".*\): \t?(.*)")
                m = logre.search(logcat[i + 1])
                if m and m.group(1):
                    exception_type = m.group(1)
                m = logre.search(logcat[i + 2])
                if m and m.group(1):
                    exception_location = m.group(1)
                if not quiet:
                    output = u"PROCESS-CRASH | {name} | java-exception {type} {loc}".format(
                        name=test_name,
                        type=exception_type,
                        loc=exception_location
                    )
                    print(output.encode("utf-8"))
            else:
                print(u"Automation Error: java exception in logcat at line "
                      "{0} of {1}: {2}".format(i, len(logcat), line))
            break

    return found_exception


if mozinfo.isWin:
    import ctypes
    import uuid

    kernel32 = ctypes.windll.kernel32
    OpenProcess = kernel32.OpenProcess
    CloseHandle = kernel32.CloseHandle

    def write_minidump(pid, dump_directory, utility_path):
        """
        Write a minidump for a process.

        :param pid: PID of the process to write a minidump for.
        :param dump_directory: Directory in which to write the minidump.
        """
        PROCESS_QUERY_INFORMATION = 0x0400
        PROCESS_VM_READ = 0x0010
        GENERIC_READ = 0x80000000
        GENERIC_WRITE = 0x40000000
        CREATE_ALWAYS = 2
        FILE_ATTRIBUTE_NORMAL = 0x80
        INVALID_HANDLE_VALUE = -1

        file_name = os.path.join(dump_directory,
                                 str(uuid.uuid4()) + ".dmp")

        if (mozinfo.info['bits'] != ctypes.sizeof(ctypes.c_voidp) * 8 and
                utility_path):
            # We're not going to be able to write a minidump with ctypes if our
            # python process was compiled for a different architecture than
            # firefox, so we invoke the minidumpwriter utility program.

            log = get_logger()
            minidumpwriter = os.path.normpath(os.path.join(utility_path,
                                                           "minidumpwriter.exe"))
            log.info(u"Using {} to write a dump to {} for [{}]".format(
                minidumpwriter, file_name, pid))
            if not os.path.exists(minidumpwriter):
                log.error(u"minidumpwriter not found in {}".format(utility_path))
                return

            if isinstance(file_name, string_types):
                # Convert to a byte string before sending to the shell.
                file_name = file_name.encode(sys.getfilesystemencoding())

            status = subprocess.Popen([minidumpwriter, str(pid), file_name]).wait()
            if status:
                log.error("minidumpwriter exited with status: %d" % status)
            return

        proc_handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                  0, pid)
        if not proc_handle:
            return

        if not isinstance(file_name, string_types):
            # Convert to unicode explicitly so our path will be valid as input
            # to CreateFileW
            file_name = text_type(file_name, sys.getfilesystemencoding())

        file_handle = kernel32.CreateFileW(file_name,
                                           GENERIC_READ | GENERIC_WRITE,
                                           0,
                                           None,
                                           CREATE_ALWAYS,
                                           FILE_ATTRIBUTE_NORMAL,
                                           None)
        if file_handle != INVALID_HANDLE_VALUE:
            ctypes.windll.dbghelp.MiniDumpWriteDump(proc_handle,
                                                    pid,
                                                    file_handle,
                                                    # Dump type - MiniDumpNormal
                                                    0,
                                                    # Exception parameter
                                                    None,
                                                    # User stream parameter
                                                    None,
                                                    # Callback parameter
                                                    None)
            CloseHandle(file_handle)
        CloseHandle(proc_handle)

    def kill_pid(pid):
        """
        Terminate a process with extreme prejudice.

        :param pid: PID of the process to terminate.
        """
        PROCESS_TERMINATE = 0x0001
        SYNCHRONIZE = 0x00100000
        WAIT_OBJECT_0 = 0x0
        WAIT_FAILED = -1
        logger = get_logger()
        handle = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, 0, pid)
        if handle:
            if kernel32.TerminateProcess(handle, 1):
                # TerminateProcess is async; wait up to 30 seconds for process to
                # actually terminate, then give up so that clients are not kept
                # waiting indefinitely for hung processes.
                status = kernel32.WaitForSingleObject(handle, 30000)
                if status == WAIT_FAILED:
                    err = kernel32.GetLastError()
                    logger.warning("kill_pid(): wait failed (%d) terminating pid %d: error %d" %
                                   (status, pid, err))
                elif status != WAIT_OBJECT_0:
                    logger.warning("kill_pid(): wait failed (%d) terminating pid %d" %
                                   (status, pid))
            else:
                err = kernel32.GetLastError()
                logger.warning("kill_pid(): unable to terminate pid %d: %d" %
                               (pid, err))
            CloseHandle(handle)
        else:
            err = kernel32.GetLastError()
            logger.warning("kill_pid(): unable to get handle for pid %d: %d" %
                           (pid, err))
else:
    def kill_pid(pid):
        """
        Terminate a process with extreme prejudice.

        :param pid: PID of the process to terminate.
        """
        os.kill(pid, signal.SIGKILL)


def kill_and_get_minidump(pid, dump_directory, utility_path=None):
    """
    Attempt to kill a process and leave behind a minidump describing its
    execution state.

    :param pid: The PID of the process to kill.
    :param dump_directory: The directory where a minidump should be written on
    Windows, where the dump will be written from outside the process.

    On Windows a dump will be written using the MiniDumpWriteDump function
    from DbgHelp.dll. On Linux and OS X the process will be sent a SIGABRT
    signal to trigger minidump writing via a Breakpad signal handler. On other
    platforms the process will simply be killed via SIGKILL.

    If the process is hung in such a way that it cannot respond to SIGABRT
    it may still be running after this function returns. In that case it
    is the caller's responsibility to deal with killing it.
    """
    needs_killing = True
    if mozinfo.isWin:
        write_minidump(pid, dump_directory, utility_path)
    elif mozinfo.isLinux or mozinfo.isMac:
        os.kill(pid, signal.SIGABRT)
        needs_killing = False
    if needs_killing:
        kill_pid(pid)


def cleanup_pending_crash_reports():
    """
    Delete any pending crash reports.

    The presence of pending crash reports may be reported by the browser,
    affecting test results; it is best to ensure that these are removed
    before starting any browser tests.

    Firefox stores pending crash reports in "<UAppData>/Crash Reports".
    If the browser is not running, it cannot provide <UAppData>, so this
    code tries to anticipate its value.

    See dom/system/OSFileConstants.cpp for platform variations of <UAppData>.
    """
    if mozinfo.isWin:
        location = os.path.expanduser("~\\AppData\\Roaming\\Mozilla\\Firefox\\Crash Reports")
    elif mozinfo.isMac:
        location = os.path.expanduser("~/Library/Application Support/firefox/Crash Reports")
    else:
        location = os.path.expanduser("~/.mozilla/firefox/Crash Reports")
    logger = get_logger()
    if os.path.exists(location):
        try:
            mozfile.remove(location)
            logger.info("Removed pending crash reports at '%s'" % location)
        except Exception:
            pass


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--stackwalk-binary', '-b')
    parser.add_argument('--dump-save-path', '-o')
    parser.add_argument('--test-name', '-n')
    parser.add_argument('dump_directory')
    parser.add_argument('symbols_path')
    args = parser.parse_args()

    check_for_crashes(args.dump_directory, args.symbols_path,
                      stackwalk_binary=args.stackwalk_binary,
                      dump_save_path=args.dump_save_path,
                      test_name=args.test_name)
