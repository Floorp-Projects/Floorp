from itertools import chain
import os
import sys

from mozharness.base.log import INFO


# BalrogMixin {{{1
class BalrogMixin(object):

    def query_python(self):
        python = sys.executable
        # A mock environment is a special case, the system python isn't
        # available there
        if 'mock_target' in self.config:
            python = 'python2.7'
        return python


    def generate_balrog_props(self, props_path):
        self.set_buildbot_property(
            "hashType", self.config.get("hash_type", "sha512"), write_to_file=True
        )

        if self.buildbot_config and "properties" in self.buildbot_config:
            buildbot_properties = self.buildbot_config["properties"].items()
        else:
            buildbot_properties = []

        balrog_props = dict(properties=dict(chain(
            buildbot_properties,
            self.buildbot_properties.items(),
        )))
        if self.config.get('stage_platform'):
            balrog_props['properties']['stage_platform'] = self.config['stage_platform']
        if self.config.get('platform'):
            balrog_props['properties']['platform'] = self.config['platform']
        if self.config.get('balrog_platform'):
            balrog_props["properties"]["platform"] = self.config['balrog_platform']
        if "branch" not in balrog_props["properties"]:
            balrog_props["properties"]["branch"] = self.branch

        self.dump_config(props_path, balrog_props)

    def lock_balrog_rules(self, rule_ids):
        c = self.config
        dirs = self.query_abs_dirs()
        submitter_script = os.path.join(
            dirs["abs_tools_dir"], "scripts", "updates",
            "balrog-nightly-locker.py"
        )
        credentials_file = os.path.join(
            dirs["base_work_dir"], c["balrog_credentials_file"]
        )

        cmd = [
            self.query_python(),
            submitter_script,
            "--credentials-file", credentials_file,
            "--api-root", c["balrog_api_root"],
            "--username", c["balrog_username"],
        ]
        for r in rule_ids:
            cmd.extend(["-r", str(r)])

        if self._log_level_at_least(INFO):
            cmd.append("--verbose")

        cmd.append("lock")

        self.info("Calling Balrog rule locking script.")
        self.retry(self.run_command, attempts=5, args=cmd,
                   kwargs={"halt_on_failure": True})
