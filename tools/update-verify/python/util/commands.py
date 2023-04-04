# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

"""Functions for running commands"""

import logging
import os
import subprocess
import time

import six

log = logging.getLogger(__name__)


# timeout message, used in TRANSIENT_HG_ERRORS and in tests.
TERMINATED_PROCESS_MSG = "timeout, process terminated"


def log_cmd(cmd, **kwargs):
    # cwd is special in that we always want it printed, even if it's not
    # explicitly chosen
    kwargs = kwargs.copy()
    if "cwd" not in kwargs:
        kwargs["cwd"] = os.getcwd()
    log.info("command: START")
    log.info("command: %s" % subprocess.list2cmdline(cmd))
    for key, value in six.iteritems(kwargs):
        log.info("command: %s: %s", key, str(value))


def merge_env(env):
    new_env = os.environ.copy()
    new_env.update(env)
    return new_env


def run_cmd(cmd, **kwargs):
    """Run cmd (a list of arguments).  Raise subprocess.CalledProcessError if
    the command exits with non-zero.  If the command returns successfully,
    return 0."""
    log_cmd(cmd, **kwargs)
    # We update this after logging because we don't want all of the inherited
    # env vars muddling up the output
    if "env" in kwargs:
        kwargs["env"] = merge_env(kwargs["env"])
    try:
        t = time.monotonic()
        log.info("command: output:")
        return subprocess.check_call(cmd, **kwargs)
    except subprocess.CalledProcessError:
        log.info("command: ERROR", exc_info=True)
        raise
    finally:
        elapsed = time.monotonic() - t
        log.info("command: END (%.2fs elapsed)\n", elapsed)
