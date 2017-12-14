# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging
import os

from StringIO import StringIO

from mozunit import main

from common import BaseConfigureTest
from mozbuild.configure.util import Version
from mozbuild.util import memoize
from mozpack import path as mozpath
from test_toolchain_helpers import (
    FakeCompiler,
    CompilerResult,
)


DEFAULT_C99 = {
    '__STDC_VERSION__': '199901L',
}

DEFAULT_C11 = {
    '__STDC_VERSION__': '201112L',
}

DEFAULT_CXX_97 = {
    '__cplusplus': '199711L',
}

DEFAULT_CXX_11 = {
    '__cplusplus': '201103L',
}

DRAFT_CXX_14 = {
    '__cplusplus': '201300L',
}

DEFAULT_CXX_14 = {
    '__cplusplus': '201402L',
}

SUPPORTS_GNU99 = {
    '-std=gnu99': DEFAULT_C99,
}

SUPPORTS_GNUXX11 = {
    '-std=gnu++11': DEFAULT_CXX_11,
}

SUPPORTS_GNUXX14 = {
    '-std=gnu++14': DEFAULT_CXX_14,
}

SUPPORTS_CXX14 = {
    '-std=c++14': DEFAULT_CXX_14,
}


@memoize
def GCC_BASE(version):
    version = Version(version)
    return FakeCompiler({
        '__GNUC__': version.major,
        '__GNUC_MINOR__': version.minor,
        '__GNUC_PATCHLEVEL__': version.patch,
        '__STDC__': 1,
        '__ORDER_LITTLE_ENDIAN__': 1234,
        '__ORDER_BIG_ENDIAN__': 4321,
    })


@memoize
def GCC(version):
    return GCC_BASE(version) + SUPPORTS_GNU99


@memoize
def GXX(version):
    return GCC_BASE(version) + DEFAULT_CXX_97 + SUPPORTS_GNUXX11

SUPPORTS_DRAFT_CXX14_VERSION = {
    '-std=gnu++14': DRAFT_CXX_14,
}

SUPPORTS_DRAFT_CXX14_VERSION = {
    '-std=gnu++14': DRAFT_CXX_14,
}

GCC_4_7 = GCC('4.7.3')
GXX_4_7 = GXX('4.7.3')
GCC_4_9 = GCC('4.9.3')
GXX_4_9 = GXX('4.9.3') + SUPPORTS_DRAFT_CXX14_VERSION
GCC_5 = GCC('5.2.1') + DEFAULT_C11
GXX_5 = GXX('5.2.1') + SUPPORTS_GNUXX14

GCC_PLATFORM_LITTLE_ENDIAN = {
    '__BYTE_ORDER__': 1234,
}

GCC_PLATFORM_BIG_ENDIAN = {
    '__BYTE_ORDER__': 4321,
}

GCC_PLATFORM_X86 = FakeCompiler(GCC_PLATFORM_LITTLE_ENDIAN) + {
    None: {
        '__i386__': 1,
    },
    '-m64': {
        '__i386__': False,
        '__x86_64__': 1,
    },
}

GCC_PLATFORM_X86_64 = FakeCompiler(GCC_PLATFORM_LITTLE_ENDIAN) + {
    None: {
        '__x86_64__': 1,
    },
    '-m32': {
        '__x86_64__': False,
        '__i386__': 1,
    },
}

GCC_PLATFORM_ARM = FakeCompiler(GCC_PLATFORM_LITTLE_ENDIAN) + {
    '__arm__': 1,
}

GCC_PLATFORM_LINUX = {
    '__linux__': 1,
}

GCC_PLATFORM_DARWIN = {
    '__APPLE__': 1,
}

GCC_PLATFORM_WIN = {
    '_WIN32': 1,
    'WINNT': 1,
}

GCC_PLATFORM_OPENBSD = {
    '__OpenBSD__': 1,
}

GCC_PLATFORM_X86_LINUX = FakeCompiler(GCC_PLATFORM_X86, GCC_PLATFORM_LINUX)
GCC_PLATFORM_X86_64_LINUX = FakeCompiler(GCC_PLATFORM_X86_64,
                                         GCC_PLATFORM_LINUX)
GCC_PLATFORM_ARM_LINUX = FakeCompiler(GCC_PLATFORM_ARM, GCC_PLATFORM_LINUX)
GCC_PLATFORM_X86_OSX = FakeCompiler(GCC_PLATFORM_X86, GCC_PLATFORM_DARWIN)
GCC_PLATFORM_X86_64_OSX = FakeCompiler(GCC_PLATFORM_X86_64,
                                       GCC_PLATFORM_DARWIN)
GCC_PLATFORM_X86_WIN = FakeCompiler(GCC_PLATFORM_X86, GCC_PLATFORM_WIN)
GCC_PLATFORM_X86_64_WIN = FakeCompiler(GCC_PLATFORM_X86_64, GCC_PLATFORM_WIN)


@memoize
def CLANG_BASE(version):
    version = Version(version)
    return FakeCompiler({
        '__clang__': 1,
        '__clang_major__': version.major,
        '__clang_minor__': version.minor,
        '__clang_patchlevel__': version.patch,
    })


@memoize
def CLANG(version):
    return GCC_BASE('4.2.1') + CLANG_BASE(version) + SUPPORTS_GNU99


@memoize
def CLANGXX(version):
    return (GCC_BASE('4.2.1') + CLANG_BASE(version) + DEFAULT_CXX_97 +
            SUPPORTS_GNUXX11 + SUPPORTS_GNUXX14)


CLANG_3_3 = CLANG('3.3.0') + DEFAULT_C99
CLANGXX_3_3 = CLANGXX('3.3.0')
CLANG_3_6 = CLANG('3.6.2') + DEFAULT_C11
CLANGXX_3_6 = CLANGXX('3.6.2') + {
    '-std=gnu++11': {
        '__has_feature(cxx_alignof)': '1',
    },
    '-std=gnu++14': {
        '__has_feature(cxx_alignof)': '1',
    },
}


def CLANG_PLATFORM(gcc_platform):
    base = {
        '--target=x86_64-linux-gnu': GCC_PLATFORM_X86_64_LINUX[None],
        '--target=x86_64-darwin11.2.0': GCC_PLATFORM_X86_64_OSX[None],
        '--target=i686-linux-gnu': GCC_PLATFORM_X86_LINUX[None],
        '--target=i686-darwin11.2.0': GCC_PLATFORM_X86_OSX[None],
        '--target=arm-linux-gnu': GCC_PLATFORM_ARM_LINUX[None],
    }
    undo_gcc_platform = {
        k: {symbol: False for symbol in gcc_platform[None]}
        for k in base
    }
    return FakeCompiler(gcc_platform, undo_gcc_platform, base)


CLANG_PLATFORM_X86_LINUX = CLANG_PLATFORM(GCC_PLATFORM_X86_LINUX)
CLANG_PLATFORM_X86_64_LINUX = CLANG_PLATFORM(GCC_PLATFORM_X86_64_LINUX)
CLANG_PLATFORM_X86_OSX = CLANG_PLATFORM(GCC_PLATFORM_X86_OSX)
CLANG_PLATFORM_X86_64_OSX = CLANG_PLATFORM(GCC_PLATFORM_X86_64_OSX)
CLANG_PLATFORM_X86_WIN = CLANG_PLATFORM(GCC_PLATFORM_X86_WIN)
CLANG_PLATFORM_X86_64_WIN = CLANG_PLATFORM(GCC_PLATFORM_X86_64_WIN)


@memoize
def VS(version):
    version = Version(version)
    return FakeCompiler({
        None: {
            '_MSC_VER': '%02d%02d' % (version.major, version.minor),
            '_MSC_FULL_VER': '%02d%02d%05d' % (version.major, version.minor,
                                               version.patch),
        },
        '*.cpp': DEFAULT_CXX_97,
    })


VS_2013u2 = VS('18.00.30501')
VS_2013u3 = VS('18.00.30723')
VS_2015 = VS('19.00.23026')
VS_2015u1 = VS('19.00.23506')
VS_2015u2 = VS('19.00.23918')
VS_2015u3 = VS('19.00.24213')

