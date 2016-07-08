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

SUPPORTS_GNU99 = {
    '-std=gnu99': DEFAULT_C99,
}

SUPPORTS_GNUXX11 = {
    '-std=gnu++11': DEFAULT_CXX_11,
}


@memoize
def GCC_BASE(version):
    version = Version(version)
    return FakeCompiler({
        '__GNUC__': version.major,
        '__GNUC_MINOR__': version.minor,
        '__GNUC_PATCHLEVEL__': version.patch,
        '__STDC__': 1,
    })


@memoize
def GCC(version):
    return GCC_BASE(version) + SUPPORTS_GNU99


@memoize
def GXX(version):
    return GCC_BASE(version) + DEFAULT_CXX_97 + SUPPORTS_GNUXX11


GCC_4_7 = GCC('4.7.3')
GXX_4_7 = GXX('4.7.3')
GCC_4_9 = GCC('4.9.3')
GXX_4_9 = GXX('4.9.3')
GCC_5 = GCC('5.2.1') + DEFAULT_C11
GXX_5 = GXX('5.2.1')


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
            SUPPORTS_GNUXX11)


CLANG_3_3 = CLANG('3.3.0') + DEFAULT_C99
CLANGXX_3_3 = CLANGXX('3.3.0')
CLANG_3_6 = CLANG('3.6.2') + DEFAULT_C11
CLANGXX_3_6 = CLANGXX('3.6.2') + {
    '-std=gnu++11': {
        '__has_feature(cxx_alignof)': '1',
    },
}


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

