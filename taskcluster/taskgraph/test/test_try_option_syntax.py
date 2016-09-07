# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest
import itertools

from ..try_option_syntax import TryOptionSyntax
from ..try_option_syntax import RIDEALONG_BUILDS
from ..graph import Graph
from ..taskgraph import TaskGraph
from .util import TestTask
from mozunit import main

# an empty graph, for things that don't look at it
empty_graph = TaskGraph({}, Graph(set(), set()))


def unittest_task(n, tp):
    return (n, TestTask('test', n, {
        'unittest_try_name': n,
        'test_platform': tp,
    }))


def talos_task(n, tp):
    return (n, TestTask('test', n, {
        'talos_try_name': n,
        'test_platform': tp,
    }))

tasks = {k: v for k, v in [
    unittest_task('mochitest-browser-chrome', 'linux'),
    unittest_task('mochitest-browser-chrome-e10s', 'linux64'),
    unittest_task('mochitest-chrome', 'linux'),
    unittest_task('mochitest-webgl', 'linux'),
    unittest_task('crashtest-e10s', 'linux'),
    unittest_task('gtest', 'linux64'),
    talos_task('dromaeojs', 'linux64'),
]}
unittest_tasks = {k: v for k, v in tasks.iteritems()
                  if 'unittest_try_name' in v.attributes}
talos_tasks = {k: v for k, v in tasks.iteritems()
               if 'talos_try_name' in v.attributes}
graph_with_jobs = TaskGraph(tasks, Graph(set(tasks), set()))


