# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Set up a browser environment before running a test.
"""
from __future__ import absolute_import, print_function

import os
import tempfile

import mozfile
import mozinfo
import mozrunner
from mozlog import get_proxy_logger
from mozprocess import ProcessHandlerMixin
from mozprofile.profile import Profile
from talos import utils
from talos.gecko_profile import GeckoProfile
from talos.utils import TalosError, run_in_debug_mode
from talos import heavy

LOG = get_proxy_logger()


class FFSetup(object):
    """
    Initialize the browser environment before running a test.

    This prepares:
     - the environment vars for running the test in the browser,
       available via the instance member *env*.
     - the profile used to run the test, available via the
       instance member *profile_dir*.
     - Gecko profiling, available via the instance member *gecko_profile*
       of type :class:`GeckoProfile` or None if not used.

    Note that the browser will be run once with the profile, to ensure
    this is basically working and negate any performance noise with the
    real test run (installing the profile the first time takes time).

    This class should be used as a context manager::

      with FFSetup(browser_config, test_config) as setup:
          # setup.env is initialized, and setup.profile_dir created
          pass
      # here the profile is removed
    """

    def __init__(self, browser_config, test_config):
        self.browser_config, self.test_config = browser_config, test_config
        self._tmp_dir = tempfile.mkdtemp()
        self.env = None
        # The profile dir must be named 'profile' because of xperf analysis
        # (in etlparser.py). TODO fix that ?
        self.profile_dir = os.path.join(self._tmp_dir, 'profile')
        self.gecko_profile = None
        self.debug_mode = run_in_debug_mode(browser_config)

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

        # downloading a profile instead of using the empty one
        if self.test_config['profile'] is not None:
            path = heavy.download_profile(self.test_config['profile'])
            self.test_config['profile_path'] = path

        profile_path = os.path.normpath(self.test_config['profile_path'])
        LOG.info("Cloning profile located at %s" % profile_path)

        def _feedback(directory, content):
            # Called by shutil.copytree on each visited directory.
            # Used here to display info.
            #
            # Returns the items that should be ignored by
            # shutil.copytree when copying the tree, so always returns
            # an empty list.
            sub = directory.split(profile_path)[-1].lstrip("/")
            if sub:
                LOG.info("=> %s" % sub)
            return []

        profile = Profile.clone(profile_path,
                                self.profile_dir,
                                ignore=_feedback,
                                restore=False)

        profile.set_preferences(preferences)

        # installing addons
        LOG.info("Installing Add-ons")
        profile.addon_manager.install_addons(extensions)

        # installing webextensions
        webextensions = self.test_config.get('webextensions', None)
        if isinstance(webextensions, basestring):
            webextensions = [webextensions]

        if webextensions is not None:
            LOG.info("Installing Webextensions")
            for webext in webextensions:
                filename = utils.interpolate(webext)
                if mozinfo.os == 'win':
                    filename = filename.replace('/', '\\')
                if not filename.endswith('.xpi'):
                    continue
                if not os.path.exists(filename):
                    continue

                profile.addon_manager.install_from_path(filename)

    def _run_profile(self):
        runner_cls = mozrunner.runners.get(
            mozinfo.info.get(
                'appname',
                'firefox'),
            mozrunner.Runner)
        args = [self.browser_config["extra_args"], self.browser_config["init_url"]]
        runner = runner_cls(profile=self.profile_dir,
                            binary=self.browser_config["browser_path"],
                            cmdargs=args,
                            env=self.env,
                            process_class=ProcessHandlerMixin,
                            process_args={})

        runner.start(outputTimeout=30)
        proc = runner.process_handler
        LOG.process_start(proc.pid, "%s %s" % (self.browser_config["browser_path"],
                                               ' '.join(args)))

        try:
            exit_code = proc.wait()
        except Exception:
            proc.kill()
            raise TalosError("Browser Failed to close properly during warmup")

        LOG.process_exit(proc.pid, exit_code)

    def _init_gecko_profile(self):
        upload_dir = os.getenv('MOZ_UPLOAD_DIR')
        if self.test_config.get('gecko_profile') and not upload_dir:
            LOG.critical("Profiling ignored because MOZ_UPLOAD_DIR was not"
                         " set")
        if upload_dir and self.test_config.get('gecko_profile'):
            self.gecko_profile = GeckoProfile(upload_dir,
                                              self.browser_config,
                                              self.test_config)
            self.gecko_profile.update_env(self.env)

    def clean(self):
        try:
            mozfile.remove(self._tmp_dir)
        except Exception as e:
            print("Exception while removing profile directory: %s" % self._tmp_dir)
            print(e)

        if self.gecko_profile:
            self.gecko_profile.clean()

    def __enter__(self):
        LOG.info('Initialising browser for %s test...'
                 % self.test_config['name'])
        self._init_env()
        self._init_profile()
        try:
            if not self.debug_mode and self.test_config['name'] != "damp":
                self._run_profile()
        except:
            self.clean()
            raise
        self._init_gecko_profile()
        LOG.info('Browser initialized.')
        return self

    def __exit__(self, type, value, tb):
        self.clean()