# Note: In reality, the -std=gnu* options are only supported when preceded by
# -Xclang.
CLANG_CL_3_9 = (CLANG_BASE('3.9.0') + VS('18.00.00000') + DEFAULT_C11 +
                SUPPORTS_GNU99 + SUPPORTS_GNUXX11) + {
    '*.cpp': {
        '__STDC_VERSION__': False,
        '__cplusplus': '201103L',
    },
    '-fms-compatibility-version=18.00.30723': VS('18.00.30723')[None],
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


class LinuxToolchainTest(BaseToolchainTest):
    PATHS = {
        '/usr/bin/gcc': GCC_4_9,
        '/usr/bin/g++': GXX_4_9,
        '/usr/bin/gcc-4.7': GCC_4_7,
        '/usr/bin/g++-4.7': GXX_4_7,
        '/usr/bin/gcc-5': GCC_5,
        '/usr/bin/g++-5': GXX_5,
        '/usr/bin/clang': CLANG_3_6,
        '/usr/bin/clang++': CLANGXX_3_6,
        '/usr/bin/clang-3.6': CLANG_3_6,
        '/usr/bin/clang++-3.6': CLANGXX_3_6,
        '/usr/bin/clang-3.3': CLANG_3_3,
        '/usr/bin/clang++-3.3': CLANGXX_3_3,
    }
    GCC_4_7_RESULT = ('Only GCC 4.8 or newer is supported '
                      '(found version 4.7.3).')
    GXX_4_7_RESULT = GCC_4_7_RESULT
    GCC_4_9_RESULT = CompilerResult(
        flags=['-std=gnu99'],
        version='4.9.3',
        type='gcc',
        compiler='/usr/bin/gcc',
    )
    GXX_4_9_RESULT = CompilerResult(
        flags=['-std=gnu++11'],
        version='4.9.3',
        type='gcc',
        compiler='/usr/bin/g++',
    )
    GCC_5_RESULT = CompilerResult(
        flags=['-std=gnu99'],
        version='5.2.1',
        type='gcc',
        compiler='/usr/bin/gcc-5',
    )
    GXX_5_RESULT = CompilerResult(
        flags=['-std=gnu++11'],
        version='5.2.1',
        type='gcc',
        compiler='/usr/bin/g++-5',
    )
    CLANG_3_3_RESULT = CompilerResult(
        flags=[],
        version='3.3.0',
        type='clang',
        compiler='/usr/bin/clang-3.3',
    )
    CLANGXX_3_3_RESULT = 'Only clang/llvm 3.4 or newer is supported.'
    CLANG_3_6_RESULT = CompilerResult(
        flags=['-std=gnu99'],
        version='3.6.2',
        type='clang',
        compiler='/usr/bin/clang',
    )
    CLANGXX_3_6_RESULT = CompilerResult(
        flags=['-std=gnu++11'],
        version='3.6.2',
        type='clang',
        compiler='/usr/bin/clang++',
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
            '/opt/clang/bin/clang': CLANG_3_6,
            '/opt/clang/bin/clang++': CLANGXX_3_6,
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
            '/usr/bin/afl-clang-fast': CLANG_3_6,
            '/usr/bin/afl-clang-fast++': CLANGXX_3_6,
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


class OSXToolchainTest(BaseToolchainTest):
    HOST = 'x86_64-apple-darwin11.2.0'
    PATHS = LinuxToolchainTest.PATHS
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
        '/opt/VS_2013u2/bin/cl': VS_2013u2,
        '/opt/VS_2013u3/bin/cl': VS_2013u3,
        '/opt/VS_2015/bin/cl': VS_2015,
        '/opt/VS_2015u1/bin/cl': VS_2015u1,
        '/usr/bin/cl': VS_2015u2,
        '/usr/bin/clang-cl': CLANG_CL_3_9,
    }
    PATHS.update(LinuxToolchainTest.PATHS)

    VS_2013u2_RESULT = (
        'This version (18.00.30501) of the MSVC compiler is not supported.\n'
        'You must install Visual C++ 2015 Update 2 or newer in order to build.\n'
        'See https://developer.mozilla.org/en/Windows_Build_Prerequisites')
    VS_2013u3_RESULT = (
        'This version (18.00.30723) of the MSVC compiler is not supported.\n'
        'You must install Visual C++ 2015 Update 2 or newer in order to build.\n'
        'See https://developer.mozilla.org/en/Windows_Build_Prerequisites')
    VS_2015_RESULT = (
        'This version (19.00.23026) of the MSVC compiler is not supported.\n'
        'You must install Visual C++ 2015 Update 2 or newer in order to build.\n'
        'See https://developer.mozilla.org/en/Windows_Build_Prerequisites')
    VS_2015u1_RESULT = (
        'This version (19.00.23506) of the MSVC compiler is not supported.\n'
        'You must install Visual C++ 2015 Update 2 or newer in order to build.\n'
        'See https://developer.mozilla.org/en/Windows_Build_Prerequisites')
    VS_2015u2_RESULT = CompilerResult(
        flags=[],
        version='19.00.23918',
        type='msvc',
        compiler='/usr/bin/cl',
    )
    CLANG_CL_3_9_RESULT = CompilerResult(
        flags=['-Xclang', '-std=gnu99',
               '-fms-compatibility-version=18.00.30723', '-fallback'],
        version='18.00.30723',
        type='clang-cl',
        compiler='/usr/bin/clang-cl',
    )
    CLANGXX_CL_3_9_RESULT = CompilerResult(
        flags=['-fms-compatibility-version=18.00.30723', '-fallback'],
        version='18.00.30723',
        type='clang-cl',
        compiler='/usr/bin/clang-cl',
    )
    CLANG_3_3_RESULT = LinuxToolchainTest.CLANG_3_3_RESULT
    CLANGXX_3_3_RESULT = LinuxToolchainTest.CLANGXX_3_3_RESULT
    CLANG_3_6_RESULT = LinuxToolchainTest.CLANG_3_6_RESULT
    CLANGXX_3_6_RESULT = LinuxToolchainTest.CLANGXX_3_6_RESULT
    GCC_4_7_RESULT = LinuxToolchainTest.GCC_4_7_RESULT
    GCC_4_9_RESULT = LinuxToolchainTest.GCC_4_9_RESULT
    GXX_4_9_RESULT = LinuxToolchainTest.GXX_4_9_RESULT
    GCC_5_RESULT = LinuxToolchainTest.GCC_5_RESULT
    GXX_5_RESULT = LinuxToolchainTest.GXX_5_RESULT

    # VS2015u2 or greater is required.
    def test_msvc(self):
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.VS_2015u2_RESULT,
            'cxx_compiler': self.VS_2015u2_RESULT,
        })

    def test_unsupported_msvc(self):
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


class CrossCompileToolchainTest(BaseToolchainTest):
    TARGET = 'arm-unknown-linux-gnu'
    PATHS = {
        '/usr/bin/arm-linux-gnu-gcc': GCC_4_9,
        '/usr/bin/arm-linux-gnu-g++': GXX_4_9,
        '/usr/bin/arm-linux-gnu-gcc-4.7': GCC_4_7,
        '/usr/bin/arm-linux-gnu-g++-4.7': GXX_4_7,
        '/usr/bin/arm-linux-gnu-gcc-5': GCC_5,
        '/usr/bin/arm-linux-gnu-g++-5': GXX_5,
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

    def test_cross_gcc(self):
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.GCC_4_9_RESULT + {
                'compiler': '/usr/bin/arm-linux-gnu-gcc',
            },
            'cxx_compiler': self.GXX_4_9_RESULT + {
                'compiler': '/usr/bin/arm-linux-gnu-g++',
            },
            'host_c_compiler': self.GCC_4_9_RESULT,
            'host_cxx_compiler': self.GXX_4_9_RESULT,
        })

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


if __name__ == '__main__':
    main()
