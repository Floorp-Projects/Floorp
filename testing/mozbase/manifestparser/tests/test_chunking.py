#!/usr/bin/env python

from itertools import chain
import os
import unittest

from manifestparser.filters import (
    chunk_by_dir,
    chunk_by_slice,
)

here = os.path.dirname(os.path.abspath(__file__))


class ChunkBySlice(unittest.TestCase):
    """Test chunking related filters"""

    def generate_tests(self, num, disabled=None):
        disabled = disabled or []
        tests = []
        for i in range(num):
            test = {'name': 'test%i' % i}
            if i in disabled:
                test['disabled'] = ''
            tests.append(test)
        return tests

    def run_all_combos(self, num_tests, disabled=None):
        tests = self.generate_tests(num_tests, disabled=disabled)

        for total in range(1, num_tests + 1):
            res = []
            res_disabled = []
            for chunk in range(1, total+1):
                f = chunk_by_slice(chunk, total)
                res.append(list(f(tests, {})))
                if disabled:
                    f.disabled = True
                    res_disabled.append(list(f(tests, {})))

            lengths = [len([t for t in c if 'disabled' not in t]) for c in res]
            # the chunk with the most tests should have at most one more test
            # than the chunk with the least tests
            self.assertLessEqual(max(lengths) - min(lengths), 1)

            # chaining all chunks back together should equal the original list
            # of tests
            self.assertEqual(list(chain.from_iterable(res)), list(tests))

            if disabled:
                lengths = [len(c) for c in res_disabled]
                self.assertLessEqual(max(lengths) - min(lengths), 1)
                self.assertEqual(list(chain.from_iterable(res_disabled)), list(tests))

    def test_chunk_by_slice(self):
        chunk = chunk_by_slice(1, 1)
        self.assertEqual(list(chunk([], {})), [])

        self.run_all_combos(num_tests=1)
        self.run_all_combos(num_tests=10, disabled=[1, 2])

        num_tests = 67
        disabled = list(i for i in xrange(num_tests) if i % 4 == 0)
        self.run_all_combos(num_tests=num_tests, disabled=disabled)


class ChunkByDir(unittest.TestCase):
    """Test chunking related filters"""

    def generate_tests(self, dirs):
        """
        :param dirs: dict of the form,
                        { <dir>: <num tests>
        """
        i = 0
        for d, num in dirs.iteritems():
            for j in range(num):
                i += 1
                name = 'test%i' % i
                test = {'name': name,
                        'path': os.path.join(d, name)}
                yield test

    def run_all_combos(self, dirs):
        tests = self.generate_tests(dirs)

        deepest = max(len(t['path'].split(os.sep))-1 for t in tests)
        for depth in range(1, deepest+1):

            def num_groups(tests):
                unique = set()
                for p in [t['path'] for t in tests]:
                    p = p.split(os.sep)
                    p = p[:min(depth, len(p)-1)]
                    unique.add(os.sep.join(p))
                return len(unique)

            for total in range(1, num_groups(tests)+1):
                res = []
                for this in range(1, total+1):
                    f = chunk_by_dir(this, total, depth)
                    res.append(f(tests, {}))

                lengths = map(num_groups, res)
                # the chunk with the most dirs should have at most one more
                # dir than the chunk with the least dirs
                self.assertLessEqual(max(lengths) - min(lengths), 1)

                all_chunks = list(chain.from_iterable(res))
                # chunk_by_dir will mess up order, but chained chunks should
                # contain all of the original tests and be the same length
                self.assertEqual(len(all_chunks), len(tests))
                for t in tests:
                    self.assertIn(t, all_chunks)

    def test_chunk_by_dir(self):
        chunk = chunk_by_dir(1, 1, 1)
        self.assertEqual(list(chunk([], {})), [])

        dirs = {
            'a': 2,
        }
        self.run_all_combos(dirs)

        dirs = {
            '': 1,
            'foo': 1,
            'bar': 0,
            '/foobar': 1,
        }
        self.run_all_combos(dirs)

        dirs = {
            'a': 1,
            'b': 1,
            'a/b': 2,
            'a/c': 1,
        }
        self.run_all_combos(dirs)

        dirs = {
            'a': 5,
            'a/b': 4,
            'a/b/c': 7,
            'a/b/c/d': 1,
            'a/b/c/e': 3,
            'b/c': 2,
            'b/d': 5,
            'b/d/e': 6,
            'c': 8,
            'c/d/e/f/g/h/i/j/k/l': 5,
            'c/d/e/f/g/i/j/k/l/m/n': 2,
            'c/e': 1,
        }
        self.run_all_combos(dirs)
