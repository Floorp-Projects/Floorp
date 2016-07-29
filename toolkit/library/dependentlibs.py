# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''Given a library, dependentlibs.py prints the list of libraries it depends
upon that are in the same directory, followed by the library itself.
'''

import os
import re
import subprocess
import sys
import mozpack.path as mozpath
from mozpack.executables import (
    get_type,
    ELF,
    MACHO,
)
from buildconfig import substs

def dependentlibs_dumpbin(lib):
    '''Returns the list of dependencies declared in the given DLL'''
    try:
        proc = subprocess.Popen(['dumpbin', '-dependents', lib], stdout = subprocess.PIPE)
    except OSError:
        # dumpbin is missing, probably mingw compilation. Try using objdump.
        return dependentlibs_mingw_objdump(lib)
    deps = []
    for line in proc.stdout:
        # Each line containing an imported library name starts with 4 spaces
        match = re.match('    (\S+)', line)
        if match:
             deps.append(match.group(1))
        elif len(deps):
             # There may be several groups of library names, but only the
             # first one is interesting. The second one is for delayload-ed
             # libraries.
             break
    proc.wait()
    return deps

def dependentlibs_mingw_objdump(lib):
    proc = subprocess.Popen(['objdump', '-x', lib], stdout = subprocess.PIPE)
    deps = []
    for line in proc.stdout:
        match = re.match('\tDLL Name: (\S+)', line)
        if match:
            deps.append(match.group(1))
    proc.wait()
    return deps

def dependentlibs_readelf(lib):
    '''Returns the list of dependencies declared in the given ELF .so'''
    proc = subprocess.Popen([substs.get('TOOLCHAIN_PREFIX', '') + 'readelf', '-d', lib], stdout = subprocess.PIPE)
    deps = []
    for line in proc.stdout:
        # Each line has the following format:
        #  tag (TYPE)          value
        # Looking for NEEDED type entries
        tmp = line.split(' ', 3)
        if len(tmp) > 3 and tmp[2] == '(NEEDED)':
            # NEEDED lines look like:
            # 0x00000001 (NEEDED)             Shared library: [libname]
            match = re.search('\[(.*)\]', tmp[3])
            if match:
                deps.append(match.group(1))
    proc.wait()
    return deps

def dependentlibs_otool(lib):
    '''Returns the list of dependencies declared in the given MACH-O dylib'''
    proc = subprocess.Popen([substs['OTOOL'], '-l', lib], stdout = subprocess.PIPE)
    deps= []
    cmd = None
    for line in proc.stdout:
        # otool -l output contains many different things. The interesting data
        # is under "Load command n" sections, with the content:
        #           cmd LC_LOAD_DYLIB
        #       cmdsize 56
        #          name libname (offset 24)
        tmp = line.split()
        if len(tmp) < 2:
            continue
        if tmp[0] == 'cmd':
            cmd = tmp[1]
        elif cmd == 'LC_LOAD_DYLIB' and tmp[0] == 'name':
            deps.append(re.sub('^@executable_path/','',tmp[1]))
    proc.wait()
    return deps

def dependentlibs(lib, libpaths, func):
    '''For a given library, returns the list of recursive dependencies that can
    be found in the given list of paths, followed by the library itself.'''
    assert(libpaths)
    assert(isinstance(libpaths, list))
    deps = {}
    for dep in func(lib):
        if dep in deps or os.path.isabs(dep):
            continue
        for dir in libpaths:
            deppath = os.path.join(dir, dep)
            if os.path.exists(deppath):
                deps.update(dependentlibs(deppath, libpaths, func))
                # Black list the ICU data DLL because preloading it at startup
                # leads to startup performance problems because of its excessive
                # size (around 10MB).
                if not dep.startswith("icu"):
                    deps[dep] = deppath
                break

    return deps

def gen_list(output, lib):
    libpaths = [os.path.join(substs['DIST'], 'bin')]
    binary_type = get_type(lib)
    if binary_type == ELF:
        func = dependentlibs_readelf
    elif binary_type == MACHO:
        func = dependentlibs_otool
    else:
        ext = os.path.splitext(lib)[1]
        assert(ext == '.dll')
        func = dependentlibs_dumpbin

    deps = dependentlibs(lib, libpaths, func)
    deps[lib] = mozpath.join(libpaths[0], lib)
    output.write('\n'.join(deps.keys()) + '\n')
    return set(deps.values())

def main():
    gen_list(sys.stdout, sys.argv[1])

if __name__ == '__main__':
    main()
