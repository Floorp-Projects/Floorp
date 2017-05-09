"""module for tooltool operations"""
import os
import sys

from mozharness.base.errors import PythonErrorList
from mozharness.base.log import ERROR, FATAL
from mozharness.mozilla.proxxy import Proxxy

TooltoolErrorList = PythonErrorList + [{
    'substr': 'ERROR - ', 'level': ERROR
}]


TOOLTOOL_PY_URL = \
    "https://raw.githubusercontent.com/mozilla/build-tooltool/master/tooltool.py"

TOOLTOOL_SERVERS = [
    'https://api.pub.build.mozilla.org/tooltool/',
]


class TooltoolMixin(object):
    """Mixin class for handling tooltool manifests.
    To use a tooltool server other than the Mozilla server, override
    config['tooltool_servers'].  To specify a different authentication
    file than that used in releng automation,override
    config['tooltool_authentication_file']; set it to None to not pass
    any authentication information (OK for public files)
    """
    def _get_auth_file(self):
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

    def tooltool_fetch(self, manifest,
                       output_dir=None, privileged=False, cache=None):
        """docstring for tooltool_fetch"""
        for d in (output_dir, cache):
            if d is not None and not os.path.exists(d):
                self.mkdir_p(d)
        # Use vendored tooltool.py if available.
        if self.topsrcdir:
            cmd = [
                sys.executable, '-u',
                os.path.join(self.topsrcdir, 'mach'),
                'artifact',
                'toolchain',
                '-v',
            ]
        elif self.config.get("download_tooltool"):
            cmd = [sys.executable, self._fetch_tooltool_py()]
        else:
            cmd = self.query_exe('tooltool.py', return_type='list')

        # get the tooltool servers from configuration
        default_urls = self.config.get('tooltool_servers', TOOLTOOL_SERVERS)

        # add slashes (bug 1155630)
        def add_slash(url):
            return url if url.endswith('/') else (url + '/')
        default_urls = [add_slash(u) for u in default_urls]

        # proxxy-ify
        proxxy = Proxxy(self.config, self.log_obj)
        proxxy_urls = proxxy.get_proxies_and_urls(default_urls)

        for proxyied_url in proxxy_urls:
            cmd.extend(['--tooltool-url' if self.topsrcdir else '--url', proxyied_url])

        # handle authentication file, if given
        auth_file = self._get_auth_file()
        if auth_file and os.path.exists(auth_file):
            cmd.extend(['--authentication-file', auth_file])

        if self.topsrcdir:
            cmd.extend(['--tooltool-manifest', manifest])
        else:
            cmd.extend(['fetch', '-m', manifest, '-o'])

        if cache:
            cmd.extend(['--cache-dir' if self.topsrcdir else '-c', cache])

        # when mock is enabled run tooltool in mock. We can't use
        # run_command_m in all cases because it won't exist unless
        # MockMixin is used on the parent class
        if self.config.get('mock_target'):
            cmd_runner = self.run_command_m
        else:
            cmd_runner = self.run_command

        timeout = self.config.get('tooltool_timeout', 10 * 60)

        self.retry(
            cmd_runner,
            args=(cmd, ),
            kwargs={'cwd': output_dir,
                    'error_list': TooltoolErrorList,
                    'privileged': privileged,
                    'output_timeout': timeout,
                    },
            good_statuses=(0, ),
            error_message="Tooltool %s fetch failed!" % manifest,
            error_level=FATAL,
        )

    def _fetch_tooltool_py(self):
        """ Retrieve tooltool.py
        """
        dirs = self.query_abs_dirs()
        file_path = os.path.join(dirs['abs_work_dir'], "tooltool.py")
        self.download_file(TOOLTOOL_PY_URL, file_path)
        if not os.path.exists(file_path):
            self.fatal("We can't get tooltool.py")
        self.chmod(file_path, 0755)
        return file_path

    def create_tooltool_manifest(self, contents, path=None):
        """ Currently just creates a manifest, given the contents.
        We may want a template and individual values in the future?
        """
        if path is None:
            dirs = self.query_abs_dirs()
            path = os.path.join(dirs['abs_work_dir'], 'tooltool.tt')
        self.write_to_file(path, contents, error_level=FATAL)
        return path
