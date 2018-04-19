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
from mozharness.mozilla.secrets import SecretsMixin
from mozharness.base.python import VirtualenvMixin
from mozharness.base.log import FATAL


# DesktopPartnerRepacks {{{1
class DesktopPartnerRepacks(ReleaseMixin, BuildbotMixin, PurgeMixin,
                            BaseScript, VirtualenvMixin, SecretsMixin):
    """Manages desktop partner repacks"""
    actions = [
                "clobber",
                "get-secrets",
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
        [["--taskid", "-t"], {
            "dest": "taskIds",
            "action": "extend",
            "help": "taskId(s) of upstream tasks for vanilla Firefox artifacts",
        }],
    ]

    def __init__(self):
        # fxbuild style:
        buildscript_kwargs = {
            'all_actions': DesktopPartnerRepacks.actions,
            'default_actions': DesktopPartnerRepacks.actions,
            'config': {
                "log_name": "partner-repacks",
                "hashType": "sha512",
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
        if os.getenv('REPACK_MANIFESTS_URL'):
            self.info('Overriding repack_manifests_url to %s' % os.getenv('REPACK_MANIFESTS_URL'))
            self.config['repack_manifests_url'] = os.getenv('REPACK_MANIFESTS_URL')
        if os.getenv('UPSTREAM_TASKIDS'):
            self.info('Overriding taskIds with %s' % os.getenv('UPSTREAM_TASKIDS'))
            self.config['taskIds'] = os.getenv('UPSTREAM_TASKIDS').split()
        self.config['scm_level'] = os.environ.get('MOZ_SCM_LEVEL', '1')

        if 'version' not in self.config:
            self.fatal("Version (-v) not supplied.")
        if 'build_number' not in self.config:
            self.fatal("Build number (-n) not supplied.")
        if 'repo_file' not in self.config:
            self.fatal("repo_file not supplied.")
        if 'repack_manifests_url' not in self.config:
            self.fatal("repack_manifests_url not supplied in config or via REPACK_MANIFESTS_URL")
        if 'taskIds' not in self.config:
            self.fatal('Need upstream taskIds from command line or in UPSTREAM_TASKIDS')

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
        partial_env = {
            'GIT_SSH_COMMAND': 'ssh -oIdentityFile={}'.format(self.config['ssh_key'])
        }
        status = self.run_command([repo, "init", "--no-repo-verify",
                                   "-u", self.config['repack_manifests_url']],
                                  cwd=self.query_abs_dirs()['abs_work_dir'],
                                  partial_env=partial_env)
        if status:
            return status
        return self.run_command([repo, "sync", "--current-branch", "--no-tags"],
                                cwd=self.query_abs_dirs()['abs_work_dir'],
                                partial_env=partial_env)

    def setup(self):
        """setup step"""
        repo = self.download_file(self.config['repo_file'],
                                  file_name='repo',
                                  parent_dir=self.query_abs_dirs()['abs_work_dir'],
                                  error_level=FATAL)
        if not os.path.exists(repo):
            self.fatal("Unable to download repo tool.")
        self.chmod(repo, 0o755)
        self.retry(self._repo_init,
                   args=(repo,),
                   error_level=FATAL,
                   cleanup=self._repo_cleanup(),
                   good_statuses=[0],
                   sleeptime=5)

    def repack(self):
        """creates the repacks"""
        repack_cmd = [sys.executable, "tc-partner-repacks.py",
                      "-v", self.config['version'],
                      "-n", str(self.config['build_number'])]
        if self.config.get('platform'):
            repack_cmd.extend(["--platform", self.config['platform']])
        if self.config.get('partner'):
            repack_cmd.extend(["--partner", self.config['partner']])
        if self.config.get('taskIds'):
            for taskId in self.config['taskIds']:
                repack_cmd.extend(["--taskid", taskId])

        return self.run_command(repack_cmd,
                                cwd=self.query_abs_dirs()['abs_scripts_dir'])


# main {{{
if __name__ == '__main__':
    partner_repacks = DesktopPartnerRepacks()
    partner_repacks.run_and_exit()
