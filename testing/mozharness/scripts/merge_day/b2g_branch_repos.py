#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
""" b2g_branch_repos.py

Branch the b2g git repos for merge day, and lock down the 3rd party repos
in b2g-manifest to a revision.
"""

import glob
import os
import pprint
import sys
import time

sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))

from mozharness.base.errors import GitErrorList
from mozharness.base.log import ERROR
from mozharness.base.transfer import TransferMixin
from mozharness.base.vcs.vcsbase import VCSScript
from mozharness.mozilla import repo_manifest


# B2GBranch {{{1
class B2GBranch(TransferMixin, VCSScript):
    config_options = [
        [['--branch-name', ], {
            "action": "store",
            "dest": "branch_name",
            "type": "string",
            "help": "Specify the branch name (e.g. v2.0).  REQUIRED",
        }],
        [['--branch-order', ], {
            "action": "extend",
            "dest": "branch_order",
            "type": "string",
            "help": """Comma separated list of branches in case manifests contain conflicting references,
e.g. --branch-order v2.0,master"""
        }],
        [['--manifest-repo-revision', ], {
            "action": "store",
            "dest": "manifest_repo_revision",
            "type": "string",
            "default": "master",
            "help": "Specify the b2g-manifest repo revision"
        }],
        [['--lock-manifest-revisions', ], {
            "action": "store_true",
            "dest": "lock_manifest_revisions",
            "default": True,
            "help": "Lock 3rd party revisions in munge-manifests"
        }],
        [['--no-lock-manifest-revisions', ], {
            "action": "store_false",
            "dest": "lock_manifest_revisions",
            "help": "Don't lock 3rd party revisions in munge-manifests"
        }],
        [['--branch-manifests', ], {
            "action": "store_true",
            "dest": "branch_manifests",
            "default": True,
            "help": "branch b2g-manifest and xml files in munge-manifests"
        }],
        [['--no-branch-manifests', ], {
            "action": "store_false",
            "dest": "branch_manifests",
            "help": "Don't branch b2g-manifest and xml files in munge-manifests"
        }],
        [['--delete-unused-manifests', ], {
            "action": "store_true",
            "dest": "delete_unused_manifests",
            "default": True,
            "help": "Delete unused manifest files in munge-manifests"
        }],
        [['--no-delete-unused-manifests', ], {
            "action": "store_false",
            "dest": "delete_unused_manifests",
            "help": "Don't delete unused manifest files in munge-manifests"
        }],
    ]
    branch_repo_dict = None
    remote_branch_revisions = {}

    def __init__(self, require_config_file=True):
        super(B2GBranch, self).__init__(
            config_options=self.config_options,
            all_actions=[
                'clobber',
                'pull',
                'pull-branch-repos',
                'clean-repos',
                'branch-repos',
                'switch-manifest-branch',
                'munge-manifests',
                'push',
            ],
            default_actions=[
                'pull',
                'pull-branch-repos',
                'clean-repos',
                'branch-repos',
                'switch-manifest-branch',
                'munge-manifests',
            ],
            require_config_file=require_config_file
        )
        self.run_sanity_check()

