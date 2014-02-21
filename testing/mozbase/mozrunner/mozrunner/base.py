#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import subprocess
import traceback

from mozprocess.processhandler import ProcessHandler
import mozcrash
import mozlog

from .errors import RunnerNotStartedError


# we can replace these methods with 'abc'
# (http://docs.python.org/library/abc.html) when we require Python 2.6+
def abstractmethod(method):
    line = method.func_code.co_firstlineno
    filename = method.func_code.co_filename

    def not_implemented(*args, **kwargs):
        raise NotImplementedError('Abstract method %s at File "%s", line %s '
                                  'should be implemented by a concrete class' %
                                  (repr(method), filename, line))
    return not_implemented


class Runner(object):

    def __init__(self, profile, clean_profile=True, process_class=None,
                 kp_kwargs=None, env=None, symbols_path=None):
        self.clean_profile = clean_profile
        self.env = env or {}
        self.kp_kwargs = kp_kwargs or {}
        self.process_class = process_class or ProcessHandler
        self.process_handler = None
        self.profile = profile
        self.log = mozlog.getLogger('MozRunner')
        self.symbols_path = symbols_path

    def __del__(self):
        self.cleanup()

    # Once we can use 'abc' it should become an abstract property
    @property
    def command(self):
        pass

    @property
    def returncode(self):
        if self.process_handler:
            return self.process_handler.poll()
        else:
            raise RunnerNotStartedError("returncode retrieved before process started")

    def start(self, debug_args=None, interactive=False, timeout=None, outputTimeout=None):
        """Run self.command in the proper environment

        returns the process id

        :param debug_args: arguments for the debugger
        :param interactive: uses subprocess.Popen directly
        :param timeout: see process_handler.run()
        :param outputTimeout: see process_handler.run()

        """
        # ensure the runner is stopped
        self.stop()

        # ensure the profile exists
        if not self.profile.exists():
            self.profile.reset()
            assert self.profile.exists(), "%s : failure to reset profile" % self.__class__.__name__

        cmd = self.command

        # attach a debugger, if specified
        if debug_args:
            cmd = list(debug_args) + cmd

        if interactive:
            self.process_handler = subprocess.Popen(cmd, env=self.env)
            # TODO: other arguments
        else:
            # this run uses the managed processhandler
            self.process_handler = self.process_class(cmd, env=self.env, **self.kp_kwargs)
            self.process_handler.run(timeout, outputTimeout)

        return self.process_handler.pid

    def wait(self, timeout=None):
        """Wait for the process to exit

        returns the process return code if the process exited,
        returns -<signal> if the process was killed (Unix only)
        returns None if the process is still running.

        :param timeout: if not None, will return after timeout seconds.
                        Use is_running() to determine whether or not a
                        timeout occured. Timeout is ignored if
                        interactive was set to True.

        """
        if self.is_running():
            # The interactive mode uses directly a Popen process instance. It's
            # wait() method doesn't have any parameters. So handle it separately.
            if isinstance(self.process_handler, subprocess.Popen):
                self.process_handler.wait()
            else:
                self.process_handler.wait(timeout)

        elif not self.process_handler:
            raise RunnerNotStartedError("Wait() called before process started")

        return self.returncode

    def is_running(self):
        """Checks if the process is running

        returns True if the process is active

        """
        return self.returncode is None

    def stop(self, sig=None):
        """Kill the process

        returns -<signal> when the process got killed (Unix only)

        :param sig: Signal used to kill the process, defaults to SIGKILL
                    (has no effect on Windows).

        """
        try:
            if not self.is_running():
                return
        except RunnerNotStartedError:
            return


        # The interactive mode uses directly a Popen process instance. It's
        # kill() method doesn't have any parameters. So handle it separately.
        if isinstance(self.process_handler, subprocess.Popen):
            self.process_handler.kill()
        else:
            self.process_handler.kill(sig=sig)

        return self.returncode

    def reset(self):
        """Reset the runner to its default state"""
        if getattr(self, 'profile', False):
            self.profile.reset()

    def check_for_crashes(self, dump_directory=None, dump_save_path=None,
                          test_name=None, quiet=False):
        """Check for a possible crash and output stack trace

        :param dump_directory: Directory to search for minidump files
        :param dump_save_path: Directory to save the minidump files to
        :param test_name: Name to use in the crash output
        :param quiet: If `True` don't print the PROCESS-CRASH message to stdout

        """
        if not dump_directory:
            dump_directory = os.path.join(self.profile.profile, 'minidumps')

        crashed = False
        try:
            crashed = mozcrash.check_for_crashes(dump_directory,
                                                 self.symbols_path,
                                                 dump_save_path=dump_save_path,
                                                 test_name=test_name,
                                                 quiet=quiet)
        except:
            traceback.print_exc()

        return crashed

    def cleanup(self):
        """Cleanup all runner state"""
        self.stop()

        if getattr(self, 'profile', False) and self.clean_profile:
            self.profile.cleanup()
