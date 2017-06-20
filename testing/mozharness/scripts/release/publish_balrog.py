#!/usr/bin/env python
# lint_ignore=E501
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
""" updates.py

A script publish a release to Balrog.

"""

import os
import sys
from datetime import datetime, timedelta

sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.buildbot import BuildbotMixin

# PublishBalrog {{{1


class PublishBalrog(MercurialScript, BuildbotMixin):

    def __init__(self, require_config_file=True):
        super(PublishBalrog, self).__init__(
            all_actions=[
                'clobber',
                'pull',
                'submit-to-balrog',
            ],
            default_actions=[
                'clobber',
                'pull',
                'submit-to-balrog',
            ],
            config={
                'buildbot_json_path': 'buildprops.json',
                'credentials_file': 'oauth.txt',
            },
            require_config_file=require_config_file
        )

    def _pre_config_lock(self, rw_config):
        super(PublishBalrog, self)._pre_config_lock(rw_config)
        # override properties from buildbot properties here as defined by
        # taskcluster properties
        self.read_buildbot_config()
        if not self.buildbot_config:
            self.warning("Skipping buildbot properties overrides")
            return
        # TODO: version and appVersion should come from repo
        props = self.buildbot_config["properties"]
        for prop in ['product', 'version', 'build_number', 'channels',
                     'balrog_api_root', 'schedule_at', 'background_rate']:
            if props.get(prop):
                self.info("Overriding %s with %s" % (prop, props[prop]))
                self.config[prop] = props.get(prop)

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        self.abs_dirs = super(PublishBalrog, self).query_abs_dirs()
        self.abs_dirs["abs_tools_dir"] = os.path.join(
            self.abs_dirs['abs_work_dir'], self.config["repo"]["dest"])
        return self.abs_dirs

    def query_channel_configs(self):
        """Return a list of channel configs.
        For RC builds it returns "release" and "beta" using
        "enabled_if_version_matches" to match RC.

        :return: list
         """
        return [(n, c) for n, c in self.config["update_channels"].items() if
                n in self.config["channels"]]

    def query_repos(self):
        """Build a list of repos to clone."""
        return [self.config["repo"]]

    def pull(self):
        super(PublishBalrog, self).pull(
            repos=self.query_repos())

    def submit_to_balrog(self):
        for _, channel_config in self.query_channel_configs():
            self._submit_to_balrog(channel_config)

    def _submit_to_balrog(self, channel_config):
        dirs = self.query_abs_dirs()
        auth = os.path.join(os.getcwd(), self.config['credentials_file'])
        cmd = [
            sys.executable,
            os.path.join(dirs["abs_tools_dir"],
                         "scripts/build-promotion/balrog-release-shipper.py")]
        cmd.extend([
            "--api-root", self.config["balrog_api_root"],
            "--credentials-file", auth,
            "--username", self.config["balrog_username"],
            "--version", self.config["version"],
            "--product", self.config["product"],
            "--build-number", str(self.config["build_number"]),
            "--verbose",
        ])
        for r in channel_config["publish_rules"]:
            cmd.extend(["--rules", r])
        if channel_config.get("schedule_asap"):
            # RC releases going to the beta channel have no ETA set for the
            # RC-to-beta push. The corresponding task is scheduled after we
            # resolve the push-to-beta human decision task, so we can schedule
            # it ASAP plus some additional 30m to avoid retry() to fail.
            schedule_at = datetime.utcnow() + timedelta(minutes=30)
            cmd.extend(["--schedule-at", schedule_at.isoformat()])
        elif self.config.get("schedule_at"):
            cmd.extend(["--schedule-at", self.config["schedule_at"]])
        if self.config.get("background_rate"):
            cmd.extend(["--background-rate", str(self.config["background_rate"])])

        self.retry(lambda: self.run_command(cmd, halt_on_failure=True))


# __main__ {{{1
if __name__ == '__main__':
    PublishBalrog().run_and_exit()
