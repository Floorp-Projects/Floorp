#!/usr/bin/env python
# lint_ignore=E501
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
""" postrelease_version_bump.py

A script to increase in-tree version number after shipping a release.
"""

import os
import sys

sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.buildbot import BuildbotMixin
from mozharness.mozilla.repo_manupulation import MercurialRepoManipulationMixin


# PostReleaseVersionBump {{{1
class PostReleaseVersionBump(MercurialScript, BuildbotMixin,
                             MercurialRepoManipulationMixin):
    config_options = [
        [['--hg-user', ], {
            "action": "store",
            "dest": "hg_user",
            "type": "string",
            "default": "ffxbld <release@mozilla.com>",
            "help": "Specify what user to use to commit to hg.",
        }],
        [['--next-version', ], {
            "action": "store",
            "dest": "next_version",
            "type": "string",
            "help": "Next version used in version bump",
        }],
        [['--ssh-user', ], {
            "action": "store",
            "dest": "ssh_user",
            "type": "string",
            "help": "SSH username with hg.mozilla.org permissions",
        }],
        [['--ssh-key', ], {
            "action": "store",
            "dest": "ssh_key",
            "type": "string",
            "help": "Path to SSH key.",
        }],
        [['--product', ], {
            "action": "store",
            "dest": "product",
            "type": "string",
            "help": "Product name",
        }],
        [['--version', ], {
            "action": "store",
            "dest": "version",
            "type": "string",
            "help": "Version",
        }],
        [['--build-number', ], {
            "action": "store",
            "dest": "build_number",
            "type": "string",
            "help": "Build number",
        }],
        [['--revision', ], {
            "action": "store",
            "dest": "revision",
            "type": "string",
            "help": "HG revision to tag",
        }],
    ]

    def __init__(self, require_config_file=True):
        super(PostReleaseVersionBump, self).__init__(
            config_options=self.config_options,
            all_actions=[
                'clobber',
                'clean-repos',
                'pull',
                'bump_postrelease',
                'commit-changes',
                'tag',
                'push',
            ],
            default_actions=[
                'clean-repos',
                'pull',
                'bump_postrelease',
                'commit-changes',
                'tag',
                'push',
            ],
            config={
                'buildbot_json_path': 'buildprops.json',
            },
            require_config_file=require_config_file
        )

    def _pre_config_lock(self, rw_config):
        super(PostReleaseVersionBump, self)._pre_config_lock(rw_config)
        # override properties from buildbot properties here as defined by
        # taskcluster properties
        self.read_buildbot_config()
        if not self.buildbot_config:
            self.warning("Skipping buildbot properties overrides")
        else:
            props = self.buildbot_config["properties"]
            for prop in ['next_version', 'product', 'version', 'build_number',
                         'revision']:
                if props.get(prop):
                    self.info("Overriding %s with %s" % (prop, props[prop]))
                    self.config[prop] = props.get(prop)

        if not self.config.get("next_version"):
            self.fatal("Next version has to be set. Use --next-version or "
                       "pass `next_version' via buildbot properties.")

    def query_abs_dirs(self):
        """ Allow for abs_from_dir and abs_to_dir
            """
        if self.abs_dirs:
            return self.abs_dirs
        self.abs_dirs = super(PostReleaseVersionBump, self).query_abs_dirs()
        self.abs_dirs["abs_gecko_dir"] = os.path.join(
                self.abs_dirs['abs_work_dir'], self.config["repo"]["dest"])
        return self.abs_dirs

    def query_repos(self):
        """Build a list of repos to clone."""
        return [self.config["repo"]]

    def query_commit_dirs(self):
        return [self.query_abs_dirs()["abs_gecko_dir"]]

    def query_commit_message(self):
        return "Automatic version bump. CLOSED TREE NO BUG a=release"

    def query_push_dirs(self):
        return self.query_commit_dirs()

    def query_push_args(self, cwd):
        # cwd is not used here
        hg_ssh_opts = "ssh -l {user} -i {key}".format(
            user=self.config["ssh_user"],
            key=os.path.expanduser(self.config["ssh_key"])
        )
        return ["-e", hg_ssh_opts, "-r", "."]

    def pull(self):
        super(PostReleaseVersionBump, self).pull(
                repos=self.query_repos())

    def bump_postrelease(self, *args, **kwargs):
        """Bump version"""
        dirs = self.query_abs_dirs()
        for f in self.config["version_files"]:
            curr_version = ".".join(
                self.get_version(dirs['abs_gecko_dir'], f["file"]))
            self.replace(os.path.join(dirs['abs_gecko_dir'], f["file"]),
                         curr_version, self.config["next_version"])

    def tag(self):
        dirs = self.query_abs_dirs()
        tags = ["{product}_{version}_BUILD{build_number}",
                "{product}_{version}_RELEASE"]
        tags = [t.format(product=self.config["product"].upper(),
                         version=self.config["version"].replace(".", "_"),
                         build_number=self.config["build_number"])
                for t in tags]
        message = "No bug - Tagging {revision} with {tags} a=release CLOSED TREE"
        message = message.format(
            revision=self.config["revision"],
            tags=', '.join(tags))
        self.hg_tag(cwd=dirs["abs_gecko_dir"], tags=tags,
                    revision=self.config["revision"], message=message,
                    user=self.config["hg_user"], force=True)

# __main__ {{{1
if __name__ == '__main__':
    PostReleaseVersionBump().run_and_exit()
