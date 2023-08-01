# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import subprocess
import threading
import time


def run_and_wait(
    args=None,
    cwd=None,
    env=None,
    text=True,
    timeout=None,
    timeout_handler=None,
    output_timeout=None,
    output_timeout_handler=None,
    output_line_handler=None,
):
    """
    Run a process and wait for it to complete, with optional support for
    line-by-line output handling and timeouts.

    On timeout or output timeout, the callback should kill the process;
    many clients use  mozcrash.kill_and_get_minidump() in the timeout
    callback.

    run_and_wait is not intended to be a generic replacement for subprocess.
    Clients requiring different options or behavior should use subprocess
    directly.

    :param args: command to run. May be a string or a list.
    :param cwd: working directory for command.
    :param env: environment to use for the process (defaults to os.environ).
    :param text: open streams in text mode if True; else use binary mode.
    :param timeout: seconds to wait for process to complete before calling timeout_handler
    :param timeout_handler: function to be called if timeout reached
    :param output_timeout: seconds to wait for process to generate output
    :param output_timeout_handler: function to be called if output_timeout is reached
    :param output_line_handler: function to be called for every line of process output
    """
    is_win = os.name == "nt"
    context = {"output_timer": None, "proc_timer": None, "timed_out": False}

    def base_timeout_handler():
        context["timed_out"] = True
        if context["output_timer"]:
            context["output_timer"].cancel()
        if timeout_handler:
            timeout_handler(proc)

    def base_output_timeout_handler():
        seconds_since_last_output = time.time() - output_time
        next_possible_output_timeout = output_timeout - seconds_since_last_output
        if next_possible_output_timeout <= 0:
            context["timed_out"] = True
            if context["proc_timer"]:
                context["proc_timer"].cancel()
            if output_timeout_handler:
                output_timeout_handler(proc)
        else:
            context["output_timer"] = threading.Timer(
                next_possible_output_timeout, base_output_timeout_handler
            )
            context["output_timer"].start()

    if env is None:
        env = os.environ.copy()

    if is_win:
        kwargs = {
            "creationflags": subprocess.CREATE_NEW_PROCESS_GROUP,
        }
    else:
        kwargs = {
            "preexec_fn": os.setsid,
        }

    proc = subprocess.Popen(
        args,
        cwd=cwd,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=text,
        **kwargs
    )

    if timeout:
        context["proc_timer"] = threading.Timer(timeout, base_timeout_handler)
        context["proc_timer"].start()

    if output_timeout:
        output_time = time.time()
        context["output_timer"] = threading.Timer(
            output_timeout, base_output_timeout_handler
        )
        context["output_timer"].start()

    for line in proc.stdout:
        output_time = time.time()
        if output_line_handler:
            output_line_handler(proc, line)
        else:
            print(line)
        if context["timed_out"]:
            break

    if not context["timed_out"]:
        proc.wait()

    if timeout:
        context["proc_timer"].cancel()
    if output_timeout:
        context["output_timer"].cancel()

    return proc
