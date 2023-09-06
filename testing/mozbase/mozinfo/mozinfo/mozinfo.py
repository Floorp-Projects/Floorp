#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# TODO: it might be a good idea of adding a system name (e.g. 'Ubuntu' for
# linux) to the information; I certainly wouldn't want anyone parsing this
# information and having behaviour depend on it

import os
import platform
import re
import sys
from ctypes.util import find_library

from .string_version import StringVersion

# keep a copy of the os module since updating globals overrides this
_os = os


class unknown(object):
    """marker class for unknown information"""

    # pylint: disable=W1629
    def __nonzero__(self):
        return False

    def __bool__(self):
        return False

    def __str__(self):
        return "UNKNOWN"


unknown = unknown()  # singleton


def get_windows_version():
    import ctypes

    class OSVERSIONINFOEXW(ctypes.Structure):
        _fields_ = [
            ("dwOSVersionInfoSize", ctypes.c_ulong),
            ("dwMajorVersion", ctypes.c_ulong),
            ("dwMinorVersion", ctypes.c_ulong),
            ("dwBuildNumber", ctypes.c_ulong),
            ("dwPlatformId", ctypes.c_ulong),
            ("szCSDVersion", ctypes.c_wchar * 128),
            ("wServicePackMajor", ctypes.c_ushort),
            ("wServicePackMinor", ctypes.c_ushort),
            ("wSuiteMask", ctypes.c_ushort),
            ("wProductType", ctypes.c_byte),
            ("wReserved", ctypes.c_byte),
        ]

    os_version = OSVERSIONINFOEXW()
    os_version.dwOSVersionInfoSize = ctypes.sizeof(os_version)
    retcode = ctypes.windll.Ntdll.RtlGetVersion(ctypes.byref(os_version))
    if retcode != 0:
        raise OSError

    return (
        os_version.dwMajorVersion,
        os_version.dwMinorVersion,
        os_version.dwBuildNumber,
    )


# get system information
info = {
    "os": unknown,
    "processor": unknown,
    "version": unknown,
    "os_version": unknown,
    "bits": unknown,
    "has_sandbox": unknown,
    "display": None,
    "automation": bool(os.environ.get("MOZ_AUTOMATION", False)),
}
(system, node, release, version, machine, processor) = platform.uname()
(bits, linkage) = platform.architecture()

# get os information and related data
if system in ["Microsoft", "Windows"]:
    info["os"] = "win"
    # There is a Python bug on Windows to determine platform values
    # http://bugs.python.org/issue7860
    if "PROCESSOR_ARCHITEW6432" in os.environ:
        processor = os.environ.get("PROCESSOR_ARCHITEW6432", processor)
    else:
        processor = os.environ.get("PROCESSOR_ARCHITECTURE", processor)
    system = os.environ.get("OS", system).replace("_", " ")
    (major, minor, _, _, service_pack) = os.sys.getwindowsversion()
    info["service_pack"] = service_pack
    if major >= 6 and minor >= 2:
        # On windows >= 8.1 the system call that getwindowsversion uses has
        # been frozen to always return the same values. In this case we call
        # the RtlGetVersion API directly, which still provides meaningful
        # values, at least for now.
        major, minor, build_number = get_windows_version()
        version = "%d.%d.%d" % (major, minor, build_number)

    os_version = "%d.%d" % (major, minor)
elif system.startswith(("MINGW", "MSYS_NT")):
    # windows/mingw python build (msys)
    info["os"] = "win"
    os_version = version = unknown
elif system == "Linux":
    # Attempt to use distro package to determine Linux distribution first.
    # Failing that, fall back to use the platform method.
    # Note that platform.linux_distribution() will be deprecated as of 3.8
    # and this block will be removed once support for 2.7/3.5 is dropped.
    try:
        from distro import linux_distribution
    except ImportError:
        from platform import linux_distribution

    output = linux_distribution()
    (distribution, os_version, codename) = tuple(str(item.title()) for item in output)

    if not processor:
        processor = machine
    if not distribution:
        distribution = "lfs"
    if not os_version:
        os_version = release
    if not codename:
        codename = "unknown"
    version = "%s %s" % (distribution, os_version)

    if os.environ.get("WAYLAND_DISPLAY"):
        info["display"] = "wayland"
    elif os.environ.get("DISPLAY"):
        info["display"] = "x11"

    info["os"] = "linux"
    info["linux_distro"] = distribution
elif system in ["DragonFly", "FreeBSD", "NetBSD", "OpenBSD"]:
    info["os"] = "bsd"
    version = os_version = sys.platform
elif system == "Darwin":
    (release, versioninfo, machine) = platform.mac_ver()
    version = "OS X %s" % release
    versionNums = release.split(".")[:2]
    os_version = "%s.%s" % (versionNums[0], versionNums[1])
    info["os"] = "mac"
elif sys.platform in ("solaris", "sunos5"):
    info["os"] = "unix"
    os_version = version = sys.platform
else:
    os_version = version = unknown

info["apple_silicon"] = False
if (
    info["os"] == "mac"
    and float(os_version) > 10.15
    and processor == "arm"
    and bits == "64bit"
):
    info["apple_silicon"] = True

info["apple_catalina"] = False
if info["os"] == "mac" and float(os_version) == 10.15:
    info["apple_catalina"] = True

info["win10_2004"] = False
if info["os"] == "win" and version == "10.0.19041":
    info["win10_2004"] = True

info["win10_2009"] = False
if info["os"] == "win" and version == "10.0.19045":
    info["win10_2009"] = True

