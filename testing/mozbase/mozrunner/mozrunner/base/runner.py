#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import six
import os
import subprocess
import sys
import traceback
from abc import ABCMeta, abstractproperty

from mozlog import get_default_logger
from mozprocess import ProcessHandler
from six import string_types, text_type

try:
    import mozcrash
except ImportError:
    mozcrash = None
from six import reraise

from ..application import DefaultContext
from ..errors import RunnerNotStartedError


@six.add_metaclass(ABCMeta)
class BaseRunner(object):
    """
    The base runner class for all mozrunner objects, both local and remote.
    """

    last_test = "mozrunner-startup"
    process_handler = None
    timeout = None
    output_timeout = None

    def __init__(
        self,
        app_ctx=None,
        profile=None,
        clean_profile=True,
        env=None,
        process_class=None,
        process_args=None,
        symbols_path=None,
        dump_save_path=None,
        addons=None,
        explicit_cleanup=False,
    ):
        self.app_ctx = app_ctx or DefaultContext()

        if isinstance(profile, string_types):
            self.profile = self.app_ctx.profile_class(profile=profile, addons=addons)
        else:
            self.profile = profile or self.app_ctx.profile_class(
                **getattr(self.app_ctx, "profile_args", {})
            )

        self.logger = get_default_logger()

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
        self.explicit_cleanup = explicit_cleanup

    def __del__(self):
        if not self.explicit_cleanup:
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

    def start(
        self, debug_args=None, interactive=False, timeout=None, outputTimeout=None
    ):
        """
        Run self.command in the proper environment.

        :param debug_args: arguments for a debugger
        :param interactive: uses subprocess.Popen directly
        :param timeout: see process_handler.run()
        :param outputTimeout: see process_handler.run()
        :returns: the process id

        :raises: RunnerNotStartedError
        """
        self.timeout = timeout
        self.output_timeout = outputTimeout
        cmd = self.command

        # ensure the runner is stopped
        self.stop()

        # attach a debugger, if specified
        if debug_args:
            cmd = list(debug_args) + cmd

        if self.logger:
            self.logger.info("Application command: %s" % " ".join(cmd))

        encoded_env = {}
        for k in self.env:
            v = self.env[k]
            if isinstance(v, text_type):
                v = v.encode("utf-8")
            if isinstance(k, text_type):
                k = k.encode("utf-8")
            encoded_env[k] = v

        if interactive:
            self.process_handler = subprocess.Popen(cmd, env=encoded_env)
            # TODO: other arguments
        else:
            # this run uses the managed processhandler
            try:
                process = self.process_class(cmd, env=encoded_env, **self.process_args)
                process.run(self.timeout, self.output_timeout)

                self.process_handler = process
            except Exception as e:
                reraise(
                    RunnerNotStartedError,
                    RunnerNotStartedError("Failed to start the process: {}".format(e)),
                    sys.exc_info()[2],
                )

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

    def stop(self, sig=None, timeout=None):
        """
        Kill the process.

        :param sig: Signal used to kill the process, defaults to SIGKILL
                    (has no effect on Windows).
        :param timeout: Maximum time to wait for the processs to exit
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
            self.process_handler.kill(sig=sig, timeout=timeout)

        return self.returncode

    def reset(self):
        """
        Reset the runner to its default state.
        """
        self.stop()
        self.process_handler = None

    def check_for_crashes(
        self, dump_directory=None, dump_save_path=None, test_name=None, quiet=False
    ):
        """Check for possible crashes and output the stack traces.

        :param dump_directory: Directory to search for minidump files
        :param dump_save_path: Directory to save the minidump files to
        :param test_name: Name to use in the crash output
        :param quiet: If `True` don't print the PROCESS-CRASH message to stdout

        :returns: Number of crashes which have been detected since the last invocation
        """
        crash_count = 0

        if not dump_directory:
            dump_directory = os.path.join(self.profile.profile, "minidumps")

        if not dump_save_path:
            dump_save_path = self.dump_save_path

        if not test_name:
            test_name = "runner.py"

        try:
            if self.logger:
                if mozcrash:
                    crash_count = mozcrash.log_crashes(
                        self.logger,
                        dump_directory,
                        self.symbols_path,
                        dump_save_path=dump_save_path,
                        test=test_name,
                    )
                else:
                    self.logger.warning("Can not log crashes without mozcrash")
            else:
                if mozcrash:
                    crash_count = mozcrash.check_for_crashes(
                        dump_directory,
                        self.symbols_path,
                        dump_save_path=dump_save_path,
                        test_name=test_name,
                        quiet=quiet,
                    )

            self.crashed += crash_count
        except Exception:
            traceback.print_exc()

        return crash_count

    def cleanup(self):
        """
        Cleanup all runner state
        """
        self.stop()