VS_PLATFORM_X86 = {
    '_M_IX86': 600,
    '_WIN32': 1,
}

VS_PLATFORM_X86_64 = {
    '_M_X64': 100,
    '_WIN32': 1,
    '_WIN64': 1,
}

# Note: In reality, the -std=gnu* options are only supported when preceded by
# -Xclang.
CLANG_CL_3_9 = (CLANG_BASE('3.9.0') + VS('18.00.00000') + DEFAULT_C11 +
                SUPPORTS_GNU99 + SUPPORTS_GNUXX11 + SUPPORTS_CXX14) + {
    '*.cpp': {
        '__STDC_VERSION__': False,
        '__cplusplus': '201103L',
    },
    '-fms-compatibility-version=19.11.25547': VS('19.11.25547')[None],
}

CLANG_CL_PLATFORM_X86 = FakeCompiler(VS_PLATFORM_X86, GCC_PLATFORM_X86[None])
CLANG_CL_PLATFORM_X86_64 = FakeCompiler(VS_PLATFORM_X86_64, GCC_PLATFORM_X86_64[None])

LIBRARY_NAME_INFOS = {
    'linux-gnu': {
        'DLL_PREFIX': 'lib',
        'DLL_SUFFIX': '.so',
        'LIB_PREFIX': 'lib',
        'LIB_SUFFIX': 'a',
        'IMPORT_LIB_SUFFIX': '',
        'RUST_LIB_PREFIX': 'lib',
        'RUST_LIB_SUFFIX': 'a',
        'OBJ_SUFFIX': 'o',
    },
    'darwin11.2.0': {
        'DLL_PREFIX': 'lib',
        'DLL_SUFFIX': '.dylib',
        'LIB_PREFIX': 'lib',
        'LIB_SUFFIX': 'a',
        'IMPORT_LIB_SUFFIX': '',
        'RUST_LIB_PREFIX': 'lib',
        'RUST_LIB_SUFFIX': 'a',
        'OBJ_SUFFIX': 'o',
    },
    'mingw32': {
        'DLL_PREFIX': '',
        'DLL_SUFFIX': '.dll',
        'LIB_PREFIX': 'lib',
        'LIB_SUFFIX': 'a',
        'IMPORT_LIB_SUFFIX': 'a',
        'RUST_LIB_PREFIX': '',
        'RUST_LIB_SUFFIX': 'lib',
        'OBJ_SUFFIX': 'o',
    },
    'msvc': {
        'DLL_PREFIX': '',
        'DLL_SUFFIX': '.dll',
        'LIB_PREFIX': '',
        'LIB_SUFFIX': 'lib',
        'IMPORT_LIB_SUFFIX': 'lib',
        'RUST_LIB_PREFIX': '',
        'RUST_LIB_SUFFIX': 'lib',
        'OBJ_SUFFIX': 'obj',
    },
    'openbsd6.1': {
        'DLL_PREFIX': 'lib',
        'DLL_SUFFIX': '.so.1.0',
        'LIB_PREFIX': 'lib',
        'LIB_SUFFIX': 'a',
        'IMPORT_LIB_SUFFIX': '',
        'RUST_LIB_PREFIX': 'lib',
        'RUST_LIB_SUFFIX': 'a',
        'OBJ_SUFFIX': 'o',
    },
}


class BaseToolchainTest(BaseConfigureTest):
    def setUp(self):
        super(BaseToolchainTest, self).setUp()
        self.out = StringIO()
        self.logger = logging.getLogger('BaseToolchainTest')
        self.logger.setLevel(logging.ERROR)
        self.handler = logging.StreamHandler(self.out)
        self.logger.addHandler(self.handler)

    def tearDown(self):
        self.logger.removeHandler(self.handler)
        del self.handler
        del self.out
        super(BaseToolchainTest, self).tearDown()

    def do_toolchain_test(self, paths, results, args=[], environ={}):
        '''Helper to test the toolchain checks from toolchain.configure.

        - `paths` is a dict associating compiler paths to FakeCompiler
          definitions from above.
        - `results` is a dict associating result variable names from
          toolchain.configure (c_compiler, cxx_compiler, host_c_compiler,
          host_cxx_compiler) with a result.
          The result can either be an error string, or a CompilerResult
          corresponding to the object returned by toolchain.configure checks.
          When the results for host_c_compiler are identical to c_compiler,
          they can be omitted. Likewise for host_cxx_compiler vs.
          cxx_compiler.
        '''
        environ = dict(environ)
        if 'PATH' not in environ:
            environ['PATH'] = os.pathsep.join(
                mozpath.abspath(p) for p in ('/bin', '/usr/bin'))

        sandbox = self.get_sandbox(paths, {}, args, environ,
                                   logger=self.logger)

        for var in ('c_compiler', 'cxx_compiler', 'host_c_compiler',
                    'host_cxx_compiler'):
            if var in results:
                result = results[var]
            elif var.startswith('host_'):
                result = results.get(var[5:], {})
            else:
                result = {}
            try:
                self.out.truncate(0)
                compiler = sandbox._value_for(sandbox[var])
                # Add var on both ends to make it clear which of the
                # variables is failing the test when that happens.
                self.assertEquals((var, compiler), (var, result))
            except SystemExit:
                self.assertEquals((var, result),
                                  (var, self.out.getvalue().strip()))
                return

        # Normalize the target os to match what we have as keys in
        # LIBRARY_NAME_INFOS.
        target_os = getattr(self, 'TARGET', self.HOST).split('-', 2)[2]
        if target_os == 'mingw32':
            compiler_type = sandbox._value_for(sandbox['c_compiler']).type
            if compiler_type in ('msvc', 'clang-cl'):
                target_os = 'msvc'
        elif target_os == 'linux-gnuabi64':
            target_os = 'linux-gnu'

        self.do_library_name_info_test(target_os, sandbox)

        # Try again on artifact builds. In that case, we always get library
        # name info for msvc on Windows
        if target_os == 'mingw32':
            target_os = 'msvc'

        sandbox = self.get_sandbox(
            paths, {}, args + ['--enable-artifact-builds'], environ,
            logger=self.logger)

        self.do_library_name_info_test(target_os, sandbox)

    def do_library_name_info_test(self, target_os, sandbox):
        library_name_info = LIBRARY_NAME_INFOS[target_os]
        for k in (
            'DLL_PREFIX',
            'DLL_SUFFIX',
            'LIB_PREFIX',
            'LIB_SUFFIX',
            'IMPORT_LIB_SUFFIX',
            'RUST_LIB_PREFIX',
            'RUST_LIB_SUFFIX',
            'OBJ_SUFFIX',
        ):
            self.assertEquals('%s=%s' % (k, sandbox.get_config(k)),
                              '%s=%s' % (k, library_name_info[k]))


