# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import division, absolute_import, print_function, unicode_literals

"""
This file contains functions used for telemetry.
"""

import distro
import os
import math
import platform
import sys

import mozpack.path as mozpath
from .base import BuildEnvironmentNotFoundException


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


def filter_args(command, argv, topsrcdir, topobjdir, cwd=None):
    """
    Given the full list of command-line arguments, remove anything up to and including `command`,
    and attempt to filter absolute pathnames out of any arguments after that.
    """
    if cwd is None:
        cwd = os.getcwd()

    # Each key is a pathname and the values are replacement sigils
    paths = {
        topsrcdir: "$topsrcdir/",
        topobjdir: "$topobjdir/",
        mozpath.normpath(os.path.expanduser("~")): "$HOME/",
        # This might override one of the existing entries, that's OK.
        # We don't use a sigil here because we treat all arguments as potentially relative
        # paths, so we'd like to get them back as they were specified.
        mozpath.normpath(cwd): "",
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


def get_shell_info():
    """Returns if the current shell was opened by vscode and if it's a SSH connection"""

    return True if "vscode" in os.getenv("TERM_PROGRAM", "") else False, os.getenv(
        "SSH_CLIENT", False
    )
