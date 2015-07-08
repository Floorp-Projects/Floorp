"""module for tooltool operations"""
import os
import sys

from mozharness.base.errors import PythonErrorList
from mozharness.base.log import ERROR, FATAL
from mozharness.mozilla.proxxy import Proxxy

from mozharness.lib.python.authentication import get_credentials_path

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
            return self.config['tooltool_authentication_file']

        if self._is_windows():
            return r'c:\builds\relengapi.tok'
        else:
            return '/builds/relengapi.tok'

    def tooltool_fetch(self, manifest, bootstrap_cmd=None,
                       output_dir=None, privileged=False, cache=None):
        """docstring for tooltool_fetch"""
        tooltool = self.query_exe('tooltool.py', return_type='list')

        if self.config.get("developer_mode"):
            tooltool = [bin for bin in tooltool if os.path.exists(bin)]
            if tooltool:
                cmd = [tooltool[0]]
            else:
                cmd = [self._fetch_tooltool_py()]
        else:
            cmd = tooltool

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
            cmd.extend(['--url', proxyied_url])

        # handle authentication file, if given
        auth_file = self._get_auth_file()
        if auth_file and os.path.exists(auth_file):
            cmd.extend(['--authentication-file', auth_file])

        cmd.extend(['fetch', '-m', manifest, '-o'])

        if cache:
            cmd.extend(['-c', cache])

        self.retry(
            self.run_command,
            args=(cmd, ),
            kwargs={'cwd': output_dir,
                    'error_list': TooltoolErrorList,
                    'privileged': privileged,
                    },
            good_statuses=(0, ),
            error_message="Tooltool %s fetch failed!" % manifest,
            error_level=FATAL,
        )
        if bootstrap_cmd is not None:
            error_message = "Tooltool bootstrap %s failed!" % str(bootstrap_cmd)
            self.retry(
                self.run_command,
                args=(bootstrap_cmd, ),
                kwargs={'cwd': output_dir,
                        'error_list': TooltoolErrorList,
                        'privileged': privileged,
                        },
                good_statuses=(0, ),
                error_message=error_message,
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
