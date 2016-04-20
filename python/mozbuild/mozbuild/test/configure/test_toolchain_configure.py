# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging
import os
import re
import types
import unittest

from collections import defaultdict
from fnmatch import fnmatch
from StringIO import StringIO
from textwrap import dedent

from mozunit import (
    main,
    MockedOpen,
)

from common import BaseConfigureTest
from mozbuild.preprocessor import Preprocessor
from mozpack import path as mozpath


class CompilerPreprocessor(Preprocessor):
    VARSUBST = re.compile('(?P<VAR>\w+)', re.U)
    NON_WHITESPACE = re.compile('\S')

    def __init__(self, *args, **kwargs):
        Preprocessor.__init__(self, *args, **kwargs)
        self.do_filter('c_substitution')
        self.setMarker('#\s*')

    def do_if(self, *args, **kwargs):
        # The C preprocessor handles numbers following C rules, which is a
        # different handling than what our Preprocessor does out of the box.
        # Hack around it enough that the configure tests work properly.
        context = self.context
        def normalize_numbers(value):
            if isinstance(value, types.StringTypes):
                if value[-1:] == 'L' and value[:-1].isdigit():
                    value = int(value[:-1])
            return value
        self.context = self.Context(
            (k, normalize_numbers(v)) for k, v in context.iteritems()
        )
        try:
            return Preprocessor.do_if(self, *args, **kwargs)
        finally:
            self.context = context

    class Context(dict):
        def __missing__(self, key):
            return None

    def filter_c_substitution(self, line):
        def repl(matchobj):
            varname = matchobj.group('VAR')
            if varname in self.context:
                result = str(self.context[varname])
                # The C preprocessor inserts whitespaces around expanded
                # symbols.
                start, end = matchobj.span('VAR')
                if self.NON_WHITESPACE.match(line[start-1:start]):
                    result = ' ' + result
                if self.NON_WHITESPACE.match(line[end:end+1]):
                    result = result + ' '
                return result
            return matchobj.group(0)
        return self.VARSUBST.sub(repl, line)


class TestCompilerPreprocessor(unittest.TestCase):
    def test_expansion(self):
        pp = CompilerPreprocessor({
            'A': 1,
            'B': '2',
            'C': 'c',
        })
        pp.out = StringIO()
        input = StringIO('A.B.C')
        input.name = 'foo'
        pp.do_include(input)

        self.assertEquals(pp.out.getvalue(), '1 . 2 . c')

    def test_condition(self):
        pp = CompilerPreprocessor({
            'A': 1,
            'B': '2',
            'C': '0L',
        })
        pp.out = StringIO()
        input = StringIO(dedent('''\
            #ifdef A
            IFDEF_A
            #endif
            #if A
            IF_A
            #endif
            #  if B
            IF_B
            #  else
            IF_NOT_B
            #  endif
            #if !C
            IF_NOT_C
            #else
            IF_C
            #endif
        '''))
        input.name = 'foo'
        pp.do_include(input)

        self.assertEquals('IFDEF_A\nIF_A\nIF_B\nIF_NOT_C\n', pp.out.getvalue())


