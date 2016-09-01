# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import subprocess


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
            return self._run(['hg', 'log', '--template', '{files % "\\n{file}"}', '-r', rev])
        elif self.is_git:
            return self._run(['git', 'diff', '--name-only', rev])
        return []

    def by_workdir(self):
        if self.is_hg:
            return self._run(['hg', 'status', '-amn'])
        elif self.is_git:
            return self._run(['git', 'diff', '--name-only'])
        return []
