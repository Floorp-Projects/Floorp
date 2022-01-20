# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import struct
import subprocess
from io import BytesIO
from mozpack.errors import errors

MACHO_SIGNATURES = [
    0xFEEDFACE,  # mach-o 32-bits big endian
    0xCEFAEDFE,  # mach-o 32-bits little endian
    0xFEEDFACF,  # mach-o 64-bits big endian
    0xCFFAEDFE,  # mach-o 64-bits little endian
]

FAT_SIGNATURE = 0xCAFEBABE  # mach-o FAT binary

ELF_SIGNATURE = 0x7F454C46  # Elf binary

UNKNOWN = 0
MACHO = 1
ELF = 2


def get_type(path_or_fileobj):
    """
    Check the signature of the give file and returns what kind of executable
    matches.
    """
    if hasattr(path_or_fileobj, "peek"):
        f = BytesIO(path_or_fileobj.peek(8))
    elif hasattr(path_or_fileobj, "read"):
        f = path_or_fileobj
    else:
        f = open(path_or_fileobj, "rb")
    signature = f.read(4)
    if len(signature) < 4:
        return UNKNOWN
    signature = struct.unpack(">L", signature)[0]
    if signature == ELF_SIGNATURE:
        return ELF
    if signature in MACHO_SIGNATURES:
        return MACHO
    if signature != FAT_SIGNATURE:
        return UNKNOWN
    # We have to sanity check the second four bytes, because Java class
    # files use the same magic number as Mach-O fat binaries.
    # This logic is adapted from file(1), which says that Mach-O uses
    # these bytes to count the number of architectures within, while
    # Java uses it for a version number. Conveniently, there are only
    # 18 labelled Mach-O architectures, and Java's first released
    # class format used the version 43.0.
    num = f.read(4)
    if len(num) < 4:
        return UNKNOWN
    num = struct.unpack(">L", num)[0]
    if num < 20:
        return MACHO
    return UNKNOWN


def is_executable(path):
    """
    Return whether a given file path points to an executable or a library,
    where an executable or library is identified by:
        - the file extension on OS/2 and WINNT
        - the file signature on OS/X and ELF systems (GNU/Linux, Android, BSD,
          Solaris)

    As this function is intended for use to choose between the ExecutableFile
    and File classes in FileFinder, and choosing ExecutableFile only matters
    on OS/2, OS/X, ELF and WINNT (in GCC build) systems, we don't bother
    detecting other kind of executables.
    """
    from buildconfig import substs

    if not os.path.exists(path):
        return False

    if substs["OS_ARCH"] == "WINNT":
        return path.lower().endswith((substs["DLL_SUFFIX"], substs["BIN_SUFFIX"]))

    return get_type(path) != UNKNOWN


def may_strip(path):
    """
    Return whether strip() should be called
    """
    from buildconfig import substs

    # Bug 1658632: clang-11-based strip complains about d3dcompiler_47.dll.
    # It's not clear why this happens, but as a quick fix just avoid stripping
    # this DLL. It's not from our build anyway.
    if "d3dcompiler" in path:
        return False
    return bool(substs.get("PKG_STRIP"))


def strip(path):
    """
    Execute the STRIP command with STRIP_FLAGS on the given path.
    """
    from buildconfig import substs

    strip = substs["STRIP"]
    flags = substs.get("STRIP_FLAGS", [])
    cmd = [strip] + flags + [path]
    if subprocess.call(cmd) != 0:
        errors.fatal("Error executing " + " ".join(cmd))


def may_elfhack(path):
    """
    Return whether elfhack() should be called
    """
    # elfhack only supports libraries. We should check the ELF header for
    # the right flag, but checking the file extension works too.
    from buildconfig import substs

    return (
        "USE_ELF_HACK" in substs
        and substs["USE_ELF_HACK"]
        and path.endswith(substs["DLL_SUFFIX"])
        and "COMPILE_ENVIRONMENT" in substs
        and substs["COMPILE_ENVIRONMENT"]
    )


def elfhack(path):
    """
    Execute the elfhack command on the given path.
    """
    from buildconfig import topobjdir

    cmd = [os.path.join(topobjdir, "build/unix/elfhack/elfhack"), path]
    if subprocess.call(cmd) != 0:
        errors.fatal("Error executing " + " ".join(cmd))
