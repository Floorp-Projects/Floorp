# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import shutil
import unittest

from mozunit import main

from mozbuild.frontend.reader import (
    MozbuildSandbox,
    SandboxCalledError,
)

from mozbuild.frontend.sandbox import (
    Sandbox,
    SandboxExecutionError,
    SandboxLoadError,
)

from mozbuild.frontend.context import (
    Context,
    FUNCTIONS,
    SPECIAL_VARIABLES,
    VARIABLES,
)

from mozbuild.test.common import MockConfig

import mozpack.path as mozpath

test_data_path = mozpath.abspath(mozpath.dirname(__file__))
test_data_path = mozpath.join(test_data_path, 'data')


class TestSandbox(unittest.TestCase):
    def sandbox(self):
        return Sandbox(Context(VARIABLES))

    def test_exec_source_success(self):
        sandbox = self.sandbox()
        context = sandbox._context

        sandbox.exec_source('foo = True', mozpath.abspath('foo.py'))

        self.assertNotIn('foo', context)
        self.assertEqual(context.main_path, mozpath.abspath('foo.py'))
        self.assertEqual(context.all_paths, set([mozpath.abspath('foo.py')]))

    def test_exec_compile_error(self):
        sandbox = self.sandbox()

        with self.assertRaises(SandboxExecutionError) as se:
            sandbox.exec_source('2f23;k;asfj', mozpath.abspath('foo.py'))

        self.assertEqual(se.exception.file_stack, [mozpath.abspath('foo.py')])
        self.assertIsInstance(se.exception.exc_value, SyntaxError)
        self.assertEqual(sandbox._context.main_path, mozpath.abspath('foo.py'))

    def test_exec_import_denied(self):
        sandbox = self.sandbox()

        with self.assertRaises(SandboxExecutionError) as se:
            sandbox.exec_source('import sys')

        self.assertIsInstance(se.exception, SandboxExecutionError)
        self.assertEqual(se.exception.exc_type, ImportError)

    def test_exec_source_multiple(self):
        sandbox = self.sandbox()

        sandbox.exec_source('DIRS = ["foo"]')
        sandbox.exec_source('DIRS += ["bar"]')

        self.assertEqual(sandbox['DIRS'], ['foo', 'bar'])

    def test_exec_source_illegal_key_set(self):
        sandbox = self.sandbox()

        with self.assertRaises(SandboxExecutionError) as se:
            sandbox.exec_source('ILLEGAL = True')

        e = se.exception
        self.assertIsInstance(e.exc_value, KeyError)

        e = se.exception.exc_value
        self.assertEqual(e.args[0], 'global_ns')
        self.assertEqual(e.args[1], 'set_unknown')

    def test_exec_source_reassign(self):
        sandbox = self.sandbox()

        sandbox.exec_source('DIRS = ["foo"]')
        with self.assertRaises(SandboxExecutionError) as se:
          sandbox.exec_source('DIRS = ["bar"]')

        self.assertEqual(sandbox['DIRS'], ['foo'])
        e = se.exception
        self.assertIsInstance(e.exc_value, KeyError)

        e = se.exception.exc_value
        self.assertEqual(e.args[0], 'global_ns')
        self.assertEqual(e.args[1], 'reassign')
        self.assertEqual(e.args[2], 'DIRS')

    def test_exec_source_reassign_builtin(self):
        sandbox = self.sandbox()

        with self.assertRaises(SandboxExecutionError) as se:
            sandbox.exec_source('True = 1')

        e = se.exception
        self.assertIsInstance(e.exc_value, KeyError)

        e = se.exception.exc_value
        self.assertEqual(e.args[0], 'Cannot reassign builtins')


