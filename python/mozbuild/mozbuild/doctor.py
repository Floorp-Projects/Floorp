# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import enum
import locale
import os
import socket
import subprocess
import sys

import attr
import psutil
import requests

from distutils.version import LooseVersion

import mozpack.path as mozpath

# Minimum recommended logical processors in system.
PROCESSORS_THRESHOLD = 4

# Minimum recommended total system memory, in gigabytes.
MEMORY_THRESHOLD = 7.4

# Minimum recommended free space on each disk, in gigabytes.
FREESPACE_THRESHOLD = 10

# Latest MozillaBuild version.
LATEST_MOZILLABUILD_VERSION = LooseVersion("3.3")

DISABLE_LASTACCESS_WIN = """
Disable the last access time feature?
This improves the speed of file and
directory access by deferring Last Access Time modification on disk by up to an
hour. Backup programs that rely on this feature may be affected.
https://technet.microsoft.com/en-us/library/cc785435.aspx
"""


def get_mount_point(path):
    """Return the mount point for a given path."""
    while path != "/" and not os.path.ismount(path):
        path = mozpath.abspath(mozpath.join(path, os.pardir))
    return path


class CheckStatus(enum.Enum):
    # Check is okay.
    OK = enum.auto()
    # We found an issue.
    WARNING = enum.auto()
    # We found an issue that will break build/configure/etc.
    FATAL = enum.auto()
    # The check was skipped.
    SKIPPED = enum.auto()


@attr.s
class DoctorCheck:
    # Name of the check.
    name = attr.ib()
    # Lines to display on screen.
    display_text = attr.ib()
    # `CheckStatus` for this given check.
    status = attr.ib()
    # Function to be called to fix the issues, if applicable.
    fix = attr.ib(default=None)


CHECKS = {}


def check(func):
    """Decorator that registers a function as a doctor check.

    The function should return a `DoctorCheck` or be an iterator of
    checks.
    """
    CHECKS[func.__name__] = func


@check
def dns(**kwargs):
    """Check DNS is queryable."""
    try:
        socket.getaddrinfo("mozilla.org", 80)
        return DoctorCheck(
            name="dns",
            status=CheckStatus.OK,
            display_text=["DNS query for mozilla.org completed successfully."],
        )

    except socket.gaierror:
        return DoctorCheck(
            name="dns",
            status=CheckStatus.FATAL,
            display_text=["Could not query DNS for mozilla.org."],
        )


@check
def internet(**kwargs):
    """Check the internet is reachable via HTTPS."""
    try:
        resp = requests.get("https://mozilla.org")
        resp.raise_for_status()

        return DoctorCheck(
            name="internet",
            status=CheckStatus.OK,
            display_text=["Internet is reachable."],
        )

    except Exception:
        return DoctorCheck(
            name="internet",
            status=CheckStatus.FATAL,
            display_text=["Could not reach a known website via HTTPS."],
        )


@check
def ssh(**kwargs):
    """Check the status of `ssh hg.mozilla.org` for common errors."""
    try:
        # We expect this command to return exit code 1 even when we hit
        # the successful code path, since we don't specify a `pash` command.
        proc = subprocess.run(
            ["ssh", "hg.mozilla.org"],
            encoding="utf-8",
            stderr=subprocess.PIPE,
            stdout=subprocess.PIPE,
        )

        # Command output from a successful `pash` run.
        if "has privileges to access Mercurial over" in proc.stdout:
            return DoctorCheck(
                name="ssh",
                status=CheckStatus.OK,
                display_text=["SSH is properly configured for access to hg."],
            )

        if "Permission denied" in proc.stdout:
            # Parse thproc.stdout for username, which looks like:
            # `<username>@hg.mozilla.org: Permission denied (reason)`
            login_string = proc.stdout.split()[0]
            username, _host = login_string.split("@hg.mozilla.org")

            # `<username>` should be an email.
            if "@" not in username:
                return DoctorCheck(
                    name="ssh",
                    status=CheckStatus.FATAL,
                    display_text=[
                        "SSH username `{}` is not an email address.".format(username),
                        "hg.mozilla.org logins should be in the form `user@domain.com`.",
                    ],
                )

            return DoctorCheck(
                name="ssh",
                status=CheckStatus.WARNING,
                display_text=[
                    "SSH username `{}` does not have permission to push to "
                    "hg.mozilla.org.".format(username)
                ],
            )
    except subprocess.CalledProcessError:
        return DoctorCheck(
            name="ssh",
            status=CheckStatus.WARNING,
            display_text=["Could not run `ssh hg.mozilla.org`."],
        )


