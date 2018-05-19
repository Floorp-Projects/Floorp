import os
import sys

from mozharness.base.log import INFO


# BalrogMixin {{{1
class BalrogMixin(object):

    def query_python(self):
        return sys.executable

    def generate_balrog_props(self, props_path):
        balrog_props = {}
        balrog_props.update(self.buildbot_properties)

        balrog_props['hashType'] = self.config.get("hash_type", "sha512")
        if self.config.get('stage_platform'):
            balrog_props['stage_platform'] = self.config['stage_platform']
        if self.config.get('platform'):
            balrog_props['platform'] = self.config['platform']
        if self.config.get('balrog_platform'):
            balrog_props["platform"] = self.config['balrog_platform']
        if "branch" not in balrog_props:
            balrog_props["branch"] = self.branch

        self.dump_config(props_path, {"properties": balrog_props})

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