class LinuxToolchainTest(BaseToolchainTest):
    PATHS = {
        '/usr/bin/gcc': GCC_4_9 + GCC_PLATFORM_X86_64_LINUX,
        '/usr/bin/g++': GXX_4_9 + GCC_PLATFORM_X86_64_LINUX,
        '/usr/bin/gcc-4.7': GCC_4_7 + GCC_PLATFORM_X86_64_LINUX,
        '/usr/bin/g++-4.7': GXX_4_7 + GCC_PLATFORM_X86_64_LINUX,
        '/usr/bin/gcc-5': GCC_5 + GCC_PLATFORM_X86_64_LINUX,
        '/usr/bin/g++-5': GXX_5 + GCC_PLATFORM_X86_64_LINUX,
        '/usr/bin/clang': CLANG_3_6 + CLANG_PLATFORM_X86_64_LINUX,
        '/usr/bin/clang++': CLANGXX_3_6 + CLANG_PLATFORM_X86_64_LINUX,
        '/usr/bin/clang-3.6': CLANG_3_6 + CLANG_PLATFORM_X86_64_LINUX,
        '/usr/bin/clang++-3.6': CLANGXX_3_6 + CLANG_PLATFORM_X86_64_LINUX,
        '/usr/bin/clang-3.3': CLANG_3_3 + CLANG_PLATFORM_X86_64_LINUX,
        '/usr/bin/clang++-3.3': CLANGXX_3_3 + CLANG_PLATFORM_X86_64_LINUX,
    }
    GCC_4_7_RESULT = ('Only GCC 4.9 or newer is supported '
                      '(found version 4.7.3).')
    GXX_4_7_RESULT = GCC_4_7_RESULT
    GCC_4_9_RESULT = CompilerResult(
        flags=['-std=gnu99'],
        version='4.9.3',
        type='gcc',
        compiler='/usr/bin/gcc',
        language='C',
    )
    GXX_4_9_RESULT = CompilerResult(
        flags=['-std=gnu++14'],
        version='4.9.3',
        type='gcc',
        compiler='/usr/bin/g++',
        language='C++',
    )
    GCC_5_RESULT = CompilerResult(
        flags=['-std=gnu99'],
        version='5.2.1',
        type='gcc',
        compiler='/usr/bin/gcc-5',
        language='C',
    )
    GXX_5_RESULT = CompilerResult(
        flags=['-std=gnu++14'],
        version='5.2.1',
        type='gcc',
        compiler='/usr/bin/g++-5',
        language='C++',
    )
    CLANG_3_3_RESULT = CompilerResult(
        flags=[],
        version='3.3.0',
        type='clang',
        compiler='/usr/bin/clang-3.3',
        language='C',
    )
    CLANGXX_3_3_RESULT = 'Only clang/llvm 3.6 or newer is supported.'
    CLANG_3_6_RESULT = CompilerResult(
        flags=['-std=gnu99'],
        version='3.6.2',
        type='clang',
        compiler='/usr/bin/clang',
        language='C',
    )
    CLANGXX_3_6_RESULT = CompilerResult(
        flags=['-std=gnu++14'],
        version='3.6.2',
        type='clang',
        compiler='/usr/bin/clang++',
        language='C++',
    )

    def test_gcc(self):
        # We'll try gcc and clang, and find gcc first.
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.GCC_4_9_RESULT,
            'cxx_compiler': self.GXX_4_9_RESULT,
        })

    def test_unsupported_gcc(self):
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.GCC_4_7_RESULT,
        }, environ={
            'CC': 'gcc-4.7',
            'CXX': 'g++-4.7',
        })

        # Maybe this should be reporting the mismatched version instead.
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.GCC_4_9_RESULT,
            'cxx_compiler': self.GXX_4_7_RESULT,
        }, environ={
            'CXX': 'g++-4.7',
        })

    def test_overridden_gcc(self):
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.GCC_5_RESULT,
            'cxx_compiler': self.GXX_5_RESULT,
        }, environ={
            'CC': 'gcc-5',
            'CXX': 'g++-5',
        })

    def test_guess_cxx(self):
        # When CXX is not set, we guess it from CC.
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.GCC_5_RESULT,
            'cxx_compiler': self.GXX_5_RESULT,
        }, environ={
            'CC': 'gcc-5',
        })

    def test_mismatched_gcc(self):
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.GCC_4_9_RESULT,
            'cxx_compiler': (
                'The target C compiler is version 4.9.3, while the target '
                'C++ compiler is version 5.2.1. Need to use the same compiler '
                'version.'),
        }, environ={
            'CXX': 'g++-5',
        })

        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.GCC_4_9_RESULT,
            'cxx_compiler': self.GXX_4_9_RESULT,
            'host_c_compiler': self.GCC_4_9_RESULT,
            'host_cxx_compiler': (
                'The host C compiler is version 4.9.3, while the host '
                'C++ compiler is version 5.2.1. Need to use the same compiler '
                'version.'),
        }, environ={
            'HOST_CXX': 'g++-5',
        })

    def test_mismatched_compiler(self):
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.GCC_4_9_RESULT,
            'cxx_compiler': (
                'The target C compiler is gcc, while the target C++ compiler '
                'is clang. Need to use the same compiler suite.'),
        }, environ={
            'CXX': 'clang++',
        })

        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.GCC_4_9_RESULT,
            'cxx_compiler': self.GXX_4_9_RESULT,
            'host_c_compiler': self.GCC_4_9_RESULT,
            'host_cxx_compiler': (
                'The host C compiler is gcc, while the host C++ compiler '
                'is clang. Need to use the same compiler suite.'),
        }, environ={
            'HOST_CXX': 'clang++',
        })

        self.do_toolchain_test(self.PATHS, {
            'c_compiler': '`%s` is not a C compiler.'
            % mozpath.abspath('/usr/bin/g++'),
        }, environ={
            'CC': 'g++',
        })

        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.GCC_4_9_RESULT,
            'cxx_compiler': '`%s` is not a C++ compiler.'
            % mozpath.abspath('/usr/bin/gcc'),
        }, environ={
            'CXX': 'gcc',
        })

    def test_clang(self):
        # We'll try gcc and clang, but since there is no gcc (gcc-x.y doesn't
        # count), find clang.
        paths = {
            k: v for k, v in self.PATHS.iteritems()
            if os.path.basename(k) not in ('gcc', 'g++')
        }
        self.do_toolchain_test(paths, {
            'c_compiler': self.CLANG_3_6_RESULT,
            'cxx_compiler': self.CLANGXX_3_6_RESULT,
        })

    def test_guess_cxx_clang(self):
        # When CXX is not set, we guess it from CC.
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.CLANG_3_6_RESULT + {
                'compiler': '/usr/bin/clang-3.6',
            },
            'cxx_compiler': self.CLANGXX_3_6_RESULT + {
                'compiler': '/usr/bin/clang++-3.6',
            },
        }, environ={
            'CC': 'clang-3.6',
        })

    def test_unsupported_clang(self):
        # clang 3.3 C compiler is perfectly fine, but we need more for C++.
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.CLANG_3_3_RESULT,
            'cxx_compiler': self.CLANGXX_3_3_RESULT,
        }, environ={
            'CC': 'clang-3.3',
            'CXX': 'clang++-3.3',
        })

    def test_no_supported_compiler(self):
        # Even if there are gcc-x.y or clang-x.y compilers available, we
        # don't try them. This could be considered something to improve.
        paths = {
            k: v for k, v in self.PATHS.iteritems()
            if os.path.basename(k) not in ('gcc', 'g++', 'clang', 'clang++')
        }
        self.do_toolchain_test(paths, {
            'c_compiler': 'Cannot find the target C compiler',
        })

    def test_absolute_path(self):
        paths = dict(self.PATHS)
        paths.update({
            '/opt/clang/bin/clang': paths['/usr/bin/clang'],
            '/opt/clang/bin/clang++': paths['/usr/bin/clang++'],
        })
        result = {
            'c_compiler': self.CLANG_3_6_RESULT + {
                'compiler': '/opt/clang/bin/clang',
            },
            'cxx_compiler': self.CLANGXX_3_6_RESULT + {
                'compiler': '/opt/clang/bin/clang++'
            },
        }
        self.do_toolchain_test(paths, result, environ={
            'CC': '/opt/clang/bin/clang',
            'CXX': '/opt/clang/bin/clang++',
        })
        # With CXX guess too.
        self.do_toolchain_test(paths, result, environ={
            'CC': '/opt/clang/bin/clang',
        })

    def test_atypical_name(self):
        paths = dict(self.PATHS)
        paths.update({
            '/usr/bin/afl-clang-fast': paths['/usr/bin/clang'],
            '/usr/bin/afl-clang-fast++': paths['/usr/bin/clang++'],
        })
        self.do_toolchain_test(paths, {
            'c_compiler': self.CLANG_3_6_RESULT + {
                'compiler': '/usr/bin/afl-clang-fast',
            },
            'cxx_compiler': self.CLANGXX_3_6_RESULT + {
                'compiler': '/usr/bin/afl-clang-fast++',
            },
        }, environ={
            'CC': 'afl-clang-fast',
            'CXX': 'afl-clang-fast++',
        })

    def test_mixed_compilers(self):
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.CLANG_3_6_RESULT,
            'cxx_compiler': self.CLANGXX_3_6_RESULT,
            'host_c_compiler': self.GCC_4_9_RESULT,
            'host_cxx_compiler': self.GXX_4_9_RESULT,
        }, environ={
            'CC': 'clang',
            'HOST_CC': 'gcc',
        })

        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.CLANG_3_6_RESULT,
            'cxx_compiler': self.CLANGXX_3_6_RESULT,
            'host_c_compiler': self.GCC_4_9_RESULT,
            'host_cxx_compiler': self.GXX_4_9_RESULT,
        }, environ={
            'CC': 'clang',
            'CXX': 'clang++',
            'HOST_CC': 'gcc',
        })


