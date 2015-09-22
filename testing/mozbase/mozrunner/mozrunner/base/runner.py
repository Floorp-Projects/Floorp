#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from abc import ABCMeta, abstractproperty
import os
import subprocess
import traceback

from mozlog import get_default_logger
from mozprocess import ProcessHandler
try:
    import mozcrash
except ImportError:
    mozcrash = None

from ..application import DefaultContext
from ..errors import RunnerNotStartedError


class BaseRunner(object):
    """
    The base runner class for all mozrunner objects, both local and remote.
    """
    __metaclass__ = ABCMeta
    last_test = 'mozrunner-startup'
    process_handler = None
    timeout = None
    output_timeout = None

    def __init__(self, app_ctx=None, profile=None, clean_profile=True, env=None,
                 process_class=None, process_args=None, symbols_path=None,
                 dump_save_path=None, addons=None):
        self.app_ctx = app_ctx or DefaultContext()

        if isinstance(profile, basestring):
            self.profile = self.app_ctx.profile_class(profile=profile,
                                                      addons=addons)
        else:
            self.profile = profile or self.app_ctx.profile_class(**getattr(self.app_ctx, 'profile_args', {}))

        # process environment
        if env is None:
            self.env = os.environ.copy()
        else:
            self.env = env.copy()

        self.clean_profile = clean_profile
        self.process_class = process_class or ProcessHandler
        self.process_args = process_args or {}
        self.symbols_path = symbols_path
        self.dump_save_path = dump_save_path

        self.crashed = 0

    def __del__(self):
        self.cleanup()

    @abstractproperty
    def command(self):
        """Returns the command list to run."""
        pass

    @property
    def returncode(self):
        """
        The returncode of the process_handler. A value of None
        indicates the process is still running. A negative
        value indicates the process was killed with the
        specified signal.

        :raises: RunnerNotStartedError
        """
        if self.process_handler:
            return self.process_handler.poll()
        else:
            raise RunnerNotStartedError("returncode accessed before runner started")

    def start(self, debug_args=None, interactive=False, timeout=None, outputTimeout=None):
        """
        Run self.command in the proper environment.

        :param debug_args: arguments for a debugger
        :param interactive: uses subprocess.Popen directly
        :param timeout: see process_handler.run()
        :param outputTimeout: see process_handler.run()
        :returns: the process id
        """
        self.timeout = timeout
        self.output_timeout = outputTimeout
        cmd = self.command

        # ensure the runner is stopped
        self.stop()

        # attach a debugger, if specified
        if debug_args:
            cmd = list(debug_args) + cmd

        if interactive:
            self.process_handler = subprocess.Popen(cmd, env=self.env)
            # TODO: other arguments
        else:
            # this run uses the managed processhandler
            self.process_handler = self.process_class(cmd, env=self.env, **self.process_args)
            self.process_handler.run(self.timeout, self.output_timeout)

        self.crashed = 0
        return self.process_handler.pid

    def wait(self, timeout=None):
        """
        Wait for the process to exit.

        :param timeout: if not None, will return after timeout seconds.
                        Timeout is ignored if interactive was set to True.
        :returns: the process return code if process exited normally,
                  -<signal> if process was killed (Unix only),
                  None if timeout was reached and the process is still running.
        :raises: RunnerNotStartedError
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
        """
        Checks if the process is running.

        :returns: True if the process is active
        """
        return self.returncode is None

    def stop(self, sig=None):
        """
        Kill the process.

        :param sig: Signal used to kill the process, defaults to SIGKILL
                    (has no effect on Windows).
        :returns: the process return code if process was already stopped,
                  -<signal> if process was killed (Unix only)
        :raises: RunnerNotStartedError
        """
        try:
            if not self.is_running():
                return self.returncode
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
        """
        Reset the runner to its default state.
        """
        self.stop()
        self.process_handler = None

    def check_for_crashes(self, dump_directory=None, dump_save_path=None,
                          test_name=None, quiet=False):
        """
        Check for a possible crash and output stack trace.

        :param dump_directory: Directory to search for minidump files
        :param dump_save_path: Directory to save the minidump files to
        :param test_name: Name to use in the crash output
        :param quiet: If `True` don't print the PROCESS-CRASH message to stdout
        :returns: True if a crash was detected, otherwise False
        """
        if not dump_directory:
            dump_directory = os.path.join(self.profile.profile, 'minidumps')

        if not dump_save_path:
            dump_save_path = self.dump_save_path

        try:
            logger = get_default_logger()
            if logger is not None:
                if test_name is None:
                    test_name = "runner.py"
                if mozcrash:
                    self.crashed += mozcrash.log_crashes(
                        logger,
                        dump_directory,
                        self.symbols_path,
                        dump_save_path=dump_save_path,
                        test=test_name)
                else:
                    logger.warning("Can not log crashes without mozcrash")
            else:
                if mozcrash:
                    crashed = mozcrash.check_for_crashes(
                        dump_directory,
                        self.symbols_path,
                        dump_save_path=dump_save_path,
                        test_name=test_name,
                        quiet=quiet)
                    if crashed:
                        self.crashed += 1
                else:
                    logger.warning("Can not log crashes without mozcrash")
        except:
            traceback.print_exc()

        return self.crashed

    def cleanup(self):
        """
        Cleanup all runner state
        """
        self.stop()