class FakeCompiler(object):
    '''Defines a fake compiler for use in toolchain tests below.

    The definition given when creating an instance can have one of two
    forms:
    - a dict giving preprocessor symbols and their respective value, e.g.
        { '__GNUC__': 4, '__STDC__': 1 }
    - a dict associating flags to preprocessor symbols. An entry for `None`
      is required in this case. Those are the baseline preprocessor symbols.
      Additional entries describe additional flags to set or existing flags
      to unset (with a value of `False`).
        {
          None: { '__GNUC__': 4, '__STDC__': 1, '__STRICT_ANSI__': 1 },
          '-std=gnu99': { '__STDC_VERSION__': '199901L',
                          '__STRICT_ANSI__': False },
        }
      With the dict above, invoking the preprocessor with no additional flags
      would define __GNUC__, __STDC__ and __STRICT_ANSI__, and with -std=gnu99,
      __GNUC__, __STDC__, and __STDC_VERSION__ (__STRICT_ANSI__ would be
      unset).
      It is also possible to have different symbols depending on the source
      file extension. In this case, the key is '*.ext'. e.g.
        {
          '*.c': { '__STDC__': 1 },
          '*.cpp': { '__cplusplus': '199711L' },
        }
    '''
    def __init__(self, definition):
        if definition.get(None) is None:
            definition = {None: definition}
        self._definition = definition

    def __call__(self, stdin, args):
        files = [arg for arg in args if not arg.startswith('-')]
        flags = [arg for arg in args if arg.startswith('-')]
        if '-E' in flags:
            assert len(files) == 1
            file = files[0]
            pp = CompilerPreprocessor(self._definition[None])

            def apply_defn(defn):
                for k, v in defn.iteritems():
                    if v is False:
                        if k in pp.context:
                            del pp.context[k]
                    else:
                        pp.context[k] = v

            for glob, defn in self._definition.iteritems():
                if glob and not glob.startswith('-') and fnmatch(file, glob):
                    apply_defn(defn)

            for flag in flags:
                apply_defn(self._definition.get(flag, {}))

            pp.out = StringIO()
            pp.do_include(file)
            return 0, pp.out.getvalue(), ''

        return 1, '', ''


class TestFakeCompiler(unittest.TestCase):
    def test_fake_compiler(self):
        with MockedOpen({
            'file': 'A B C',
            'file.c': 'A B C',
        }):
            compiler = FakeCompiler({
                'A': '1',
                'B': '2',
            })
            self.assertEquals(compiler(None, ['-E', 'file']),
                              (0, '1 2 C', ''))

            compiler = FakeCompiler({
                None: {
                    'A': '1',
                    'B': '2',
                },
                '-foo': {
                    'C': 'foo',
                },
                '-bar': {
                    'B': 'bar',
                    'C': 'bar',
                },
                '-qux': {
                    'B': False,
                },
                '*.c': {
                    'B': '42',
                },
            })
            self.assertEquals(compiler(None, ['-E', 'file']),
                              (0, '1 2 C', ''))
            self.assertEquals(compiler(None, ['-E', '-foo', 'file']),
                              (0, '1 2 foo', ''))
            self.assertEquals(compiler(None, ['-E', '-bar', 'file']),
                              (0, '1 bar bar', ''))
            self.assertEquals(compiler(None, ['-E', '-qux', 'file']),
                              (0, '1 B C', ''))
            self.assertEquals(compiler(None, ['-E', '-foo', '-bar', 'file']),
                              (0, '1 bar bar', ''))
            self.assertEquals(compiler(None, ['-E', '-bar', '-foo', 'file']),
                              (0, '1 bar foo', ''))
            self.assertEquals(compiler(None, ['-E', '-bar', '-qux', 'file']),
                              (0, '1 B bar', ''))
            self.assertEquals(compiler(None, ['-E', '-qux', '-bar', 'file']),
                              (0, '1 bar bar', ''))
            self.assertEquals(compiler(None, ['-E', 'file.c']),
                              (0, '1 42 C', ''))
            self.assertEquals(compiler(None, ['-E', '-bar', 'file.c']),
                              (0, '1 bar bar', ''))


GCC_4_7 = FakeCompiler({
    None: {
        '__GNUC__': 4,
        '__GNUC_MINOR__': 7,
        '__GNUC_PATCHLEVEL__': 3,
        '__STDC__': 1,
    },
    '-std=gnu99': {
        '__STDC_VERSION__': '199901L',
    },
})

GXX_4_7 = FakeCompiler({
    None: {
        '__GNUC__': 4,
        '__GNUC_MINOR__': 7,
        '__GNUC_PATCHLEVEL__': 3,
        '__STDC__': 1,
        '__cplusplus': '199711L',
    },
    '-std=gnu++11': {
        '__cplusplus': '201103L',
    },
})

GCC_4_9 = FakeCompiler({
    None: {
        '__GNUC__': 4,
        '__GNUC_MINOR__': 9,
        '__GNUC_PATCHLEVEL__': 3,
        '__STDC__': 1,
    },
    '-std=gnu99': {
        '__STDC_VERSION__': '199901L',
    },
})