class LinuxSimpleCrossToolchainTest(BaseToolchainTest):
    TARGET = 'i686-pc-linux-gnu'
    PATHS = LinuxToolchainTest.PATHS
    GCC_4_9_RESULT = LinuxToolchainTest.GCC_4_9_RESULT
    GXX_4_9_RESULT = LinuxToolchainTest.GXX_4_9_RESULT
    CLANG_3_6_RESULT = LinuxToolchainTest.CLANG_3_6_RESULT
    CLANGXX_3_6_RESULT = LinuxToolchainTest.CLANGXX_3_6_RESULT

    def test_cross_gcc(self):
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.GCC_4_9_RESULT + {
                'flags': ['-m32']
            },
            'cxx_compiler': self.GXX_4_9_RESULT + {
                'flags': ['-m32']
            },
            'host_c_compiler': self.GCC_4_9_RESULT,
            'host_cxx_compiler': self.GXX_4_9_RESULT,
        })

    def test_cross_clang(self):
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.CLANG_3_6_RESULT + {
                'flags': ['--target=i686-linux-gnu'],
            },
            'cxx_compiler': self.CLANGXX_3_6_RESULT + {
                'flags': ['--target=i686-linux-gnu'],
            },
            'host_c_compiler': self.CLANG_3_6_RESULT,
            'host_cxx_compiler': self.CLANGXX_3_6_RESULT,
        }, environ={
            'CC': 'clang',
        })


class LinuxX86_64CrossToolchainTest(BaseToolchainTest):
    HOST = 'i686-pc-linux-gnu'
    TARGET = 'x86_64-pc-linux-gnu'
    PATHS = {
        '/usr/bin/gcc': GCC_4_9 + GCC_PLATFORM_X86_LINUX,
        '/usr/bin/g++': GXX_4_9 + GCC_PLATFORM_X86_LINUX,
        '/usr/bin/clang': CLANG_3_6 + CLANG_PLATFORM_X86_LINUX,
        '/usr/bin/clang++': CLANGXX_3_6 + CLANG_PLATFORM_X86_LINUX,
    }
    GCC_4_9_RESULT = LinuxToolchainTest.GCC_4_9_RESULT
    GXX_4_9_RESULT = LinuxToolchainTest.GXX_4_9_RESULT
    CLANG_3_6_RESULT = LinuxToolchainTest.CLANG_3_6_RESULT
    CLANGXX_3_6_RESULT = LinuxToolchainTest.CLANGXX_3_6_RESULT

    def test_cross_gcc(self):
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.GCC_4_9_RESULT + {
                'flags': ['-m64']
            },
            'cxx_compiler': self.GXX_4_9_RESULT + {
                'flags': ['-m64']
            },
            'host_c_compiler': self.GCC_4_9_RESULT,
            'host_cxx_compiler': self.GXX_4_9_RESULT,
        })

    def test_cross_clang(self):
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.CLANG_3_6_RESULT + {
                'flags': ['--target=x86_64-linux-gnu'],
            },
            'cxx_compiler': self.CLANGXX_3_6_RESULT + {
                'flags': ['--target=x86_64-linux-gnu'],
            },
            'host_c_compiler': self.CLANG_3_6_RESULT,
            'host_cxx_compiler': self.CLANGXX_3_6_RESULT,
        }, environ={
            'CC': 'clang',
        })


class OSXToolchainTest(BaseToolchainTest):
    HOST = 'x86_64-apple-darwin11.2.0'
    PATHS = {
        '/usr/bin/gcc': GCC_4_9 + GCC_PLATFORM_X86_64_OSX,
        '/usr/bin/g++': GXX_4_9 + GCC_PLATFORM_X86_64_OSX,
        '/usr/bin/gcc-4.7': GCC_4_7 + GCC_PLATFORM_X86_64_OSX,
        '/usr/bin/g++-4.7': GXX_4_7 + GCC_PLATFORM_X86_64_OSX,
        '/usr/bin/gcc-5': GCC_5 + GCC_PLATFORM_X86_64_OSX,
        '/usr/bin/g++-5': GXX_5 + GCC_PLATFORM_X86_64_OSX,
        '/usr/bin/clang': CLANG_3_6 + CLANG_PLATFORM_X86_64_OSX,
        '/usr/bin/clang++': CLANGXX_3_6 + CLANG_PLATFORM_X86_64_OSX,
        '/usr/bin/clang-3.6': CLANG_3_6 + CLANG_PLATFORM_X86_64_OSX,
        '/usr/bin/clang++-3.6': CLANGXX_3_6 + CLANG_PLATFORM_X86_64_OSX,
        '/usr/bin/clang-3.3': CLANG_3_3 + CLANG_PLATFORM_X86_64_OSX,
        '/usr/bin/clang++-3.3': CLANGXX_3_3 + CLANG_PLATFORM_X86_64_OSX,
    }
    CLANG_3_3_RESULT = LinuxToolchainTest.CLANG_3_3_RESULT
    CLANGXX_3_3_RESULT = LinuxToolchainTest.CLANGXX_3_3_RESULT
    CLANG_3_6_RESULT = LinuxToolchainTest.CLANG_3_6_RESULT
    CLANGXX_3_6_RESULT = LinuxToolchainTest.CLANGXX_3_6_RESULT
    GCC_4_7_RESULT = LinuxToolchainTest.GCC_4_7_RESULT
    GCC_5_RESULT = LinuxToolchainTest.GCC_5_RESULT
    GXX_5_RESULT = LinuxToolchainTest.GXX_5_RESULT

    def test_clang(self):
        # We only try clang because gcc is known not to work.
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.CLANG_3_6_RESULT,
            'cxx_compiler': self.CLANGXX_3_6_RESULT,
        })

    def test_not_gcc(self):
        # We won't pick GCC if it's the only thing available.
        paths = {
            k: v for k, v in self.PATHS.iteritems()
            if os.path.basename(k) not in ('clang', 'clang++')
        }
        self.do_toolchain_test(paths, {
            'c_compiler': 'Cannot find the target C compiler',
        })

    def test_unsupported_clang(self):
        # clang 3.3 C compiler is perfectly fine, but we need more for C++.
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.CLANG_3_3_RESULT,
            'cxx_compiler': self.CLANGXX_3_3_RESULT,
        }, environ={
            'CC': 'clang-3.3',
            'CXX': 'clang++-3.3',
        })

    def test_forced_gcc(self):
        # GCC can still be forced if the user really wants it.
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.GCC_5_RESULT,
            'cxx_compiler': self.GXX_5_RESULT,
        }, environ={
            'CC': 'gcc-5',
            'CXX': 'g++-5',
        })

    def test_forced_unsupported_gcc(self):
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.GCC_4_7_RESULT,
        }, environ={
            'CC': 'gcc-4.7',
            'CXX': 'g++-4.7',
        })


