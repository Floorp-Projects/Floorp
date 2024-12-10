# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Given a library, dependentlibs.py prints the list of libraries it depends
upon that are in the same directory, followed by the library itself.
"""

import os
import re
import subprocess
import sys
import mozpack.path as mozpath
from collections import OrderedDict
from mozpack.executables import (
    get_type,
    ELF,
    MACHO,
)
from buildconfig import substs


def dependentlibs_win32_objdump(lib):
    proc = subprocess.Popen(
        [substs["LLVM_OBJDUMP"], "--private-headers", lib],
        stdout=subprocess.PIPE,
        universal_newlines=True,
    )
    deps = []
    for line in proc.stdout:
        match = re.match(r"\s+DLL Name: (\S+)", line)
        if match:
            # The DLL Name found might be mixed-case or upper-case. When cross-compiling,
            # the actual DLLs in dist/bin are all lowercase, whether they are produced
            # by the build system or copied from WIN32_REDIST_DIR. By turning everything
            # to lowercase, we ensure we always find the files.
            # At runtime, when Firefox reads the dependentlibs.list file on Windows, the
            # case doesn't matter.
            deps.append(match.group(1).lower())
    proc.wait()
    return deps


def dependentlibs_readelf(lib):
    """Returns the list of dependencies declared in the given ELF .so"""
    proc = subprocess.Popen(
        [substs.get("READELF", "readelf"), "-d", lib],
        stdout=subprocess.PIPE,
        universal_newlines=True,
    )
    deps = []
    for line in proc.stdout:
        # Each line has the following format:
        #  tag (TYPE)          value
        # or with BSD readelf:
        #  tag TYPE            value
        # Looking for NEEDED type entries
        tmp = line.strip().split(" ", 3)
        if len(tmp) > 3 and "NEEDED" in tmp[1]:
            # NEEDED lines look like:
            # 0x00000001 (NEEDED)             Shared library: [libname]
            # or with BSD readelf:
            # 0x00000001 NEEDED               Shared library: [libname]
            match = re.search(r"\[(.*)\]", tmp[3])
            if match:
                deps.append(match.group(1))
    proc.wait()
    return deps


def dependentlibs_mac_objdump(lib):
    """Returns the list of dependencies declared in the given MACH-O dylib"""
    proc = subprocess.Popen(
        [substs["LLVM_OBJDUMP"], "--private-headers", lib],
        stdout=subprocess.PIPE,
        universal_newlines=True,
    )
    deps = []
    cmd = None
    for line in proc.stdout:
        # llvm-objdump --private-headers output contains many different
        # things. The interesting data
        # is under "Load command n" sections, with the content:
        #           cmd LC_LOAD_DYLIB
        #       cmdsize 56
        #          name libname (offset 24)
        tmp = line.split()
        if len(tmp) < 2:
            continue
        if tmp[0] == "cmd":
            cmd = tmp[1]
        elif cmd == "LC_LOAD_DYLIB" and tmp[0] == "name":
            deps.append(re.sub("@(?:rpath|executable_path)/", "", tmp[1]))
    proc.wait()
    return deps


def is_skiplisted(dep):
    # Skip the ICU data DLL because preloading it at startup
    # leads to startup performance problems because of its excessive
    # size (around 10MB).
    if dep.startswith("icu"):
        return True
    # Skip the MSVC Runtimes. See bug #1921713.
    if substs.get("WIN32_REDIST_DIR"):
        for runtime in [
            "MSVC_C_RUNTIME_DLL",
            "MSVC_C_RUNTIME_1_DLL",
            "MSVC_CXX_RUNTIME_DLL",
        ]:
            dll = substs.get(runtime)
            if dll and dep == dll:
                return True


def dependentlibs(lib, libpaths, func):
    """For a given library, returns the list of recursive dependencies that can
    be found in the given list of paths, followed by the library itself."""
    assert libpaths
    assert isinstance(libpaths, list)
    deps = OrderedDict()
    for dep in func(lib):
        if dep in deps or os.path.isabs(dep):
            continue
        for dir in libpaths:
            deppath = os.path.join(dir, dep)
            if os.path.exists(deppath):
                deps.update(dependentlibs(deppath, libpaths, func))
                if not is_skiplisted(dep):
                    deps[dep] = deppath
                break

    return deps


def gen_list(output, lib):
    libpaths = [os.path.join(substs["DIST"], "bin")]
    binary_type = get_type(lib)
    if binary_type == ELF:
        func = dependentlibs_readelf
    elif binary_type == MACHO:
        func = dependentlibs_mac_objdump
    else:
        ext = os.path.splitext(lib)[1]
        assert ext == ".dll"
        func = dependentlibs_win32_objdump

    deps = dependentlibs(lib, libpaths, func)
    base_lib = mozpath.basename(lib)
    deps[base_lib] = mozpath.join(libpaths[0], base_lib)
    output.write("\n".join(deps.keys()) + "\n")

    with open(output.name + ".gtest", "w") as gtest_out:
        libs = list(deps.keys())
        libs[-1] = "gtest/" + libs[-1]
        gtest_out.write("\n".join(libs) + "\n")

    return set(deps.values())


def main():
    gen_list(sys.stdout, sys.argv[1])


if __name__ == "__main__":
    main()
