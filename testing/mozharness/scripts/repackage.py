import os
import sys

sys.path.insert(1, os.path.dirname(sys.path[0]))  # noqa - don't warn about imports

from mozharness.base.log import FATAL
from mozharness.base.script import BaseScript
from mozharness.mozilla.mock import ERROR_MSGS


class Repackage(BaseScript):

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
            require_config_file=require_config_file,
            **script_kwargs
        )

    def download_input(self):
        config = self.config
        dirs = self.query_abs_dirs()

        input_home = config['input_home'].format(**dirs)

        for path, url in config["download_config"].items():
            status = self.download_file(url=url,
                                        file_name=path,
                                        parent_dir=input_home)
            if not status:
                self.fatal("Unable to fetch signed input from %s" % url)

            if 'mar' in path:
                # Ensure mar is executable
                self.chmod(os.path.join(input_home, path), 0755)

    def setup(self):
        self._run_tooltool()
        self._get_mozconfig()
        self._run_configure()

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(Repackage, self).query_abs_dirs()
        config = self.config
        for directory in abs_dirs:
            value = abs_dirs[directory]
            abs_dirs[directory] = value
        dirs = {}
        dirs['abs_tools_dir'] = os.path.join(abs_dirs['abs_work_dir'], 'tools')
        dirs['abs_mozilla_dir'] = os.path.join(abs_dirs['abs_work_dir'], 'src')
        locale_dir = ''
        if config.get('locale'):
            locale_dir = "{}{}".format(os.path.sep, config['locale'])
        dirs['output_home'] = config['output_home'].format(locale=locale_dir, **abs_dirs)

        for key in dirs.keys():
            if key not in abs_dirs:
                abs_dirs[key] = dirs[key]
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def repackage(self):
        config = self.config
        dirs = self.query_abs_dirs()

        # Make sure the upload dir is around.
        self.mkdir_p(dirs['output_home'])

        for repack_config in config["repackage_config"]:
            command = [sys.executable, 'mach', '--log-no-times', 'repackage'] + \
                [arg.format(**dirs)
                    for arg in list(repack_config)]
            self.run_command(
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

        tooltool_manifest_path = os.path.join(dirs['abs_mozilla_dir'],
                                              manifest_src)
        cmd = [
            sys.executable, '-u',
            os.path.join(dirs['abs_mozilla_dir'], 'mach'),
            'artifact',
            'toolchain',
            '-v',
            '--retry', '4',
            '--tooltool-manifest',
            tooltool_manifest_path,
            '--tooltool-url',
            config['tooltool_url'],
        ]
        auth_file = self._get_tooltool_auth_file()
        if auth_file:
            cmd.extend(['--authentication-file', auth_file])
        cache = config.get('tooltool_cache')
        if cache:
            cmd.extend(['--cache-dir', cache])
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