class WindowsToolchainTest(BaseToolchainTest):
    HOST = 'i686-pc-mingw32'

    # For the purpose of this test, it doesn't matter that the paths are not
    # real Windows paths.
    PATHS = {
        '/opt/VS_2013u2/bin/cl': VS_2013u2 + VS_PLATFORM_X86,
        '/opt/VS_2013u3/bin/cl': VS_2013u3 + VS_PLATFORM_X86,
        '/opt/VS_2015/bin/cl': VS_2015 + VS_PLATFORM_X86,
        '/opt/VS_2015u1/bin/cl': VS_2015u1 + VS_PLATFORM_X86,
        '/opt/VS_2015u2/bin/cl': VS_2015u2 + VS_PLATFORM_X86,
        '/usr/bin/cl': VS_2015u3 + VS_PLATFORM_X86,
        '/usr/bin/clang-cl': CLANG_CL_3_9 + CLANG_CL_PLATFORM_X86,
        '/usr/bin/gcc': GCC_4_9 + GCC_PLATFORM_X86_WIN,
        '/usr/bin/g++': GXX_4_9 + GCC_PLATFORM_X86_WIN,
        '/usr/bin/gcc-4.7': GCC_4_7 + GCC_PLATFORM_X86_WIN,
        '/usr/bin/g++-4.7': GXX_4_7 + GCC_PLATFORM_X86_WIN,
        '/usr/bin/gcc-5': GCC_5 + GCC_PLATFORM_X86_WIN,
        '/usr/bin/g++-5': GXX_5 + GCC_PLATFORM_X86_WIN,
        '/usr/bin/clang': CLANG_3_6 + CLANG_PLATFORM_X86_WIN,
        '/usr/bin/clang++': CLANGXX_3_6 + CLANG_PLATFORM_X86_WIN,
        '/usr/bin/clang-3.6': CLANG_3_6 + CLANG_PLATFORM_X86_WIN,
        '/usr/bin/clang++-3.6': CLANGXX_3_6 + CLANG_PLATFORM_X86_WIN,
        '/usr/bin/clang-3.3': CLANG_3_3 + CLANG_PLATFORM_X86_WIN,
        '/usr/bin/clang++-3.3': CLANGXX_3_3 + CLANG_PLATFORM_X86_WIN,
    }

    VS_2013u2_RESULT = (
        'This version (18.00.30501) of the MSVC compiler is not supported.\n'
        'You must install Visual C++ 2015 Update 3 or newer in order to build.\n'
        'See https://developer.mozilla.org/en/Windows_Build_Prerequisites')
    VS_2013u3_RESULT = (
        'This version (18.00.30723) of the MSVC compiler is not supported.\n'
        'You must install Visual C++ 2015 Update 3 or newer in order to build.\n'
        'See https://developer.mozilla.org/en/Windows_Build_Prerequisites')
    VS_2015_RESULT = (
        'This version (19.00.23026) of the MSVC compiler is not supported.\n'
        'You must install Visual C++ 2015 Update 3 or newer in order to build.\n'
        'See https://developer.mozilla.org/en/Windows_Build_Prerequisites')
    VS_2015u1_RESULT = (
        'This version (19.00.23506) of the MSVC compiler is not supported.\n'
        'You must install Visual C++ 2015 Update 3 or newer in order to build.\n'
        'See https://developer.mozilla.org/en/Windows_Build_Prerequisites')
    VS_2015u2_RESULT = (
        'This version (19.00.23918) of the MSVC compiler is not supported.\n'
        'You must install Visual C++ 2015 Update 3 or newer in order to build.\n'
        'See https://developer.mozilla.org/en/Windows_Build_Prerequisites')
    VS_2015u3_RESULT = CompilerResult(
        flags=[],
        version='19.00.24213',
        type='msvc',
        compiler='/usr/bin/cl',
        language='C',
    )
    VSXX_2015u3_RESULT = CompilerResult(
        flags=[],
        version='19.00.24213',
        type='msvc',
        compiler='/usr/bin/cl',
        language='C++',
    )
    CLANG_CL_3_9_RESULT = CompilerResult(
        flags=['-Xclang', '-std=gnu99',
               '-fms-compatibility-version=19.11.25547'],
        version='19.11.25547',
        type='clang-cl',
        compiler='/usr/bin/clang-cl',
        language='C',
    )
    CLANGXX_CL_3_9_RESULT = CompilerResult(
        flags=['-Xclang', '-std=c++14',
               '-fms-compatibility-version=19.11.25547'],
        version='19.11.25547',
        type='clang-cl',
        compiler='/usr/bin/clang-cl',
        language='C++',
    )
    CLANG_3_3_RESULT = LinuxToolchainTest.CLANG_3_3_RESULT
    CLANGXX_3_3_RESULT = LinuxToolchainTest.CLANGXX_3_3_RESULT
    CLANG_3_6_RESULT = LinuxToolchainTest.CLANG_3_6_RESULT
    CLANGXX_3_6_RESULT = LinuxToolchainTest.CLANGXX_3_6_RESULT
    GCC_4_7_RESULT = LinuxToolchainTest.GCC_4_7_RESULT
    GCC_4_9_RESULT = LinuxToolchainTest.GCC_4_9_RESULT
    GXX_4_9_RESULT = CompilerResult(
        flags=['-std=gnu++14'],
        version='4.9.3',
        type='gcc',
        compiler='/usr/bin/g++',
        language='C++',
    )
    GCC_5_RESULT = LinuxToolchainTest.GCC_5_RESULT
    GXX_5_RESULT = CompilerResult(
        flags=['-std=gnu++14'],
        version='5.2.1',
        type='gcc',
        compiler='/usr/bin/g++-5',
        language='C++',
    )

    # VS2015u3 or greater is required.
    def test_msvc(self):
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.VS_2015u3_RESULT,
            'cxx_compiler': self.VSXX_2015u3_RESULT,
        })

    def test_unsupported_msvc(self):
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.VS_2015u2_RESULT,
        }, environ={
            'CC': '/opt/VS_2015u2/bin/cl',
        })

        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.VS_2015u1_RESULT,
        }, environ={
            'CC': '/opt/VS_2015u1/bin/cl',
        })

        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.VS_2015_RESULT,
        }, environ={
            'CC': '/opt/VS_2015/bin/cl',
        })

        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.VS_2013u3_RESULT,
        }, environ={
            'CC': '/opt/VS_2013u3/bin/cl',
        })

        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.VS_2013u2_RESULT,
        }, environ={
            'CC': '/opt/VS_2013u2/bin/cl',
        })

    def test_clang_cl(self):
        # We'll pick clang-cl if msvc can't be found.
        paths = {
            k: v for k, v in self.PATHS.iteritems()
            if os.path.basename(k) != 'cl'
        }
        self.do_toolchain_test(paths, {
            'c_compiler': self.CLANG_CL_3_9_RESULT,
            'cxx_compiler': self.CLANGXX_CL_3_9_RESULT,
        })

    def test_gcc(self):
        # We'll pick GCC if msvc and clang-cl can't be found.
        paths = {
            k: v for k, v in self.PATHS.iteritems()
            if os.path.basename(k) not in ('cl', 'clang-cl')
        }
        self.do_toolchain_test(paths, {
            'c_compiler': self.GCC_4_9_RESULT,
            'cxx_compiler': self.GXX_4_9_RESULT,
        })

    def test_overridden_unsupported_gcc(self):
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.GCC_4_7_RESULT,
        }, environ={
            'CC': 'gcc-4.7',
            'CXX': 'g++-4.7',
        })

    def test_clang(self):
        # We'll pick clang if nothing else is found.
        paths = {
            k: v for k, v in self.PATHS.iteritems()
            if os.path.basename(k) not in ('cl', 'clang-cl', 'gcc')
        }
        self.do_toolchain_test(paths, {
            'c_compiler': self.CLANG_3_6_RESULT,
            'cxx_compiler': self.CLANGXX_3_6_RESULT,
        })

    def test_overridden_unsupported_clang(self):
        # clang 3.3 C compiler is perfectly fine, but we need more for C++.
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.CLANG_3_3_RESULT,
            'cxx_compiler': self.CLANGXX_3_3_RESULT,
        }, environ={
            'CC': 'clang-3.3',
            'CXX': 'clang++-3.3',
        })

    def test_cannot_cross(self):
        paths = {
            '/usr/bin/cl': VS_2015u3 + VS_PLATFORM_X86_64,
        }
        self.do_toolchain_test(paths, {
            'c_compiler': ('Target C compiler target CPU (x86_64) '
                           'does not match --target CPU (i686)'),
        })


