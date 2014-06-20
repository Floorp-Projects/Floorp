#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

class RunnerException(Exception):
    """Base exception handler for mozrunner related errors"""

class RunnerNotStartedError(RunnerException):
    """Exception handler in case the runner hasn't been started"""

class TimeoutException(RunnerException):
    """Raised on timeout waiting for targets to start."""

class ScriptTimeoutException(RunnerException):
    """Raised on timeout waiting for execute_script to finish."""
