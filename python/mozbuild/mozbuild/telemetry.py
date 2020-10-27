# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import division, absolute_import, print_function, unicode_literals

"""
This file contains a voluptuous schema definition for build system telemetry, and functions
to fill an instance of that schema for a single mach invocation.
"""

import distro
import json
import os
import math
import platform
import pprint
import sys
from datetime import datetime

from six import string_types, PY3
from voluptuous import (
    Any,
    Optional,
    MultipleInvalid,
    Required,
    Schema,
)
from voluptuous.validators import Datetime

import mozpack.path as mozpath
from .base import BuildEnvironmentNotFoundException
from .configure.constants import CompilerType

schema = Schema(
    {
        Required("client_id", description="A UUID to uniquely identify a client"): Any(
            *string_types
        ),
        Required("time", description="Time at which this event happened"): Datetime(),
        Required("command", description="The mach command that was invoked"): Any(
            *string_types
        ),
        Required(
            "argv",
            description=(
                "Full mach commandline. "
                + "If the commandline contains "
                + "absolute paths they will be sanitized."
            ),
        ): [Any(*string_types)],
        Required("success", description="true if the command succeeded"): bool,
        Optional(
            "exception",
            description=(
                "If a Python exception was encountered during the execution "
                + "of the command, this value contains the result of calling `repr` "
                + "on the exception object."
            ),
        ): Any(*string_types),
        Optional(
            "file_types_changed",
            description=(
                "This array contains a list of objects with {ext, count} properties giving the "
                + "count of files changed since the last invocation grouped by file type"
            ),
        ): [
            {
                Required("ext", description="File extension"): Any(*string_types),
                Required(
                    "count", description="Count of changed files with this extension"
                ): int,
            }
        ],
        Required("duration_ms", description="Command duration in milliseconds"): int,
        Required("build_opts", description="Selected build options"): {
            Optional("compiler", description="The compiler type in use (CC_TYPE)"): Any(
                *CompilerType.POSSIBLE_VALUES
            ),
            Optional("artifact", description="true if --enable-artifact-builds"): bool,
            Optional(
                "debug", description="true if build is debug (--enable-debug)"
            ): bool,
            Optional(
                "opt", description="true if build is optimized (--enable-optimize)"
            ): bool,
            Optional(
                "ccache", description="true if ccache is in use (--with-ccache)"
            ): bool,
            Optional("sccache", description="true if ccache in use is sccache"): bool,
            Optional("icecream", description="true if icecream in use"): bool,
        },
        Optional("build_attrs", description="Attributes characterizing a build"): {
            Optional(
                "cpu_percent", description="cpu utilization observed during a build"
            ): int,
            Optional(
                "clobber", description="true if the build was a clobber/full build"
            ): bool,
        },
        Required("system"): {
            # We don't need perfect granularity here.
            Required("os", description="Operating system"): Any(
                "windows", "macos", "linux", "other"
            ),
            Optional("cpu_brand", description="CPU brand string from CPUID"): Any(
                *string_types
            ),
            Optional(
                "logical_cores", description="Number of logical CPU cores present"
            ): int,
            Optional(
                "physical_cores", description="Number of physical CPU cores present"
            ): int,
            Optional("memory_gb", description="System memory in GB"): int,
            Optional(
                "drive_is_ssd",
                description="true if the source directory is on a solid-state disk",
            ): bool,
            Optional(
                "virtual_machine",
                description="true if the OS appears to be running in a virtual machine",
            ): bool,
        },
    }
)


def get_client_id(state_dir):
    """
    Get a client id, which is a UUID, from a file in the state directory. If the file doesn't
    exist, generate a UUID and save it to a file.
    """
    path = os.path.join(state_dir, "telemetry_client_id.json")
    if os.path.exists(path):
        with open(path, "r") as f:
            return json.load(f)["client_id"]
    import uuid

    # uuid4 is random, other uuid types may include identifiers from the local system.
    client_id = str(uuid.uuid4())
    if PY3:
        file_mode = "w"
    else:
        file_mode = "wb"
    with open(path, file_mode) as f:
        json.dump({"client_id": client_id}, f)
    return client_id


def cpu_brand_linux():
    """
    Read the CPU brand string out of /proc/cpuinfo on Linux.
    """
    with open("/proc/cpuinfo", "r") as f:
        for line in f:
            if line.startswith("model name"):
                _, brand = line.split(": ", 1)
                return brand.rstrip()
    # not found?
    return None