class TestMozbuildSandbox(unittest.TestCase):
    def sandbox(self, data_path=None, metadata={}):
        config = None

        if data_path is not None:
            config = MockConfig(mozpath.join(test_data_path, data_path))
        else:
            config = MockConfig()

        return MozbuildSandbox(Context(VARIABLES, config), metadata)

    def test_default_state(self):
        sandbox = self.sandbox()
        sandbox._context.add_source(sandbox.normalize_path('moz.build'))
        config = sandbox._context.config

        self.assertEqual(sandbox['TOPSRCDIR'], config.topsrcdir)
        self.assertEqual(sandbox['TOPOBJDIR'], config.topobjdir)
        self.assertEqual(sandbox['RELATIVEDIR'], '')
        self.assertEqual(sandbox['SRCDIR'], config.topsrcdir)
        self.assertEqual(sandbox['OBJDIR'], config.topobjdir)

    def test_symbol_presence(self):
        # Ensure no discrepancies between the master symbol table and what's in
        # the sandbox.
        sandbox = self.sandbox()
        sandbox._context.add_source(sandbox.normalize_path('moz.build'))

        all_symbols = set()
        all_symbols |= set(FUNCTIONS.keys())
        all_symbols |= set(SPECIAL_VARIABLES.keys())

        for symbol in all_symbols:
            self.assertIsNotNone(sandbox[symbol])

    def test_path_calculation(self):
        sandbox = self.sandbox()
        sandbox._context.add_source(sandbox.normalize_path('foo/bar/moz.build'))
        config = sandbox._context.config

        self.assertEqual(sandbox['TOPSRCDIR'], config.topsrcdir)
        self.assertEqual(sandbox['TOPOBJDIR'], config.topobjdir)
        self.assertEqual(sandbox['RELATIVEDIR'], 'foo/bar')
        self.assertEqual(sandbox['SRCDIR'],
            mozpath.join(config.topsrcdir, 'foo/bar'))
        self.assertEqual(sandbox['OBJDIR'],
            mozpath.join(config.topobjdir, 'foo/bar'))

    def test_config_access(self):
        sandbox = self.sandbox()
        config = sandbox._context.config

        self.assertEqual(sandbox['CONFIG']['MOZ_TRUE'], '1')
        self.assertEqual(sandbox['CONFIG']['MOZ_FOO'], config.substs['MOZ_FOO'])

        # Access to an undefined substitution should return None.
        self.assertNotIn('MISSING', sandbox['CONFIG'])
        self.assertIsNone(sandbox['CONFIG']['MISSING'])

        # Should shouldn't be allowed to assign to the config.
        with self.assertRaises(Exception):
            sandbox['CONFIG']['FOO'] = ''

    def test_special_variables(self):
        sandbox = self.sandbox()

        for k in SPECIAL_VARIABLES:
            with self.assertRaises(KeyError):
                sandbox[k] = 0

    def test_exec_source_reassign_exported(self):
        config = MockConfig()

        exports = {'DIST_SUBDIR': 'browser'}

        sandbox = MozbuildSandbox(Context(VARIABLES, config),
            metadata={'exports': exports})

        self.assertEqual(sandbox['DIST_SUBDIR'], 'browser')

        sandbox.exec_source('DIST_SUBDIR = "foo"')
        with self.assertRaises(SandboxExecutionError) as se:
          sandbox.exec_source('DIST_SUBDIR = "bar"')

        self.assertEqual(sandbox['DIST_SUBDIR'], 'foo')
        e = se.exception
        self.assertIsInstance(e.exc_value, KeyError)

        e = se.exception.exc_value
        self.assertEqual(e.args[0], 'global_ns')
        self.assertEqual(e.args[1], 'reassign')
        self.assertEqual(e.args[2], 'DIST_SUBDIR')

    def test_add_tier_dir_regular_str(self):
        sandbox = self.sandbox()

        sandbox.exec_source('add_tier_dir("t1", "foo")')

        self.assertEqual(sandbox['TIERS']['t1'],
            {'regular': ['foo'], 'external': []})

    def test_add_tier_dir_regular_list(self):
        sandbox = self.sandbox()

        sandbox.exec_source('add_tier_dir("t1", ["foo", "bar"])')

        self.assertEqual(sandbox['TIERS']['t1'],
            {'regular': ['foo', 'bar'], 'external': []})

    def test_add_tier_dir_external(self):
        sandbox = self.sandbox()

        sandbox.exec_source('add_tier_dir("t1", "foo", external=True)')

        self.assertEqual(sandbox['TIERS']['t1'],
            {'regular': [], 'external': ['foo']})

    def test_tier_order(self):
        sandbox = self.sandbox()

        source = '''
add_tier_dir('t1', 'foo')
add_tier_dir('t1', 'bar')
add_tier_dir('t2', 'baz')
add_tier_dir('t3', 'biz')
add_tier_dir('t1', 'bat')
'''

        sandbox.exec_source(source)

        self.assertEqual([k for k in sandbox['TIERS'].keys()], ['t1', 't2', 't3'])

    def test_tier_multiple_registration(self):
        sandbox = self.sandbox()

        sandbox.exec_source('add_tier_dir("t1", "foo")')

        with self.assertRaises(SandboxExecutionError):
            sandbox.exec_source('add_tier_dir("t1", "foo")')

    def test_include_basic(self):
        sandbox = self.sandbox(data_path='include-basic')

        sandbox.exec_file('moz.build')

        self.assertEqual(sandbox['DIRS'], ['foo', 'bar'])
        self.assertEqual(sandbox._context.main_path,
            sandbox.normalize_path('moz.build'))
        self.assertEqual(len(sandbox._context.all_paths), 2)

    def test_include_outside_topsrcdir(self):
        sandbox = self.sandbox(data_path='include-outside-topsrcdir')

        with self.assertRaises(SandboxLoadError) as se:
            sandbox.exec_file('relative.build')

        self.assertEqual(se.exception.illegal_path, '../moz.build')

    def test_include_error_stack(self):
        # Ensure the path stack is reported properly in exceptions.
        sandbox = self.sandbox(data_path='include-file-stack')

        with self.assertRaises(SandboxExecutionError) as se:
            sandbox.exec_file('moz.build')

        e = se.exception
        self.assertIsInstance(e.exc_value, KeyError)

        args = e.exc_value.args
        self.assertEqual(args[0], 'global_ns')
        self.assertEqual(args[1], 'set_unknown')
        self.assertEqual(args[2], 'ILLEGAL')

        expected_stack = [mozpath.join(sandbox._context.config.topsrcdir, p) for p in [
            'moz.build', 'included-1.build', 'included-2.build']]

        self.assertEqual(e.file_stack, expected_stack)

    def test_include_missing(self):
        sandbox = self.sandbox(data_path='include-missing')

        with self.assertRaises(SandboxLoadError) as sle:
            sandbox.exec_file('moz.build')

        self.assertIsNotNone(sle.exception.read_error)

    def test_include_relative_from_child_dir(self):
        # A relative path from a subdirectory should be relative from that
        # child directory.
        sandbox = self.sandbox(data_path='include-relative-from-child')
        sandbox.exec_file('child/child.build')
        self.assertEqual(sandbox['DIRS'], ['foo'])

        sandbox = self.sandbox(data_path='include-relative-from-child')
        sandbox.exec_file('child/child2.build')
        self.assertEqual(sandbox['DIRS'], ['foo'])

    def test_include_topsrcdir_relative(self):
        # An absolute path for include() is relative to topsrcdir.

        sandbox = self.sandbox(data_path='include-topsrcdir-relative')
        sandbox.exec_file('moz.build')

        self.assertEqual(sandbox['DIRS'], ['foo'])

    def test_error(self):
        sandbox = self.sandbox()

        with self.assertRaises(SandboxCalledError) as sce:
            sandbox.exec_source('error("This is an error.")')

        e = sce.exception
        self.assertEqual(e.message, 'This is an error.')

    def test_substitute_config_files(self):
        sandbox = self.sandbox()

        sandbox.exec_source('CONFIGURE_SUBST_FILES += ["bar", "foo"]')
        self.assertEqual(sandbox['CONFIGURE_SUBST_FILES'], ['bar', 'foo'])

    def test_invalid_utf8_substs(self):
        """Ensure invalid UTF-8 in substs is converted with an error."""

        # This is really mbcs. It's a bunch of invalid UTF-8.
        config = MockConfig(extra_substs={'BAD_UTF8': b'\x83\x81\x83\x82\x3A'})

        sandbox = MozbuildSandbox(Context(VARIABLES, config))

        self.assertEqual(sandbox['CONFIG']['BAD_UTF8'],
            u'\ufffd\ufffd\ufffd\ufffd:')

    def test_invalid_exports_set_base(self):
        sandbox = self.sandbox()

        with self.assertRaises(SandboxExecutionError) as se:
            sandbox.exec_source('EXPORTS = "foo.h"')

        self.assertEqual(se.exception.exc_type, ValueError)

    def test_templates(self):
        sandbox = self.sandbox(data_path='templates')

        # Templates need to be defined in actual files because of
        # inspect.getsourcelines.
        sandbox.exec_file('templates.mozbuild')

        sandbox2 = self.sandbox(metadata={'templates': sandbox.templates})
        source = '''
Template([
    'foo.cpp',
])
'''
        sandbox2.exec_source(source, sandbox.normalize_path('foo.mozbuild'))

        self.assertEqual(sandbox2._context, {
            'SOURCES': ['foo.cpp'],
            'DIRS': [],
        })

        sandbox2 = self.sandbox(metadata={'templates': sandbox.templates})
        source = '''
SOURCES += ['qux.cpp']
Template([
    'bar.cpp',
    'foo.cpp',
],[
    'foo',
])
SOURCES += ['hoge.cpp']
'''
        sandbox2.exec_source(source, sandbox.normalize_path('foo.mozbuild'))

        self.assertEqual(sandbox2._context, {
            'SOURCES': ['qux.cpp', 'bar.cpp', 'foo.cpp', 'hoge.cpp'],
            'DIRS': ['foo'],
        })

        source = '''
TemplateError([
    'foo.cpp',
])
'''
        with self.assertRaises(SandboxExecutionError) as se:
            sandbox2.exec_source(source, sandbox.normalize_path('foo.mozbuild'))

        e = se.exception
        self.assertIsInstance(e.exc_value, KeyError)

        e = se.exception.exc_value
        self.assertEqual(e.args[0], 'global_ns')
        self.assertEqual(e.args[1], 'set_unknown')

        # TemplateGlobalVariable tries to access 'illegal' but that is expected
        # to throw.
        source = '''
illegal = True
TemplateGlobalVariable()
'''
        with self.assertRaises(SandboxExecutionError) as se:
            sandbox2.exec_source(source, sandbox.normalize_path('foo.mozbuild'))

        e = se.exception
        self.assertIsInstance(e.exc_value, NameError)

        # TemplateGlobalUPPERVariable sets SOURCES with DIRS, but the context
        # used when running the template is not expected to access variables
        # from the global context.
        sandbox2 = self.sandbox(metadata={'templates': sandbox.templates})
        source = '''
DIRS += ['foo']
TemplateGlobalUPPERVariable()
'''
        sandbox2.exec_source(source, sandbox.normalize_path('foo.mozbuild'))
        self.assertEqual(sandbox2._context, {
            'SOURCES': [],
            'DIRS': ['foo'],
        })

        # However, the result of the template is mixed with the global
        # context.
        sandbox2 = self.sandbox(metadata={'templates': sandbox.templates})
        source = '''
SOURCES += ['qux.cpp']
TemplateInherit([
    'bar.cpp',
    'foo.cpp',
])
SOURCES += ['hoge.cpp']
'''
        sandbox2.exec_source(source, sandbox.normalize_path('foo.mozbuild'))

        self.assertEqual(sandbox2._context, {
            'SOURCES': ['qux.cpp', 'bar.cpp', 'foo.cpp', 'hoge.cpp'],
            'USE_LIBS': ['foo'],
            'DIRS': [],
        })

        # Template names must be CamelCase. Here, we can define the template
        # inline because the error happens before inspect.getsourcelines.
        source = '''
@template
def foo():
    pass
'''

        with self.assertRaises(SandboxExecutionError) as se:
            sandbox2.exec_source(source, sandbox.normalize_path('foo.mozbuild'))

        e = se.exception
        self.assertIsInstance(e.exc_value, NameError)

        e = se.exception.exc_value
        self.assertEqual(e.message,
            'Template function names must be CamelCase.')

        # Template names must not already be registered.
        source = '''
@template
def Template():
    pass
'''
        with self.assertRaises(SandboxExecutionError) as se:
            sandbox2.exec_source(source, sandbox.normalize_path('foo.mozbuild'))

        e = se.exception
        self.assertIsInstance(e.exc_value, KeyError)

        e = se.exception.exc_value
        self.assertEqual(e.message,
            'A template named "Template" was already declared in %s.' %
            sandbox.normalize_path('templates.mozbuild'))


if __name__ == '__main__':
    main()