# Helper methods {{{1
    def run_sanity_check(self):
        message = ""
        if not self.config.get("branch_name"):
            message += "You need to specify --branch-name (e.g. v2.0)!\n"
        if message:
            self.fatal(message)

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        super(B2GBranch, self).query_abs_dirs()
        self.abs_dirs['abs_manifest_dir'] = os.path.join(self.abs_dirs['abs_work_dir'], 'b2g-manifest')
        return self.abs_dirs

    def query_manifests(self):
        dirs = self.query_abs_dirs()
        manifests = []
        for manifest in glob.glob(os.path.join(dirs['abs_manifest_dir'], "*.xml")):
            if os.path.islink(manifest):
                self.info("Skipping %s (softlink)" % manifest)
                continue
            if os.path.basename(manifest) in self.config["ignored_manifests"]:
                self.info("Skipping %s (ignored)" % manifest)
                continue
            manifests.append(manifest)
        return manifests

    def _add_branch_revision(self, repo_config, name, revision, manifest):
        """ query_branch_repos() helper.

            Pass in branch-repos['name'] for 'repo', name, revision, manifest.
            """
        if repo_manifest.is_commitid(revision):
            self.debug("%s is a commit id! Skipping for %s %s" % (revision, name, manifest))
            return
        new_branch = self.config['branch_name']
        branch_order = [new_branch] + list(self.config.get('branch_order', []))
        repo_config.setdefault('all_revisions', {}).setdefault(revision, []).append(manifest)
        if "revision" not in repo_config:
            repo_config['revision'] = revision
        else:
            previous_revision = repo_config["revision"]
            if previous_revision != revision:
                if revision in branch_order:
                    if previous_revision not in branch_order or \
                       branch_order.index(revision) < branch_order.index(previous_revision):
                        self.info("Prefering %s over %s for %s" % (revision,
                                  previous_revision, name))
                        del(repo_config["branch_revisions"][previous_revision])
                        repo_config["revision"] = revision
                    else:
                        self.info("%s: not branching off %s since %s is in branch_order" % (name, revision, previous_revision))
                        return
                elif previous_revision in branch_order:
                    self.info("%s: not keeping %s since %s is in branch_order" % (name, revision, previous_revision))
                    self.debug(pprint.pformat(repo_config['branch_revisions']))
                    return
                else:
                    # We shouldn't ever hit this line
                    self.critical("WAT %s %s %s" % (name, revision, previous_revision))
        repo_config.setdefault('branch_revisions', {}).setdefault(revision, []).append(manifest)
        self.debug("%s %s %s" % (name, repo_config['revision'], pprint.pformat(repo_config['branch_revisions'])))

    def _query_remote_branch_revision(self, fetch, branch, manifest, retry=True):
        """ Helper method for munge_manifests().

            Since we want to lock revisions, we need to know what revision goes
            with a specific git url + branch.
            """
        sleep_time = 10
        if self.remote_branch_revisions.get(fetch, {}).get(branch):
            return self.remote_branch_revisions[fetch][branch]
        git = self.query_exe('git', return_type='list')
        output = self.get_output_from_command(
            git + ["ls-remote", fetch, branch]
        )
        if not output:
            if retry:
                self.info("Trying again in %d seconds..." % sleep_time)
                time.sleep(sleep_time)
                return self._query_remote_branch_revision(manifest, fetch, branch, retry=False)
            # TODO figure out what to do here
            # self.fatal("Can't find revision for %s %s!" % (fetch, branch))
            self.critical("%s: Can't find revision for %s %s!" % (fetch, branch, manifest))
            return None
        r = output[0:40]
        if not repo_manifest.is_commitid(r):
            self.fatal("Can't grok ls-remote output:\n %s" % output)
        self.info("Found revision %s for %s %s" % (r, fetch, branch))
        self.remote_branch_revisions.setdefault(fetch, {})[branch] = r
        return r

    def _query_repo_name(self, name_attr):
        """ query_branch_repos() helper, so gaia and gaia.git are equivalent
            """
        if name_attr.endswith(".git"):
            name_attr = name_attr[:-4]
        return name_attr

    def _query_do_branch(self, fetch, name):
        """ query_branch_repos() helper, to see if we want to [potentially]
            branch a repo based on the 'fetch' url
            """
        do_branch = False
        for rstring in self.config.get("branch_remote_substrings", []):
            if rstring in fetch:
                do_branch = True
                break
        if do_branch:
            if name in self.config.get('no_branch_repos', []):
                self.info("Skipping %s (in no_branch_repos)" % name)
                return False
            return True
        self.debug("Not going to branch %s (not in branch_remote_substrings)" % name)
        return False

    def query_branch_repos(self):
        """ Parse all manifests and build a dictionary of repos with
            expected revisions and/or branches.

            The format will be {
                name: {
                    'revision': branch,
                    'fetch': git_url,
                    'branch_revisions': {
                        revision: [manifest list]  # This should only have one key/value pair
                    },
                    'all_revisions': {
                        revision: [manifest list]  # This will have all key/value pairs
                    },
                },
            }

            This depends on the pull action having run at least once.
            """
        if self.branch_repo_dict is not None:
            return self.branch_repo_dict
        self.info("Building branch_repo_dict...")
        branch_repo_dict = {
            'b2g-manifest': {
                'fetch': self.config['manifest_repo_url'],
                'revision': self.config['manifest_repo_revision'],
            },
        }
        for manifest in self.query_manifests():
            self.info("Processing %s" % manifest)
            doc = repo_manifest.load_manifest(manifest)
            try:
                default_revision = repo_manifest.get_default(doc).getAttribute("revision")
            except IndexError:
                self.info("Skipping %s (no defaults)" % manifest)
                continue

            for p in doc.getElementsByTagName('project'):
                name = self._query_repo_name(p.getAttribute('name'))
                fetch = repo_manifest.get_project_remote_url(doc, p)
                self.debug("Remote %s Name %s" % (fetch, name))
                # We branch github.com/mozilla repos only
                if not self._query_do_branch(fetch, name):
                    continue
                # Now start building the branch info
                branch_repo_dict.setdefault(name, {}).setdefault("fetch", fetch)
                revision = p.getAttribute("revision")
                if not revision:
                    # fall back to default revision
                    if default_revision:
                        self.info("%s: %s, using default revision (%s)" % (manifest, name, default_revision))
                        self._add_branch_revision(branch_repo_dict[name], name, default_revision, manifest)
                    else:
                        self.warning("Can't determine revision for %s in %s" % (name, manifest))
                elif revision and branch_repo_dict.get(name, {}).get("revision", revision) != revision:
                    self._add_branch_revision(branch_repo_dict[name], name, revision, manifest)
                else:
                    self._add_branch_revision(branch_repo_dict[name], name, revision, manifest)
        self.info("Outputting branch_repo_dict:")
        self.info(pprint.pformat(branch_repo_dict))
        message = ""
        for name, r in branch_repo_dict.iteritems():
            if r.get("revision") is None:
                self.warning("Sanity: No revision set for %s %s; we'll fall back to master" % (name, r["fetch"]))
            if len(r.get("branch_revisions", {})) > 1:
                message += "Sanity: Not clear where to branch for %s %s %s\n" % (name, r["fetch"], pprint.pformat(r["branch_revisions"]))
        if message:
            self.fatal(message + "Use --branch-order or self.config['no_branch_repos'] to fix!")
        self.branch_repo_dict = branch_repo_dict
        return branch_repo_dict

    def _get_sha_from_show_ref(self, output):
        """ Helper method for check_existing_branch().

            'output' is the output from |git show-ref|.
            """
        if not output or 'refs' not in output:
            return None
        return output[0:41]

    def check_existing_branch(self, new_branch, cwd, error_level=ERROR):
        """ Returns a tuple (local, remote) with the corresponding sha
            of each branch if they exist, or None if not.
            """
        self.info("Checking for existing branch %s in %s..." % (new_branch, cwd))
        git = self.query_exe('git', return_type='list')
        local_branch = self._get_sha_from_show_ref(
            self.get_output_from_command(
                git + ["show-ref", new_branch],
                cwd=cwd,
                success_codes=[0, 1],
            )
        )
        remote_branch = self._get_sha_from_show_ref(
            self.get_output_from_command(
                git + ["show-ref", "remotes/origin/%s" % new_branch],
                cwd=cwd,
                success_codes=[0, 1],
            )
        )
        if local_branch and remote_branch and local_branch != remote_branch:
            self.log(
                "%s out of sync! Local branch %s on %s; remote on %s!" % (
                    cwd, new_branch, local_branch, remote_branch
                ),
                level=error_level,
            )
        return (local_branch, remote_branch)

    def _delete_unused_manifests(self, unused_manifests):
        """ Helper method for munge_manifests().  Delete unused manifests
            if self.config["delete_unused_manifests"]; otherwise skip.
            """
        self.info("Deleting unused manifests...")
        git = self.query_exe("git", return_type="list")
        for manifest in unused_manifests:
            cwd = os.path.dirname(manifest)
            self.run_command(
                git + ['rm', '-f', os.path.basename(manifest)],
                cwd=cwd,
                error_list=GitErrorList,
                halt_on_failure=True,
            )