class Windows64ToolchainTest(WindowsToolchainTest):
    HOST = 'x86_64-pc-mingw32'

    # For the purpose of this test, it doesn't matter that the paths are not
    # real Windows paths.
    PATHS = {
        '/opt/VS_2013u2/bin/cl': VS_2013u2 + VS_PLATFORM_X86_64,
        '/opt/VS_2013u3/bin/cl': VS_2013u3 + VS_PLATFORM_X86_64,
        '/opt/VS_2015/bin/cl': VS_2015 + VS_PLATFORM_X86_64,
        '/opt/VS_2015u1/bin/cl': VS_2015u1 + VS_PLATFORM_X86_64,
        '/opt/VS_2015u2/bin/cl': VS_2015u2 + VS_PLATFORM_X86_64,
        '/usr/bin/cl': VS_2015u3 + VS_PLATFORM_X86_64,
        '/usr/bin/clang-cl': CLANG_CL_3_9 + CLANG_CL_PLATFORM_X86_64,
        '/usr/bin/gcc': GCC_4_9 + GCC_PLATFORM_X86_64_WIN,
        '/usr/bin/g++': GXX_4_9 + GCC_PLATFORM_X86_64_WIN,
        '/usr/bin/gcc-4.7': GCC_4_7 + GCC_PLATFORM_X86_64_WIN,
        '/usr/bin/g++-4.7': GXX_4_7 + GCC_PLATFORM_X86_64_WIN,
        '/usr/bin/gcc-5': GCC_5 + GCC_PLATFORM_X86_64_WIN,
        '/usr/bin/g++-5': GXX_5 + GCC_PLATFORM_X86_64_WIN,
        '/usr/bin/clang': CLANG_3_6 + CLANG_PLATFORM_X86_64_WIN,
        '/usr/bin/clang++': CLANGXX_3_6 + CLANG_PLATFORM_X86_64_WIN,
        '/usr/bin/clang-3.6': CLANG_3_6 + CLANG_PLATFORM_X86_64_WIN,
        '/usr/bin/clang++-3.6': CLANGXX_3_6 + CLANG_PLATFORM_X86_64_WIN,
        '/usr/bin/clang-3.3': CLANG_3_3 + CLANG_PLATFORM_X86_64_WIN,
        '/usr/bin/clang++-3.3': CLANGXX_3_3 + CLANG_PLATFORM_X86_64_WIN,
    }

    def test_cannot_cross(self):
        paths = {
            '/usr/bin/cl': VS_2015u3 + VS_PLATFORM_X86,
        }
        self.do_toolchain_test(paths, {
            'c_compiler': ('Target C compiler target CPU (x86) '
                           'does not match --target CPU (x86_64)'),
        })


