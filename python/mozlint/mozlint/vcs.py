# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import subprocess


class VCSHelper(object):
    """A base VCS helper that always returns an empty list
    for the case when no version control was found.
    """
    def __init__(self, root):
        self.root = root

    @classmethod
    def find_vcs(cls):
        # First check if we're in an hg repo, if not try git
        commands = (
            ['hg', 'root'],
            ['git', 'rev-parse', '--show-toplevel'],
        )

        for cmd in commands:
            proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            output = proc.communicate()[0].strip()

            if proc.returncode == 0:
                return cmd[0], output
        return 'none', ''

    @classmethod
    def create(cls):
        vcs, root = cls.find_vcs()
        return vcs_class[vcs](root)

    def run(self, cmd):
        try:
            files = subprocess.check_output(cmd, stderr=subprocess.STDOUT).split()
        except subprocess.CalledProcessError as e:
            print(' '.join(cmd))
            print(e.output)
            return []
        return [os.path.join(self.root, f) for f in files if f]

    def by_workdir(self, workdir):
        return []

    def by_outgoing(self, dest='default'):
        return []


class HgHelper(VCSHelper):
    """A helper to find files to lint from Mercurial."""

    def by_outgoing(self, dest='default'):
        return self.run(['hg', 'outgoing', '--quiet', '-r .',
                         dest, '--template', '{files % "\n{file}"}'])

    def by_workdir(self):
        return self.run(['hg', 'status', '-amn'])


class GitHelper(VCSHelper):
    """A helper to find files to lint from Git."""

    def by_outgoing(self, dest='default'):
        if dest == 'default':
            comparing = 'origin/master..HEAD'
        else:
            comparing = '{}..HEAD'.format(dest)
        return self.run(['git', 'log', '--name-only', comparing])

    def by_workdir(self):
        return self.run(['git', 'diff', '--name-only'])


vcs_class = {
    'git': GitHelper,
    'hg': HgHelper,
    'none': VCSHelper,
}
