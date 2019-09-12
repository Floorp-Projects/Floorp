import os
import sys

sys.path.insert(1, os.path.dirname(sys.path[0]))  # noqa - don't warn about imports

from mozharness.base.log import FATAL
from mozharness.base.script import BaseScript


class Repackage(BaseScript):

    def __init__(self, require_config_file=False):
        script_kwargs = {
            'all_actions': [
                "setup",
                "repackage",
            ],
        }
        BaseScript.__init__(
            self,
            require_config_file=require_config_file,
            **script_kwargs
        )

    def setup(self):
        dirs = self.query_abs_dirs()

        self._run_tooltool()

        mar_path = os.path.join(dirs['abs_input_dir'], 'mar')
        if self._is_windows():
            mar_path += '.exe'
        if mar_path and os.path.exists(mar_path):
            self.chmod(mar_path, 0o755)
        if self.config.get("run_configure", True):
            self._get_mozconfig()
            self._run_configure()

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(Repackage, self).query_abs_dirs()
        config = self.config

        dirs = {}
        dirs['abs_tools_dir'] = os.path.join(abs_dirs['abs_work_dir'], 'tools')
        dirs['abs_mozilla_dir'] = os.path.join(abs_dirs['abs_work_dir'], 'src')
        dirs['abs_input_dir'] = os.path.join(abs_dirs['base_work_dir'], 'fetches')
        output_dir_suffix = []
        if config.get('locale'):
            output_dir_suffix.append(config['locale'])
        if config.get('repack_id'):
            output_dir_suffix.append(config['repack_id'])
        dirs['abs_output_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'outputs', *output_dir_suffix)
        for key in dirs.keys():
            if key not in abs_dirs:
                abs_dirs[key] = dirs[key]
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def repackage(self):
        config = self.config
        dirs = self.query_abs_dirs()

        subst = {
            'package-name': config['package-name'],
            # sfx-stub is only defined for Windows targets
            'sfx-stub': config.get('sfx-stub'),
            'installer-tag': config['installer-tag'],
            'stub-installer-tag': config['stub-installer-tag'],
            'wsx-stub': config['wsx-stub'],
        }
        subst.update(dirs)
        if config.get('fetch-dir'):
            subst.update({'fetch-dir': os.path.abspath(config['fetch-dir'])})

        # Make sure the upload dir is around.
        self.mkdir_p(dirs['abs_output_dir'])

        for repack_config in config["repackage_config"]:
            command = [sys.executable, 'mach', '--log-no-times', 'repackage']
            command.extend([arg.format(**subst) for arg in repack_config['args']])
            for arg, filename in repack_config['inputs'].items():
                command.extend([
                    '--{}'.format(arg),
                    os.path.join(dirs['abs_input_dir'], filename),
                ])
            command.extend([
                '--output', os.path.join(dirs['abs_output_dir'], repack_config['output']),
            ])
            self.run_command(
                command=command,
                cwd=dirs['abs_mozilla_dir'],
                halt_on_failure=True,
                env=self.query_env(),
            )

    def _run_tooltool(self):
        config = self.config
        dirs = self.query_abs_dirs()
        toolchains = os.environ.get('MOZ_TOOLCHAINS')
        manifest_src = os.environ.get('TOOLTOOL_MANIFEST')
        if not manifest_src:
            manifest_src = config.get('tooltool_manifest_src')
        if not manifest_src and not toolchains:
            return

        cmd = [
            sys.executable, '-u',
            os.path.join(dirs['abs_mozilla_dir'], 'mach'),
            'artifact',
            'toolchain',
            '-v',
            '--retry', '4',
            '--artifact-manifest',
            os.path.join(dirs['abs_mozilla_dir'], 'toolchains.json'),
        ]
        if manifest_src:
            cmd.extend([
                '--tooltool-manifest',
                os.path.join(dirs['abs_mozilla_dir'], manifest_src),
                '--tooltool-url',
                config['tooltool_url'],
            ])
            auth_file = self._get_tooltool_auth_file()
            if auth_file:
                cmd.extend(['--authentication-file', auth_file])
        cache = config.get('tooltool_cache')
        if cache:
            cmd.extend(['--cache-dir', cache])
        if toolchains:
            cmd.extend(toolchains.split())
        self.info(str(cmd))
        self.run_command(cmd, cwd=dirs['abs_mozilla_dir'], halt_on_failure=True)

    def _get_tooltool_auth_file(self):
        # set the default authentication file based on platform; this
        # corresponds to where puppet puts the token
        if 'tooltool_authentication_file' in self.config:
            fn = self.config['tooltool_authentication_file']
        elif self._is_windows():
            fn = r'c:\builds\relengapi.tok'
        else:
            fn = '/builds/relengapi.tok'

        # if the file doesn't exist, don't pass it to tooltool (it will just
        # fail).  In taskcluster, this will work OK as the relengapi-proxy will
        # take care of auth.  Everywhere else, we'll get auth failures if
        # necessary.
        if os.path.exists(fn):
            return fn

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