info["win11_2009"] = False
if info["os"] == "win" and version == "10.0.22621":
    info["win11_2009"] = True

info["version"] = version
info["os_version"] = StringVersion(os_version)


# processor type and bits
if processor in ["i386", "i686"]:
    if bits == "32bit":
        processor = "x86"
    elif bits == "64bit":
        processor = "x86_64"
elif processor.upper() == "AMD64":
    bits = "64bit"
    processor = "x86_64"
elif processor.upper() == "ARM64":
    bits = "64bit"
    processor = "aarch64"
elif processor == "Power Macintosh":
    processor = "ppc"
bits = re.search("(\d+)bit", bits).group(1)
info.update(
    {
        "processor": processor,
        "bits": int(bits),
    }
)

if info["os"] == "linux":
    import ctypes
    import errno

    PR_SET_SECCOMP = 22
    SECCOMP_MODE_FILTER = 2
    ctypes.CDLL(find_library("c"), use_errno=True).prctl(
        PR_SET_SECCOMP, SECCOMP_MODE_FILTER, 0
    )
    info["has_sandbox"] = ctypes.get_errno() == errno.EFAULT
else:
    info["has_sandbox"] = True

# standard value of choices, for easy inspection
choices = {
    "os": ["linux", "bsd", "win", "mac", "unix"],
    "bits": [32, 64],
    "processor": ["x86", "x86_64", "ppc"],
}


def sanitize(info):
    """Do some sanitization of input values, primarily
    to handle universal Mac builds."""
    if "processor" in info and info["processor"] == "universal-x86-x86_64":
        # If we're running on OS X 10.6 or newer, assume 64-bit
        if release[:4] >= "10.6":  # Note this is a string comparison
            info["processor"] = "x86_64"
            info["bits"] = 64
        else:
            info["processor"] = "x86"
            info["bits"] = 32


# method for updating information


def update(new_info):
    """
    Update the info.

    :param new_info: Either a dict containing the new info or a path/url
                     to a json file containing the new info.
    """
    from six import string_types

    if isinstance(new_info, string_types):
        # lazy import
        import json

        import mozfile

        f = mozfile.load(new_info)
        new_info = json.loads(f.read())
        f.close()

    info.update(new_info)
    sanitize(info)
    globals().update(info)

    # convenience data for os access
    for os_name in choices["os"]:
        globals()["is" + os_name.title()] = info["os"] == os_name
    # unix is special
    if isLinux or isBsd:  # noqa
        globals()["isUnix"] = True


def find_and_update_from_json(*dirs, **kwargs):
    """Find a mozinfo.json file, load it, and update global symbol table.

    This method will first check the relevant objdir directory for the
    necessary mozinfo.json file, if the current script is being run from a
    Mozilla objdir.

    If the objdir directory did not supply the necessary data, this method
    will then look for the required mozinfo.json file from the provided
    tuple of directories.

    If file is found, the global symbols table is updated via a helper method.

    If no valid files are found, this method no-ops unless the raise_exception
    kwargs is provided with explicit boolean value of True.

    :param tuple dirs: Directories in which to look for the file.
    :param dict kwargs: optional values:
                        raise_exception: if True, exceptions are raised.
                        False by default.
    :returns: None: default behavior if mozinfo.json cannot be found.
              json_path: string representation of mozinfo.json path.
    :raises: IOError: if raise_exception is True and file is not found.
    """
    # First, see if we're in an objdir
    try:
        from mozboot.mozconfig import MozconfigFindException
        from mozbuild.base import BuildEnvironmentNotFoundException, MozbuildObject

        build = MozbuildObject.from_environment()
        json_path = _os.path.join(build.topobjdir, "mozinfo.json")
        if _os.path.isfile(json_path):
            update(json_path)
            return json_path
    except ImportError:
        pass
    except (BuildEnvironmentNotFoundException, MozconfigFindException):
        pass

    for d in dirs:
        d = _os.path.abspath(d)
        json_path = _os.path.join(d, "mozinfo.json")
        if _os.path.isfile(json_path):
            update(json_path)
            return json_path

    # by default, exceptions are suppressed. Set this to True if otherwise
    # desired.
    if kwargs.get("raise_exception", False):
        raise IOError("mozinfo.json could not be found.")
    return None


def output_to_file(path):
    import json

    with open(path, "w") as f:
        f.write(json.dumps(info))


update({})

# exports
__all__ = list(info.keys())
__all__ += ["is" + os_name.title() for os_name in choices["os"]]
__all__ += [
    "info",
    "unknown",
    "main",
    "choices",
    "update",
    "find_and_update_from_json",
    "output_to_file",
    "StringVersion",
]


def main(args=None):
    # parse the command line
    from optparse import OptionParser

    parser = OptionParser(description=__doc__)
    for key in choices:
        parser.add_option(
            "--%s" % key,
            dest=key,
            action="store_true",
            default=False,
            help="display choices for %s" % key,
        )
    options, args = parser.parse_args()

    # args are JSON blobs to override info
    if args:
        # lazy import
        import json

        for arg in args:
            if _os.path.exists(arg):
                string = open(arg).read()
            else:
                string = arg
            update(json.loads(string))

    # print out choices if requested
    flag = False
    for key, value in options.__dict__.items():
        if value is True:
            print(
                "%s choices: %s"
                % (key, " ".join([str(choice) for choice in choices[key]]))
            )
            flag = True
    if flag:
        return

    # otherwise, print out all info
    for key, value in info.items():
        print("%s: %s" % (key, value))


if __name__ == "__main__":
    main()
