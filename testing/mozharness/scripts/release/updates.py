#!/usr/bin/env python
# lint_ignore=E501
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
""" updates.py

A script to bump patcher configs, generate update verification configs, and
publish top-level release blob information to Balrog.

It clones the tools repo, modifies the existing patcher config to include
current release build information, generates update verification configs,
commits the changes and tags the repo using tags by Releng convention.
After the changes are pushed to the repo, the script submits top-level release
information to Balrog.
"""

import os
import re
import sys

sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.buildbot import BuildbotMixin
from mozharness.mozilla.repo_manipulation import MercurialRepoManipulationMixin
from mozharness.mozilla.release import get_previous_version


# UpdatesBumper {{{1
class UpdatesBumper(MercurialScript, BuildbotMixin,
                    MercurialRepoManipulationMixin):
    config_options = [
        [['--hg-user', ], {
            "action": "store",
            "dest": "hg_user",
            "type": "string",
            "default": "ffxbld <release@mozilla.com>",
            "help": "Specify what user to use to commit to hg.",
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
    ]

    def __init__(self, require_config_file=True):
        super(UpdatesBumper, self).__init__(
            config_options=self.config_options,
            all_actions=[
                'clobber',
                'pull',
                'download-shipped-locales',
                'bump-configs',
                'commit-changes',
                'tag',
                'push',
                'submit-to-balrog',
            ],
            default_actions=[
                'clobber',
                'pull',
                'download-shipped-locales',
                'bump-configs',
                'commit-changes',
                'tag',
                'push',
                'submit-to-balrog',
            ],
            config={
                'buildbot_json_path': 'buildprops.json',
                'credentials_file': 'oauth.txt',
            },
            require_config_file=require_config_file
        )

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        self.abs_dirs = super(UpdatesBumper, self).query_abs_dirs()
        self.abs_dirs["abs_tools_dir"] = os.path.join(
            self.abs_dirs['abs_work_dir'], self.config["repo"]["dest"])
        return self.abs_dirs

    def query_repos(self):
        """Build a list of repos to clone."""
        return [self.config["repo"]]

    def query_commit_dirs(self):
        return [self.query_abs_dirs()["abs_tools_dir"]]

    def query_commit_message(self):
        return "Automated configuration bump"

    def query_push_dirs(self):
        return self.query_commit_dirs()

    def query_push_args(self, cwd):
        # cwd is not used here
        hg_ssh_opts = "ssh -l {user} -i {key}".format(
            user=self.config["ssh_user"],
            key=os.path.expanduser(self.config["ssh_key"])
        )
        return ["-e", hg_ssh_opts]

    def query_shipped_locales_path(self):
        dirs = self.query_abs_dirs()
        return os.path.join(dirs["abs_work_dir"], "shipped-locales")

    def query_channel_configs(self):
        """Return a list of channel configs.
        For RC builds it returns "release" and "beta" using
        "enabled_if_version_matches" to match RC.

        :return: list
        """
        return [(n, c) for n, c in self.config["update_channels"].items() if
                n in self.config["channels"]]

    def pull(self):
        super(UpdatesBumper, self).pull(
            repos=self.query_repos())

    def download_shipped_locales(self):
        dirs = self.query_abs_dirs()
        self.mkdir_p(dirs["abs_work_dir"])
        url = self.config["shipped-locales-url"].format(
            revision=self.config["revision"])
        if not self.download_file(url=url,
                                  file_name=self.query_shipped_locales_path()):
            self.fatal("Unable to fetch shipped-locales from %s" % url)

    def bump_configs(self):
        for channel, channel_config in self.query_channel_configs():
            self.bump_patcher_config(channel_config)
            self.bump_update_verify_configs(channel, channel_config)

    def query_matching_partials(self, channel_config):
        return [(v, b) for v, b in self.config["partial_versions"] if
                re.match(channel_config["version_regex"], v)]

    def query_patcher_config(self, channel_config):
        dirs = self.query_abs_dirs()
        patcher_config = os.path.join(
            dirs["abs_tools_dir"], "release/patcher-configs",
            channel_config["patcher_config"])
        return patcher_config

    def query_patcher_config_product(self, channel_config):
        return channel_config.get("patcher_config_product_override") or \
            self.config["product"]

    def query_update_verify_config(self, channel, platform):
        dirs = self.query_abs_dirs()
        uvc = os.path.join(
            dirs["abs_tools_dir"], "release/updates",
            "{}-{}-{}.cfg".format(channel, self.config["product"], platform))
        return uvc

    def bump_patcher_config(self, channel_config):
        # TODO: to make it possible to run this before we have files copied to
        # the candidates directory, we need to add support to fetch build IDs
        # from tasks.
        dirs = self.query_abs_dirs()
        env = {"PERL5LIB": os.path.join(dirs["abs_tools_dir"], "lib/perl")}
        partial_versions = [v[0] for v in
                            self.query_matching_partials(channel_config)]
        script = os.path.join(
            dirs["abs_tools_dir"], "release/patcher-config-bump.pl")
        patcher_config = self.query_patcher_config(channel_config)
        patcher_config_product = self.query_patcher_config_product(channel_config)
        cmd = [self.query_exe("perl"), script]
        cmd.extend([
            "-p", patcher_config_product,
            "-r", patcher_config_product.capitalize(),
            "-v", self.config["version"],
            "-a", self.config["appVersion"],
            "-o", get_previous_version(
                self.config["version"], partial_versions),
            "-b", str(self.config["build_number"]),
            "-c", patcher_config,
            "-f", self.config["archive_domain"],
            "-d", self.config["download_domain"],
            "-l", self.query_shipped_locales_path(),
        ])
        for v in partial_versions:
            cmd.extend(["--partial-version", v])
        for p in self.config["platforms"]:
            cmd.extend(["--platform", p])
        for mar_channel_id in channel_config["mar_channel_ids"]:
            cmd.extend(["--mar-channel-id", mar_channel_id])
        if "stage_product" in self.config:
            cmd.extend(["--stage-product", self.config["stage_product"]])
        if "bouncer_product" in self.config:
            cmd.extend(["--bouncer-product", self.config["bouncer_product"]])
        self.run_command(cmd, halt_on_failure=True, env=env)

    def bump_update_verify_configs(self, channel, channel_config):
        dirs = self.query_abs_dirs()
        script = os.path.join(
            dirs["abs_tools_dir"],
            "scripts/build-promotion/create-update-verify-config.py")
        patcher_config = self.query_patcher_config(channel_config)
        patcher_config_product = self.query_patcher_config_product(channel_config)
        for platform in self.config["platforms"]:
            cmd = [sys.executable, script]
            output = self.query_update_verify_config(channel, platform)
            cmd.extend([
                "--config", patcher_config,
                "--platform", platform,
                "--update-verify-channel",
                channel_config["update_verify_channel"],
                "--output", output,
                "--archive-prefix", self.config["archive_prefix"],
                "--previous-archive-prefix",
                self.config["previous_archive_prefix"],
                "--product", patcher_config_product,
                "--balrog-url", self.config["balrog_url"],
                "--build-number", str(self.config["build_number"]),
            ])
            if "stage_product" in self.config:
                cmd.extend(["--stage-product", self.config["stage_product"]])
            if "updater_platform" in self.config:
                cmd.extend(["--updater-platform", self.config["updater_platform"]])

            self.run_command(cmd, halt_on_failure=True)

    def tag(self):
        dirs = self.query_abs_dirs()
        tags = ["{product}_{version}_BUILD{build_number}_RUNTIME",
                "{product}_{version}_RELEASE_RUNTIME"]
        tags = [t.format(product=self.config["product"].upper(),
                         version=self.config["version"].replace(".", "_"),
                         build_number=self.config["build_number"])
                for t in tags]
        self.hg_tag(cwd=dirs["abs_tools_dir"], tags=tags,
                    user=self.config["hg_user"], force=True)

    def submit_to_balrog(self):
        for _, channel_config in self.query_channel_configs():
            self._submit_to_balrog(channel_config)
        if 'generate_bz2_blob' in self.config and \
                self.config['generate_bz2_blob']:
            for _, channel_config in self.query_channel_configs():
                self._submit_to_balrog_bz2(channel_config)

    def _submit_to_balrog(self, channel_config):
        dirs = self.query_abs_dirs()
        auth = os.path.join(os.getcwd(), self.config['credentials_file'])
        cmd = [
            sys.executable,
            os.path.join(dirs["abs_tools_dir"],
                         "scripts/build-promotion/balrog-release-pusher.py")]
        cmd.extend([
            "--api-root", self.config["balrog_api_root"],
            "--download-domain", self.config["download_domain"],
            "--archive-domain", self.config["archive_domain"],
            "--credentials-file", auth,
            "--product", self.config["product"],
            "--version", self.config["version"],
            "--build-number", str(self.config["build_number"]),
            "--app-version", self.config["appVersion"],
            "--username", self.config["balrog_username"],
            "--verbose",
        ])
        for c in channel_config["channel_names"]:
            cmd.extend(["--channel", c])
        for r in channel_config["rules_to_update"]:
            cmd.extend(["--rule-to-update", r])
        for p in self.config["platforms"]:
            cmd.extend(["--platform", p])
        for v, build_number in self.query_matching_partials(channel_config):
            partial = "{version}build{build_number}".format(
                version=v, build_number=build_number)
            cmd.extend(["--partial-update", partial])
        if channel_config["requires_mirrors"]:
            cmd.append("--requires-mirrors")
        if self.config["balrog_use_dummy_suffix"]:
            cmd.append("--dummy")

        self.retry(lambda: self.run_command(cmd, halt_on_failure=True))

    def _submit_to_balrog_bz2(self, channel_config):
        if "bz2_blob_suffix" not in channel_config:
            self.info("No need to generate BZ2 blob")
            return

        dirs = self.query_abs_dirs()
        # Use env varialbe instead of command line to avoid issues with blob
        # names starting with "-", e.g. "-bz2"
        env = {"BALROG_BLOB_SUFFIX": channel_config["bz2_blob_suffix"]}
        auth = os.path.join(os.getcwd(), self.config['credentials_file'])
        cmd = [
            sys.executable,
            os.path.join(dirs["abs_tools_dir"],
                         "scripts/build-promotion/balrog-release-pusher.py")]
        cmd.extend([
            "--api-root", self.config["balrog_api_root"],
            "--download-domain", self.config["download_domain"],
            "--archive-domain", self.config["archive_domain"],
            "--credentials-file", auth,
            "--product", self.config["product"],
            "--version", self.config["version"],
            "--build-number", str(self.config["build_number"]),
            "--app-version", self.config["appVersion"],
            "--username", self.config["balrog_username"],
            "--complete-mar-filename-pattern",
            channel_config["complete_mar_filename_pattern"],
            "--complete-mar-bouncer-product-pattern",
            channel_config["complete_mar_bouncer_product_pattern"],
            "--verbose",
        ])

        for v, build_number in self.query_matching_partials(channel_config):
            if v < "56.0":
                self.info("Adding %s to partials" % v)
                partial = "{version}build{build_number}".format(
                    version=v, build_number=build_number)
                cmd.extend(["--partial-update", partial])
            else:
                self.info("Not adding %s to partials" % v)

        for c in channel_config["channel_names"]:
            cmd.extend(["--channel", c])
        for r in channel_config["bz2_rules_to_update"]:
            cmd.extend(["--rule-to-update", r])
        for p in self.config["platforms"]:
            cmd.extend(["--platform", p])
        if channel_config["requires_mirrors"]:
            cmd.append("--requires-mirrors")
        if self.config["balrog_use_dummy_suffix"]:
            cmd.append("--dummy")

        self.retry(lambda: self.run_command(cmd, halt_on_failure=True, env=env))


# __main__ {{{1
if __name__ == '__main__':
    UpdatesBumper().run_and_exit()