class LinuxCrossCompileToolchainTest(BaseToolchainTest):
    TARGET = 'arm-unknown-linux-gnu'
    PATHS = {
        '/usr/bin/arm-linux-gnu-gcc': GCC_4_9 + GCC_PLATFORM_ARM_LINUX,
        '/usr/bin/arm-linux-gnu-g++': GXX_4_9 + GCC_PLATFORM_ARM_LINUX,
        '/usr/bin/arm-linux-gnu-gcc-4.7': GCC_4_7 + GCC_PLATFORM_ARM_LINUX,
        '/usr/bin/arm-linux-gnu-g++-4.7': GXX_4_7 + GCC_PLATFORM_ARM_LINUX,
        '/usr/bin/arm-linux-gnu-gcc-5': GCC_5 + GCC_PLATFORM_ARM_LINUX,
        '/usr/bin/arm-linux-gnu-g++-5': GXX_5 + GCC_PLATFORM_ARM_LINUX,
    }
    PATHS.update(LinuxToolchainTest.PATHS)
    ARM_GCC_4_7_RESULT = LinuxToolchainTest.GXX_4_7_RESULT
    ARM_GCC_5_RESULT = LinuxToolchainTest.GCC_5_RESULT + {
        'compiler': '/usr/bin/arm-linux-gnu-gcc-5',
    }
    ARM_GXX_5_RESULT = LinuxToolchainTest.GXX_5_RESULT + {
        'compiler': '/usr/bin/arm-linux-gnu-g++-5',
    }
    CLANG_3_6_RESULT = LinuxToolchainTest.CLANG_3_6_RESULT
    CLANGXX_3_6_RESULT = LinuxToolchainTest.CLANGXX_3_6_RESULT
    GCC_4_9_RESULT = LinuxToolchainTest.GCC_4_9_RESULT
    GXX_4_9_RESULT = LinuxToolchainTest.GXX_4_9_RESULT

    little_endian = FakeCompiler(GCC_PLATFORM_LINUX,
                                 GCC_PLATFORM_LITTLE_ENDIAN)
    big_endian = FakeCompiler(GCC_PLATFORM_LINUX, GCC_PLATFORM_BIG_ENDIAN)

    PLATFORMS = {
        'i686-pc-linux-gnu': GCC_PLATFORM_X86_LINUX,
        'x86_64-pc-linux-gnu': GCC_PLATFORM_X86_64_LINUX,
        'arm-unknown-linux-gnu': GCC_PLATFORM_ARM_LINUX,
        'aarch64-unknown-linux-gnu': little_endian + {
            '__aarch64__': 1,
        },
        'ia64-unknown-linux-gnu': little_endian + {
            '__ia64__': 1,
        },
        's390x-unknown-linux-gnu': big_endian + {
            '__s390x__': 1,
            '__s390__': 1,
        },
        's390-unknown-linux-gnu': big_endian + {
            '__s390__': 1,
        },
        'powerpc64-unknown-linux-gnu': big_endian + {
            None: {
                '__powerpc64__': 1,
                '__powerpc__': 1,
            },
            '-m32': {
                '__powerpc64__': False,
            },
        },
        'powerpc-unknown-linux-gnu': big_endian + {
            None: {
                '__powerpc__': 1,
            },
            '-m64': {
                '__powerpc64__': 1,
            },
        },
        'alpha-unknown-linux-gnu': little_endian + {
            '__alpha__': 1,
        },
        'hppa-unknown-linux-gnu': big_endian + {
            '__hppa__': 1,
        },
        'sparc64-unknown-linux-gnu': big_endian + {
            None: {
                '__arch64__': 1,
                '__sparc__': 1,
            },
            '-m32': {
                '__arch64__': False,
            },
        },
        'sparc-unknown-linux-gnu': big_endian + {
            None: {
                '__sparc__': 1,
            },
            '-m64': {
                '__arch64__': 1,
            },
        },
        'mips64-unknown-linux-gnuabi64': big_endian + {
            '__mips64': 1,
            '__mips__': 1,
        },
        'mips-unknown-linux-gnu': big_endian + {
            '__mips__': 1,
        },
        'sh4-unknown-linux-gnu': little_endian + {
            '__sh__': 1,
        },
    }

    PLATFORMS['powerpc64le-unknown-linux-gnu'] = \
        PLATFORMS['powerpc64-unknown-linux-gnu'] + GCC_PLATFORM_LITTLE_ENDIAN
    PLATFORMS['mips64el-unknown-linux-gnuabi64'] = \
        PLATFORMS['mips64-unknown-linux-gnuabi64'] + GCC_PLATFORM_LITTLE_ENDIAN
    PLATFORMS['mipsel-unknown-linux-gnu'] = \
        PLATFORMS['mips-unknown-linux-gnu'] + GCC_PLATFORM_LITTLE_ENDIAN

    def do_test_cross_gcc_32_64(self, host, target):
        self.HOST = host
        self.TARGET = target
        paths = {
            '/usr/bin/gcc': GCC_4_9 + self.PLATFORMS[host],
            '/usr/bin/g++': GXX_4_9 + self.PLATFORMS[host],
        }
        cross_flags = {
            'flags': ['-m64' if '64' in target else '-m32']
        }
        self.do_toolchain_test(paths, {
            'c_compiler': self.GCC_4_9_RESULT + cross_flags,
            'cxx_compiler': self.GXX_4_9_RESULT + cross_flags,
            'host_c_compiler': self.GCC_4_9_RESULT,
            'host_cxx_compiler': self.GXX_4_9_RESULT,
        })
        self.HOST = LinuxCrossCompileToolchainTest.HOST
        self.TARGET = LinuxCrossCompileToolchainTest.TARGET

    def test_cross_x86_x64(self):
        self.do_test_cross_gcc_32_64(
            'i686-pc-linux-gnu', 'x86_64-pc-linux-gnu')
        self.do_test_cross_gcc_32_64(
            'x86_64-pc-linux-gnu', 'i686-pc-linux-gnu')

    def test_cross_sparc_sparc64(self):
        self.do_test_cross_gcc_32_64(
            'sparc-unknown-linux-gnu', 'sparc64-unknown-linux-gnu')
        self.do_test_cross_gcc_32_64(
            'sparc64-unknown-linux-gnu', 'sparc-unknown-linux-gnu')

    def test_cross_ppc_ppc64(self):
        self.do_test_cross_gcc_32_64(
            'powerpc-unknown-linux-gnu', 'powerpc64-unknown-linux-gnu')
        self.do_test_cross_gcc_32_64(
            'powerpc64-unknown-linux-gnu', 'powerpc-unknown-linux-gnu')

    def do_test_cross_gcc(self, host, target):
        self.HOST = host
        self.TARGET = target
        host_cpu = host.split('-')[0]
        cpu, manufacturer, os = target.split('-', 2)
        toolchain_prefix = '/usr/bin/%s-%s' % (cpu, os)
        paths = {
            '/usr/bin/gcc': GCC_4_9 + self.PLATFORMS[host],
            '/usr/bin/g++': GXX_4_9 + self.PLATFORMS[host],
        }
        self.do_toolchain_test(paths, {
            'c_compiler': ('Target C compiler target CPU (%s) '
                           'does not match --target CPU (%s)'
                           % (host_cpu, cpu)),
        })

        paths.update({
            '%s-gcc' % toolchain_prefix: GCC_4_9 + self.PLATFORMS[target],
            '%s-g++' % toolchain_prefix: GXX_4_9 + self.PLATFORMS[target],
        })
        self.do_toolchain_test(paths, {
            'c_compiler': self.GCC_4_9_RESULT + {
                'compiler': '%s-gcc' % toolchain_prefix,
            },
            'cxx_compiler': self.GXX_4_9_RESULT + {
                'compiler': '%s-g++' % toolchain_prefix,
            },
            'host_c_compiler': self.GCC_4_9_RESULT,
            'host_cxx_compiler': self.GXX_4_9_RESULT,
        })
        self.HOST = LinuxCrossCompileToolchainTest.HOST
        self.TARGET = LinuxCrossCompileToolchainTest.TARGET

    def test_cross_gcc_misc(self):
        for target in self.PLATFORMS:
            if not target.endswith('-pc-linux-gnu'):
                self.do_test_cross_gcc('x86_64-pc-linux-gnu', target)

    def test_cannot_cross(self):
        self.TARGET = 'mipsel-unknown-linux-gnu'

        paths = {
            '/usr/bin/gcc': GCC_4_9 + self.PLATFORMS['mips-unknown-linux-gnu'],
            '/usr/bin/g++': GXX_4_9 + self.PLATFORMS['mips-unknown-linux-gnu'],
        }
        self.do_toolchain_test(paths, {
            'c_compiler': ('Target C compiler target endianness (big) '
                           'does not match --target endianness (little)'),
        })
        self.TARGET = LinuxCrossCompileToolchainTest.TARGET

    def test_overridden_cross_gcc(self):
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.ARM_GCC_5_RESULT,
            'cxx_compiler': self.ARM_GXX_5_RESULT,
            'host_c_compiler': self.GCC_4_9_RESULT,
            'host_cxx_compiler': self.GXX_4_9_RESULT,
        }, environ={
            'CC': 'arm-linux-gnu-gcc-5',
            'CXX': 'arm-linux-gnu-g++-5',
        })

    def test_overridden_unsupported_cross_gcc(self):
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.ARM_GCC_4_7_RESULT,
        }, environ={
            'CC': 'arm-linux-gnu-gcc-4.7',
            'CXX': 'arm-linux-gnu-g++-4.7',
        })

    def test_guess_cross_cxx(self):
        # When CXX is not set, we guess it from CC.
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.ARM_GCC_5_RESULT,
            'cxx_compiler': self.ARM_GXX_5_RESULT,
            'host_c_compiler': self.GCC_4_9_RESULT,
            'host_cxx_compiler': self.GXX_4_9_RESULT,
        }, environ={
            'CC': 'arm-linux-gnu-gcc-5',
        })

        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.ARM_GCC_5_RESULT,
            'cxx_compiler': self.ARM_GXX_5_RESULT,
            'host_c_compiler': self.CLANG_3_6_RESULT,
            'host_cxx_compiler': self.CLANGXX_3_6_RESULT,
        }, environ={
            'CC': 'arm-linux-gnu-gcc-5',
            'HOST_CC': 'clang',
        })

        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.ARM_GCC_5_RESULT,
            'cxx_compiler': self.ARM_GXX_5_RESULT,
            'host_c_compiler': self.CLANG_3_6_RESULT,
            'host_cxx_compiler': self.CLANGXX_3_6_RESULT,
        }, environ={
            'CC': 'arm-linux-gnu-gcc-5',
            'CXX': 'arm-linux-gnu-g++-5',
            'HOST_CC': 'clang',
        })

    def test_cross_clang(self):
        cross_clang_result = self.CLANG_3_6_RESULT + {
            'flags': ['--target=arm-linux-gnu'],
        }
        cross_clangxx_result = self.CLANGXX_3_6_RESULT + {
            'flags': ['--target=arm-linux-gnu'],
        }
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': cross_clang_result,
            'cxx_compiler': cross_clangxx_result,
            'host_c_compiler': self.CLANG_3_6_RESULT,
            'host_cxx_compiler': self.CLANGXX_3_6_RESULT,
        }, environ={
            'CC': 'clang',
            'HOST_CC': 'clang',
        })

        self.do_toolchain_test(self.PATHS, {
            'c_compiler': cross_clang_result,
            'cxx_compiler': cross_clangxx_result,
            'host_c_compiler': self.CLANG_3_6_RESULT,
            'host_cxx_compiler': self.CLANGXX_3_6_RESULT,
        }, environ={
            'CC': 'clang',
        })

    def test_cross_atypical_clang(self):
        paths = dict(self.PATHS)
        paths.update({
            '/usr/bin/afl-clang-fast': paths['/usr/bin/clang'],
            '/usr/bin/afl-clang-fast++': paths['/usr/bin/clang++'],
        })
        afl_clang_result = self.CLANG_3_6_RESULT + {
            'compiler': '/usr/bin/afl-clang-fast',
        }
        afl_clangxx_result = self.CLANGXX_3_6_RESULT + {
            'compiler': '/usr/bin/afl-clang-fast++',
        }
        self.do_toolchain_test(paths, {
            'c_compiler': afl_clang_result + {
                'flags': ['--target=arm-linux-gnu'],
            },
            'cxx_compiler': afl_clangxx_result + {
                'flags': ['--target=arm-linux-gnu'],
            },
            'host_c_compiler': afl_clang_result,
            'host_cxx_compiler': afl_clangxx_result,
        }, environ={
            'CC': 'afl-clang-fast',
            'CXX': 'afl-clang-fast++',
        })


class OSXCrossToolchainTest(BaseToolchainTest):
    TARGET = 'i686-apple-darwin11.2.0'
    PATHS = LinuxToolchainTest.PATHS
    CLANG_3_6_RESULT = LinuxToolchainTest.CLANG_3_6_RESULT
    CLANGXX_3_6_RESULT = LinuxToolchainTest.CLANGXX_3_6_RESULT

    def test_osx_cross(self):
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.CLANG_3_6_RESULT + {
                'flags': ['--target=i686-darwin11.2.0'],
            },
            'cxx_compiler': self.CLANGXX_3_6_RESULT + {
                'flags': ['--target=i686-darwin11.2.0'],
            },
            'host_c_compiler': self.CLANG_3_6_RESULT,
            'host_cxx_compiler': self.CLANGXX_3_6_RESULT,
        }, environ={
            'CC': 'clang',
        })

    def test_cannot_osx_cross(self):
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': 'Target C compiler target kernel (Linux) does not '
                          'match --target kernel (Darwin)',
        }, environ={
            'CC': 'gcc',
        })


