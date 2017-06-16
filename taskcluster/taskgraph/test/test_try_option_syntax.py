# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest

from ..try_option_syntax import TryOptionSyntax
from ..try_option_syntax import RIDEALONG_BUILDS
from ..graph import Graph
from ..taskgraph import TaskGraph
from ..task import Task
from mozunit import main


def unittest_task(n, tp, bt='opt'):
    return (n, Task('test', n, {
        'unittest_try_name': n,
        'test_platform': tp.split('/')[0],
        'build_type': bt,
    }, {}))


def talos_task(n, tp, bt='opt'):
    return (n, Task('test', n, {
        'talos_try_name': n,
        'test_platform': tp.split('/')[0],
        'build_type': bt,
    }, {}))


tasks = {k: v for k, v in [
    unittest_task('mochitest-browser-chrome', 'linux/opt'),
    unittest_task('mochitest-browser-chrome-e10s', 'linux64/opt'),
    unittest_task('mochitest-chrome', 'linux/debug', 'debug'),
    unittest_task('mochitest-webgl', 'linux/debug', 'debug'),
    unittest_task('extra1', 'linux', 'debug/opt'),
    unittest_task('extra2', 'win32/opt'),
    unittest_task('crashtest-e10s', 'linux/other'),
    unittest_task('gtest', 'linux64/asan'),
    talos_task('dromaeojs', 'linux64/psan'),
    unittest_task('extra3', 'linux/opt'),
    unittest_task('extra4', 'linux64/debug', 'debug'),
    unittest_task('extra5', 'linux/this'),
    unittest_task('extra6', 'linux/that'),
    unittest_task('extra7', 'linux/other'),
    unittest_task('extra8', 'linux64/asan'),
    talos_task('extra9', 'linux64/psan'),
]}

for r in RIDEALONG_BUILDS.values():
    tasks.update({k: v for k, v in [
        unittest_task(n + '-test', n) for n in r
    ]})

unittest_tasks = {k: v for k, v in tasks.iteritems()
                  if 'unittest_try_name' in v.attributes}
talos_tasks = {k: v for k, v in tasks.iteritems()
               if 'talos_try_name' in v.attributes}
graph_with_jobs = TaskGraph(tasks, Graph(set(tasks), set()))


