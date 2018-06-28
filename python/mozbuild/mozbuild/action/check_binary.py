# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import argparse
import os
import subprocess
import sys

from distutils.version import StrictVersion as Version

import buildconfig
from mozbuild.util import memoize
from mozpack.executables import (
    get_type,
    ELF,
    MACHO,
)


STDCXX_MAX_VERSION = Version('3.4.16')
GLIBC_MAX_VERSION = Version('2.12')

HOST = {
    'MOZ_LIBSTDCXX_VERSION':
        buildconfig.substs.get('MOZ_LIBSTDCXX_HOST_VERSION'),
    'platform': buildconfig.substs['HOST_OS_ARCH'],
    'readelf': 'readelf',
    'nm': 'nm',
}

TARGET = {
    'MOZ_LIBSTDCXX_VERSION':
        buildconfig.substs.get('MOZ_LIBSTDCXX_TARGET_VERSION'),
    'platform': buildconfig.substs['OS_TARGET'],
    'readelf': '{}readelf'.format(
        buildconfig.substs.get('TOOLCHAIN_PREFIX', '')),
    'nm': '{}nm'.format(buildconfig.substs.get('TOOLCHAIN_PREFIX', '')),
}

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


def iter_readelf_symbols(target, binary):
    for line in get_output(target['readelf'], '-sW', binary):
        data = line.split()
        if len(data) >= 8 and data[0].endswith(':') and data[0][:-1].isdigit():
            n, addr, size, type, bind, vis, index, name = data[:8]
            if '@' in name:
                name, ver = name.rsplit('@', 1)
                while name.endswith('@'):
                    name = name[:-1]
            else:
                ver = None
            yield {
                'addr': int(addr, 16),
                # readelf output may contain decimal values or hexadecimal
                # values prefixed with 0x for the size. Let python autodetect.
                'size': int(size, 0),
                'type': type,
                'binding': bind,
                'visibility': vis,
                'index': index,
                'name': name,
                'version': ver,
            }


def iter_readelf_dynamic(target, binary):
    for line in get_output(target['readelf'], '-d', binary):
        data = line.split(None, 2)
        if data and data[0].startswith('0x'):
            yield data[1].rstrip(')').lstrip('('), data[2]


def check_dep_versions(target, binary, lib, prefix, max_version):
    if get_type(binary) != ELF:
        raise Skip()
    unwanted = []
    prefix = prefix + '_'
    try:
        for sym in at_least_one(iter_readelf_symbols(target, binary)):
            if sym['index'] == 'UND' and sym['version'] and \
                    sym['version'].startswith(prefix):
                version = Version(sym['version'][len(prefix):])
                if version > max_version:
                    unwanted.append(sym)
    except Empty:
        raise RuntimeError('Could not parse readelf output?')
    if unwanted:
        raise RuntimeError('\n'.join([
            'We do not want these {} symbol versions to be used:'.format(lib)
        ] + [
            ' {} ({})'.format(s['name'], s['version']) for s in unwanted
        ]))


def check_stdcxx(target, binary):
    check_dep_versions(
        target, binary, 'libstdc++', 'GLIBCXX', STDCXX_MAX_VERSION)


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


def check_nsmodules(target, binary):
    if target is HOST or not is_libxul(binary):
        raise Skip()
    symbols = []
    if buildconfig.substs.get('_MSC_VER'):
        for line in get_output('dumpbin', '-exports', binary):
            data = line.split(None, 3)
            if data and len(data) == 4 and data[0].isdigit() and \
                    ishex(data[1]) and ishex(data[2]):
                # - Some symbols in the table can be aliases, and appear as
                #   `foo = bar`.
                # - The MSVC mangling has some type info following `@@`
                # - Any namespacing that can happen on the symbol appears as a
                #   suffix, after a `@`.
                # - Mangled symbols are prefixed with `?`.
                name = data[3].split(' = ')[0].split('@@')[0].split('@')[0] \
                              .lstrip('?')
                if name.endswith('_NSModule') or name in (
                        '__start_kPStaticModules',
                        '__stop_kPStaticModules'):
                    symbols.append((int(data[2], 16), GUESSED_NSMODULE_SIZE,
                                    name))
    else:
        for line in get_output(target['nm'], '-P', binary):
            data = line.split()
            # Some symbols may not have a size listed at all.
            if len(data) == 3:
                data.append('0')
            if len(data) == 4:
                sym, _, addr, size = data
                # NSModules symbols end with _NSModule or _NSModuleE when
                # C++-mangled.
                if sym.endswith(('_NSModule', '_NSModuleE')):
                    # On mac, nm doesn't actually print anything other than 0
                    # for the size. So take our best guess.
                    size = int(size, 16) or GUESSED_NSMODULE_SIZE
                    symbols.append((int(addr, 16), size, sym))
                elif sym.endswith(('__start_kPStaticModules',
                                   '__stop_kPStaticModules')):
                    # On ELF and mac systems, these symbols have no size, such
                    # that the first actual NSModule has the same address as
                    # the start symbol.
                    symbols.append((int(addr, 16), 0, sym))
    if not symbols:
        raise RuntimeError('Could not find NSModules')

    def print_symbols(symbols):
        for addr, size, sym in symbols:
            print('%x %d %s' % (addr, size, sym))

    symbols = sorted(symbols)
    next_addr = None
    # MSVC linker, when doing incremental linking, adds padding when
    # merging sections. Allow there to be more space between the NSModule
    # symbols, as long as they are in the right order.
    if buildconfig.substs.get('_MSC_VER') and \
            buildconfig.substs.get('DEVELOPER_OPTIONS'):
        sym_cmp = lambda guessed, actual: guessed <= actual
    else:
        sym_cmp = lambda guessed, actual: guessed == actual

    for addr, size, sym in symbols:
        if next_addr is not None and not sym_cmp(next_addr, addr):
            print_symbols(symbols)
            raise RuntimeError('NSModules are not adjacent')
        next_addr = addr + size

    # The mac linker doesn't emit the start/stop symbols in the symbol table.
    # We'll just assume it did the job correctly.
    if get_type(binary) == MACHO:
        return

    first = symbols[0][2]
    last = symbols[-1][2]
    # On some platforms, there are extra underscores on symbol names.
    if first.lstrip('_') != 'start_kPStaticModules' or \
            last.lstrip('_') != 'stop_kPStaticModules':
        print_symbols(symbols)
        syms = set(sym for add, size, sym in symbols)
        if 'start_kPStaticModules' not in syms:
            raise RuntimeError('Could not find start_kPStaticModules symbol')
        if 'stop_kPStaticModules' not in syms:
            raise RuntimeError('Could not find stop_kPStaticModules symbol')
        raise RuntimeError('NSModules are not ordered appropriately')


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


def checks(target, binary):
    # The clang-plugin is built as target but is really a host binary.
    # Cheat and pretend we were passed the right argument.
    if 'clang-plugin' in binary:
        target = HOST
    checks = []
    if target['MOZ_LIBSTDCXX_VERSION']:
        checks.append(check_stdcxx)
        checks.append(check_glibc)
    checks.append(check_textrel)
    checks.append(check_nsmodules)
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

    parser.add_argument('binary', metavar='PATH',
                        help='Location of the binary to check')

    options = parser.parse_args(args)

    if options.host == options.target:
        print('Exactly one of --host or --target must be given',
              file=sys.stderr)
        return 1

    if options.host:
        return checks(HOST, options.binary)
    elif options.target:
        return checks(TARGET, options.binary)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