def cpu_brand_windows():
    """
    Read the CPU brand string from the registry on Windows.
    """
    try:
        import _winreg
    except ImportError:
        import winreg as _winreg

    try:
        h = _winreg.OpenKey(
            _winreg.HKEY_LOCAL_MACHINE,
            r"HARDWARE\DESCRIPTION\System\CentralProcessor\0",
        )
        (brand, ty) = _winreg.QueryValueEx(h, "ProcessorNameString")
        if ty == _winreg.REG_SZ:
            return brand
    except WindowsError:
        pass
    return None


def cpu_brand_mac():
    """
    Get the CPU brand string via sysctl on macos.
    """
    import ctypes
    import ctypes.util

    libc = ctypes.cdll.LoadLibrary(ctypes.util.find_library("c"))
    # First, find the required buffer size.
    bufsize = ctypes.c_size_t(0)
    result = libc.sysctlbyname(
        b"machdep.cpu.brand_string", None, ctypes.byref(bufsize), None, 0
    )
    if result != 0:
        return None
    bufsize.value += 1
    buf = ctypes.create_string_buffer(bufsize.value)
    # Now actually get the value.
    result = libc.sysctlbyname(
        b"machdep.cpu.brand_string", buf, ctypes.byref(bufsize), None, 0
    )
    if result != 0:
        return None

    return buf.value.decode()


def get_cpu_brand():
    """
    Get the CPU brand string as returned by CPUID.
    """
    return {
        "Linux": cpu_brand_linux,
        "Windows": cpu_brand_windows,
        "Darwin": cpu_brand_mac,
    }.get(platform.system(), lambda: None)()


def get_os_name():
    return {
        "Linux": "linux",
        "Windows": "windows",
        "Darwin": "macos",
    }.get(platform.system(), "other")


def get_psutil_stats():
    """Return whether psutil exists and its associated stats.

    @returns (bool, int, int, int) whether psutil exists, the logical CPU count,
        physical CPU count, and total number of bytes of memory.
    """
    try:
        import psutil

        return (
            True,
            psutil.cpu_count(),
            psutil.cpu_count(logical=False),
            psutil.virtual_memory().total,
        )
    except ImportError:
        return False, None, None, None


def get_system_info():
    """
    Gather info to fill the `system` keys in the schema.
    """
    # Normalize OS names a bit, and bucket non-tier-1 platforms into "other".
    has_psutil, logical_cores, physical_cores, memory_total = get_psutil_stats()
    info = {
        "os": get_os_name(),
    }
    if has_psutil:
        # `total` on Linux is gathered from /proc/meminfo's `MemTotal`, which is the
        # total amount of physical memory minus some kernel usage, so round up to the
        # nearest GB to get a sensible answer.
        info["memory_gb"] = int(math.ceil(float(memory_total) / (1024 * 1024 * 1024)))
        info["logical_cores"] = logical_cores
        if physical_cores is not None:
            info["physical_cores"] = physical_cores
    cpu_brand = get_cpu_brand()
    if cpu_brand is not None:
        info["cpu_brand"] = cpu_brand
    # TODO: drive_is_ssd, virtual_machine: https://bugzilla.mozilla.org/show_bug.cgi?id=1481613
    return info


def get_build_opts(substs):
    """
    Translate selected items from `substs` into `build_opts` keys in the schema.
    """
    try:
        opts = {
            k: ty(substs.get(s, None))
            for (k, s, ty) in (
                # Selected substitutions.
                ("artifact", "MOZ_ARTIFACT_BUILDS", bool),
                ("debug", "MOZ_DEBUG", bool),
                ("opt", "MOZ_OPTIMIZE", bool),
                ("ccache", "CCACHE", bool),
                ("sccache", "MOZ_USING_SCCACHE", bool),
            )
        }
        compiler = substs.get("CC_TYPE", None)
        if compiler:
            opts["compiler"] = str(compiler)
        if substs.get("CXX_IS_ICECREAM", None):
            opts["icecream"] = True
        return opts
    except BuildEnvironmentNotFoundException:
        return {}