class TestTryOptionSyntax(unittest.TestCase):

    def test_empty_message(self):
        "Given an empty message, it should return an empty value"
        tos = TryOptionSyntax('', empty_graph)
        self.assertEqual(tos.build_types, [])
        self.assertEqual(tos.jobs, [])
        self.assertEqual(tos.unittests, [])
        self.assertEqual(tos.talos, [])
        self.assertEqual(tos.platforms, [])

    def test_message_without_try(self):
        "Given a non-try message, it should return an empty value"
        tos = TryOptionSyntax('Bug 1234: frobnicte the foo', empty_graph)
        self.assertEqual(tos.build_types, [])
        self.assertEqual(tos.jobs, [])
        self.assertEqual(tos.unittests, [])
        self.assertEqual(tos.talos, [])
        self.assertEqual(tos.platforms, [])

    def test_unknown_args(self):
        "unknown arguments are ignored"
        tos = TryOptionSyntax('try: --doubledash -z extra', empty_graph)
        # equilvant to "try:"..
        self.assertEqual(tos.build_types, [])
        self.assertEqual(tos.jobs, None)

    def test_b_do(self):
        "-b do should produce both build_types"
        tos = TryOptionSyntax('try: -b do', empty_graph)
        self.assertEqual(sorted(tos.build_types), ['debug', 'opt'])

    def test_b_d(self):
        "-b d should produce build_types=['debug']"
        tos = TryOptionSyntax('try: -b d', empty_graph)
        self.assertEqual(sorted(tos.build_types), ['debug'])

    def test_b_o(self):
        "-b o should produce build_types=['opt']"
        tos = TryOptionSyntax('try: -b o', empty_graph)
        self.assertEqual(sorted(tos.build_types), ['opt'])

    def test_build_o(self):
        "--build o should produce build_types=['opt']"
        tos = TryOptionSyntax('try: --build o', empty_graph)
        self.assertEqual(sorted(tos.build_types), ['opt'])

    def test_b_dx(self):
        "-b dx should produce build_types=['debug'], silently ignoring the x"
        tos = TryOptionSyntax('try: -b dx', empty_graph)
        self.assertEqual(sorted(tos.build_types), ['debug'])

    def test_j_job(self):
        "-j somejob sets jobs=['somejob']"
        tos = TryOptionSyntax('try: -j somejob', empty_graph)
        self.assertEqual(sorted(tos.jobs), ['somejob'])

    def test_j_jobs(self):
        "-j job1,job2 sets jobs=['job1', 'job2']"
        tos = TryOptionSyntax('try: -j job1,job2', empty_graph)
        self.assertEqual(sorted(tos.jobs), ['job1', 'job2'])

    def test_j_all(self):
        "-j all sets jobs=None"
        tos = TryOptionSyntax('try: -j all', empty_graph)
        self.assertEqual(tos.jobs, None)

    def test_j_twice(self):
        "-j job1 -j job2 sets jobs=job1, job2"
        tos = TryOptionSyntax('try: -j job1 -j job2', empty_graph)
        self.assertEqual(sorted(tos.jobs), sorted(['job1', 'job2']))

    def test_p_all(self):
        "-p all sets platforms=None"
        tos = TryOptionSyntax('try: -p all', empty_graph)
        self.assertEqual(tos.platforms, None)

    def test_p_linux(self):
        "-p linux sets platforms=['linux', 'linux-l10n']"
        tos = TryOptionSyntax('try: -p linux', empty_graph)
        self.assertEqual(tos.platforms, ['linux', 'linux-l10n'])

    def test_p_linux_win32(self):
        "-p linux,win32 sets platforms=['linux', 'linux-l10n', 'win32']"
        tos = TryOptionSyntax('try: -p linux,win32', empty_graph)
        self.assertEqual(sorted(tos.platforms), ['linux', 'linux-l10n', 'win32'])

    def test_p_expands_ridealongs(self):
        "-p linux,linux64 includes the RIDEALONG_BUILDS"
        tos = TryOptionSyntax('try: -p linux,linux64', empty_graph)
        ridealongs = list(task
                          for task in itertools.chain.from_iterable(
                                RIDEALONG_BUILDS.itervalues()
                          )
                          if 'android' not in task)  # Don't include android-l10n
        self.assertEqual(sorted(tos.platforms), sorted(['linux', 'linux64'] + ridealongs))

    def test_u_none(self):
        "-u none sets unittests=[]"
        tos = TryOptionSyntax('try: -u none', graph_with_jobs)
        self.assertEqual(sorted(tos.unittests), [])

    def test_u_all(self):
        "-u all sets unittests=[..whole list..]"
        tos = TryOptionSyntax('try: -u all', graph_with_jobs)
        self.assertEqual(sorted(tos.unittests), sorted([{'test': t} for t in unittest_tasks]))

    def test_u_single(self):
        "-u mochitest-webgl sets unittests=[mochitest-webgl]"
        tos = TryOptionSyntax('try: -u mochitest-webgl', graph_with_jobs)
        self.assertEqual(sorted(tos.unittests), sorted([{'test': 'mochitest-webgl'}]))

    def test_u_alias(self):
        "-u mochitest-gl sets unittests=[mochitest-webgl]"
        tos = TryOptionSyntax('try: -u mochitest-gl', graph_with_jobs)
        self.assertEqual(sorted(tos.unittests), sorted([{'test': 'mochitest-webgl'}]))

    def test_u_multi_alias(self):
        "-u e10s sets unittests=[all e10s unittests]"
        tos = TryOptionSyntax('try: -u e10s', graph_with_jobs)
        self.assertEqual(sorted(tos.unittests), sorted([
            {'test': t} for t in unittest_tasks if 'e10s' in t
        ]))

    def test_u_commas(self):
        "-u mochitest-webgl,gtest sets unittests=both"
        tos = TryOptionSyntax('try: -u mochitest-webgl,gtest', graph_with_jobs)
        self.assertEqual(sorted(tos.unittests), sorted([
            {'test': 'mochitest-webgl'},
            {'test': 'gtest'},
        ]))

    def test_u_chunks(self):
        "-u gtest-3,gtest-4 selects the third and fourth chunk of gtest"
        tos = TryOptionSyntax('try: -u gtest-3,gtest-4', graph_with_jobs)
        self.assertEqual(sorted(tos.unittests), sorted([
            {'test': 'gtest', 'only_chunks': set('34')},
        ]))

    def test_u_platform(self):
        "-u gtest[linux] selects the linux platform for gtest"
        tos = TryOptionSyntax('try: -u gtest[linux]', graph_with_jobs)
        self.assertEqual(sorted(tos.unittests), sorted([
            {'test': 'gtest', 'platforms': ['linux']},
        ]))

    def test_u_platforms(self):
        "-u gtest[linux,win32] selects the linux and win32 platforms for gtest"
        tos = TryOptionSyntax('try: -u gtest[linux,win32]', graph_with_jobs)
        self.assertEqual(sorted(tos.unittests), sorted([
            {'test': 'gtest', 'platforms': ['linux', 'win32']},
        ]))

    def test_u_platforms_pretty(self):
        "-u gtest[Ubuntu] selects the linux and linux64 platforms for gtest"
        tos = TryOptionSyntax('try: -u gtest[Ubuntu]', graph_with_jobs)
        self.assertEqual(sorted(tos.unittests), sorted([
            {'test': 'gtest', 'platforms': ['linux', 'linux64']},
        ]))

    def test_u_platforms_negated(self):
        "-u gtest[-linux] selects all platforms but linux for gtest"
        tos = TryOptionSyntax('try: -u gtest[-linux]', graph_with_jobs)
        self.assertEqual(sorted(tos.unittests), sorted([
            {'test': 'gtest', 'platforms': ['linux64']},
        ]))

    def test_u_platforms_negated_pretty(self):
        "-u gtest[Ubuntu,-x64] selects just linux for gtest"
        tos = TryOptionSyntax('try: -u gtest[Ubuntu,-x64]', graph_with_jobs)
        self.assertEqual(sorted(tos.unittests), sorted([
            {'test': 'gtest', 'platforms': ['linux']},
        ]))

    def test_u_chunks_platforms(self):
        "-u gtest-1[linux,win32] selects the linux and win32 platforms for chunk 1 of gtest"
        tos = TryOptionSyntax('try: -u gtest-1[linux,win32]', graph_with_jobs)
        self.assertEqual(sorted(tos.unittests), sorted([
            {'test': 'gtest', 'platforms': ['linux', 'win32'], 'only_chunks': set('1')},
        ]))

    def test_t_none(self):
        "-t none sets talos=[]"
        tos = TryOptionSyntax('try: -t none', graph_with_jobs)
        self.assertEqual(sorted(tos.talos), [])

    def test_t_all(self):
        "-t all sets talos=[..whole list..]"
        tos = TryOptionSyntax('try: -t all', graph_with_jobs)
        self.assertEqual(sorted(tos.talos), sorted([{'test': t} for t in talos_tasks]))

    def test_t_single(self):
        "-t mochitest-webgl sets talos=[mochitest-webgl]"
        tos = TryOptionSyntax('try: -t mochitest-webgl', graph_with_jobs)
        self.assertEqual(sorted(tos.talos), sorted([{'test': 'mochitest-webgl'}]))

    # -t shares an implementation with -u, so it's not tested heavily

    def test_trigger_tests(self):
        "--rebuild 10 sets trigger_tests"
        tos = TryOptionSyntax('try: --rebuild 10', empty_graph)
        self.assertEqual(tos.trigger_tests, 10)

    def test_interactive(self):
        "--interactive sets interactive"
        tos = TryOptionSyntax('try: --interactive', empty_graph)
        self.assertEqual(tos.interactive, True)

if __name__ == '__main__':
    main()
