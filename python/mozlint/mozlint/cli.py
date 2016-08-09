# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import os
import subprocess
import sys
from argparse import ArgumentParser


SEARCH_PATHS = []


class MozlintParser(ArgumentParser):
    arguments = [
        [['paths'],
         {'nargs': '*',
          'default': None,
          'help': "Paths to file or directories to lint, like "
                  "'browser/components/loop' or 'mobile/android'. "
                  "Defaults to the current directory if not given.",
          }],
        [['-l', '--linter'],
         {'dest': 'linters',
          'default': [],
          'action': 'append',
          'help': "Linters to run, e.g 'eslint'. By default all linters "
                  "are run for all the appropriate files.",
          }],
        [['-f', '--format'],
         {'dest': 'fmt',
          'default': 'stylish',
          'help': "Formatter to use. Defaults to 'stylish'.",
          }],
        [['-n', '--no-filter'],
         {'dest': 'use_filters',
          'default': True,
          'action': 'store_false',
          'help': "Ignore all filtering. This is useful for quickly "
                  "testing a directory that otherwise wouldn't be run, "
                  "without needing to modify the config file.",
          }],
        [['-r', '--rev'],
         {'default': None,
          'help': "Lint files touched by the given revision(s). Works with "
                  "mercurial or git."
          }],
        [['-w', '--workdir'],
         {'default': False,
          'action': 'store_true',
          'help': "Lint files touched by changes in the working directory "
                  "(i.e haven't been committed yet). Works with mercurial or git.",
          }],
    ]

    def __init__(self, **kwargs):
        ArgumentParser.__init__(self, usage=self.__doc__, **kwargs)

        for cli, args in self.arguments:
            self.add_argument(*cli, **args)


class VCSFiles(object):
    def __init__(self):
        self._root = None
        self._vcs = None

    @property
    def root(self):
        if self._root:
            return self._root

        # First check if we're in an hg repo, if not try git
        commands = (
            ['hg', 'root'],
            ['git', 'rev-parse', '--show-toplevel'],
        )

        for cmd in commands:
            proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)
            output = proc.communicate()[0].strip()

            if proc.returncode == 0:
                self._vcs = cmd[0]
                self._root = output
                return self._root

    @property
    def vcs(self):
        return self._vcs or (self.root and self._vcs)

    @property
    def is_hg(self):
        return self.vcs == 'hg'

    @property
    def is_git(self):
        return self.vcs == 'git'

    def _run(self, cmd):
        files = subprocess.check_output(cmd).split()
        return [os.path.join(self.root, f) for f in files]

    def by_rev(self, rev):
        if self.is_hg:
            return self._run(['hg', 'log', '-T', '{files % "\\n{file}"}', '-r', rev])
        elif self.is_git:
            return self._run(['git', 'diff', '--name-only', rev])
        return []

    def by_workdir(self):
        if self.is_hg:
            return self._run(['hg', 'status', '-amn'])
        elif self.is_git:
            return self._run(['git', 'diff', '--name-only'])
        return []


def find_linters(linters=None):
    lints = []
    for search_path in SEARCH_PATHS:
        if not os.path.isdir(search_path):
            continue

        files = os.listdir(search_path)
        for f in files:
            name, ext = os.path.splitext(f)
            if ext != '.lint':
                continue

            if linters and name not in linters:
                continue

            lints.append(os.path.join(search_path, f))
    return lints


def run(paths, linters, fmt, rev, workdir, **lintargs):
    from mozlint import LintRoller, formatters

    # Calculate files from VCS
    vcs = VCSFiles()
    if rev:
        paths.extend(vcs.by_rev(rev))
    if workdir:
        paths.extend(vcs.by_workdir())
    paths = paths or ['.']

    lint = LintRoller(**lintargs)
    lint.read(find_linters(linters))

    # run all linters
    results = lint.roll(paths)

    formatter = formatters.get(fmt)
    print(formatter(results))
    return 1 if results else 0


if __name__ == '__main__':
    parser = MozlintParser()
    args = vars(parser.parse_args())
    sys.exit(run(**args))
