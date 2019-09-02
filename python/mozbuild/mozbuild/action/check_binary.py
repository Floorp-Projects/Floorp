# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import os
import re
import subprocess
import sys

from distutils.version import StrictVersion as Version

import buildconfig
from mozbuild.util import memoize
from mozpack.executables import (
    get_type,
    ELF,
    MACHO,
    UNKNOWN,
)


STDCXX_MAX_VERSION = Version('3.4.17')
GLIBC_MAX_VERSION = Version('2.12')
LIBGCC_MAX_VERSION = Version('4.8')

HOST = {
    'MOZ_LIBSTDCXX_VERSION':
        buildconfig.substs.get('MOZ_LIBSTDCXX_HOST_VERSION'),
    'platform': buildconfig.substs['HOST_OS_ARCH'],
    'readelf': 'readelf',
}

TARGET = {
    'MOZ_LIBSTDCXX_VERSION':
        buildconfig.substs.get('MOZ_LIBSTDCXX_TARGET_VERSION'),
    'platform': buildconfig.substs['OS_TARGET'],
    'readelf': '{}readelf'.format(
        buildconfig.substs.get('TOOLCHAIN_PREFIX', '')),
}

ADDR_RE = re.compile(r'[0-9a-f]{8,16}')

if buildconfig.substs.get('HAVE_64BIT_BUILD'):
    GUESSED_NSMODULE_SIZE = 8
else:
    GUESSED_NSMODULE_SIZE = 4


get_type = memoize(get_type)


@memoize
def get_output(*cmd):
    env = dict(os.environ)
    env[b'LC_ALL'] = b'C'
    return subprocess.check_output(cmd, env=env).splitlines()


class Skip(RuntimeError):
    pass


class Empty(RuntimeError):
    pass


def at_least_one(iter):
    saw_one = False
    for item in iter:
        saw_one = True
        yield item
    if not saw_one:
        raise Empty()


# Iterates the symbol table on ELF and MACHO, and the export table on
# COFF/PE.
def iter_symbols(binary):
    ty = get_type(binary)
    # XXX: Static libraries on ELF, MACHO and COFF/PE systems are all
    # ar archives. So technically speaking, the following is wrong
    # but is enough for now. llvm-objdump -t can actually be used on all
    # platforms for static libraries, but its format is different for
    # Windows .obj files, so the following won't work for them, but
    # it currently doesn't matter.
    if ty == UNKNOWN and open(binary).read(8) == '!<arch>\n':
        ty = ELF
    if ty in (ELF, MACHO):
        for line in get_output(buildconfig.substs['LLVM_OBJDUMP'], '-t',
                               binary):
            m = ADDR_RE.match(line)
            if not m:
                continue
            addr = int(m.group(0), 16)
            # The second "column" is 7 one-character items that can be
            # whitespaces. We don't have use for their value, so just skip
            # those.
            rest = line[m.end() + 9:].split()
            # The number of remaining colums will vary between ELF and MACHO.
            # On ELF, we have:
            #   Section Size .hidden? Name
            # On Macho, the size is skipped.
            # In every case, the symbol name is last.
            name = rest[-1]
            if '@' in name:
                name, ver = name.rsplit('@', 1)
                while name.endswith('@'):
                    name = name[:-1]
            else:
                ver = None
            yield {
                'addr': addr,
                'size': int(rest[1], 16) if ty == ELF else 0,
                'name': name,
                'version': ver or None,
            }
    else:
        export_table = False
        for line in get_output(buildconfig.substs['LLVM_OBJDUMP'], '-p',
                               binary):
            if line.strip() == 'Export Table:':
                export_table = True
                continue
            elif not export_table:
                continue

            cols = line.split()
            # The data we're interested in comes in 3 columns, and the first
            # column is a number.
            if len(cols) != 3 or not cols[0].isdigit():
                continue
            _, rva, name = cols
            # - The MSVC mangling has some type info following `@@`
            # - Any namespacing that can happen on the symbol appears as a
            #   suffix, after a `@`.
            # - Mangled symbols are prefixed with `?`.
            name = name.split('@@')[0].split('@')[0].lstrip('?')
            yield {
                'addr': int(rva, 16),
                'size': 0,
                'name': name,
                'version': None,
            }


def iter_readelf_dynamic(target, binary):
    for line in get_output(target['readelf'], '-d', binary):
        data = line.split(None, 2)
        if data and len(data) == 3 and data[0].startswith('0x'):
            yield data[1].rstrip(')').lstrip('('), data[2]


def check_dep_versions(target, binary, lib, prefix, max_version):
    if get_type(binary) != ELF:
        raise Skip()
    unwanted = []
    prefix = prefix + '_'
    try:
        for sym in at_least_one(iter_symbols(binary)):
            if sym['addr'] == 0 and sym['version'] and \
                    sym['version'].startswith(prefix):
                version = Version(sym['version'][len(prefix):])
                if version > max_version:
                    unwanted.append(sym)
    except Empty:
        raise RuntimeError('Could not parse llvm-objdump output?')
    if unwanted:
        raise RuntimeError('\n'.join([
            'We do not want these {} symbol versions to be used:'.format(lib)
        ] + [
            ' {} ({})'.format(s['name'], s['version']) for s in unwanted
        ]))


def check_stdcxx(target, binary):
    check_dep_versions(
        target, binary, 'libstdc++', 'GLIBCXX', STDCXX_MAX_VERSION)


def check_libgcc(target, binary):
    check_dep_versions(target, binary, 'libgcc', 'GCC', LIBGCC_MAX_VERSION)


