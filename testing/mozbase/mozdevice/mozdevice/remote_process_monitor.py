# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re
import time

import six

from .adb import ADBTimeoutError


class RemoteProcessMonitor:
    """
    RemoteProcessMonitor provides a convenient way to run a remote process,
    dump its log file, and wait for it to end.
    """

    def __init__(
        self,
        app_name,
        device,
        log,
        message_logger,
        remote_log_file,
        remote_profile,
    ):
        self.app_name = app_name
        self.device = device
        self.log = log
        self.remote_log_file = remote_log_file
        self.remote_profile = remote_profile
        self.counts = {}
        self.counts["pass"] = 0
        self.counts["fail"] = 0
        self.counts["todo"] = 0
        self.last_test_seen = "RemoteProcessMonitor"
        self.message_logger = message_logger
        if self.device.is_file(self.remote_log_file):
            self.device.rm(self.remote_log_file)
            self.log.info("deleted remote log %s" % self.remote_log_file)

    def launch(self, app, debugger_info, test_url, extra_args, env, e10s):
        """
        Start the remote activity.
        """
        if self.app_name and self.device.process_exist(self.app_name):
            self.log.info("%s is already running. Stopping..." % self.app_name)
            self.device.stop_application(self.app_name)
        args = []
        if debugger_info:
            args.extend(debugger_info.args)
            args.append(app)
        args.extend(extra_args)
        activity = "TestRunnerActivity"
        self.device.launch_activity(
            self.app_name,
            activity_name=activity,
            e10s=e10s,
            moz_env=env,
            extra_args=args,
            url=test_url,
        )
        return self.pid

    @property
    def pid(self):
        """
        Determine the pid of the remote process (or the first process with
        the same name).
        """
        procs = self.device.get_process_list()
        # limit the comparison to the first 75 characters due to a
        # limitation in processname length in android.
        pids = [proc[0] for proc in procs if proc[1] == self.app_name[:75]]
        if pids is None or len(pids) < 1:
            return 0
        return pids[0]

    def read_stdout(self):
        """
        Fetch the full remote log file, log any new content and return True if new
        content is processed.
        """
        try:
            new_log_content = self.device.get_file(
                self.remote_log_file, offset=self.stdout_len
            )
        except ADBTimeoutError:
            raise
        except Exception as e:
            self.log.error(
                "%s | exception reading log: %s" % (self.last_test_seen, str(e))
            )
            return False
        if not new_log_content:
            return False

        self.stdout_len += len(new_log_content)
        new_log_content = six.ensure_str(new_log_content, errors="replace")

        self.log_buffer += new_log_content
        lines = self.log_buffer.split("\n")
        lines = [l for l in lines if l]

        if lines:
            if self.log_buffer.endswith("\n"):
                # all lines are complete; no need to buffer
                self.log_buffer = ""
            else:
                # keep the last (unfinished) line in the buffer
                self.log_buffer = lines[-1]
                del lines[-1]
        if not lines:
            return False

        for line in lines:
            # This passes the line to the logger (to be logged or buffered)
            if isinstance(line, six.text_type):
                # if line is unicode - let's encode it to bytes
                parsed_messages = self.message_logger.write(
                    line.encode("UTF-8", "replace")
                )
            else:
                # if line is bytes type, write it as it is
                parsed_messages = self.message_logger.write(line)

            for message in parsed_messages:
                if isinstance(message, dict):
                    if message.get("action") == "test_start":
                        self.last_test_seen = message["test"]
                    elif message.get("action") == "test_end":
                        self.last_test_seen = "{} (finished)".format(message["test"])
                    elif message.get("action") == "suite_end":
                        self.last_test_seen = "Last test finished"
                    elif message.get("action") == "log":
                        line = message["message"].strip()
                        m = re.match(r".*:\s*(\d*)", line)
                        if m:
                            try:
                                val = int(m.group(1))
                                if "Passed:" in line:
                                    self.counts["pass"] += val
                                    self.last_test_seen = "Last test finished"
                                elif "Failed:" in line:
                                    self.counts["fail"] += val
                                elif "Todo:" in line:
                                    self.counts["todo"] += val
                            except ADBTimeoutError:
                                raise
                            except Exception:
                                pass

        return True

    def wait(self, timeout=None):
        """
        Wait for the remote process to end (or for its activity to go to background).
        While waiting, periodically retrieve the process output and print it.
        If the process is still running but no output is received in *timeout*
        seconds, return False; else, once the process exits/goes to background,
        return True.
        """
        self.log_buffer = ""
        self.stdout_len = 0

        timer = 0
        output_timer = 0
        interval = 10
        status = True
        top = self.app_name

        # wait for log creation on startup
        retries = 0
        while retries < 20 and not self.device.is_file(self.remote_log_file):
            retries += 1
            time.sleep(1)
        if self.device.is_file(self.remote_log_file):
            # We must change the remote log's permissions so that the shell can read it.
            self.device.chmod(self.remote_log_file, mask="666")
        else:
            self.log.warning(
                "Failed wait for remote log: %s missing?" % self.remote_log_file
            )

        while top == self.app_name:
            has_output = self.read_stdout()
            if has_output:
                output_timer = 0
            if self.counts["pass"] > 0:
                interval = 0.5
            time.sleep(interval)
            timer += interval
            output_timer += interval
            if timeout and output_timer > timeout:
                status = False
                break
            if not has_output:
                top = self.device.get_top_activity(timeout=60)
                if top is None:
                    self.log.info("Failed to get top activity, retrying, once...")
                    top = self.device.get_top_activity(timeout=60)

        # Flush anything added to stdout during the sleep
        self.read_stdout()
        self.log.info("wait for %s complete; top activity=%s" % (self.app_name, top))
        if top == self.app_name:
            self.log.info("%s unexpectedly found running. Killing..." % self.app_name)
            self.kill()
        if not status:
            self.log.error(
                "TEST-UNEXPECTED-FAIL | %s | "
                "application timed out after %d seconds with no output"
                % (self.last_test_seen, int(timeout))
            )
        return status

    def kill(self):
        """
        End a troublesome remote process: Trigger ANR and breakpad dumps, then
        force the application to end.
        """

        # Trigger an ANR report with "kill -3" (SIGQUIT)
        try:
            self.device.pkill(self.app_name, sig=3, attempts=1)
        except ADBTimeoutError:
            raise
        except:  # NOQA: E722
            pass
        time.sleep(3)

        # Trigger a breakpad dump with "kill -6" (SIGABRT)
        try:
            self.device.pkill(self.app_name, sig=6, attempts=1)
        except ADBTimeoutError:
            raise
        except:  # NOQA: E722
            pass

        # Wait for process to end
        retries = 0
        while retries < 3:
            if self.device.process_exist(self.app_name):
                self.log.info(
                    "%s still alive after SIGABRT: waiting..." % self.app_name
                )
                time.sleep(5)
            else:
                break
            retries += 1
        if self.device.process_exist(self.app_name):
            try:
                self.device.pkill(self.app_name, sig=9, attempts=1)
            except ADBTimeoutError:
                raise
            except:  # NOQA: E722
                self.log.error("%s still alive after SIGKILL!" % self.app_name)
        if self.device.process_exist(self.app_name):
            self.device.stop_application(self.app_name)

        # Test harnesses use the MOZ_CRASHREPORTER environment variables to suppress
        # the interactive crash reporter, but that may not always be effective;
        # check for and cleanup errant crashreporters.
        crashreporter = "%s.CrashReporter" % self.app_name
        if self.device.process_exist(crashreporter):
            self.log.warning(
                "%s unexpectedly found running. Killing..." % crashreporter
            )
            try:
                self.device.pkill(crashreporter)
            except ADBTimeoutError:
                raise
            except:  # NOQA: E722
                pass
        if self.device.process_exist(crashreporter):
            self.log.error("%s still running!!" % crashreporter)

    @staticmethod
    def elf_arm(filename):
        """
        Determine if the specified file is an ARM binary.
        """
        data = open(filename, "rb").read(20)
        return data[:4] == "\x7fELF" and ord(data[18]) == 40  # EM_ARM
