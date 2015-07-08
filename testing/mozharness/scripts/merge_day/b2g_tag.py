#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""b2g_tag.py

Tag the b2g repos for merge day.
"""

import os
import pprint
import sys
import time

try:
    import simplejson as json
    assert json
except ImportError:
    import json

sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))

from mozharness.base.errors import GitErrorList, HgErrorList, VCSException
from mozharness.base.log import INFO
from mozharness.base.transfer import TransferMixin
from mozharness.base.vcs.vcsbase import MercurialScript


# B2GTag {{{1
class B2GTag(TransferMixin, MercurialScript):
    config_options = [
        [['--gecko-repo', ], {
            "action": "extend",
            "dest": "gecko_repos",
            "type": "string",
            "help": "Specify which gecko repo(s) to tag, along with gaia."
        }],
        [['--date-string', ], {
            "action": "store",
            "dest": "date_string",
            "type": "string",
            "default": time.strftime('%Y%m%d'),
            "help": "Specify the date string to use in the tag name."
        }],
    ]
    gecko_repos = None

    def __init__(self, require_config_file=True):
        super(B2GTag, self).__init__(
            config_options=self.config_options,
            all_actions=[
                'clobber',
                'clean-repos',
                'pull',
                'push-loop',
                'summary',
            ],
            default_actions=[
                'clean-repos',
                'pull',
                'push-loop',
                'summary',
            ],
            require_config_file=require_config_file
        )
        self.run_sanity_check()

# Helper methods {{{1
    def run_sanity_check(self):
        """ Make sure we're set up correctly before we start cloning or tagging
            anything.

            First, verify any specified gecko_repos exist in the b2g_branches
            dict.

            Next, make sure we can reach mapper and it's working.

            If there are any errors, compile the list and fatal().
            """
        sanity_message = ""
        if self.config.get("gecko_repos"):
            bad_branch_names = []
            for repo_name in self.query_gecko_repos():
                if repo_name not in self.config["b2g_branches"].keys():
                    bad_branch_names.append(repo_name)
            if bad_branch_names:
                sanity_message += "The following branch name(s) aren't in b2g_branches: %s\n" % ",".join(bad_branch_names)
        if 'push_loop' in self.actions:
            try:
                json_contents = self.load_json_from_url(
                    "%s/1" % self.config["gaia_mapper_base_url"],
                    log_level=INFO,
                )
                assert json_contents["git_rev"]
            except:
                sanity_message += "Can't load %s/1 for hg->git mapping!\n" % self.config["gaia_mapper_base_url"]
        if sanity_message:
            self.fatal(sanity_message)

    def query_gecko_repos(self):
        """ Which gecko repos + gaia are we converting?
            By default everything in self.config['b2g_branches'],
            but we can override that list with --gecko-repos.
            """
        if self.gecko_repos:
            return self.gecko_repos
        self.gecko_repos = list(self.config.get("gecko_repos",
                                self.config['b2g_branches'].keys()))
        self.gecko_repos.sort()
        return self.gecko_repos

    def query_repo_pull_config(self, repo_name, b2g_branch_config):
        """ Build the repo pull url for a repo. This is for convenience.
            """
        return {
            "repo": '%s/%s' % (self.config['hg_base_pull_url'], repo_name),
            "dest": repo_name,
            "tag": b2g_branch_config.get("tag", "default")
        }

    def query_repo_push_url(self, repo_name):
        """ Build the repo push url for a repo. This is for convenience.
            """
        return '%s/%s' % (self.config['hg_base_push_url'], repo_name)

    def query_gaia_git_revision(self, repo_name, b2g_branch_config):
        """ For most repos, read b2g/config/gaia.json,
            read the hg gaia revision, convert that in mapper, and return it.
            In the case of not being able to determine the git revision,
            throw a VCSException.

            Otherwise return None, and we'll tag the tip of the branch.
            """
        dirs = self.query_abs_dirs()
        if not b2g_branch_config.get("use_gaia_json", True):
            return None
        json_path = os.path.join(dirs["abs_work_dir"], repo_name, "b2g", "config", "gaia.json")
        if not os.path.exists(json_path):
            raise VCSException("%s doesn't exist!" % json_path)
        contents = self.read_from_file(json_path)
        try:
            json_contents = json.loads(contents)
            hg_revision = json_contents['revision']
        except ValueError:
            raise VCSException("%s is invalid json!" % json_path)
        except KeyError:
            raise VCSException("%s has no 'revision' set!" % json_path)
        try:
            url = "%s/%s" % (self.config["gaia_mapper_base_url"], hg_revision)
            json_contents = self.load_json_from_url(url)
            git_rev = json_contents["git_rev"]
            assert git_rev
            return git_rev
        except:
            raise VCSException("Unable to get git_rev from %s !" % url)

    def query_abs_dirs(self):
        """ Override the built-in query_abs_dirs to provide an absolute path
            for the gaia clone.
            """
        if self.abs_dirs:
            return self.abs_dirs
        dirs = super(B2GTag, self).query_abs_dirs()
        dirs['abs_gaia_dir'] = os.path.join(dirs['abs_work_dir'], 'gaia')
        self.abs_dirs = dirs
        return self.abs_dirs

    def query_tag_name(self, b2g_branch_config):
        """ Return the tag name for a specific branch config.
            e.g. B2G_1_3_20140428_MERGEDAY
            """
        return b2g_branch_config["tag_name"] % {'DATE': self.config['date_string']}

    def query_short_tag_name(self, b2g_branch_config):
        """ Return the shortened tag name for a specific branch config.
            This assumes there's going to be a _%(DATE)s in the tag name.

            e.g. B2G_1_3
            """
        return b2g_branch_config["tag_name"].split("_%")[0]

    def hg_tag(self, repo_name, b2g_branch_config):
        """ Attempt to tag and push gecko.  This assumes the trees are open.

            On failure, throw a VCSException.
            """
        hg = self.query_exe("hg", return_type="list")
        dirs = self.query_abs_dirs()
        hg_dir = os.path.join(dirs["abs_work_dir"], repo_name)
        tag_name = self.query_tag_name(b2g_branch_config)
        short_tag_name = self.query_short_tag_name(b2g_branch_config)
        push_url = self.query_repo_push_url(repo_name)
        cmd = hg + ["tag", tag_name, "-m",
                    "tagging %s for mergeday. r=a=mergeday CLOSED TREE DONTBUILD" % short_tag_name,
                    ]
        if self.run_command(cmd, cwd=hg_dir, error_list=HgErrorList):
            raise VCSException("Can't tag %s with %s" % (repo_name, tag_name))
        # Debugging! Echo only for now.
        # cmd = hg + ["push", push_url]
        cmd = hg + ["push", push_url]
        if self.run_command(cmd, cwd=hg_dir, error_list=HgErrorList):
            self.run_command(hg + ["--config", "extensions.mq=",
                                   "strip", "--no-backup", "outgoing()"],
                             cwd=hg_dir)
            self.run_command(hg + ["up", "-C"],
                             cwd=hg_dir)
            self.run_command(hg + ["--config", "extensions.purge=",
                                   "purge", "--all"],
                             cwd=hg_dir)
            raise VCSException("Can't push to %s!" % push_url)

    def git_tag(self, hg_repo_name, gaia_git_revision, b2g_branch_config):
        """ Attempt to tag and push gaia.

            On failure, throw a VCSException.
            """
        git = self.query_exe("git", return_type="list")
        dirs = self.query_abs_dirs()
        tag_name = self.query_tag_name(b2g_branch_config)
        cmd = git + ['tag', tag_name]
        if gaia_git_revision is None:
            if self.run_command(
                git + ["checkout", b2g_branch_config["gaia_branch"]],
                cwd=dirs["abs_gaia_dir"],
                error_list=GitErrorList,
            ):
                raise VCSException("Can't checkout %s branch!" % b2g_branch_config["gaia_branch"])
        else:
            cmd.append(gaia_git_revision)
        if self.run_command(
            cmd,
            cwd=dirs["abs_gaia_dir"],
            error_list=GitErrorList,
        ):
            raise VCSException("Can't tag gaia for %s!" % hg_repo_name)
        if self.run_command(
            # Debugging! Echo only for now.
            git + ["push", "--tags"],
            # comment out for testing
            # git + ["push", "--tags"],
            cwd=dirs["abs_gaia_dir"],
            error_list=GitErrorList,
        ):
            raise VCSException("Can't push gaia tag for %s!" % hg_repo_name)

# Actions {{{1
    def clean_repos(self):
        """ We may end up with contaminated local repos at some point, but
            we don't want to have to clobber and reclone from scratch every
            time.

            This is an attempt to clean up the local repos without needing a
            clobber.
            """
        dirs = self.query_abs_dirs()
        hg = self.query_exe("hg", return_type="list")
        git = self.query_exe("git", return_type="list")
        hg_repos = self.query_gecko_repos()
        hg_strip_error_list = [{
            'substr': r'''abort: empty revision set''', 'level': INFO,
            'explanation': "Nothing to clean up; we're good!",
        }] + HgErrorList
        for repo_name in hg_repos:
            repo_path = os.path.join(dirs['abs_work_dir'], repo_name)
            if os.path.exists(repo_path):
                self.retry(
                    self.run_command,
                    args=(hg + ["--config", "extensions.mq=", "strip",
                          "--no-backup", "outgoing()"], ),
                    kwargs={
                        'cwd': repo_path,
                        'error_list': hg_strip_error_list,
                        'return_type': 'num_errors',
                        'success_codes': (0, 255),
                    },
                )
        if os.path.exists(dirs['abs_gaia_dir']):
            remote_tags = []
            output = self.get_output_from_command(
                git + ["ls-remote", "--tags"],
                cwd=dirs['abs_gaia_dir'],
            )
            for line in output.splitlines():
                if 'refs/tags' not in line:
                    continue
                remote_tags.append(line.split('/')[-1])
            output = self.get_output_from_command(
                git + ["tag", "-l"],
                cwd=dirs['abs_gaia_dir'],
            )
            local_tags = output.splitlines()
            for tag in local_tags:
                if tag not in remote_tags:
                    self.info("Found local tag %s that doesn't exist on remote!" % tag)
                    self.run_command(
                        git + ['tag', '-d', tag],
                        cwd=dirs["abs_gaia_dir"],
                    )
            # Verify
            output = self.get_output_from_command(
                git + ["tag", "-l"],
                cwd=dirs['abs_gaia_dir'],
            )
            local_tags = output.splitlines()
            if not set(local_tags).issubset(set(remote_tags)):
                self.fatal("Gaia still has tags that don't exist on remote!")
            else:
                self.info("Looks like we're good, local_tags is a subset of remote_tags.")

    def pull(self):
        """ Pull action.

            Builds an hg repo list-of-dicts and sends them to
            MercurialScript.pull().  Also pulls the gaia_url.

            We'll potentially run another pull in the push-loop action, but
            this action makes sure we have an up-to-date clone on disk to
            operate on.
            """
        dirs = self.query_abs_dirs()
        git = self.query_exe("git", return_type="list")
        hg_repos = []
        for repo_name in self.query_gecko_repos():
            b2g_branch_config = self.config['b2g_branches'][repo_name]
            hg_repos.append(self.query_repo_pull_config(repo_name, b2g_branch_config))
        self.debug("HG repos: %s" % pprint.pformat(hg_repos))
        super(B2GTag, self).pull(repos=hg_repos)
        if not os.path.exists(dirs['abs_gaia_dir']):
            self.run_command(
                git + ["clone", self.config["gaia_url"], "gaia"],
                cwd=dirs['abs_work_dir'],
                error_list=GitErrorList,
                halt_on_failure=True,
            )
        else:
            self.run_command(
                git + ["pull"],
                cwd=dirs['abs_gaia_dir'],
                error_list=GitErrorList,
                halt_on_failure=True,
            )

    def push_loop(self):
        """ Create the tag and push for each gecko+gaia pair.
            This sometimes requires a pull+rebase, hence the loop.
            """
        for repo_name in self.query_gecko_repos():
            b2g_branch_config = self.config['b2g_branches'][repo_name]
            repo_config = self.query_repo_pull_config(repo_name, b2g_branch_config)
            super(B2GTag, self).pull(repos=[repo_config])
            try:
                # We'll get None (tag tip-of-branch) or a git revision or a
                # VCSException here.
                gaia_git_revision = self.query_gaia_git_revision(repo_name, b2g_branch_config)
            except VCSException, e:
                self.add_failure(repo_name, message=str(e))
                continue
            if self.retry(
                self.hg_tag,
                retry_exceptions=(VCSException, ),
                args=(repo_name, b2g_branch_config),
            ):
                self.add_failure(repo_name, message=str(e))
                continue
            if self.retry(
                self.git_tag,
                retry_exceptions=(VCSException, ),
                args=(repo_name, gaia_git_revision, b2g_branch_config),
            ):
                self.add_failure(repo_name, message=str(e))
                continue


# __main__ {{{1
if __name__ == '__main__':
    b2g_tag = B2GTag()
    b2g_tag.run_and_exit()
