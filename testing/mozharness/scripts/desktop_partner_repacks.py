#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""desktop_partner_repacks.py

This script manages Desktop partner repacks for beta/release builds.
"""
import os
import sys

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.script import BaseScript
from mozharness.mozilla.buildbot import BuildbotMixin
from mozharness.mozilla.purge import PurgeMixin
from mozharness.mozilla.release import ReleaseMixin
from mozharness.base.python import VirtualenvMixin
from mozharness.base.log import FATAL


# DesktopPartnerRepacks {{{1
class DesktopPartnerRepacks(ReleaseMixin, BuildbotMixin, PurgeMixin,
                            BaseScript, VirtualenvMixin):
    """Manages desktop partner repacks"""
    actions = [
                "clobber",
                "create-virtualenv",
                "activate-virtualenv",
                "setup",
                "repack",
                "summary",
              ]
    config_options = [
        [["--version", "-v"], {
          "dest": "version",
          "help": "Version of Firefox to repack",
          }],
        [["--build-number", "-n"], {
          "dest": "build_number",
          "help": "Build number of Firefox to repack",
          }],
        [["--platform"], {
          "dest": "platform",
          "help": "Platform to repack (e.g. linux64, macosx64, ...)",
          }],
        [["--partner", "-p"], {
          "dest": "partner",
          "help": "Limit repackaging to partners matching this string",
          }],
        [["--s3cfg"], {
          "dest": "s3cfg",
          "help": "Configuration file for uploading to S3 using s3cfg",
          }],
        [["--hgroot"], {
          "dest": "hgroot",
          "help": "Use a different hg server for retrieving files",
          }],
        [["--hgrepo"], {
          "dest": "hgrepo",
          "help": "Use a different base repo for retrieving files",
          }],
        [["--require-buildprops"], {
            "action": "store_true",
            "dest": "require_buildprops",
            "default": False,
            "help": "Read in config options (like partner) from the buildbot properties file."
          }],
    ]

    def __init__(self):
        # fxbuild style:
        buildscript_kwargs = {
            'all_actions': DesktopPartnerRepacks.actions,
            'default_actions': DesktopPartnerRepacks.actions,
            'config': {
                'buildbot_json_path': 'buildprops.json',
                "log_name": "partner-repacks",
                "hashType": "sha512",
                'virtualenv_modules': [
                    'requests==2.2.1',
                    'PyHawk-with-a-single-extra-commit==0.1.5',
                    'taskcluster==0.0.15',
                    's3cmd==1.6.0',
                ],
                'virtualenv_path': 'venv',
                'workdir': 'partner-repacks',
            },
        }
        #

        BaseScript.__init__(
            self,
            config_options=self.config_options,
            **buildscript_kwargs
        )


    def _pre_config_lock(self, rw_config):
        self.read_buildbot_config()
        if not self.buildbot_config:
            self.warning("Skipping buildbot properties overrides")
        else:
            if self.config.get('require_buildprops', False) is True:
                if not self.buildbot_config:
                    self.fatal("Unable to load properties from file: %s" % self.config.get('buildbot_json_path'))
            props = self.buildbot_config["properties"]
            for prop in ['version', 'build_number', 'revision', 'repo_file', 'repack_manifests_url', 'partner']:
                if props.get(prop):
                    self.info("Overriding %s with %s" % (prop, props[prop]))
                    self.config[prop] = props.get(prop)

        if 'version' not in self.config:
            self.fatal("Version (-v) not supplied.")
        if 'build_number' not in self.config:
            self.fatal("Build number (-n) not supplied.")
        if 'repo_file' not in self.config:
            self.fatal("repo_file not supplied.")
        if 'repack_manifests_url' not in self.config:
            self.fatal("repack_manifests_url not supplied.")

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(DesktopPartnerRepacks, self).query_abs_dirs()
        for directory in abs_dirs:
            value = abs_dirs[directory]
            abs_dirs[directory] = value
        dirs = {}
        dirs['abs_repo_dir'] = os.path.join(abs_dirs['abs_work_dir'], '.repo')
        dirs['abs_partners_dir'] = os.path.join(abs_dirs['abs_work_dir'], 'partners')
        dirs['abs_scripts_dir'] = os.path.join(abs_dirs['abs_work_dir'], 'scripts')
        for key in dirs.keys():
            if key not in abs_dirs:
                abs_dirs[key] = dirs[key]
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    # Actions {{{
    def _repo_cleanup(self):
        self.rmtree(self.query_abs_dirs()['abs_repo_dir'])
        self.rmtree(self.query_abs_dirs()['abs_partners_dir'])
        self.rmtree(self.query_abs_dirs()['abs_scripts_dir'])

    def _repo_init(self, repo):
        status = self.run_command([repo, "init", "--no-repo-verify",
                                   "-u", self.config['repack_manifests_url']],
                                  cwd=self.query_abs_dirs()['abs_work_dir'])
        if status:
            return status
        return self.run_command([repo, "sync"],
                                cwd=self.query_abs_dirs()['abs_work_dir'])

    def setup(self):
        """setup step"""
        repo = self.download_file(self.config['repo_file'],
                                  file_name='repo',
                                  parent_dir=self.query_abs_dirs()['abs_work_dir'],
                                  error_level=FATAL)
        if not os.path.exists(repo):
            self.fatal("Unable to download repo tool.")
        self.chmod(repo, 0755)
        self.retry(self._repo_init,
                   args=(repo,),
                   error_level=FATAL,
                   cleanup=self._repo_cleanup(),
                   good_statuses=[0],
                   sleeptime=5)

    def repack(self):
        """creates the repacks"""
        repack_cmd = [sys.executable, "partner-repacks.py",
                      "-v", self.config['version'],
                      "-n", str(self.config['build_number'])]
        if self.config.get('platform'):
            repack_cmd.extend(["--platform", self.config['platform']])
        if self.config.get('partner'):
            repack_cmd.extend(["--partner", self.config['partner']])
        if self.config.get('s3cfg'):
            repack_cmd.extend(["--s3cfg", self.config['s3cfg']])
        if self.config.get('hgroot'):
            repack_cmd.extend(["--hgroot", self.config['hgroot']])
        if self.config.get('hgrepo'):
            repack_cmd.extend(["--repo", self.config['hgrepo']])
        if self.config.get('revision'):
            repack_cmd.extend(["--tag", self.config["revision"]])

        return self.run_command(repack_cmd,
                                cwd=self.query_abs_dirs()['abs_scripts_dir'])

# main {{{
if __name__ == '__main__':
    partner_repacks = DesktopPartnerRepacks()
    partner_repacks.run_and_exit()
