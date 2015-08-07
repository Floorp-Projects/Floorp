# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import os
import itertools
import subprocess
import which

from collections import defaultdict


TRY_SYNTAX_TMPL = """
try: -b %s -p %s -u %s -t none %s %s
"""

class AutoTry(object):

    test_flavors = [
        'browser-chrome',
        'chrome',
        'devtools-chrome',
        'mochitest',
        'xpcshell',
        'reftest',
        'crashtest',
    ]

    def __init__(self, topsrcdir, resolver, mach_context):
        self.topsrcdir = topsrcdir
        self.resolver = resolver
        self.mach_context = mach_context

        if os.path.exists(os.path.join(self.topsrcdir, '.hg')):
            self._use_git = False
        else:
            self._use_git = True

    def resolve_manifests(self, paths=None, tags=None):
        assert tags or paths
        tests = list(self.resolver.resolve_tests(tags=tags,
                                                 paths=paths,
                                                 cwd=self.mach_context.cwd))
        manifests_by_flavor = defaultdict(set)

        for t in tests:
            if t['flavor'] in AutoTry.test_flavors:
                flavor = t['flavor']
                if 'subsuite' in t and t['subsuite'] == 'devtools':
                    flavor = 'devtools-chrome'
                manifest = os.path.relpath(t['manifest'], self.topsrcdir)
                manifests_by_flavor[flavor].add(manifest)

        return dict(manifests_by_flavor)

    def calc_try_syntax(self, platforms, flavors, tests, extra_tests, builds,
                        manifests, tags):

        # Maps from flavors to the try syntax snippets implied by that flavor.
        # TODO: put selected tests under their own builder/label to avoid
        # confusion reading results on treeherder.
        flavor_suites = {
            'mochitest': ['mochitest-1', 'mochitest-e10s-1'],
            'xpcshell': ['xpcshell'],
            'chrome': ['mochitest-o'],
            'browser-chrome': ['mochitest-browser-chrome-1',
                               'mochitest-e10s-browser-chrome-1'],
            'devtools-chrome': ['mochitest-dt',
                                'mochitest-e10s-devtools-chrome'],
            'crashtest': ['crashtest', 'crashtest-e10s'],
            'reftest': ['reftest', 'reftest-e10s'],
        }

        if tags:
            tags = ' '.join('--tag %s' % t for t in tags)
        else:
            tags = ''

        if not tests:
            tests = ','.join(itertools.chain(*(flavor_suites[f] for f in flavors)))
            if extra_tests:
                tests += ',%s' % (extra_tests)

        manifests = ' '.join(manifests)
        if manifests:
            manifests = '--try-test-paths %s' % manifests
        return TRY_SYNTAX_TMPL % (builds, platforms, tests, manifests, tags)

    def _run_git(self, *args):
        args = ['git'] + list(args)
        ret = subprocess.call(args)
        if ret:
            print('ERROR git command %s returned %s' %
                  (args, ret))
            sys.exit(1)

    def _git_push_to_try(self, msg):
        self._run_git('commit', '--allow-empty', '-m', msg)
        self._run_git('push', 'hg::ssh://hg.mozilla.org/try',
                      '+HEAD:refs/heads/branches/default/tip')
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
                      'by running ./mach mercurial-setup')
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