class OpenBSDToolchainTest(BaseToolchainTest):
    HOST = 'x86_64-unknown-openbsd6.1'
    TARGET = 'x86_64-unknown-openbsd6.1'
    PATHS = {
        '/usr/bin/gcc': GCC_4_9 + GCC_PLATFORM_X86_64 + GCC_PLATFORM_OPENBSD,
        '/usr/bin/g++': GXX_4_9 + GCC_PLATFORM_X86_64 + GCC_PLATFORM_OPENBSD,
    }
    GCC_4_9_RESULT = LinuxToolchainTest.GCC_4_9_RESULT
    GXX_4_9_RESULT = LinuxToolchainTest.GXX_4_9_RESULT

    def test_gcc(self):
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.GCC_4_9_RESULT,
            'cxx_compiler': self.GXX_4_9_RESULT,
        })


class RustTest(BaseConfigureTest):
    def invoke_cargo(self, stdin, args):
        if args == ('--version', '--verbose'):
            return 0, 'cargo 2.0\nrelease: 2.0', ''
        raise NotImplementedError('unsupported arguments')

    def invoke_rustc(self, stdin, args):
        if args == ('--version', '--verbose'):
            return 0, 'rustc 2.0\nrelease: 2.0\nhost: x86_64-unknown-linux-gnu', ''
        if args == ('--print', 'target-list'):
            # Raw list returned by rustc version 1.19, + ios, which somehow
            # don't appear in the default list.
            # https://github.com/rust-lang/rust/issues/36156
            rust_targets = [
                'aarch64-apple-ios',
                'aarch64-linux-android',
                'aarch64-unknown-freebsd',
                'aarch64-unknown-fuchsia',
                'aarch64-unknown-linux-gnu',
                'arm-linux-androideabi',
                'arm-unknown-linux-gnueabi',
                'arm-unknown-linux-gnueabihf',
                'arm-unknown-linux-musleabi',
                'arm-unknown-linux-musleabihf',
                'armv5te-unknown-linux-gnueabi',
                'armv7-linux-androideabi',
                'armv7-unknown-linux-gnueabihf',
                'armv7-unknown-linux-musleabihf',
                'armv7s-apple-ios',
                'asmjs-unknown-emscripten',
                'i386-apple-ios',
                'i586-pc-windows-msvc',
                'i586-unknown-linux-gnu',
                'i686-apple-darwin',
                'i686-linux-android',
                'i686-pc-windows-gnu',
                'i686-pc-windows-msvc',
                'i686-unknown-dragonfly',
                'i686-unknown-freebsd',
                'i686-unknown-haiku',
                'i686-unknown-linux-gnu',
                'i686-unknown-linux-musl',
                'i686-unknown-netbsd',
                'i686-unknown-openbsd',
                'le32-unknown-nacl',
                'mips-unknown-linux-gnu',
                'mips-unknown-linux-musl',
                'mips-unknown-linux-uclibc',
                'mips64-unknown-linux-gnuabi64',
                'mips64el-unknown-linux-gnuabi64',
                'mipsel-unknown-linux-gnu',
                'mipsel-unknown-linux-musl',
                'mipsel-unknown-linux-uclibc',
                'powerpc-unknown-linux-gnu',
                'powerpc64-unknown-linux-gnu',
                'powerpc64le-unknown-linux-gnu',
                's390x-unknown-linux-gnu',
                'sparc64-unknown-linux-gnu',
                'sparc64-unknown-netbsd',
                'sparcv9-sun-solaris',
                'thumbv6m-none-eabi',
                'thumbv7em-none-eabi',
                'thumbv7em-none-eabihf',
                'thumbv7m-none-eabi',
                'wasm32-unknown-emscripten',
                'x86_64-apple-darwin',
                'x86_64-apple-ios',
                'x86_64-linux-android',
                'x86_64-pc-windows-gnu',
                'x86_64-pc-windows-msvc',
                'x86_64-rumprun-netbsd',
                'x86_64-sun-solaris',
                'x86_64-unknown-bitrig',
                'x86_64-unknown-dragonfly',
                'x86_64-unknown-freebsd',
                'x86_64-unknown-fuchsia',
                'x86_64-unknown-haiku',
                'x86_64-unknown-linux-gnu',
                'x86_64-unknown-linux-musl',
                'x86_64-unknown-netbsd',
                'x86_64-unknown-openbsd',
                'x86_64-unknown-redox',
            ]
            return 0, '\n'.join(rust_targets), ''
        if (len(args) == 6 and args[:2] == ('--crate-type', 'staticlib') and
            args[2].startswith('--target=') and args[3] == '-o'):
            with open(args[4], 'w') as fh:
                fh.write('foo')
            return 0, '', ''
        raise NotImplementedError('unsupported arguments')

    def get_rust_target(self, target, building_with_gcc=True):
        environ = {
            'PATH': os.pathsep.join(
                mozpath.abspath(p) for p in ('/bin', '/usr/bin')),
        }

        paths = {
            mozpath.abspath('/usr/bin/cargo'): self.invoke_cargo,
            mozpath.abspath('/usr/bin/rustc'): self.invoke_rustc,
        }

        self.TARGET = target
        sandbox = self.get_sandbox(paths, {}, [], environ)

        # Trick the sandbox into not running the target compiler check
        dep = sandbox._depends[sandbox['building_with_gcc']]
        getattr(sandbox, '__value_for_depends')[(dep, False)] = \
            building_with_gcc
        return sandbox._value_for(sandbox['rust_target_triple'])

    def test_rust_target(self):
        # Cases where the output of config.sub matches a rust target
        for straightforward in (
            'x86_64-unknown-dragonfly',
            'aarch64-unknown-freebsd',
            'i686-unknown-freebsd',
            'x86_64-unknown-freebsd',
            'sparc64-unknown-netbsd',
            'i686-unknown-netbsd',
            'x86_64-unknown-netbsd',
            'i686-unknown-openbsd',
            'x86_64-unknown-openbsd',
            'aarch64-unknown-linux-gnu',
            'armv7-unknown-linux-gnueabihf',
            'sparc64-unknown-linux-gnu',
            'i686-unknown-linux-gnu',
            'i686-apple-darwin',
            'x86_64-apple-darwin',
            'aarch64-apple-ios',
            'armv7s-apple-ios',
            'i386-apple-ios',
            'x86_64-apple-ios',
            'mips-unknown-linux-gnu',
            'mipsel-unknown-linux-gnu',
            'mips64-unknown-linux-gnuabi64',
            'mips64el-unknown-linux-gnuabi64',
            'powerpc64-unknown-linux-gnu',
            'powerpc64le-unknown-linux-gnu',
        ):
            self.assertEqual(self.get_rust_target(straightforward), straightforward)

        # Cases where the output of config.sub is different
        for autoconf, rust in (
            ('aarch64-unknown-linux-android', 'aarch64-linux-android'),
            ('arm-unknown-linux-androideabi', 'armv7-linux-androideabi'),
            ('armv7-unknown-linux-androideabi', 'armv7-linux-androideabi'),
            ('i386-unknown-linux-android', 'i686-linux-android'),
            ('i686-unknown-linux-android', 'i686-linux-android'),
            ('x86_64-unknown-linux-android', 'x86_64-linux-android'),
            ('x86_64-pc-linux-gnu', 'x86_64-unknown-linux-gnu'),
            ('sparcv9-sun-solaris2', 'sparcv9-sun-solaris'),
            ('x86_64-sun-solaris2', 'x86_64-sun-solaris'),
        ):
            self.assertEqual(self.get_rust_target(autoconf), rust)

        # Windows
        for autoconf, building_with_gcc, rust in (
            ('i686-pc-mingw32', False, 'i686-pc-windows-msvc'),
            ('x86_64-pc-mingw32', False, 'x86_64-pc-windows-msvc'),
            ('i686-pc-mingw32', True, 'i686-pc-windows-gnu'),
            ('x86_64-pc-mingw32', True, 'x86_64-pc-windows-gnu'),
        ):
            self.assertEqual(self.get_rust_target(autoconf, building_with_gcc), rust)


if __name__ == '__main__':
    main()
