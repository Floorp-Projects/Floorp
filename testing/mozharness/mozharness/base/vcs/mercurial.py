#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""Mercurial VCS support.

Largely copied/ported from
https://hg.mozilla.org/build/tools/file/cf265ea8fb5e/lib/python/util/hg.py .
"""

import os
import re
import subprocess
from urlparse import urlsplit

import sys
sys.path.insert(1, os.path.dirname(os.path.dirname(os.path.dirname(sys.path[0]))))

from mozharness.base.errors import HgErrorList, VCSException
from mozharness.base.log import LogMixin
from mozharness.base.script import ScriptMixin

HG_OPTIONS = ['--config', 'ui.merge=internal:merge']

# MercurialVCS {{{1
# TODO Make the remaining functions more mozharness-friendly.
# TODO Add the various tag functionality that are currently in
# build/tools/scripts to MercurialVCS -- generic tagging logic belongs here.
REVISION, BRANCH = 0, 1


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


class MercurialVCS(ScriptMixin, LogMixin, object):
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

    # hg share methods {{{2
    def query_can_share(self):
        if self.can_share is not None:
            return self.can_share
        # Check that 'hg share' works
        self.can_share = True
        try:
            self.info("Checking if share extension works.")
            output = self.get_output_from_command(self.hg + ['help', 'share'],
                                                  silent=True,
                                                  throw_exception=True)
            if 'no commands defined' in output:
                # Share extension is enabled, but not functional
                self.warning("Disabling sharing since share extension doesn't seem to work (1)")
                self.can_share = False
            elif 'unknown command' in output or 'hg help extensions' in output:
                # Share extension is disabled
                self.warning("Disabling sharing since share extension doesn't seem to work (2)")
                self.can_share = False
        except subprocess.CalledProcessError:
            # The command failed, so disable sharing
            self.warning("Disabling sharing since share extension doesn't seem to work (3)")
            self.can_share = False
        if self.can_share:
            self.info("hg share works.")
        return self.can_share

    def _ensure_shared_repo_and_revision(self, share_base):
        """The shared dir logic is complex enough to warrant its own
        helper method.

        If allow_unshared_local_clones is True and we're trying to use the
        share extension but fail, then we will be able to clone from the
        shared repo to our destination.  If this is False, the default, the
        if we don't have the share extension we will just clone from the
        remote repository.
        """
        c = self.vcs_config
        dest = os.path.abspath(c['dest'])
        repo = c['repo']
        revision = c.get('revision')
        branch = c.get('branch')
        if not self.query_can_share():
            raise VCSException("%s called when sharing is not allowed!" % __name__)

        # If the working directory already exists and isn't using share
        # when we want to use share, clobber.
        #
        # The original util.hg.mercurial() tried to pull repo into dest
        # instead. That can help if the share extension fails.
        # But it can also result in pulling a different repo B into an
        # existing clone of repo A, which may have unexpected results.
        if os.path.exists(dest):
            sppath = os.path.join(dest, ".hg", "sharedpath")
            if not os.path.exists(sppath):
                self.info("No file %s; removing %s." % (sppath, dest))
                self.rmtree(dest)
        if not os.path.exists(os.path.dirname(dest)):
            self.mkdir_p(os.path.dirname(dest))
        shared_repo = os.path.join(share_base, self.get_repo_path(repo))
        dest_shared_path = os.path.join(dest, '.hg', 'sharedpath')
        if os.path.exists(dest_shared_path):
            # Make sure that the sharedpath points to shared_repo
            dest_shared_path_data = os.path.realpath(open(dest_shared_path).read())
            norm_shared_repo = os.path.realpath(os.path.join(shared_repo, '.hg'))
            if dest_shared_path_data != norm_shared_repo:
                # Clobber!
                self.info("We're currently shared from %s, but are being requested to pull from %s (%s); clobbering" % (dest_shared_path_data, repo, norm_shared_repo))
                self.rmtree(dest)

        self.info("Updating shared repo")
        if os.path.exists(shared_repo):
            try:
                self.pull(repo, shared_repo)
            except VCSException:
                self.warning("Error pulling changes into %s from %s; clobbering" % (shared_repo, repo))
                self.exception(level='debug')
                self.clone(repo, shared_repo)
        else:
            self.clone(repo, shared_repo)

        if os.path.exists(dest):
            try:
                self.pull(shared_repo, dest)
                status = self.update(dest, branch=branch, revision=revision)
                return status
            except VCSException:
                self.rmtree(dest)
        try:
            self.info("Trying to share %s to %s" % (shared_repo, dest))
            return self.share(shared_repo, dest, branch=branch, revision=revision)
        except VCSException:
            if not c.get('allow_unshared_local_clones'):
                # Re-raise the exception so it gets caught below.
                # We'll then clobber dest, and clone from original
                # repo
                raise

        self.warning("Error calling hg share from %s to %s; falling back to normal clone from shared repo" % (shared_repo, dest))
        # Do a full local clone first, and then update to the
        # revision we want
        # This lets us use hardlinks for the local clone if the
        # OS supports it
        try:
            self.clone(shared_repo, dest, update_dest=False)
            return self.update(dest, branch=branch, revision=revision)
        except VCSException:
            # Need better fallback
            self.error("Error updating %s from shared_repo (%s): " % (dest, shared_repo))
            self.exception(level='error')
            self.rmtree(dest)

    def share(self, source, dest, branch=None, revision=None):
        """Creates a new working directory in "dest" that shares history
        with "source" using Mercurial's share extension
        """
        self.info("Sharing %s to %s." % (source, dest))
        self.mkdir_p(dest)
        if self.run_command(self.hg + ['share', '-U', source, dest],
                            error_list=HgErrorList):
            raise VCSException("Unable to share %s to %s!" % (source, dest))
        return self.update(dest, branch=branch, revision=revision)

    # End hg share methods 2}}}

    def ensure_repo_and_revision(self):
        """Makes sure that `dest` is has `revision` or `branch` checked out
        from `repo`.

        Do what it takes to make that happen, including possibly clobbering
        dest.
        """
        c = self.vcs_config
        for conf_item in ('dest', 'repo'):
            assert self.vcs_config[conf_item]
        dest = os.path.abspath(c['dest'])
        repo = c['repo']
        revision = c.get('revision')
        branch = c.get('branch')
        share_base = c.get('vcs_share_base',
                           os.environ.get("HG_SHARE_BASE_DIR", None))
        msg = "Setting %s to %s" % (dest, repo)
        if branch:
            msg += " on branch %s" % branch
        if revision:
            msg += " revision %s" % revision
        if share_base:
            msg += " using shared directory %s" % share_base
        self.info("%s." % msg)
        if share_base and not self.query_can_share():
            share_base = None

        if share_base:
            return self._ensure_shared_repo_and_revision(share_base)

        # Non-shared
        if os.path.exists(dest):
            try:
                self.pull(repo, dest)
                return self.update(dest, branch=branch, revision=revision)
            except VCSException:
                self.warning("Error pulling changes into %s from %s; clobbering" % (dest, repo))
                self.exception(level='debug')
                self.rmtree(dest)
        elif not os.path.exists(os.path.dirname(dest)):
            self.mkdir_p(os.path.dirname(dest))
        self.clone(repo, dest)
        return self.update(dest, branch=branch, revision=revision)

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


# __main__ {{{1
if __name__ == '__main__':
    pass
