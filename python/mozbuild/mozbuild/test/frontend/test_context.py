# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import unittest

from mozunit import main

from mozbuild.frontend.context import (
    Context,
    FUNCTIONS,
    SPECIAL_VARIABLES,
    VARIABLES,
)

from mozpack import path as mozpath


class TestContext(unittest.TestCase):
    def test_defaults(self):
        test = Context({
            'foo': (int, int, '', None),
            'bar': (bool, bool, '', None),
            'baz': (dict, dict, '', None),
        })

        self.assertEqual(test.keys(), [])

        self.assertEqual(test['foo'], 0)

        self.assertEqual(set(test.keys()), { 'foo' })

        self.assertEqual(test['bar'], False)

        self.assertEqual(set(test.keys()), { 'foo', 'bar' })

        self.assertEqual(test['baz'], {})

        self.assertEqual(set(test.keys()), { 'foo', 'bar', 'baz' })

        with self.assertRaises(KeyError):
            test['qux']

        self.assertEqual(set(test.keys()), { 'foo', 'bar', 'baz' })

    def test_type_check(self):
        test = Context({
            'foo': (int, int, '', None),
            'baz': (dict, list, '', None),
        })

        test['foo'] = 5

        self.assertEqual(test['foo'], 5)

        with self.assertRaises(ValueError):
            test['foo'] = {}

        self.assertEqual(test['foo'], 5)

        with self.assertRaises(KeyError):
            test['bar'] = True

        test['baz'] = [('a', 1), ('b', 2)]

        self.assertEqual(test['baz'], { 'a': 1, 'b': 2 })

    def test_update(self):
        test = Context({
            'foo': (int, int, '', None),
            'bar': (bool, bool, '', None),
            'baz': (dict, list, '', None),
        })

        self.assertEqual(test.keys(), [])

        with self.assertRaises(ValueError):
            test.update(bar=True, foo={})

        self.assertEqual(test.keys(), [])

        test.update(bar=True, foo=1)

        self.assertEqual(set(test.keys()), { 'foo', 'bar' })
        self.assertEqual(test['foo'], 1)
        self.assertEqual(test['bar'], True)

        test.update([('bar', False), ('foo', 2)])
        self.assertEqual(test['foo'], 2)
        self.assertEqual(test['bar'], False)

        test.update([('foo', 0), ('baz', { 'a': 1, 'b': 2 })])
        self.assertEqual(test['foo'], 0)
        self.assertEqual(test['baz'], { 'a': 1, 'b': 2 })

        test.update([('foo', 42), ('baz', [('c', 3), ('d', 4)])])
        self.assertEqual(test['foo'], 42)
        self.assertEqual(test['baz'], { 'c': 3, 'd': 4 })

    def test_paths(self):
        test = Context()

        # Newly created context has no paths.
        self.assertIsNone(test.main_path)
        self.assertIsNone(test.current_path)
        self.assertEqual(test.all_paths, set())
        self.assertEqual(test.source_stack, [])

        foo = os.path.abspath('foo')
        test.add_source(foo)

        # Adding the first source makes it the main and current path.
        self.assertEqual(test.main_path, foo)
        self.assertEqual(test.current_path, foo)
        self.assertEqual(test.all_paths, set([foo]))
        self.assertEqual(test.source_stack, [foo])

        bar = os.path.abspath('bar')
        test.add_source(bar)

        # Adding the second source makes leaves main and current paths alone.
        self.assertEqual(test.main_path, foo)
        self.assertEqual(test.current_path, foo)
        self.assertEqual(test.all_paths, set([bar, foo]))
        self.assertEqual(test.source_stack, [foo])

        qux = os.path.abspath('qux')
        test.push_source(qux)

        # Pushing a source makes it the current path
        self.assertEqual(test.main_path, foo)
        self.assertEqual(test.current_path, qux)
        self.assertEqual(test.all_paths, set([bar, foo, qux]))
        self.assertEqual(test.source_stack, [foo, qux])

        hoge = os.path.abspath('hoge')
        test.push_source(hoge)
        self.assertEqual(test.main_path, foo)
        self.assertEqual(test.current_path, hoge)
        self.assertEqual(test.all_paths, set([bar, foo, hoge, qux]))
        self.assertEqual(test.source_stack, [foo, qux, hoge])

        fuga = os.path.abspath('fuga')

        # Adding a source after pushing doesn't change the source stack
        test.add_source(fuga)
        self.assertEqual(test.main_path, foo)
        self.assertEqual(test.current_path, hoge)
        self.assertEqual(test.all_paths, set([bar, foo, fuga, hoge, qux]))
        self.assertEqual(test.source_stack, [foo, qux, hoge])

        # Adding a source twice doesn't change anything
        test.add_source(qux)
        self.assertEqual(test.main_path, foo)
        self.assertEqual(test.current_path, hoge)
        self.assertEqual(test.all_paths, set([bar, foo, fuga, hoge, qux]))
        self.assertEqual(test.source_stack, [foo, qux, hoge])

        last = test.pop_source()

        # Popping a source returns the last pushed one, not the last added one.
        self.assertEqual(last, hoge)
        self.assertEqual(test.main_path, foo)
        self.assertEqual(test.current_path, qux)
        self.assertEqual(test.all_paths, set([bar, foo, fuga, hoge, qux]))
        self.assertEqual(test.source_stack, [foo, qux])

        last = test.pop_source()
        self.assertEqual(last, qux)
        self.assertEqual(test.main_path, foo)
        self.assertEqual(test.current_path, foo)
        self.assertEqual(test.all_paths, set([bar, foo, fuga, hoge, qux]))
        self.assertEqual(test.source_stack, [foo])

        # Popping the main path is allowed.
        last = test.pop_source()
        self.assertEqual(last, foo)
        self.assertEqual(test.main_path, foo)
        self.assertIsNone(test.current_path)
        self.assertEqual(test.all_paths, set([bar, foo, fuga, hoge, qux]))
        self.assertEqual(test.source_stack, [])

        # Popping past the main path asserts.
        with self.assertRaises(AssertionError):
            test.pop_source()

        # Pushing after the main path was popped asserts.
        with self.assertRaises(AssertionError):
            test.push_source(foo)

        test = Context()
        test.push_source(foo)
        test.push_source(bar)

        # Pushing the same file twice is allowed.
        test.push_source(bar)
        test.push_source(foo)
        self.assertEqual(last, foo)
        self.assertEqual(test.main_path, foo)
        self.assertEqual(test.current_path, foo)
        self.assertEqual(test.all_paths, set([bar, foo]))
        self.assertEqual(test.source_stack, [foo, bar, bar, foo])

    def test_dirs(self):
        class Config(object): pass
        config = Config()
        config.topsrcdir = mozpath.abspath(os.curdir)
        config.topobjdir = mozpath.abspath('obj')
        test = Context(config=config)
        foo = mozpath.abspath('foo')
        test.push_source(foo)

        self.assertEqual(test.srcdir, config.topsrcdir)
        self.assertEqual(test.relsrcdir, '')
        self.assertEqual(test.objdir, config.topobjdir)
        self.assertEqual(test.relobjdir, '')

        foobar = os.path.abspath('foo/bar')
        test.push_source(foobar)
        self.assertEqual(test.srcdir, mozpath.join(config.topsrcdir, 'foo'))
        self.assertEqual(test.relsrcdir, 'foo')
        self.assertEqual(test.objdir, config.topobjdir)
        self.assertEqual(test.relobjdir, '')


class TestSymbols(unittest.TestCase):
    def _verify_doc(self, doc):
        # Documentation should be of the format:
        # """SUMMARY LINE
        #
        # EXTRA PARAGRAPHS
        # """

        self.assertNotIn('\r', doc)

        lines = doc.split('\n')

        # No trailing whitespace.
        for line in lines[0:-1]:
            self.assertEqual(line, line.rstrip())

        self.assertGreater(len(lines), 0)
        self.assertGreater(len(lines[0].strip()), 0)

        # Last line should be empty.
        self.assertEqual(lines[-1].strip(), '')

    def test_documentation_formatting(self):
        for typ, inp, doc, tier in VARIABLES.values():
            self._verify_doc(doc)

        for attr, args, doc in FUNCTIONS.values():
            self._verify_doc(doc)

        for func, typ, doc in SPECIAL_VARIABLES.values():
            self._verify_doc(doc)


if __name__ == '__main__':
    main()
