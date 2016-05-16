#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""Mercurial VCS support.
"""

import os
import re
import subprocess
from collections import namedtuple
from urlparse import urlsplit

import sys
sys.path.insert(1, os.path.dirname(os.path.dirname(os.path.dirname(sys.path[0]))))

import mozharness
from mozharness.base.errors import HgErrorList, VCSException
from mozharness.base.log import LogMixin, OutputParser
from mozharness.base.script import ScriptMixin
from mozharness.base.transfer import TransferMixin

external_tools_path = os.path.join(
    os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__))),
    'external_tools',
)


HG_OPTIONS = ['--config', 'ui.merge=internal:merge']

# MercurialVCS {{{1
# TODO Make the remaining functions more mozharness-friendly.
# TODO Add the various tag functionality that are currently in
# build/tools/scripts to MercurialVCS -- generic tagging logic belongs here.
REVISION, BRANCH = 0, 1


class RepositoryUpdateRevisionParser(OutputParser):
    """Parse `hg pull` output for "repository unrelated" errors."""
    revision = None
    RE_UPDATED = re.compile('^updated to ([a-f0-9]{40})$')

    def parse_single_line(self, line):
        m = self.RE_UPDATED.match(line)
        if m:
            self.revision = m.group(1)

        return super(RepositoryUpdateRevisionParser, self).parse_single_line(line)


def make_hg_url(hg_host, repo_path, protocol='http', revision=None,
                filename=None):
    """Helper function.

    Construct a valid hg url from a base hg url (hg.mozilla.org),
    repo_path, revision and possible filename
    """
    base = '%s://%s' % (protocol, hg_host)
    repo = '/'.join(p.strip('/') for p in [base, repo_path])
    if not filename:
        if not revision:
            return repo
        else:
            return '/'.join([p.strip('/') for p in [repo, 'rev', revision]])
    else:
        assert revision
        return '/'.join([p.strip('/') for p in [repo, 'raw-file', revision, filename]])


class MercurialVCS(ScriptMixin, LogMixin, TransferMixin):
    # For the most part, scripts import mercurial, update,
    # hgtool uses mercurial, share, out
    # tag-release.py imports
    #  apply_and_push, update, get_revision, out, BRANCH, REVISION,
    #  get_branches, cleanOutgoingRevs

    def __init__(self, log_obj=None, config=None, vcs_config=None,
                 script_obj=None):
        super(MercurialVCS, self).__init__()
        self.can_share = None
        self.log_obj = log_obj
        self.script_obj = script_obj
        if config:
            self.config = config
        else:
            self.config = {}
        # vcs_config = {
        #  hg_host: hg_host,
        #  repo: repository,
        #  branch: branch,
        #  revision: revision,
        #  ssh_username: ssh_username,
        #  ssh_key: ssh_key,
        # }
        self.vcs_config = vcs_config or {}
        self.hg = self.query_exe("hg", return_type="list") + HG_OPTIONS

    def _make_absolute(self, repo):
        if repo.startswith("file://"):
            path = repo[len("file://"):]
            repo = "file://%s" % os.path.abspath(path)
        elif "://" not in repo:
            repo = os.path.abspath(repo)
        return repo

    def get_repo_name(self, repo):
        return repo.rstrip('/').split('/')[-1]

    def get_repo_path(self, repo):
        repo = self._make_absolute(repo)
        if repo.startswith("/"):
            return repo.lstrip("/")
        else:
            return urlsplit(repo).path.lstrip("/")

    def get_revision_from_path(self, path):
        """Returns which revision directory `path` currently has checked out."""
        return self.get_output_from_command(
            self.hg + ['parent', '--template', '{node}'], cwd=path
        )

    def get_branch_from_path(self, path):
        branch = self.get_output_from_command(self.hg + ['branch'], cwd=path)
        return str(branch).strip()

    def get_branches_from_path(self, path):
        branches = []
        for line in self.get_output_from_command(self.hg + ['branches', '-c'],
                                                 cwd=path).splitlines():
            branches.append(line.split()[0])
        return branches

    def hg_ver(self):
        """Returns the current version of hg, as a tuple of
        (major, minor, build)"""
        ver_string = self.get_output_from_command(self.hg + ['-q', 'version'])
        match = re.search("\(version ([0-9.]+)\)", ver_string)
        if match:
            bits = match.group(1).split(".")
            if len(bits) < 3:
                bits += (0,)
            ver = tuple(int(b) for b in bits)
        else:
            ver = (0, 0, 0)
        self.debug("Running hg version %s" % str(ver))
        return ver

    def update(self, dest, branch=None, revision=None):
        """Updates working copy `dest` to `branch` or `revision`.
        If revision is set, branch will be ignored.
        If neither is set then the working copy will be updated to the
        latest revision on the current branch.  Local changes will be
        discarded.
        """
        # If we have a revision, switch to that
        msg = "Updating %s" % dest
        if branch:
            msg += " to branch %s" % branch
        if revision:
            msg += " revision %s" % revision
        self.info("%s." % msg)
        if revision is not None:
            cmd = self.hg + ['update', '-C', '-r', revision]
            if self.run_command(cmd, cwd=dest, error_list=HgErrorList):
                raise VCSException("Unable to update %s to %s!" % (dest, revision))
        else:
            # Check & switch branch
            local_branch = self.get_branch_from_path(dest)

            cmd = self.hg + ['update', '-C']

            # If this is different, checkout the other branch
            if branch and branch != local_branch:
                cmd.append(branch)

            if self.run_command(cmd, cwd=dest, error_list=HgErrorList):
                raise VCSException("Unable to update %s!" % dest)
        return self.get_revision_from_path(dest)

    def clone(self, repo, dest, branch=None, revision=None, update_dest=True):
        """Clones hg repo and places it at `dest`, replacing whatever else
        is there.  The working copy will be empty.

        If `revision` is set, only the specified revision and its ancestors
        will be cloned.  If revision is set, branch is ignored.

        If `update_dest` is set, then `dest` will be updated to `revision`
        if set, otherwise to `branch`, otherwise to the head of default.
        """
        msg = "Cloning %s to %s" % (repo, dest)
        if branch:
            msg += " on branch %s" % branch
        if revision:
            msg += " to revision %s" % revision
        self.info("%s." % msg)
        parent_dest = os.path.dirname(dest)
        if parent_dest and not os.path.exists(parent_dest):
            self.mkdir_p(parent_dest)
        if os.path.exists(dest):
            self.info("Removing %s before clone." % dest)
            self.rmtree(dest)

        cmd = self.hg + ['clone']
        if not update_dest:
            cmd.append('-U')

        if revision:
            cmd.extend(['-r', revision])
        elif branch:
            # hg >= 1.6 supports -b branch for cloning
            ver = self.hg_ver()
            if ver >= (1, 6, 0):
                cmd.extend(['-b', branch])

        cmd.extend([repo, dest])
        output_timeout = self.config.get("vcs_output_timeout",
                                         self.vcs_config.get("output_timeout"))
        if self.run_command(cmd, error_list=HgErrorList,
                            output_timeout=output_timeout) != 0:
            raise VCSException("Unable to clone %s to %s!" % (repo, dest))

        if update_dest:
            return self.update(dest, branch, revision)

    def common_args(self, revision=None, branch=None, ssh_username=None,
                    ssh_key=None):
        """Fill in common hg arguments, encapsulating logic checks that
        depend on mercurial versions and provided arguments
        """
        args = []
        if ssh_username or ssh_key:
            opt = ['-e', 'ssh']
            if ssh_username:
                opt[1] += ' -l %s' % ssh_username
            if ssh_key:
                opt[1] += ' -i %s' % ssh_key
            args.extend(opt)
        if revision:
            args.extend(['-r', revision])
        elif branch:
            if self.hg_ver() >= (1, 6, 0):
                args.extend(['-b', branch])
        return args

    def pull(self, repo, dest, update_dest=True, **kwargs):
        """Pulls changes from hg repo and places it in `dest`.

        If `revision` is set, only the specified revision and its ancestors
        will be pulled.

        If `update_dest` is set, then `dest` will be updated to `revision`
        if set, otherwise to `branch`, otherwise to the head of default.
        """
        msg = "Pulling %s to %s" % (repo, dest)
        if update_dest:
            msg += " and updating"
        self.info("%s." % msg)
        if not os.path.exists(dest):
            # Error or clone?
            # If error, should we have a halt_on_error=False above?
            self.error("Can't hg pull in  nonexistent directory %s." % dest)
            return -1
        # Convert repo to an absolute path if it's a local repository
        repo = self._make_absolute(repo)
        cmd = self.hg + ['pull']
        cmd.extend(self.common_args(**kwargs))
        cmd.append(repo)
        output_timeout = self.config.get("vcs_output_timeout",
                                         self.vcs_config.get("output_timeout"))
        if self.run_command(cmd, cwd=dest, error_list=HgErrorList,
                            output_timeout=output_timeout) != 0:
            raise VCSException("Can't pull in %s!" % dest)

        if update_dest:
            branch = self.vcs_config.get('branch')
            revision = self.vcs_config.get('revision')
            return self.update(dest, branch=branch, revision=revision)

    # Defines the places of attributes in the tuples returned by `out'

    def out(self, src, remote, **kwargs):
        """Check for outgoing changesets present in a repo"""
        self.info("Checking for outgoing changesets from %s to %s." % (src, remote))
        cmd = self.hg + ['-q', 'out', '--template', '{node} {branches}\n']
        cmd.extend(self.common_args(**kwargs))
        cmd.append(remote)
        if os.path.exists(src):
            try:
                revs = []
                for line in self.get_output_from_command(cmd, cwd=src, throw_exception=True).rstrip().split("\n"):
                    try:
                        rev, branch = line.split()
                    # Mercurial displays no branch at all if the revision
                    # is on "default"
                    except ValueError:
                        rev = line.rstrip()
                        branch = "default"
                    revs.append((rev, branch))
                return revs
            except subprocess.CalledProcessError, inst:
                # In some situations, some versions of Mercurial return "1"
                # if no changes are found, so we need to ignore this return
                # code
                if inst.returncode == 1:
                    return []
                raise

    def push(self, src, remote, push_new_branches=True, **kwargs):
        # This doesn't appear to work with hg_ver < (1, 6, 0).
        # Error out, or let you try?
        self.info("Pushing new changes from %s to %s." % (src, remote))
        cmd = self.hg + ['push']
        cmd.extend(self.common_args(**kwargs))
        if push_new_branches and self.hg_ver() >= (1, 6, 0):
            cmd.append('--new-branch')
        cmd.append(remote)
        status = self.run_command(cmd, cwd=src, error_list=HgErrorList, success_codes=(0, 1),
                                  return_type="num_errors")
        if status:
            raise VCSException("Can't push %s to %s!" % (src, remote))
        return status

    @property
    def robustcheckout_path(self):
        """Path to the robustcheckout extension."""
        ext = os.path.join(external_tools_path, 'robustcheckout.py')
        if os.path.exists(ext):
            return ext

    def ensure_repo_and_revision(self):
        """Makes sure that `dest` is has `revision` or `branch` checked out
        from `repo`.

        Do what it takes to make that happen, including possibly clobbering
        dest.
        """
        c = self.vcs_config
        dest = c['dest']
        repo_url = c['repo']
        rev = c.get('revision')
        branch = c.get('branch')
        purge = c.get('clone_with_purge', False)
        upstream = c.get('clone_upstream_url')

        # The API here is kind of bad because we're relying on state in
        # self.vcs_config instead of passing arguments. This confuses
        # scripts that have multiple repos. This includes the clone_tools()
        # step :(

        if not rev and not branch:
            self.warning('did not specify revision or branch; assuming "default"')
            branch = 'default'

        share_base = c.get('vcs_share_base', os.environ.get('HG_SHARE_BASE_DIR', None))

        # We require shared storage is configured because it guarantees we
        # only have 1 local copy of logical repo stores.
        if not share_base:
            raise VCSException('vcs share base not defined; '
                               'refusing to operate sub-optimally')

        if not self.robustcheckout_path:
            raise VCSException('could not find the robustcheckout Mercurial extension')

        # Log HG version and install info to aid debugging.
        self.run_command(self.hg + ['--version'])
        self.run_command(self.hg + ['debuginstall'])

        args = self.hg + [
            '--config', 'extensions.robustcheckout=%s' % self.robustcheckout_path,
            'robustcheckout', repo_url, dest, '--sharebase', share_base,
        ]
        if purge:
            args.append('--purge')
        if upstream:
            args.extend(['--upstream', upstream])

        if rev:
            args.extend(['--revision', rev])
        if branch:
            args.extend(['--branch', branch])

        parser = RepositoryUpdateRevisionParser(config=self.config,
                                                log_obj=self.log_obj)
        if self.run_command(args, output_parser=parser):
            raise VCSException('repo checkout failed!')

        if not parser.revision:
            raise VCSException('could not identify revision updated to')

        return parser.revision

    def apply_and_push(self, localrepo, remote, changer, max_attempts=10,
                       ssh_username=None, ssh_key=None):
        """This function calls `changer' to make changes to the repo, and
        tries its hardest to get them to the origin repo. `changer' must be
        a callable object that receives two arguments: the directory of the
        local repository, and the attempt number. This function will push
        ALL changesets missing from remote.
        """
        self.info("Applying and pushing local changes from %s to %s." % (localrepo, remote))
        assert callable(changer)
        branch = self.get_branch_from_path(localrepo)
        changer(localrepo, 1)
        for n in range(1, max_attempts + 1):
            try:
                new_revs = self.out(src=localrepo, remote=remote,
                                    ssh_username=ssh_username,
                                    ssh_key=ssh_key)
                if len(new_revs) < 1:
                    raise VCSException("No revs to push")
                self.push(src=localrepo, remote=remote,
                          ssh_username=ssh_username,
                          ssh_key=ssh_key)
                return
            except VCSException, e:
                self.debug("Hit error when trying to push: %s" % str(e))
                if n == max_attempts:
                    self.debug("Tried %d times, giving up" % max_attempts)
                    for r in reversed(new_revs):
                        self.run_command(self.hg + ['strip', '-n', r[REVISION]],
                                         cwd=localrepo, error_list=HgErrorList)
                    raise VCSException("Failed to push")
                self.pull(remote, localrepo, update_dest=False,
                          ssh_username=ssh_username, ssh_key=ssh_key)
                # After we successfully rebase or strip away heads the push
                # is is attempted again at the start of the loop
                try:
                    self.run_command(self.hg + ['rebase'], cwd=localrepo,
                                     error_list=HgErrorList,
                                     throw_exception=True)
                except subprocess.CalledProcessError, e:
                    self.debug("Failed to rebase: %s" % str(e))
                    # clean up any hanging rebase. ignore errors if we aren't
                    # in the middle of a rebase.
                    self.run_command(self.hg + ['rebase', '--abort'],
                                     cwd=localrepo, success_codes=[0, 255])
                    self.update(localrepo, branch=branch)
                    for r in reversed(new_revs):
                        self.run_command(self.hg + ['strip', '-n', r[REVISION]],
                                         cwd=localrepo, error_list=HgErrorList)
                    changer(localrepo, n + 1)

    def cleanOutgoingRevs(self, reponame, remote, username, sshKey):
        # TODO retry
        self.info("Wiping outgoing local changes from %s to %s." % (reponame, remote))
        outgoingRevs = self.out(src=reponame, remote=remote,
                                ssh_username=username, ssh_key=sshKey)
        for r in reversed(outgoingRevs):
            self.run_command(self.hg + ['strip', '-n', r[REVISION]],
                             cwd=reponame, error_list=HgErrorList)

    def query_pushinfo(self, repository, revision):
        """Query the pushdate and pushid of a repository/revision.
        This is intended to be used on hg.mozilla.org/mozilla-central and
        similar. It may or may not work for other hg repositories.
        """
        PushInfo = namedtuple('PushInfo', ['pushid', 'pushdate'])

        try:
            url = '%s/json-pushes?changeset=%s' % (repository, revision)
            self.info('Pushdate URL is: %s' % url)
            contents = self.retry(self.load_json_from_url, args=(url,))

            # The contents should be something like:
            # {
            #   "28537": {
            #    "changesets": [
            #     "1d0a914ae676cc5ed203cdc05c16d8e0c22af7e5",
            #    ],
            #    "date": 1428072488,
            #    "user": "user@mozilla.com"
            #   }
            # }
            #
            # So we grab the first element ("28537" in this case) and then pull
            # out the 'date' field.
            pushid = contents.iterkeys().next()
            self.info('Pushid is: %s' % pushid)
            pushdate = contents[pushid]['date']
            self.info('Pushdate is: %s' % pushdate)
            return PushInfo(pushid, pushdate)

        except Exception:
            self.exception("Failed to get push info from hg.mozilla.org")
            raise


# __main__ {{{1
if __name__ == '__main__':
    pass