@check
def cpu(**kwargs):
    """Check the host machine has the recommended processing power to develop Firefox."""
    cpu_count = psutil.cpu_count()
    if cpu_count < PROCESSORS_THRESHOLD:
        status = CheckStatus.WARNING
        desc = "%d logical processors detected, <%d" % (cpu_count, PROCESSORS_THRESHOLD)
    else:
        status = CheckStatus.OK
        desc = "%d logical processors detected, >=%d" % (
            cpu_count,
            PROCESSORS_THRESHOLD,
        )

    return DoctorCheck(name="cpu", display_text=[desc], status=status)


@check
def memory(**kwargs):
    """Check the host machine has the recommended memory to develop Firefox."""
    memory = psutil.virtual_memory().total
    # Convert to gigabytes.
    memory_GB = memory / 1024 ** 3.0
    if memory_GB < MEMORY_THRESHOLD:
        status = CheckStatus.WARNING
        desc = "%.1fGB of physical memory, <%.1fGB" % (memory_GB, MEMORY_THRESHOLD)
    else:
        status = CheckStatus.OK
        desc = "%.1fGB of physical memory, >%.1fGB" % (memory_GB, MEMORY_THRESHOLD)

    return DoctorCheck(name="memory", status=status, display_text=[desc])


@check
def storage_freespace(topsrcdir, topobjdir, **kwargs):
    """Check the host machine has the recommended disk space to develop Firefox."""
    topsrcdir_mount = get_mount_point(topsrcdir)
    topobjdir_mount = get_mount_point(topobjdir)

    mounts = [
        ("topsrcdir", topsrcdir, topsrcdir_mount),
        ("topobjdir", topobjdir, topobjdir_mount),
    ]

    mountpoint_line = topsrcdir_mount != topobjdir_mount
    checks = []

    for purpose, path, mount in mounts:
        if not mountpoint_line:
            mountpoint_line = True
            continue

        desc = ["%s = %s" % (purpose, path)]

        try:
            usage = psutil.disk_usage(mount)
            freespace, size = usage.free, usage.total
            freespace_GB = freespace / 1024 ** 3
            size_GB = size / 1024 ** 3
            if freespace_GB < FREESPACE_THRESHOLD:
                status = CheckStatus.WARNING
                desc.append(
                    "mountpoint = %s\n%dGB of %dGB free, <%dGB"
                    % (mount, freespace_GB, size_GB, FREESPACE_THRESHOLD)
                )
            else:
                status = CheckStatus.OK
                desc.append(
                    "mountpoint = %s\n%dGB of %dGB free, >=%dGB"
                    % (mount, freespace_GB, size_GB, FREESPACE_THRESHOLD)
                )

        except OSError:
            status = CheckStatus.FATAL
            desc.append("path invalid")

        checks.append(
            DoctorCheck(name="%s mount check" % mount, status=status, display_text=desc)
        )

    return checks


def fix_lastaccess_win():
    """Run `fsutil` to fix lastaccess behaviour."""
    try:
        print("Disabling filesystem lastaccess")

        command = ["fsutil", "behavior", "set", "disablelastaccess", "1"]
        subprocess.check_output(command)

        print("Filesystem lastaccess disabled.")

    except subprocess.CalledProcessError:
        print("Could not disable filesystem lastaccess.")


@check
def fs_lastaccess(topsrcdir, topobjdir, **kwargs):
    """Check for the `lastaccess` behaviour on the filsystem, which can slow
    down filesystem operations."""
    if sys.platform.startswith("win"):
        # See 'fsutil behavior':
        # https://technet.microsoft.com/en-us/library/cc785435.aspx
        try:
            command = ["fsutil", "behavior", "query", "disablelastaccess"]
            fsutil_output = subprocess.check_output(command)
            disablelastaccess = int(fsutil_output.partition("=")[2][1])
        except subprocess.CalledProcessError:
            return DoctorCheck(
                status=CheckStatus.WARNING, desc=["unable to check lastaccess behavior"]
            )

        if disablelastaccess == 1:
            return DoctorCheck(
                status=CheckStatus.OK, display_text=["lastaccess disabled systemwide"]
            )
        elif disablelastaccess == 0:
            return DoctorCheck(
                status=CheckStatus.WARNING,
                display_text=["lastaccess enabled"],
                fix=fix_lastaccess_win,
            )

    elif any(
        sys.platform.startswith(prefix) for prefix in ["freebsd", "linux", "openbsd"]
    ):
        topsrcdir_mount = get_mount_point(topsrcdir)
        topobjdir_mount = get_mount_point(topobjdir)
        mounts = [
            ("topsrcdir", topsrcdir, topsrcdir_mount),
            ("topobjdir", topobjdir, topobjdir_mount),
        ]

        common_mountpoint = topsrcdir_mount == topobjdir_mount

        mount_checks = []
        for _purpose, _path, mount in mounts:
            mount_checks.append(check_mount_lastaccess(mount))
            if common_mountpoint:
                break

        return mount_checks


