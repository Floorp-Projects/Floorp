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
            if e.output:
                print(' '.join(cmd))
                print(e.output)
            return []
        return [os.path.join(self.root, f) for f in files if f]

    def by_workdir(self, mode):
        return []

    def by_outgoing(self, dest='default'):
        return []


class HgHelper(VCSHelper):
    """A helper to find files to lint from Mercurial."""

    def by_outgoing(self, dest='default'):
        return self.run(['hg', 'outgoing', '--quiet', '--template',
                         "{file_mods % '\\n{file}'}{file_adds % '\\n{file}'}", '-r', '.', dest])

    def by_workdir(self, mode):
        return self.run(['hg', 'status', '-amn'])


class GitHelper(VCSHelper):
    """A helper to find files to lint from Git."""
    _default = None

    @property
    def default(self):
        if self._default:
            return self._default

        ref = subprocess.check_output(['git', 'symbolic-ref', '-q', 'HEAD']).strip()
        dest = subprocess.check_output(
            ['git', 'for-each-ref', '--format=%(upstream:short)', ref]).strip()

        if not dest:
            branches = subprocess.check_output(['git', 'branch', '--list'])
            for b in ('master', 'central', 'default'):
                if b in branches and not ref.endswith(b):
                    dest = b
                    break

        self._default = dest
        return self._default

    def by_outgoing(self, dest='default'):
        if dest == 'default':
            if not self.default:
                print("warning: could not find default push, specify a remote for --outgoing")
                return []
            dest = self.default
        comparing = '{}..HEAD'.format(self.default)
        return self.run(['git', 'log', '--name-only', '--diff-filter=AM',
                         '--oneline', '--pretty=format:', comparing])

    def by_workdir(self, mode):
        cmd = ['git', 'diff', '--name-only', '--diff-filter=AM']
        if mode == 'staged':
            cmd.append('--cached')
        else:
            cmd.append('HEAD')
        return self.run(cmd)


vcs_class = {
    'git': GitHelper,
    'hg': HgHelper,
    'none': VCSHelper,
}
