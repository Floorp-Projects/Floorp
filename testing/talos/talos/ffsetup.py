# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Set up a browser environment before running a test.
"""

import os
import re
import tempfile
import mozfile
from mozprocess import ProcessHandler
from mozprofile.profile import Profile

from mozlog import get_proxy_logger

from talos import utils
from talos.utils import TalosError
from talos.sps_profile import SpsProfile


LOG = get_proxy_logger()


class FFSetup(object):
    """
    Initialize the browser environment before running a test.

    This prepares:
     - the environment vars for running the test in the browser,
       available via the instance member *env*.
     - the profile used to run the test, available via the
       instance member *profile_dir*.
     - sps profiling, available via the instance member *sps_profile*
       of type :class:`SpsProfile` or None if not used.

    Note that the browser will be run once with the profile, to ensure
    this is basically working and negate any performance noise with the
    real test run (installing the profile the first time takes time).

    This class should be used as a context manager::

      with FFSetup(browser_config, test_config) as setup:
          # setup.env is initialized, and setup.profile_dir created
          pass
      # here the profile is removed
    """

    PROFILE_REGEX = re.compile('__metrics(.*)__metrics',
                               re.DOTALL | re.MULTILINE)

    def __init__(self, browser_config, test_config):
        self.browser_config, self.test_config = browser_config, test_config
        self._tmp_dir = tempfile.mkdtemp()
        self.env = None
        # The profile dir must be named 'profile' because of xperf analysis
        # (in etlparser.py). TODO fix that ?
        self.profile_dir = os.path.join(self._tmp_dir, 'profile')
        self.sps_profile = None

    def _init_env(self):
        self.env = dict(os.environ)
        for k, v in self.browser_config['env'].iteritems():
            self.env[k] = str(v)
        self.env['MOZ_CRASHREPORTER_NO_REPORT'] = '1'
        if self.browser_config['symbols_path']:
            self.env['MOZ_CRASHREPORTER'] = '1'
        else:
            self.env['MOZ_CRASHREPORTER_DISABLE'] = '1'

        self.env['MOZ_DISABLE_NONLOCAL_CONNECTIONS'] = '1'

        self.env["LD_LIBRARY_PATH"] = \
            os.path.dirname(self.browser_config['browser_path'])

    def _init_profile(self):
        preferences = dict(self.browser_config['preferences'])
        if self.test_config.get('preferences'):
            test_prefs = dict(
                [(i, utils.parse_pref(j))
                 for i, j in self.test_config['preferences'].items()]
            )
            preferences.update(test_prefs)
        # interpolate webserver value in prefs
        webserver = self.browser_config['webserver']
        if '://' not in webserver:
            webserver = 'http://' + webserver
        for name, value in preferences.items():
            if type(value) is str:
                value = utils.interpolate(value, webserver=webserver)
                preferences[name] = value

        extensions = self.browser_config['extensions'][:]
        if self.test_config.get('extensions'):
            extensions.append(self.test_config['extensions'])

        if self.browser_config['develop'] or \
           self.browser_config['branch_name'] == 'Try':
            extensions = [os.path.dirname(i) for i in extensions]

        profile = Profile.clone(
            os.path.normpath(self.test_config['profile_path']),
            self.profile_dir,
            restore=False)

        profile.set_preferences(preferences)
        profile.addon_manager.install_addons(extensions)

    def _run_profile(self):
        command_args = utils.GenerateBrowserCommandLine(
            self.browser_config["browser_path"],
            self.browser_config["extra_args"],
            self.profile_dir,
            self.browser_config["init_url"]
        )

        def browser_log(line):
            LOG.process_output(browser.pid, line)

        browser = ProcessHandler(command_args, env=self.env,
                                 processOutputLine=browser_log)
        browser.run()
        LOG.process_start(browser.pid, ' '.join(command_args))
        try:
            exit_code = browser.wait()
        except KeyboardInterrupt:
            browser.kill()
            raise

        LOG.process_exit(browser.pid, exit_code)
        results_raw = '\n'.join(browser.output)

        if not self.PROFILE_REGEX.search(results_raw):
            LOG.info("Could not find %s in browser output"
                     % self.PROFILE_REGEX.pattern)
            LOG.info("Raw results:%s" % results_raw)
            raise TalosError("browser failed to close after being initialized")

    def _init_sps_profile(self):
        upload_dir = os.getenv('MOZ_UPLOAD_DIR')
        if self.test_config.get('sps_profile') and not upload_dir:
            LOG.critical("Profiling ignored because MOZ_UPLOAD_DIR was not"
                         " set")
        if upload_dir and self.test_config.get('sps_profile'):
            self.sps_profile = SpsProfile(upload_dir,
                                          self.browser_config,
                                          self.test_config)
            self.sps_profile.update_env(self.env)

    def clean(self):
        mozfile.remove(self._tmp_dir)
        if self.sps_profile:
            self.sps_profile.clean()

    def __enter__(self):
        LOG.info('Initialising browser for %s test...'
                 % self.test_config['name'])
        self._init_env()
        self._init_profile()
        try:
            self._run_profile()
        except:
            self.clean()
            raise
        self._init_sps_profile()
        LOG.info('Browser initialized.')
        return self

    def __exit__(self, type, value, tb):
        self.clean()