GXX_4_9 = FakeCompiler({
    None: {
        '__GNUC__': 4,
        '__GNUC_MINOR__': 9,
        '__GNUC_PATCHLEVEL__': 3,
        '__STDC__': 1,
        '__cplusplus': '199711L',
    },
    '-std=gnu++11': {
        '__cplusplus': '201103L',
    },
})

GCC_5 = FakeCompiler({
    None: {
        '__GNUC__': 5,
        '__GNUC_MINOR__': 2,
        '__GNUC_PATCHLEVEL__': 1,
        '__STDC__': 1,
        '__STDC_VERSION__': '201112L',
    },
    '-std=gnu99': {
        '__STDC_VERSION__': '199901L',
    },
})

GXX_5 = FakeCompiler({
    None: {
        '__GNUC__': 5,
        '__GNUC_MINOR__': 2,
        '__GNUC_PATCHLEVEL__': 1,
        '__STDC__': 1,
        '__cplusplus': '199711L',
    },
    '-std=gnu++11': {
        '__cplusplus': '201103L',
    },
})

CLANG_3_3 = FakeCompiler({
    '__GNUC__': 4,
    '__GNUC_MINOR__': 2,
    '__GNUC_PATCHLEVEL__': 1,
    '__clang__': 1,
    '__clang_major__': 3,
    '__clang_minor__': 3,
    '__clang_patchlevel__': 0,
    '__STDC__': 1,
    '__STDC_VERSION__': '199901L',
})

CLANGXX_3_3 = FakeCompiler({
    None: {
        '__GNUC__': 4,
        '__GNUC_MINOR__': 2,
        '__GNUC_PATCHLEVEL__': 1,
        '__clang__': 1,
        '__clang_major__': 3,
        '__clang_minor__': 3,
        '__clang_patchlevel__': 0,
        '__STDC__': 1,
        '__cplusplus': '199711L',
    },
    '-std=gnu++11': {
        '__cplusplus': '201103L',
    },
})

CLANG_3_6 = FakeCompiler({
    None: {
        '__GNUC__': 4,
        '__GNUC_MINOR__': 2,
        '__GNUC_PATCHLEVEL__': 1,
        '__clang__': 1,
        '__clang_major__': 3,
        '__clang_minor__': 6,
        '__clang_patchlevel__': 2,
        '__STDC__': 1,
        '__STDC_VERSION__': '201112L',
    },
    '-std=gnu99': {
        '__STDC_VERSION__': '199901L',
    },
})

CLANGXX_3_6 = FakeCompiler({
    None: {
        '__GNUC__': 4,
        '__GNUC_MINOR__': 2,
        '__GNUC_PATCHLEVEL__': 1,
        '__clang__': 1,
        '__clang_major__': 3,
        '__clang_minor__': 6,
        '__clang_patchlevel__': 2,
        '__STDC__': 1,
        '__cplusplus': '199711L',
    },
    '-std=gnu++11': {
        '__cplusplus': '201103L',
        '__cpp_static_assert': '200410',
    },
})

VS_2013u2 = FakeCompiler({
    None: {
        '_MSC_VER': '1800',
        '_MSC_FULL_VER': '180030501',
    },
    '*.cpp': {
        '__cplusplus': '199711L',
    },
})

VS_2013u3 = FakeCompiler({
    None: {
        '_MSC_VER': '1800',
        '_MSC_FULL_VER': '180030723',
    },
    '*.cpp': {
        '__cplusplus': '199711L',
    },
})

VS_2015 = FakeCompiler({
    None: {
        '_MSC_VER': '1900',
        '_MSC_FULL_VER': '190023026',
    },
    '*.cpp': {
        '__cplusplus': '199711L',
    },
})

VS_2015u1 = FakeCompiler({
    None: {
        '_MSC_VER': '1900',
        '_MSC_FULL_VER': '190023506',
    },
    '*.cpp': {
        '__cplusplus': '199711L',
    },
})