class TestTryOptionSyntax(unittest.TestCase):

    def test_empty_message(self):
        "Given an empty message, it should return an empty value"
        tos = TryOptionSyntax('', graph_with_jobs)
        self.assertEqual(tos.build_types, [])
        self.assertEqual(tos.jobs, [])
        self.assertEqual(tos.unittests, [])
        self.assertEqual(tos.talos, [])
        self.assertEqual(tos.platforms, [])
        self.assertEqual(tos.trigger_tests, 0)
        self.assertEqual(tos.talos_trigger_tests, 0)
        self.assertEqual(tos.env, [])
        self.assertFalse(tos.profile)
        self.assertIsNone(tos.tag)
        self.assertFalse(tos.no_retry)

    def test_message_without_try(self):
        "Given a non-try message, it should return an empty value"
        tos = TryOptionSyntax('Bug 1234: frobnicte the foo', graph_with_jobs)
        self.assertEqual(tos.build_types, [])
        self.assertEqual(tos.jobs, [])
        self.assertEqual(tos.unittests, [])
        self.assertEqual(tos.talos, [])
        self.assertEqual(tos.platforms, [])
        self.assertEqual(tos.trigger_tests, 0)
        self.assertEqual(tos.talos_trigger_tests, 0)
        self.assertEqual(tos.env, [])
        self.assertFalse(tos.profile)
        self.assertIsNone(tos.tag)
        self.assertFalse(tos.no_retry)

    def test_unknown_args(self):
        "unknown arguments are ignored"
        tos = TryOptionSyntax('try: --doubledash -z extra', graph_with_jobs)
        # equilvant to "try:"..
        self.assertEqual(tos.build_types, [])
        self.assertEqual(tos.jobs, None)

    def test_apostrophe_in_message(self):
        "apostrophe does not break parsing"
        tos = TryOptionSyntax('Increase spammy log\'s log level. try: -b do', graph_with_jobs)
        self.assertEqual(sorted(tos.build_types), ['debug', 'opt'])

    def test_b_do(self):
        "-b do should produce both build_types"
        tos = TryOptionSyntax('try: -b do', graph_with_jobs)
        self.assertEqual(sorted(tos.build_types), ['debug', 'opt'])

    def test_b_d(self):
        "-b d should produce build_types=['debug']"
        tos = TryOptionSyntax('try: -b d', graph_with_jobs)
        self.assertEqual(sorted(tos.build_types), ['debug'])

    def test_b_o(self):
        "-b o should produce build_types=['opt']"
        tos = TryOptionSyntax('try: -b o', graph_with_jobs)
        self.assertEqual(sorted(tos.build_types), ['opt'])

    def test_build_o(self):
        "--build o should produce build_types=['opt']"
        tos = TryOptionSyntax('try: --build o', graph_with_jobs)
        self.assertEqual(sorted(tos.build_types), ['opt'])

    def test_b_dx(self):
        "-b dx should produce build_types=['debug'], silently ignoring the x"
        tos = TryOptionSyntax('try: -b dx', graph_with_jobs)
        self.assertEqual(sorted(tos.build_types), ['debug'])

    def test_j_job(self):
        "-j somejob sets jobs=['somejob']"
        tos = TryOptionSyntax('try: -j somejob', graph_with_jobs)
        self.assertEqual(sorted(tos.jobs), ['somejob'])

    def test_j_jobs(self):
        "-j job1,job2 sets jobs=['job1', 'job2']"
        tos = TryOptionSyntax('try: -j job1,job2', graph_with_jobs)
        self.assertEqual(sorted(tos.jobs), ['job1', 'job2'])

    def test_j_all(self):
        "-j all sets jobs=None"
        tos = TryOptionSyntax('try: -j all', graph_with_jobs)
        self.assertEqual(tos.jobs, None)

    def test_j_twice(self):
        "-j job1 -j job2 sets jobs=job1, job2"
        tos = TryOptionSyntax('try: -j job1 -j job2', graph_with_jobs)
        self.assertEqual(sorted(tos.jobs), sorted(['job1', 'job2']))

    def test_p_all(self):
        "-p all sets platforms=None"
        tos = TryOptionSyntax('try: -p all', graph_with_jobs)
        self.assertEqual(tos.platforms, None)

    def test_p_linux(self):
        "-p linux sets platforms=['linux', 'linux-l10n']"
        tos = TryOptionSyntax('try: -p linux', graph_with_jobs)
        self.assertEqual(tos.platforms, ['linux', 'linux-l10n'])

    def test_p_linux_win32(self):
        "-p linux,win32 sets platforms=['linux', 'linux-l10n', 'win32']"
        tos = TryOptionSyntax('try: -p linux,win32', graph_with_jobs)
        self.assertEqual(sorted(tos.platforms), ['linux', 'linux-l10n', 'win32'])

    def test_p_expands_ridealongs(self):
        "-p linux,linux64 includes the RIDEALONG_BUILDS"
        tos = TryOptionSyntax('try: -p linux,linux64', graph_with_jobs)
        platforms = set(['linux'] + RIDEALONG_BUILDS['linux'])
        platforms |= set(['linux64'] + RIDEALONG_BUILDS['linux64'])
        self.assertEqual(sorted(tos.platforms), sorted(platforms))

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
        "-u gtest[Ubuntu] selects the linux, linux64 and linux64-asan platforms for gtest"
        tos = TryOptionSyntax('try: -u gtest[Ubuntu]', graph_with_jobs)
        self.assertEqual(sorted(tos.unittests), sorted([
            {'test': 'gtest', 'platforms': ['linux32', 'linux64', 'linux64-asan']},
        ]))

    def test_u_platforms_negated(self):
        "-u gtest[-linux] selects all platforms but linux for gtest"
        tos = TryOptionSyntax('try: -u gtest[-linux]', graph_with_jobs)
        all_platforms = set([x.attributes['test_platform'] for x in unittest_tasks.values()])
        self.assertEqual(sorted(tos.unittests[0]['platforms']), sorted(
            [x for x in all_platforms if x != 'linux']
        ))

    def test_u_platforms_negated_pretty(self):
        "-u gtest[Ubuntu,-x64] selects just linux for gtest"
        tos = TryOptionSyntax('try: -u gtest[Ubuntu,-x64]', graph_with_jobs)
        self.assertEqual(sorted(tos.unittests), sorted([
            {'test': 'gtest', 'platforms': ['linux32']},
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
        tos = TryOptionSyntax('try: --rebuild 10', graph_with_jobs)
        self.assertEqual(tos.trigger_tests, 10)

    def test_talos_trigger_tests(self):
        "--rebuild-talos 10 sets talos_trigger_tests"
        tos = TryOptionSyntax('try: --rebuild-talos 10', graph_with_jobs)
        self.assertEqual(tos.talos_trigger_tests, 10)

    def test_interactive(self):
        "--interactive sets interactive"
        tos = TryOptionSyntax('try: --interactive', graph_with_jobs)
        self.assertEqual(tos.interactive, True)

    def test_all_email(self):
        "--all-emails sets notifications"
        tos = TryOptionSyntax('try: --all-emails', graph_with_jobs)
        self.assertEqual(tos.notifications, 'all')

    def test_fail_email(self):
        "--failure-emails sets notifications"
        tos = TryOptionSyntax('try: --failure-emails', graph_with_jobs)
        self.assertEqual(tos.notifications, 'failure')

    def test_no_email(self):
        "no email settings don't set notifications"
        tos = TryOptionSyntax('try:', graph_with_jobs)
        self.assertEqual(tos.notifications, None)

    def test_setenv(self):
        "--setenv VAR=value adds a environment variables setting to env"
        tos = TryOptionSyntax('try: --setenv VAR1=value1 --setenv VAR2=value2', graph_with_jobs)
        self.assertEqual(tos.env, ['VAR1=value1', 'VAR2=value2'])

    def test_profile(self):
        "--geckoProfile sets profile to true"
        tos = TryOptionSyntax('try: --geckoProfile', graph_with_jobs)
        self.assertTrue(tos.profile)

    def test_tag(self):
        "--tag TAG sets tag to TAG value"
        tos = TryOptionSyntax('try: --tag tagName', graph_with_jobs)
        self.assertEqual(tos.tag, 'tagName')

    def test_no_retry(self):
        "--no-retry sets no_retry to true"
        tos = TryOptionSyntax('try: --no-retry', graph_with_jobs)
        self.assertTrue(tos.no_retry)


if __name__ == '__main__':
    main()
