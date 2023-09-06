# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import abc
import re
from pathlib import Path
from threading import Thread

import sentry_sdk
from mozversioncontrol import (
    InvalidRepoPath,
    MissingUpstreamRepo,
    MissingVCSTool,
    get_repository_object,
)
from six import string_types

from mach.telemetry import is_telemetry_enabled
from mach.util import get_state_dir

# https://sentry.io/organizations/mozilla/projects/mach/
_SENTRY_DSN = (
    "https://5cfe351fb3a24e8d82c751252b48722b@o1069899.ingest.sentry.io/6250014"
)


class ErrorReporter(object):
    @abc.abstractmethod
    def report_exception(self, exception):
        """Report the exception to remote error-tracking software."""


class SentryErrorReporter(ErrorReporter):
    """Reports errors using Sentry."""

    def report_exception(self, exception):
        return sentry_sdk.capture_exception(exception)


class NoopErrorReporter(ErrorReporter):
    """Drops errors instead of reporting them.

    This is useful in cases where error-reporting is specifically disabled, such as
    when telemetry hasn't been allowed.
    """

    def report_exception(self, exception):
        return None


def register_sentry(argv, settings, topsrcdir: Path):
    if not is_telemetry_enabled(settings):
        return NoopErrorReporter()

    global _is_unmodified_mach_core_thread
    _is_unmodified_mach_core_thread = Thread(
        target=_is_unmodified_mach_core,
        args=[topsrcdir],
        daemon=True,
    )
    _is_unmodified_mach_core_thread.start()

    sentry_sdk.init(
        _SENTRY_DSN, before_send=lambda event, _: _process_event(event, topsrcdir)
    )
    sentry_sdk.add_breadcrumb(message="./mach {}".format(" ".join(argv)))
    return SentryErrorReporter()


def _process_event(sentry_event, topsrcdir: Path):
    # Returning nothing causes the event to be dropped:
    # https://docs.sentry.io/platforms/python/configuration/filtering/#using-beforesend
    repo = _get_repository_object(topsrcdir)
    if repo is None:
        # We don't know the repo state, so we don't know if mach files are
        # unmodified.
        return

    base_ref = repo.base_ref_as_hg()
    if not base_ref:
        # If we don't know which revision this exception is attached to, then it's
        # not worth sending
        return

    _is_unmodified_mach_core_thread.join()
    if not _is_unmodified_mach_core_result:
        return

    for map_fn in (_settle_mach_module_id, _patch_absolute_paths, _delete_server_name):
        sentry_event = map_fn(sentry_event, topsrcdir)

    sentry_event["release"] = "hg-rev-{}".format(base_ref)
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

        module = re.sub(
            "mach\\.commands\\.[a-f0-9]{32}", "mach.commands.<generated>", module
        )
        frame["module"] = module
    return sentry_event


def _patch_absolute_paths(sentry_event, topsrcdir: Path):
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
                key = needle.sub(replacement, key)
                value[key] = recursive_patch(next_value, needle, replacement)
            return value
        elif isinstance(value, string_types):
            return needle.sub(replacement, value)
        else:
            return value

    for target_path, replacement in (
        (get_state_dir(), "<statedir>"),
        (str(topsrcdir), "<topsrcdir>"),
        (str(Path.home()), "~"),
    ):
        # Sentry converts "vars" to their "representations". When paths are in local
        # variables on Windows, "C:\Users\MozillaUser\Desktop" becomes
        # "'C:\\Users\\MozillaUser\\Desktop'". To still catch this case, we "repr"
        # the home directory and scrub the beginning and end quotes, then
        # find-and-replace on that.
        repr_path = repr(target_path)[1:-1]

        for target in (target_path, repr_path):
            # Paths in the Sentry event aren't consistent:
            # * On *nix, they're mostly forward slashes.
            # * On *nix, not all absolute paths start with a leading forward slash.
            # * On Windows, they're mostly backslashes.
            # * On Windows, `.extra."sys.argv"` uses forward slashes.
            # * The Python variables in-scope captured by the Sentry report may be
            #   inconsistent, even for a single path. For example, on
            #   Windows, Mach calculates the state_dir as "C:\Users\<user>/.mozbuild".

            # Handle the case where not all absolute paths start with a leading
            # forward slash: make the initial slash optional in the search string.
            if target.startswith("/"):
                target = "/?" + target[1:]

            # Handle all possible slash variants: our search string should match
            # both forward slashes and backslashes. This is done by dynamically
            # replacing each "/" and "\" with the regex "[\/\\]" (match both).
            slash_regex = re.compile(r"[\/\\]")
            # The regex module parses string backslash escapes before compiling the
            # regex, so we need to add more backslashes:
            # "[\\/\\\\]" => [\/\\] => match "/" and "\"
            target = slash_regex.sub(r"[\\/\\\\]", target)

            # Compile the regex and patch the event.
            needle_regex = re.compile(target, re.IGNORECASE)
            sentry_event = recursive_patch(sentry_event, needle_regex, replacement)
    return sentry_event


def _delete_server_name(sentry_event, _):
    sentry_event.pop("server_name")
    return sentry_event


def _get_repository_object(topsrcdir: Path):
    try:
        return get_repository_object(str(topsrcdir))
    except (InvalidRepoPath, MissingVCSTool):
        return None


def _is_unmodified_mach_core(topsrcdir: Path):
    """True if mach is unmodified compared to the public tree.

    To avoid submitting Sentry events for errors caused by user's
    local changes, we attempt to detect if mach (or code affecting mach)
    has been modified in the user's local state:
    * In a revision off of a "ancestor to central" revision, or:
    * In the working, uncommitted state.

    If "$topsrcdir/mach" and "*.py" haven't been touched, then we can be
    pretty confident that the Mach behaviour that caused the exception
    also exists in the public tree.
    """
    global _is_unmodified_mach_core_result

    repo = _get_repository_object(topsrcdir)
    try:
        files = set(repo.get_outgoing_files()) | set(repo.get_changed_files())
        _is_unmodified_mach_core_result = not any(
            [file for file in files if file == "mach" or file.endswith(".py")]
        )
    except MissingUpstreamRepo:
        # If we don't know the upstream state, we don't know if the mach files
        # have been unmodified.
        _is_unmodified_mach_core_result = False


_is_unmodified_mach_core_result = None
_is_unmodified_mach_core_thread = None