CLANG_CL = FakeCompiler({
    None: {
        '__clang__': 1,
        '__clang_major__': 3,
        '__clang_minor__': 9,
        '__clang_patchlevel__': 0,
        '__STDC_VERSION__': '201112L',
        '_MSC_VER': '1800',
        '_MSC_FULL_VER': '180000000',
    },
    '-std=gnu99': {  # In reality, the option needs to be preceded by -Xclang.
        '__STDC_VERSION__': '199901L',
    },
    '*.cpp': {
        '__STDC_VERSION__': False,
        '__cplusplus': '201103L',
    },
    '-fms-compatibility-version=18.00.30723': {
        '_MSC_FULL_VER': '180030723',
    },
})


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
          The result can either be an error string, or a dict with the
          following items: flags, version, type, compiler, wrapper. (wrapper
          can be omitted when it's empty). Those items correspond to the
          attributes of the object returned by toolchain.configure checks
          and will be compared to them.
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
            if isinstance(result, dict):
                result = dict(result)
                result.setdefault('wrapper', [])
                result['compiler'] = mozpath.abspath(result['compiler'])
            try:
                self.out.truncate(0)
                compiler = sandbox._value_for(sandbox[var])
                # Add var on both ends to make it clear which of the
                # variables is failing the test when that happens.
                self.assertEquals((var, compiler.__dict__), (var, result))
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
    GCC_4_9_RESULT = {
        'flags': ['-std=gnu99'],
        'version': '4.9.3',
        'type': 'gcc',
        'compiler': '/usr/bin/gcc',
    }
    GXX_4_9_RESULT = {
        'flags': ['-std=gnu++11'],
        'version': '4.9.3',
        'type': 'gcc',
        'compiler': '/usr/bin/g++',
    }
    GCC_5_RESULT = {
        'flags': ['-std=gnu99'],
        'version': '5.2.1',
        'type': 'gcc',
        'compiler': '/usr/bin/gcc-5',
    }
    GXX_5_RESULT = {
        'flags': ['-std=gnu++11'],
        'version': '5.2.1',
        'type': 'gcc',
        'compiler': '/usr/bin/g++-5',
    }
    CLANG_3_3_RESULT = {
        'flags': [],
        'version': '3.3.0',
        'type': 'clang',
        'compiler': '/usr/bin/clang-3.3',
    }
    CLANGXX_3_3_RESULT = 'Only clang/llvm 3.4 or newer is supported.'
    CLANG_3_6_RESULT = {
        'flags': ['-std=gnu99'],
        'version': '3.6.2',
        'type': 'clang',
        'compiler': '/usr/bin/clang',
    }
    CLANGXX_3_6_RESULT = {
        'flags': ['-std=gnu++11'],
        'version': '3.6.2',
        'type': 'clang',
        'compiler': '/usr/bin/clang++',
    }

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
        clang_3_6_result = dict(self.CLANG_3_6_RESULT)
        clang_3_6_result['compiler'] = '/usr/bin/clang-3.6'
        clangxx_3_6_result = dict(self.CLANGXX_3_6_RESULT)
        clangxx_3_6_result['compiler'] = '/usr/bin/clang++-3.6'
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': clang_3_6_result,
            'cxx_compiler': clangxx_3_6_result,
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
        absolute_clang = dict(self.CLANG_3_6_RESULT)
        absolute_clang['compiler'] = '/opt/clang/bin/clang'
        absolute_clangxx = dict(self.CLANGXX_3_6_RESULT)
        absolute_clangxx['compiler'] = '/opt/clang/bin/clang++'
        result = {
            'c_compiler': absolute_clang,
            'cxx_compiler': absolute_clangxx,
        }
        self.do_toolchain_test(paths, result, environ={
            'CC': '/opt/clang/bin/clang',
            'CXX': '/opt/clang/bin/clang++',
        })
        # With CXX guess too.
        self.do_toolchain_test(paths, result, environ={
            'CC': '/opt/clang/bin/clang',
        })

    @unittest.expectedFailure  # Bug 1264609
    def test_atypical_name(self):
        paths = dict(self.PATHS)
        paths.update({
            '/usr/bin/afl-clang-fast': CLANG_3_6,
            '/usr/bin/afl-clang-fast++': CLANGXX_3_6,
        })
        afl_clang_result = dict(self.CLANG_3_6_RESULT)
        afl_clang_result['compiler'] = '/usr/bin/afl-clang-fast'
        afl_clangxx_result = dict(self.CLANGXX_3_6_RESULT)
        afl_clangxx_result['compiler'] = '/usr/bin/afl-clang-fast++'
        self.do_toolchain_test(paths, {
            'c_compiler': afl_clang_result,
            'cxx_compiler': afl_clangxx_result,
        }, environ={
            'CC': 'afl-clang-fast',
            'CXX': 'afl-clang-fast++',
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
        '/usr/bin/cl': VS_2015u1,
        '/usr/bin/clang-cl': CLANG_CL,
    }
    PATHS.update(LinuxToolchainTest.PATHS)

    VS_2013u2_RESULT = (
        'This version (18.00.30501) of the MSVC compiler is not supported.\n'
        'You must install Visual C++ 2013 Update 3, Visual C++ 2015 Update 1, '
        'or newer in order to build.\n'
        'See https://developer.mozilla.org/en/Windows_Build_Prerequisites')
    VS_2013u3_RESULT = {
        'flags': [],
        'version': '18.00.30723',
        'type': 'msvc',
        'compiler': '/opt/VS_2013u3/bin/cl',
    }
    VS_2015_RESULT = (
        'This version (19.00.23026) of the MSVC compiler is not supported.\n'
        'You must install Visual C++ 2013 Update 3, Visual C++ 2015 Update 1, '
        'or newer in order to build.\n'
        'See https://developer.mozilla.org/en/Windows_Build_Prerequisites')
    VS_2015u1_RESULT = {
        'flags': [],
        'version': '19.00.23506',
        'type': 'msvc',
        'compiler': '/usr/bin/cl',
    }
    CLANG_CL_RESULT = {
        'flags': ['-Xclang', '-std=gnu99',
                  '-fms-compatibility-version=18.00.30723', '-fallback'],
        'version': '18.00.30723',
        'type': 'clang-cl',
        'compiler': '/usr/bin/clang-cl',
    }
    CLANGXX_CL_RESULT = {
        'flags': ['-fms-compatibility-version=18.00.30723', '-fallback'],
        'version': '18.00.30723',
        'type': 'clang-cl',
        'compiler': '/usr/bin/clang-cl',
    }
    CLANG_3_3_RESULT = LinuxToolchainTest.CLANG_3_3_RESULT
    CLANGXX_3_3_RESULT = LinuxToolchainTest.CLANGXX_3_3_RESULT
    CLANG_3_6_RESULT = LinuxToolchainTest.CLANG_3_6_RESULT
    CLANGXX_3_6_RESULT = LinuxToolchainTest.CLANGXX_3_6_RESULT
    GCC_4_7_RESULT = LinuxToolchainTest.GCC_4_7_RESULT
    GCC_4_9_RESULT = LinuxToolchainTest.GCC_4_9_RESULT
    GXX_4_9_RESULT = LinuxToolchainTest.GXX_4_9_RESULT
    GCC_5_RESULT = LinuxToolchainTest.GCC_5_RESULT
    GXX_5_RESULT = LinuxToolchainTest.GXX_5_RESULT

    def test_msvc(self):
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.VS_2015u1_RESULT,
            'cxx_compiler': self.VS_2015u1_RESULT,
        })

    def test_msvc_2013(self):
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.VS_2013u3_RESULT,
            'cxx_compiler': self.VS_2013u3_RESULT,
        }, environ={
            'CC': '/opt/VS_2013u3/bin/cl',
        })

    def test_unsupported_msvc(self):
        # While 2013 is supported, update 3 or higher is needed.
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.VS_2013u2_RESULT,
        }, environ={
            'CC': '/opt/VS_2013u2/bin/cl',
        })

        # Likewise with 2015, update 1 or higher is needed.
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.VS_2015_RESULT,
        }, environ={
            'CC': '/opt/VS_2015/bin/cl',
        })

    def test_clang_cl(self):
        # We'll pick clang-cl if msvc can't be found.
        paths = {
            k: v for k, v in self.PATHS.iteritems()
            if os.path.basename(k) != 'cl'
        }
        self.do_toolchain_test(paths, {
            'c_compiler': self.CLANG_CL_RESULT,
            'cxx_compiler': self.CLANGXX_CL_RESULT,
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
    PATHS = {
        '/usr/bin/arm-linux-gnu-gcc': GCC_4_9,
        '/usr/bin/arm-linux-gnu-g++': GXX_4_9,
        '/usr/bin/arm-linux-gnu-gcc-4.7': GCC_4_7,
        '/usr/bin/arm-linux-gnu-g++-4.7': GXX_4_7,
        '/usr/bin/arm-linux-gnu-gcc-5': GCC_5,
        '/usr/bin/arm-linux-gnu-g++-5': GXX_5,
    }
    PATHS.update(LinuxToolchainTest.PATHS)
    ARM_GCC_4_9_RESULT = dict(LinuxToolchainTest.GCC_4_9_RESULT)
    ARM_GCC_4_9_RESULT['compiler'] = '/usr/bin/arm-linux-gnu-gcc'
    ARM_GXX_4_9_RESULT = dict(LinuxToolchainTest.GXX_4_9_RESULT)
    ARM_GXX_4_9_RESULT['compiler'] = '/usr/bin/arm-linux-gnu-g++'
    ARM_GCC_4_7_RESULT = LinuxToolchainTest.GXX_4_7_RESULT
    ARM_GCC_5_RESULT = dict(LinuxToolchainTest.GCC_5_RESULT)
    ARM_GCC_5_RESULT['compiler'] = '/usr/bin/arm-linux-gnu-gcc-5'
    ARM_GXX_5_RESULT = dict(LinuxToolchainTest.GXX_5_RESULT)
    ARM_GXX_5_RESULT['compiler'] = '/usr/bin/arm-linux-gnu-g++-5'
    CLANG_3_6_RESULT = LinuxToolchainTest.CLANG_3_6_RESULT
    CLANGXX_3_6_RESULT = LinuxToolchainTest.CLANGXX_3_6_RESULT
    GCC_4_9_RESULT = LinuxToolchainTest.GCC_4_9_RESULT
    GXX_4_9_RESULT = LinuxToolchainTest.GXX_4_9_RESULT

    def test_cross_gcc(self):
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.ARM_GCC_4_9_RESULT,
            'cxx_compiler': self.ARM_GXX_4_9_RESULT,
            'host_c_compiler': self.GCC_4_9_RESULT,
            'host_cxx_compiler': self.GXX_4_9_RESULT,
        }, args=['--target=arm-unknown-linux-gnu'])

    def test_overridden_cross_gcc(self):
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.ARM_GCC_5_RESULT,
            'cxx_compiler': self.ARM_GXX_5_RESULT,
            'host_c_compiler': self.GCC_4_9_RESULT,
            'host_cxx_compiler': self.GXX_4_9_RESULT,
        }, args=['--target=arm-unknown-linux-gnu'], environ={
            'CC': 'arm-linux-gnu-gcc-5',
            'CXX': 'arm-linux-gnu-g++-5',
        })

    def test_overridden_unsupported_cross_gcc(self):
        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.ARM_GCC_4_7_RESULT,
        }, args=['--target=arm-unknown-linux-gnu'], environ={
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
        }, args=['--target=arm-unknown-linux-gnu'], environ={
            'CC': 'arm-linux-gnu-gcc-5',
        })

        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.ARM_GCC_5_RESULT,
            'cxx_compiler': self.ARM_GXX_5_RESULT,
            'host_c_compiler': self.CLANG_3_6_RESULT,
            'host_cxx_compiler': self.CLANGXX_3_6_RESULT,
        }, args=['--target=arm-unknown-linux-gnu'], environ={
            'CC': 'arm-linux-gnu-gcc-5',
            'HOST_CC': 'clang',
        })

        self.do_toolchain_test(self.PATHS, {
            'c_compiler': self.ARM_GCC_5_RESULT,
            'cxx_compiler': self.ARM_GXX_5_RESULT,
            'host_c_compiler': self.CLANG_3_6_RESULT,
            'host_cxx_compiler': self.CLANGXX_3_6_RESULT,
        }, args=['--target=arm-unknown-linux-gnu'], environ={
            'CC': 'arm-linux-gnu-gcc-5',
            'CXX': 'arm-linux-gnu-g++-5',
            'HOST_CC': 'clang',
        })


if __name__ == '__main__':
    main()
