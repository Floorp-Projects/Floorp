# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import re
import sys
from os.path import expanduser

import mozpack.path as mozpath
import sentry_sdk
from mozboot.util import get_state_dir
from six import string_types
from six.moves.configparser import SafeConfigParser, NoOptionError

# https://sentry.prod.mozaws.net/operations/mach/
_SENTRY_DSN = "https://8228c9aff64949c2ba4a2154dc515f55@sentry.prod.mozaws.net/525"


def register_sentry(topsrcdir=None):
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

    sentry_sdk.init(_SENTRY_DSN,
                    before_send=lambda event, _: _process_event(event, topsrcdir))


def _process_event(sentry_event, topsrcdir):
    for map_fn in (_settle_mach_module_id, _patch_absolute_paths, _delete_server_name):
        sentry_event = map_fn(sentry_event, topsrcdir)
    return sentry_event


def _settle_mach_module_id(sentry_event, _):
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


def _resolve_topobjdir():
    topobjdir = os.path.join(os.path.dirname(sys.prefix), "..")
    return mozpath.normsep(os.path.normpath(topobjdir))


def _patch_absolute_paths(sentry_event, topsrcdir):
    # As discussed here (https://bugzilla.mozilla.org/show_bug.cgi?id=1636251#c28),
    # we remove usernames from file names with a best-effort basis. The most likely
    # place for usernames to manifest in Sentry information is within absolute paths,
    # such as: "/home/mitch/dev/firefox/mach"
    # We replace the state_dir, obj_dir, src_dir with "<...>" placeholders.
    # Note that we also do a blanket find-and-replace of the user's name with "<user>",
    # which may have ill effects if the user's name is, by happenstance, a substring
    # of some other value within the Sentry event.
    def recursive_patch(value, needle, replacement):
        if isinstance(value, list):
            return [recursive_patch(v, needle, replacement) for v in value]
        elif isinstance(value, dict):
            for key in list(value.keys()):
                next_value = value.pop(key)
                key = key.replace(needle, replacement)
                value[key] = recursive_patch(next_value, needle, replacement)
            return value
        elif isinstance(value, string_types):
            return value.replace(needle, replacement)
        else:
            return value

    for (needle, replacement) in (
            (get_state_dir(), "<statedir>"),
            (_resolve_topobjdir(), "<topobjdir>"),
            (topsrcdir, "<topsrcdir>"),
            (expanduser("~"), "~"),
            # Sentry converts "vars" to their "representations". When paths are in local
            # variables on Windows, "C:\Users\MozillaUser\Desktop" becomes
            # "'C:\\Users\\MozillaUser\\Desktop'". To still catch this case, we "repr"
            # the home directory and scrub the beginning and end quotes, then
            # find-and-replace on that.
            (repr(expanduser("~"))[1:-1], "~"),
    ):
        if needle is None:
            continue  # topsrcdir isn't always defined
        sentry_event = recursive_patch(sentry_event, needle, replacement)
    return sentry_event


def _delete_server_name(sentry_event, _):
    sentry_event.pop("server_name")
    return sentry_event


def report_exception(exception):
    # sentry_sdk won't report the exception if `sentry-sdk.init(...)` hasn't been called
    sentry_sdk.capture_exception(exception)