def check_mount_lastaccess(mount):
    """Check `lastaccess` behaviour for a Linux mount."""
    partitions = psutil.disk_partitions(all=True)
    atime_opts = {"atime", "noatime", "relatime", "norelatime"}
    option = ""
    fstype = ""
    for partition in partitions:
        if partition.mountpoint == mount:
            mount_opts = set(partition.opts.split(","))
            intersection = list(atime_opts & mount_opts)
            fstype = partition.fstype
            if len(intersection) == 1:
                option = intersection[0]
            break

    if fstype == "tmpfs":
        status = CheckStatus.OK
        desc = "%s is a tmpfs so noatime/reltime is not needed" % (mount)
    elif not option:
        status = CheckStatus.WARNING
        if sys.platform.startswith("linux"):
            option = "noatime/relatime"
        else:
            option = "noatime"
        desc = "%s has no explicit %s mount option" % (mount, option)
    elif option == "atime" or option == "norelatime":
        status = CheckStatus.WARNING
        desc = "%s has %s mount option" % (mount, option)
    elif option == "noatime" or option == "relatime":
        status = CheckStatus.OK
        desc = "%s has %s mount option" % (mount, option)

    return DoctorCheck(
        name="%s mount lastaccess" % mount, status=status, display_text=[desc]
    )


@check
def mozillabuild(**kwargs):
    """Check that MozillaBuild is the latest version."""
    if not sys.platform.startswith("win"):
        return DoctorCheck(
            name="mozillabuild",
            status=CheckStatus.SKIPPED,
            display_text=["Non-Windows platform, MozillaBuild not relevant"],
        )

    MOZILLABUILD = mozpath.normpath(os.environ.get("MOZILLABUILD", ""))
    if not MOZILLABUILD or not os.path.exists(MOZILLABUILD):
        return DoctorCheck(
            name="mozillabuild",
            status=CheckStatus.WARNING,
            display_text=["Not running under MozillaBuild."],
        )

    try:
        with open(mozpath.join(MOZILLABUILD, "VERSION"), "r") as fh:
            local_version = fh.readline()

        if not local_version:
            return DoctorCheck(
                name="mozillabuild",
                status=CheckStatus.WARNING,
                display_text=["Could not get local MozillaBuild version."],
            )

        if LooseVersion(local_version) < LATEST_MOZILLABUILD_VERSION:
            status = CheckStatus.WARNING
            desc = "MozillaBuild %s in use, <%s" % (
                local_version,
                LATEST_MOZILLABUILD_VERSION,
            )

        else:
            status = CheckStatus.OK
            desc = "MozillaBuild %s in use" % local_version

    except (IOError, ValueError):
        status = CheckStatus.FATAL
        desc = "MozillaBuild version not found"

    return DoctorCheck(name="mozillabuild", status=status, display_text=[desc])


@check
def bad_locale_utf8(**kwargs):
    """Check to detect the invalid locale `UTF-8` on pre-3.8 Python."""
    if sys.version_info >= (3, 8):
        return DoctorCheck(
            name="utf8 locale",
            status=CheckStatus.SKIPPED,
            display_text=["Python version has fixed utf-8 locale bug."],
        )

    try:
        # This line will attempt to get and parse the locale.
        locale.getdefaultlocale()

        return DoctorCheck(
            name="utf8 locale",
            status=CheckStatus.OK,
            display_text=["Python's locale is set to a valid value."],
        )
    except ValueError:
        return DoctorCheck(
            name="utf8 locale",
            status=CheckStatus.FATAL,
            display_text=[
                "Your Python is using an invalid value for its locale.",
                "Either update Python to version 3.8+, or set the following variables in ",
                "your environment:",
                "  export LC_ALL=en_US.UTF-8",
                "  export LANG=en_US.UTF-8",
            ],
        )


def run_doctor(fix=False, verbose=False, **kwargs):
    """Run the doctor checks.

    If `fix` is `True`, run fixing functions for issues that can be resolved
    automatically.

    By default, only print output from checks that result in a warning or
    fatal issue. `verbose` will cause all output to be printed to the screen.
    """
    issues_found = False

    fixes = []
    for _name, check_func in CHECKS.items():
        results = check_func(**kwargs)

        if isinstance(results, DoctorCheck):
            results = [results]

        for result in results:
            if result.status == CheckStatus.SKIPPED and not verbose:
                continue

            if result.status != CheckStatus.OK:
                # If we ever have a non-OK status, we shouldn't print
                # the "No issues detected" line.
                issues_found = True

            if result.status != CheckStatus.OK or verbose:
                print("\n".join(result.display_text))

            if result.fix:
                fixes.append(result.fix)

    if not issues_found:
        print("No issues detected.")
        return 0

    # If we can fix something but the user didn't ask us to, advise
    # them to run with `--fix`.
    if not fix:
        if fixes:
            print(
                "Some of the issues found can be fixed; run "
                "`./mach doctor --fix` to fix them."
            )
        return 1

    # Attempt to run the fix functions.
    for fix in fixes:
        try:
            fix()
        except Exception:
            pass
