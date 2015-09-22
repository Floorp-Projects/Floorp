# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import sys
import os
import itertools
import subprocess
import which

from collections import defaultdict

import ConfigParser


def parser():
    parser = argparse.ArgumentParser()
    parser.add_argument('paths', nargs='*', help='Paths to search for tests to run on try.')
    parser.add_argument('-p', dest='platforms', action="append",
                        help='Platforms to run. (required if not found in the environment)')
    parser.add_argument('-u', dest='tests', action="append",
                        help='Test suites to run in their entirety')
    parser.add_argument('-b', dest='builds', default='do',
                        help='Build types to run (d for debug, o for optimized)')
    parser.add_argument('--tag', dest='tags', action='append',
                        help='Restrict tests to the given tag (may be specified multiple times)')
    parser.add_argument('--and', action='store_true', dest="intersection",
                        help='When -u and paths are supplied run only the intersection of the tests specified by the two arguments')
    parser.add_argument('--no-push', dest='push', action='store_false',
                        help='Do not push to try as a result of running this command (if '
                        'specified this command will only print calculated try '
                        'syntax and selection info).')
    parser.add_argument('extra_args', nargs=argparse.REMAINDER,
                        help='Extra arguments to put in the try push')
    parser.add_argument('-v', "--verbose", dest='verbose', action='store_true', default=False,
                        help='Print detailed information about the resulting test selection '
                        'and commands performed.')
    return parser


class AutoTry(object):

    # Maps from flavors to the job names needed to run that flavour
    flavor_jobs = {
        'mochitest': ['mochitest-1', 'mochitest-e10s-1'],
        'xpcshell': ['xpcshell'],
        'chrome': ['mochitest-o'],
        'browser-chrome': ['mochitest-browser-chrome-1',
                           'mochitest-e10s-browser-chrome-1'],
        'devtools-chrome': ['mochitest-dt',
                            'mochitest-e10s-devtools-chrome'],
        'crashtest': ['crashtest', 'crashtest-e10s'],
        'reftest': ['reftest', 'reftest-e10s'],
        'web-platform-tests': ['web-platform-tests-1'],
    }

    flavor_suites = {
        "mochitest": "mochitests",
        "xpcshell": "xpcshell",
        "chrome": "mochitest-o",
        "browser-chrome": "mochitest-bc",
        "devtools-chrome": "mochitest-dt",
        "crashtest": "crashtest",
        "reftest": "reftest",
        "web-platform-tests": "web-platform-tests",
    }

    def __init__(self, topsrcdir, resolver, mach_context):
        self.topsrcdir = topsrcdir
        self.resolver = resolver
        self.mach_context = mach_context

        if os.path.exists(os.path.join(self.topsrcdir, '.hg')):
            self._use_git = False
        else:
            self._use_git = True

    def paths_by_flavor(self, paths=None, tags=None):
        paths_by_flavor = defaultdict(set)

        if not (paths or tags):
            return dict(paths_by_flavor)

        tests = list(self.resolver.resolve_tests(paths=paths,
                                                 tags=tags))

        for t in tests:
            if t['flavor'] in self.flavor_suites:
                flavor = t['flavor']
                if 'subsuite' in t and t['subsuite'] == 'devtools':
                    flavor = 'devtools-chrome'

                for path in paths:
                    if flavor in ["crashtest", "reftest"]:
                        manifest_relpath = os.path.relpath(t['manifest'], self.topsrcdir)
                        if manifest_relpath.startswith(path):
                            paths_by_flavor[flavor].add(manifest_relpath)
                    else:
                        if t['file_relpath'].startswith(path):
                            paths_by_flavor[flavor].add(path)

        return dict(paths_by_flavor)

    def remove_duplicates(self, paths_by_flavor, tests):
        rv = {}
        for item in paths_by_flavor:
            if self.flavor_suites[item] not in tests:
                rv[item] = paths_by_flavor[item].copy()
        return rv

    def calc_try_syntax(self, platforms, tests, builds, paths_by_flavor, tags, extra_args,
                        intersection):
        parts = ["try:", "-b", builds, "-p", ",".join(platforms)]

        suites = set(tests) if not intersection else set()
        paths = set()
        for flavor, flavor_tests in paths_by_flavor.iteritems():
            suite = self.flavor_suites[flavor]
            if suite not in suites and (not intersection or suite in tests):
                for job_name in self.flavor_jobs[flavor]:
                    for test in flavor_tests:
                        paths.add("%s:%s" % (flavor, test))
                    suites.add(job_name)

        if not suites:
            raise ValueError("No tests found matching filters")

        parts.append("-u")
        parts.append(",".join(sorted(suites)))

        if tags:
            parts.append(' '.join('--tag %s' % t for t in tags))

        if extra_args is not None:
            parts.extend(extra_args)

        if paths:
            parts.append("--try-test-paths %s" % " ".join(sorted(paths)))

        return " ".join(parts)

    def _run_git(self, *args):
        args = ['git'] + list(args)
        ret = subprocess.call(args)
        if ret:
            print('ERROR git command %s returned %s' %
                  (args, ret))
            sys.exit(1)

    def _git_push_to_try(self, msg):
        self._run_git('commit', '--allow-empty', '-m', msg)
        try:
            self._run_git('push', 'hg::ssh://hg.mozilla.org/try',
                          '+HEAD:refs/heads/branches/default/tip')
        finally:
            self._run_git('reset', 'HEAD~')

    def push_to_try(self, msg, verbose):
        if not self._use_git:
            try:
                hg_args = ['hg', 'push-to-try', '-m', msg]
                subprocess.check_call(hg_args, stderr=subprocess.STDOUT)
            except subprocess.CalledProcessError as e:
                print('ERROR hg command %s returned %s' % (hg_args, e.returncode))
                print('The "push-to-try" hg extension is required to push from '
                      'hg to try with the autotry command.\n\nIt can be installed '
                      'to Mercurial 3.3 or above by running ./mach mercurial-setup')
                sys.exit(1)
        else:
            try:
                which.which('git-cinnabar')
                self._git_push_to_try(msg)
            except which.WhichError:
                print('ERROR git-cinnabar is required to push from git to try with'
                      'the autotry command.\n\nMore information can by found at '
                      'https://github.com/glandium/git-cinnabar')
                sys.exit(1)

    def find_uncommited_changes(self):
        if self._use_git:
            stat = subprocess.check_output(['git', 'status', '-z'])
            return any(len(entry.strip()) and entry.strip()[0] in ('A', 'M', 'D')
                       for entry in stat.split('\0'))
        else:
            stat = subprocess.check_output(['hg', 'status'])
            return any(len(entry.strip()) and entry.strip()[0] in ('A', 'M', 'R')
                       for entry in stat.splitlines())