def check_glibc(target, binary):
    check_dep_versions(target, binary, 'libc', 'GLIBC', GLIBC_MAX_VERSION)


def check_textrel(target, binary):
    if target is HOST or get_type(binary) != ELF:
        raise Skip()
    try:
        for tag, value in at_least_one(iter_readelf_dynamic(target, binary)):
            if tag == 'TEXTREL' or (tag == 'FLAGS' and 'TEXTREL' in value):
                raise RuntimeError(
                    'We do not want text relocations in libraries and programs'
                )
    except Empty:
        raise RuntimeError('Could not parse readelf output?')


def ishex(s):
    try:
        int(s, 16)
        return True
    except ValueError:
        return False


def is_libxul(binary):
    basename = os.path.basename(binary).lower()
    return 'xul' in basename


def check_pt_load(target, binary):
    if target is HOST or get_type(binary) != ELF or not is_libxul(binary):
        raise Skip()
    count = 0
    for line in get_output(target['readelf'], '-l', binary):
        data = line.split()
        if data and data[0] == 'LOAD':
            count += 1
    if count <= 1:
        raise RuntimeError('Expected more than one PT_LOAD segment')


def check_mozglue_order(target, binary):
    if target is HOST or target['platform'] != 'Android':
        raise Skip()
    # While this is very unlikely (libc being added by the compiler at the end
    # of the linker command line), if libmozglue.so ends up after libc.so, all
    # hell breaks loose, so better safe than sorry, and check it's actually the
    # case.
    try:
        mozglue = libc = None
        for n, (tag, value) in enumerate(
                at_least_one(iter_readelf_dynamic(target, binary))):
            if tag == 'NEEDED':
                if '[libmozglue.so]' in value:
                    mozglue = n
                elif '[libc.so]' in value:
                    libc = n
        if libc is None:
            raise RuntimeError('libc.so is not linked?')
        if mozglue is not None and libc < mozglue:
            raise RuntimeError('libmozglue.so must be linked before libc.so')
    except Empty:
        raise RuntimeError('Could not parse readelf output?')


def check_networking(binary):
    retcode = 0
    networking_functions = set([
        # socketpair is not concerning; it is restricted to AF_UNIX
        "connect", "accept", "listen", "getsockname", "getsockopt",
        "recv", "send",
        # We would be concerned by recvmsg and sendmsg; but we believe
        # they are okay as documented in 1376621#c23
        "gethostbyname", "gethostbyaddr", "gethostent", "sethostent", "endhostent",
        "gethostent_r", "gethostbyname2", "gethostbyaddr_r", "gethostbyname_r",
        "gethostbyname2_r",
        "getservent", "getservbyname", "getservbyport", "setservent",
        "getprotoent", "getprotobyname", "getprotobynumber", "setprotoent",
        "endprotoent"])
    bad_occurences_names = set()

    try:
        for sym in at_least_one(iter_symbols(binary)):
            if sym['addr'] == 0 and sym['name'] in networking_functions:
                bad_occurences_names.add(sym['name'])
    except Empty:
        raise RuntimeError('Could not parse llvm-objdump output?')

    basename = os.path.basename(binary)
    if bad_occurences_names:
        s = 'TEST-UNEXPECTED-FAIL | check_networking | {} | Identified {} ' + \
            'networking function(s) being imported in the rust static library ({})'
        print(s.format(basename, len(bad_occurences_names),
                       ",".join(sorted(bad_occurences_names))),
              file=sys.stderr)
        retcode = 1
    elif buildconfig.substs.get('MOZ_AUTOMATION'):
        print('TEST-PASS | check_networking | {}'.format(basename))
    return retcode


def checks(target, binary):
    # The clang-plugin is built as target but is really a host binary.
    # Cheat and pretend we were passed the right argument.
    if 'clang-plugin' in binary:
        target = HOST
    checks = []
    if target['MOZ_LIBSTDCXX_VERSION']:
        checks.append(check_stdcxx)
        checks.append(check_libgcc)
        checks.append(check_glibc)

    # Disabled for local builds because of readelf performance: See bug 1472496
    if not buildconfig.substs.get('DEVELOPER_OPTIONS'):
        checks.append(check_textrel)
        checks.append(check_pt_load)
        checks.append(check_mozglue_order)

    retcode = 0
    basename = os.path.basename(binary)
    for c in checks:
        try:
            name = c.__name__
            c(target, binary)
            if buildconfig.substs.get('MOZ_AUTOMATION'):
                print('TEST-PASS | {} | {}'.format(name, basename))
        except Skip:
            pass
        except RuntimeError as e:
            print('TEST-UNEXPECTED-FAIL | {} | {} | {}'
                  .format(name, basename, e.message),
                  file=sys.stderr)
            retcode = 1
    return retcode


def main(args):
    parser = argparse.ArgumentParser(
        description='Check built binaries')

    parser.add_argument('--host', action='store_true',
                        help='Perform checks for a host binary')
    parser.add_argument('--target', action='store_true',
                        help='Perform checks for a target binary')
    parser.add_argument('--networking', action='store_true',
                        help='Perform checks for networking functions')

    parser.add_argument('binary', metavar='PATH',
                        help='Location of the binary to check')

    options = parser.parse_args(args)

    if options.host == options.target:
        print('Exactly one of --host or --target must be given',
              file=sys.stderr)
        return 1

    if options.networking and options.host:
        print('--networking is only valid with --target',
              file=sys.stderr)
        return 1

    if options.networking:
        return check_networking(options.binary)
    elif options.host:
        return checks(HOST, options.binary)
    elif options.target:
        return checks(TARGET, options.binary)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