def get_build_attrs(attrs):
    """
    Extracts clobber and cpu usage info from command attributes.
    """
    res = {}
    clobber = attrs.get("clobber")
    if clobber:
        res["clobber"] = clobber
    usage = attrs.get("usage")
    if usage:
        cpu_percent = usage.get("cpu_percent")
        if cpu_percent:
            res["cpu_percent"] = int(round(cpu_percent))
    return res


def filter_args(command, argv, instance):
    """
    Given the full list of command-line arguments, remove anything up to and including `command`,
    and attempt to filter absolute pathnames out of any arguments after that.
    """

    # Each key is a pathname and the values are replacement sigils
    paths = {
        instance.topsrcdir: "$topsrcdir/",
        instance.topobjdir: "$topobjdir/",
        mozpath.normpath(os.path.expanduser("~")): "$HOME/",
        # This might override one of the existing entries, that's OK.
        # We don't use a sigil here because we treat all arguments as potentially relative
        # paths, so we'd like to get them back as they were specified.
        mozpath.normpath(os.getcwd()): "",
    }

    args = list(argv)
    while args:
        a = args.pop(0)
        if a == command:
            break

    def filter_path(p):
        p = mozpath.abspath(p)
        base = mozpath.basedir(p, paths.keys())
        if base:
            return paths[base] + mozpath.relpath(p, base)
        # Best-effort.
        return "<path omitted>"

    return [filter_path(arg) for arg in args]


def gather_telemetry(
    command, success, start_time, end_time, mach_context, instance, command_attrs
):
    """
    Gather telemetry about the build and the user's system and pass it to the telemetry
    handler to be stored for later submission.

    Any absolute paths on the command line will be made relative to a relevant base path
    or replaced with a placeholder to avoid including paths from developer's machines.
    """
    try:
        substs = instance.substs
    except BuildEnvironmentNotFoundException:
        substs = {}

    data = {
        "client_id": get_client_id(mach_context.state_dir),
        # Get an rfc3339 datetime string.
        "time": datetime.utcfromtimestamp(start_time).strftime("%Y-%m-%dT%H:%M:%S.%fZ"),
        "command": command,
        "argv": filter_args(command, sys.argv, instance),
        "success": success,
        # TODO: use a monotonic clock: https://bugzilla.mozilla.org/show_bug.cgi?id=1481624
        "duration_ms": int((end_time - start_time) * 1000),
        "build_opts": get_build_opts(substs),
        "build_attrs": get_build_attrs(command_attrs),
        "system": get_system_info(),
        # TODO: exception: https://bugzilla.mozilla.org/show_bug.cgi?id=1481617
        # TODO: file_types_changed: https://bugzilla.mozilla.org/show_bug.cgi?id=1481774
    }
    try:
        # Validate against the schema.
        schema(data)
        return data
    except MultipleInvalid as exc:
        msg = ["Build telemetry is invalid:"]
        for error in exc.errors:
            msg.append(str(error))
        print("\n".join(msg) + "\n" + pprint.pformat(data))
    return None


def verify_statedir(statedir):
    """
    Verifies the statedir is structured correctly. Returns the outgoing,
    submitted and log paths.

    Requires presence of the following directories; will raise if absent:
    - statedir/telemetry
    - statedir/telemetry/outgoing

    Creates the following directories and files if absent (first submission):
    - statedir/telemetry/submitted
    """

    telemetry_dir = os.path.join(statedir, "telemetry")
    outgoing = os.path.join(telemetry_dir, "outgoing")
    submitted = os.path.join(telemetry_dir, "submitted")
    telemetry_log = os.path.join(telemetry_dir, "telemetry.log")

    if not os.path.isdir(telemetry_dir):
        raise Exception("{} does not exist".format(telemetry_dir))

    if not os.path.isdir(outgoing):
        raise Exception("{} does not exist".format(outgoing))

    if not os.path.isdir(submitted):
        os.mkdir(submitted)

    return outgoing, submitted, telemetry_log


def get_distro_and_version():
    if sys.platform.startswith("linux"):
        dist, version, _ = distro.linux_distribution(full_distribution_name=False)
        return dist, version
    elif sys.platform.startswith("darwin"):
        return "macos", platform.mac_ver()[0]
    elif sys.platform.startswith("win32") or sys.platform.startswith("msys"):
        ver = sys.getwindowsversion()
        return "windows", "%s.%s.%s" % (ver.major, ver.minor, ver.build)
    else:
        return sys.platform, ""
