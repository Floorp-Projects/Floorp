# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import copy
import os
import sys
import time

# load modules from parent dir
sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))

from mozharness.base.log import INFO, ERROR, WARNING, FATAL
from mozharness.base.script import PreScriptAction
from mozharness.base.transfer import TransferMixin
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.blob_upload import BlobUploadMixin, blobupload_config_options
from mozharness.mozilla.buildbot import TBPL_SUCCESS, TBPL_WARNING, TBPL_FAILURE
from mozharness.mozilla.gaia import GaiaMixin, gaia_config_options
from mozharness.mozilla.testing.testbase import TestingMixin, testing_config_options
from mozharness.mozilla.proxxy import Proxxy


class GaiaTest(MercurialScript, TestingMixin, BlobUploadMixin, GaiaMixin, TransferMixin):
    config_options = [[
        ["--application"],
        {"action": "store",
         "dest": "application",
         "default": "b2g",
         "help": "application binary name"
         }
    ], [
        ["--browser-arg"],
        {"action": "store",
         "dest": "browser_arg",
         "default": None,
         "help": "optional command-line argument to pass to the browser"
         }
    ], [
        ["--xre-path"],
        {"action": "store",
         "dest": "xre_path",
         "default": "xulrunner-sdk",
         "help": "directory (relative to gaia repo) of xulrunner-sdk"
         }
    ], [
        ["--xre-url"],
        {"action": "store",
         "dest": "xre_url",
         "default": None,
         "help": "url of desktop xre archive"
         }
    ], [
        ["--npm-registry"],
        {"action": "store",
         "dest": "npm_registry",
         "default": "http://npm-mirror.pub.build.mozilla.org",
         "help": "where to go for node packages"
         }
    ], [
        ["--total-chunks"],
        {"action": "store",
         "dest": "total_chunks",
         "help": "Number of total chunks",
         }
    ], [
        ["--this-chunk"],
        {"action": "store",
         "dest": "this_chunk",
         "help": "Number of this chunk",
         }
    ]] + copy.deepcopy(testing_config_options) + \
        copy.deepcopy(blobupload_config_options) + \
        copy.deepcopy(gaia_config_options)

    error_list = [
        {'substr': 'FAILED (errors=', 'level': WARNING},
    ]

    virtualenv_modules = []

    repos = []

    def __init__(self, require_config_file=False):
        super(GaiaTest, self).__init__(
            config_options=self.config_options,
            all_actions=['clobber',
                         'read-buildbot-config',
                         'pull',
                         'download-and-extract',
                         'create-virtualenv',
                         'install',
                         'run-tests'],
            default_actions=['clobber',
                             'pull',
                             'download-and-extract',
                             'create-virtualenv',
                             'install',
                             'run-tests'],
            require_config_file=require_config_file,
            config={'virtualenv_modules': self.virtualenv_modules,
                    'repos': self.repos,
                    'require_test_zip': True})

        # these are necessary since self.config is read only
        c = self.config
        self.installer_url = c.get('installer_url')
        self.installer_path = c.get('installer_path')
        self.binary_path = c.get('binary_path')
        self.test_url = c.get('test_url')
        self.test_packages_url = c.get('test_packages_url')

    def pull(self, **kwargs):
        GaiaMixin.pull(self, **kwargs)

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(GaiaTest, self).query_abs_dirs()
        dirs = {}
        gaia_root_dir = self.config.get('gaia_dir')
        if not gaia_root_dir:
            gaia_root_dir = self.config['base_work_dir']
        dirs['abs_gaia_dir'] = os.path.join(gaia_root_dir, 'gaia')
        dirs['abs_runner_dir'] = os.path.join(dirs['abs_gaia_dir'],
                                              'tests', 'python', 'gaia-unit-tests')
        dirs['abs_test_install_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'tests')
        dirs['abs_blob_upload_dir'] = os.path.join(abs_dirs['abs_work_dir'], 'blobber_upload_dir')

        for key in dirs.keys():
            if key not in abs_dirs:
                abs_dirs[key] = dirs[key]
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    @PreScriptAction('create-virtualenv')
    def _pre_create_virtualenv(self, action):

        dirs = self.query_abs_dirs()
        requirements = os.path.join(dirs['abs_test_install_dir'],
                                    'config',
                                    'mozbase_requirements.txt')
        if os.access(requirements, os.F_OK):
            self.register_virtualenv_module(requirements=[requirements],
                                            two_pass=True)
        else:
            self.fatal('mozbase_requirements.txt not found!')

        self.register_virtualenv_module(
            'gaia-unit-tests', url=dirs['abs_runner_dir'])

    def _build_arg(self, option, value):
        """
        Build a command line argument
        """
        if not value:
            return []
        return [str(option), str(value)]

    def _query_proxxy(self):
        if not self.proxxy:
            # we don't have a proxxy object. Create it.
            # By default, Proxxy accepts a full configuration looks for the
            # 'proxxy' element. If 'proxxy' is not defined it uses PROXXY_CONFIG
            # For GaiaTest, if there's no proxxy element, don't use a proxxy at
            # all. To do this we must pass a special configuraion
            proxxy_conf = {'proxxy': self.config.get('proxxy', {})}
            proxxy = Proxxy(proxxy_conf, self.log_obj)
            self.proxxy = proxxy
        return self.proxxy

    def _retry_download(self, url, file_name, error_level=FATAL, retry_config=None):
        if self.config.get("bypass_download_cache"):
            n = 0
            # ignore retry_config in this case
            max_attempts = 5
            sleeptime = 60

            while n < max_attempts:
                n += 1
                try:
                    _url = "%s?rand=%s" % (url, time.strftime("%Y%m%d%H%M%S"))
                    self.info("Trying %s..." % _url)
                    status = self._download_file(_url, file_name)
                    return status
                except Exception:
                    if n >= max_attempts:
                        self.log("Can't download from %s to %s!" % (url, file_name),
                                 level=error_level, exit_code=3)
                        return None
                    self.info("Sleeping %s before retrying..." % sleeptime)
                    time.sleep(sleeptime)
        else:
            # Since we're overwritting _retry_download() we can't call download_file() directly
            return super(GaiaTest, self)._retry_download(
                url=url, file_name=file_name, error_level=error_level, retry_config=retry_config,
            )

    def run_tests(self):
        """
        Run the test suite.
        """
        pass

    def publish(self, code, passed=None, failed=None):
        """
        Publish the results of the test suite.
        """
        if code == 0:
            level = INFO
            # We used to check for passed == 0 as well but it can
            # be normal to end up with empty chunks especially when
            # large amounts of tests are disabled.
            if failed > 0:
                status = 'test failures'
                tbpl_status = TBPL_WARNING
            else:
                status = 'success'
                tbpl_status = TBPL_SUCCESS
        elif code == 10:
            level = INFO
            status = 'test failures'
            tbpl_status = TBPL_WARNING
        else:
            level = ERROR
            status = 'harness failures'
            tbpl_status = TBPL_FAILURE

        self.log('Tests exited with return code %s: %s' % (code, status),
                 level=level)
        self.buildbot_status(tbpl_status)