# Actions {{{1
    def pull(self):
        """ Pull action.
            """
        dirs = self.query_abs_dirs()
        super(B2GBranch, self).pull(
            repos=[{
                "vcs": "hg",
                "repo": self.config["tools_repo_url"],
                "revision": self.config["tools_repo_revision"],
                "dest": "tools",
            }, {
                "vcs": "gittool",
                "repo": self.config["manifest_repo_url"],
                "revision": self.config["manifest_repo_revision"],
                "dest": dirs['abs_manifest_dir'],
            }],
        )

    def pull_branch_repos(self):
        repos = []
        for name, conf in self.query_branch_repos().iteritems():
            self.info("%s: %s" % (name, pprint.pformat(conf)))
            repos.append({
                "vcs": "gittool",
                "repo": conf['fetch'],
                "revision": conf.get('revision', 'master'),
                "dest": name,
            })
        super(B2GBranch, self).pull(repos=repos)

    def clean_repos(self):
        """ Clean the existing branches.
            This assumes the branches were created locally and not pushed remote yet.
            """
        repos = self.query_branch_repos()
        dirs = self.query_abs_dirs()
        git = self.query_exe("git", return_type="list")
        new_branch = self.config['branch_name']
        # b2g-manifest is special
        if os.path.exists(dirs['abs_manifest_dir']):
            for cmd in (
                git + ["reset", "--hard"],
                git + ["clean", "-fdx"],
                git + ["checkout", self.config["manifest_repo_revision"]],
            ):
                self.run_command(
                    cmd,
                    cwd=dirs["abs_manifest_dir"],
                    error_list=GitErrorList,
                    halt_on_failure=True,
                )
        for name, r in repos.iteritems():
            cwd = os.path.join(dirs['abs_work_dir'], name)
            local_sha, remote_sha = self.check_existing_branch(new_branch, cwd)
            if local_sha and not remote_sha:
                # We have a local branch that doesn't exist on origin
                self.info("Cleaning up existing %s branch from %s." % (new_branch, name))
                self.run_command(
                    git + ['branch', '-D', new_branch],
                    cwd=os.path.join(dirs['abs_work_dir'], name),
                    halt_on_failure=True,
                    error_list=GitErrorList,
                )

    def branch_repos(self):
        """ Branch the mozilla-b2g repos and update the manifests.
            """
        repos = self.query_branch_repos()
        dirs = self.query_abs_dirs()
        git = self.query_exe("git", return_type="list")
        new_branch = self.config['branch_name']
        for name, r in repos.iteritems():
            revision = r["revision"]
            cwd = os.path.join(dirs['abs_work_dir'], name)
            local_sha, remote_sha = self.check_existing_branch(new_branch, cwd)
            if local_sha:
                self.warning("Local branch %s already exists in %s. Skipping." % (new_branch, name))
                continue
            if remote_sha:
                self.error("Remote branch %s already exists in %s. Do you need to re- --pull-branch-repos? Skipping." % (new_branch, name))
                continue
            self.info("Creating %s branch in %s based on %s" % (new_branch, name,
                      revision))
            self.run_command(git + ['branch', new_branch, "origin/%s" % revision],
                             cwd=cwd, error_list=GitErrorList, halt_on_failure=True)

    def switch_manifest_branch(self):
        """ Switch b2g-manifest to the new branch.
            This is in a separate action because we may not run --branch-repos
            (e.g. if we're locking down revisions in a previously created branch)
            and switching branches after making changes can be detrimental to
            our previous changes.

            This action will die if there are uncommitted changes, currently
            by design.
            """
        dirs = self.query_abs_dirs()
        git = self.query_exe("git", return_type="list")
        new_branch = self.config['branch_name']
        self.run_command(
            git + ["checkout", new_branch],
            cwd=dirs['abs_manifest_dir'],
            error_list=GitErrorList,
            halt_on_failure=True
        )

    def munge_manifests(self):
        """ Switch the branched repos to the new branch; lock down third
            party revisions.
            """
        branch_repos = self.query_branch_repos()
        dirs = self.query_abs_dirs()
        new_branch = self.config['branch_name']
        unused_manifests = []
        if not self.check_existing_branch(new_branch, cwd=dirs['abs_manifest_dir'])[0]:
            self.fatal("b2g-manifest isn't branched properly!  Run --clean-repos --branch-repos")
        for manifest in self.query_manifests():
            self.info("Munging %s..." % manifest)
            doc = repo_manifest.load_manifest(manifest)
            try:
                repo_manifest.get_default(doc).getAttribute("revision")
            except IndexError:
                self.info("No default revision; skipping.")
                unused_manifests.append(manifest)
                continue
            for p in doc.getElementsByTagName('project'):
                name = self._query_repo_name(p.getAttribute('name'))
                fetch = repo_manifest.get_project_remote_url(doc, p)
                self.debug("Remote %s Name %s" % (fetch, name))
                current_revision = repo_manifest.get_project_revision(doc, p)
                if repo_manifest.is_commitid(current_revision):
                    self.info("%s: %s is already locked to %s; skipping." % (manifest, name, current_revision))
                    # I could setAttribute() here, but I very much doubt the
                    # default_revision is a commitid.
                    continue
                # We've branched this repo; do we set the revision to
                # new_branch or not?  ('fetch' needs to match, since we have
                # same-named repos with different urls =P )
                if name in branch_repos and branch_repos[name]['fetch'] == fetch:
                    orig_branch = branch_repos[name]['branch_revisions'].keys()[0]
                    if manifest in branch_repos[name]['branch_revisions'][orig_branch]:
                        if current_revision != orig_branch:
                            self.fatal("I don't know how we got here, but %s in %s's revision %s is not the branching point %s." %
                                       (name, manifest, current_revision, orig_branch))
                        self.info("Setting %s (%s) to %s (was %s)" % (name, manifest, new_branch, current_revision))
                        p.setAttribute('revision', new_branch)
                        continue
                    # Should we keep the old branch or lock revision?  Doing
                    # the former for now.
                    self.info("%s %s is off a different branch point (%s, not %s).  Keeping the old branch..." %
                              (manifest, name, current_revision, orig_branch))
                    continue
                if name in self.config['extra_branch_manifest_repos']:
                    p.setAttribute('revision', new_branch)
                    continue
                # Lock revision?
                if not self.config["lock_manifest_revisions"]:
                    self.info("%s: Not locking revision for %s due to config." % (manifest, name))
                    continue
                lock_revision = self._query_remote_branch_revision(fetch, current_revision, manifest)
                if lock_revision is not None:
                    p.setAttribute('revision', lock_revision)
            with self.opened(manifest, open_mode='w') as (fh, err):
                if err:
                    self.fatal("Can't open %s for writing!" % manifest)
                else:
                    doc.writexml(fh)
            fh.close()
        if self.config["delete_unused_manifests"]:
            self._delete_unused_manifests(unused_manifests)
        self.info("TODO: diff, commit, --push!")

    def push(self):
        """ Push the repos.
            """
        repos = self.query_branch_repos()
        dirs = self.query_abs_dirs()
        git = self.query_exe("git", return_type="list")
        new_branch = self.config['branch_name']
        problem_repos = []
        for name, r in repos.iteritems():
            fetch = r["fetch"]
            fetch = fetch.replace("git://github.com/", "git@github.com:")
            cwd = os.path.join(dirs['abs_work_dir'], name)
            self.info("pushing %s" % name)
            status = self.run_command(
                git + ['push', fetch, "%s:%s" % (new_branch, new_branch)],
                cwd=cwd, error_list=GitErrorList
            )
            if status:
                problem_repos.append(fetch)
        if problem_repos:
            self.error("Had problems with the following repos:\n%s" % pprint.pformat(problem_repos))


# __main__ {{{1
if __name__ == '__main__':
    b2g_branch = B2GBranch()
    b2g_branch.run_and_exit()
