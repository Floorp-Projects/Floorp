# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import re

import sentry_sdk
from six.moves.configparser import SafeConfigParser, NoOptionError

from mozboot.util import get_state_dir


# https://sentry.prod.mozaws.net/operations/mach/
_SENTRY_DSN = "https://8228c9aff64949c2ba4a2154dc515f55@sentry.prod.mozaws.net/525"


def register_sentry():
    cfg_file = os.path.join(get_state_dir(), 'machrc')
    config = SafeConfigParser()

    if not config.read(cfg_file):
        return

    try:
        telemetry_enabled = config.getboolean("build", "telemetry")
    except NoOptionError:
        return

    if not telemetry_enabled:
        return

    sentry_sdk.init(_SENTRY_DSN, before_send=_settle_mach_module_id)


def _settle_mach_module_id(sentry_event, exception):
    # Sentry groups issues according to the stack frames and their associated
    # "module" properties. However, one of the modules is being reported
    # like "mach.commands.26a828ef5164403eaff4305ab4cb0fab" (with a generated id).
    # This function replaces that generated id with the static string "<generated>"
    # so that grouping behaves as expected

    stacktrace_frames = sentry_event["exception"]["values"][0]["stacktrace"]["frames"]
    for frame in stacktrace_frames:
        module = frame.get("module")
        if not module:
            continue

        module = re.sub("mach\\.commands\\.[a-f0-9]{32}", "mach.commands.<generated>",
                        module)
        frame["module"] = module
    return sentry_event


def report_exception(exception):
    # sentry_sdk won't report the exception if `sentry-sdk.init(...)` hasn't been called
    sentry_sdk.capture_exception(exception)
