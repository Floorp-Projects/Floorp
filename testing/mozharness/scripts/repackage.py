import os
import sys

sys.path.insert(1, os.path.dirname(sys.path[0]))  # noqa - don't warn about imports

from mozharness.base.log import FATAL
from mozharness.base.script import BaseScript
from mozharness.mozilla.mock import ERROR_MSGS


class Repackage(BaseScript):

    config_options = [[
        ['--signed-input', ],
        {"action": "store",
         "dest": "signed_input",
         "type": "string",
         "default": os.environ.get('SIGNED_INPUT'),
         "help": "Specify the signed input (url)"}
    ], [
        ['--output-file', ],
        {"action": "store",
         "dest": "output_file",
         "type": "string",
         "help": "Specify the output filename"}
    ]]

    def __init__(self, require_config_file=False):
        script_kwargs = {
            'all_actions': [
                "download_input",
                "setup",
                "repackage",
            ],
        }
        BaseScript.__init__(
            self,
            config_options=self.config_options,
            require_config_file=require_config_file,
            **script_kwargs
        )

        # Assert we have it either passed in or in environment
        assert self.config.get('signed_input'), \
            "Must pass --signed-input or be set in the environment as SIGNED_INPUT"

    def download_input(self):
        config = self.config

        url = config['signed_input']
        status = self.download_file(url=url,
                                    file_name=config['input_filename'],
                                    parent_dir=config['input_home'])
        if not status:
            self.fatal("Unable to fetch signed input from %s" % config['signed_input'])

    def setup(self):
        self._run_tooltool()
        self._get_mozconfig()
        self._run_configure()

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(Repackage, self).query_abs_dirs()
        for directory in abs_dirs:
            value = abs_dirs[directory]
            abs_dirs[directory] = value
        dirs = {}
        dirs['abs_tools_dir'] = os.path.join(abs_dirs['abs_work_dir'], 'tools')
        dirs['abs_mozilla_dir'] = os.path.join(abs_dirs['abs_work_dir'], 'src')
        for key in dirs.keys():
            if key not in abs_dirs:
                abs_dirs[key] = dirs[key]
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def repackage(self):
        config = self.config
        dirs = self.query_abs_dirs()
        infile = os.path.join(config['input_home'], config['input_filename'])
        outfile = os.path.join(dirs['abs_upload_dir'], config['output_filename'])
        command = [sys.executable, 'mach', '--log-no-times', 'repackage',
                   'dmg',
                   '--input', infile,
                   '--output', outfile]
        return self.run_command(
            command=command,
            cwd=dirs['abs_mozilla_dir'],
            halt_on_failure=True,
        )

    def _run_tooltool(self):
        config = self.config
        dirs = self.query_abs_dirs()
        manifest_src = os.environ.get('TOOLTOOL_MANIFEST')
        if not manifest_src:
            manifest_src = config.get('tooltool_manifest_src')
        if not manifest_src:
            return self.warning(ERROR_MSGS['tooltool_manifest_undetermined'])
        fetch_script_path = os.path.join(dirs['abs_tools_dir'],
                                         'scripts/tooltool/tooltool_wrapper.sh')
        tooltool_manifest_path = os.path.join(dirs['abs_mozilla_dir'],
                                              manifest_src)
        cmd = [
            'sh',
            fetch_script_path,
            tooltool_manifest_path,
            config['tooltool_url'],
            config['tooltool_bootstrap'],
        ]
        cmd.extend(config['tooltool_script'])
        cache = config.get('tooltool_cache')
        if cache:
            cmd.extend(['-c', cache])
        self.info(str(cmd))
        self.run_command(cmd, cwd=dirs['abs_mozilla_dir'], halt_on_failure=True)

    def _get_mozconfig(self):
        """assign mozconfig."""
        c = self.config
        dirs = self.query_abs_dirs()
        abs_mozconfig_path = ''

        # first determine the mozconfig path
        if c.get('src_mozconfig'):
            self.info('Using in-tree mozconfig')
            abs_mozconfig_path = os.path.join(dirs['abs_mozilla_dir'], c['src_mozconfig'])
        else:
            self.fatal("'src_mozconfig' must be in the config "
                       "in order to determine the mozconfig.")

        # print its contents
        self.read_from_file(abs_mozconfig_path, error_level=FATAL)

        # finally, copy the mozconfig to a path that 'mach build' expects it to be
        self.copyfile(abs_mozconfig_path, os.path.join(dirs['abs_mozilla_dir'], '.mozconfig'))

    def _run_configure(self):
        dirs = self.query_abs_dirs()
        command = [sys.executable, 'mach', '--log-no-times', 'configure']
        return self.run_command(
            command=command,
            cwd=dirs['abs_mozilla_dir'],
            output_timeout=60*3,
            halt_on_failure=True,
        )


if __name__ == '__main__':
    repack = Repackage()
    repack.run_and_exit()
