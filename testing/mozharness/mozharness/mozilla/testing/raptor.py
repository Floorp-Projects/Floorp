# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import copy
import glob
import os
import re
import sys
import subprocess

from shutil import copyfile

import mozharness

from mozharness.base.errors import PythonErrorList
from mozharness.base.log import OutputParser, DEBUG, ERROR, CRITICAL, INFO
from mozharness.mozilla.testing.android import AndroidMixin
from mozharness.mozilla.testing.errors import HarnessErrorList
from mozharness.mozilla.testing.testbase import TestingMixin, testing_config_options
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.testing.codecoverage import (
    CodeCoverageMixin,
    code_coverage_config_options
)

scripts_path = os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__)))
external_tools_path = os.path.join(scripts_path, 'external_tools')
here = os.path.abspath(os.path.dirname(__file__))

RaptorErrorList = PythonErrorList + HarnessErrorList + [
    {'regex': re.compile(r'''run-as: Package '.*' is unknown'''), 'level': DEBUG},
    {'substr': r'''raptorDebug''', 'level': DEBUG},
    {'regex': re.compile(r'''^raptor[a-zA-Z-]*( - )?( )?(?i)error(:)?'''), 'level': ERROR},
    {'regex': re.compile(r'''^raptor[a-zA-Z-]*( - )?( )?(?i)critical(:)?'''), 'level': CRITICAL},
    {'regex': re.compile(r'''No machine_name called '.*' can be found'''), 'level': CRITICAL},
    {'substr': r"""No such file or directory: 'browser_output.txt'""",
     'level': CRITICAL,
     'explanation': "Most likely the browser failed to launch, or the test was otherwise "
     "unsuccessful in even starting."},
]


class Raptor(TestingMixin, MercurialScript, CodeCoverageMixin, AndroidMixin):
    """
    install and run raptor tests
    """

    # Options to browsertime.  Paths are expected to be absolute.
    browsertime_options = [
        [["--browsertime-node"], {
            "dest": "browsertime_node",
            "default": None,
            "help": argparse.SUPPRESS
        }],
        [["--browsertime-browsertimejs"], {
            "dest": "browsertime_browsertimejs",
            "default": None,
            "help": argparse.SUPPRESS
        }],
        [["--browsertime-chromedriver"], {
            "dest": "browsertime_chromedriver",
            "default": None,
            "help": argparse.SUPPRESS
        }],
        [["--browsertime-geckodriver"], {
            "dest": "browsertime_geckodriver",
            "default": None,
            "help": argparse.SUPPRESS
        }],
        [["--browsertime"], {
            "dest": "browsertime",
            "action": "store_true",
            "default": False,
            "help": argparse.SUPPRESS
        }],
    ]

    config_options = [
        [["--test"],
         {"action": "store",
          "dest": "test",
          "help": "Raptor test to run"
          }],
        [["--app"],
         {"default": "firefox",
          "choices": ["firefox", "chrome", "chromium", "fennec", "geckoview", "refbrow", "fenix"],
          "dest": "app",
          "help": "name of the application we are testing (default: firefox)"
          }],
        [["--activity"],
         {"dest": "activity",
          "help": "the android activity used to launch the android app. "
                  "ex: org.mozilla.fenix.browser.BrowserPerformanceTestActivity"
          }],
        [["--intent"],
         {"dest": "intent",
          "help": "name of the android intent action used to launch the android app"
          }],
        [["--is-release-build"],
         {"action": "store_true",
          "dest": "is_release_build",
          "help": "Whether the build is a release build which requires work arounds "
                  "using MOZ_DISABLE_NONLOCAL_CONNECTIONS to support installing unsigned "
                  "webextensions. Defaults to False."
          }],
        [["--add-option"],
         {"action": "extend",
          "dest": "raptor_cmd_line_args",
          "default": None,
          "help": "extra options to raptor"
          }],
        [["--enable-webrender"], {
            "action": "store_true",
            "dest": "enable_webrender",
            "default": False,
            "help": "Enable the WebRender compositor in Gecko.",
        }],
        [["--geckoProfile"], {
            "dest": "gecko_profile",
            "action": "store_true",
            "default": False,
            "help": argparse.SUPPRESS
        }],
        [["--geckoProfileInterval"], {
            "dest": "gecko_profile_interval",
            "type": "int",
            "help": argparse.SUPPRESS
        }],
        [["--geckoProfileEntries"], {
            "dest": "gecko_profile_entries",
            "type": "int",
            "help": argparse.SUPPRESS
        }],
        [["--gecko-profile"], {
            "dest": "gecko_profile",
            "action": "store_true",
            "default": False,
            "help": "Whether or not to profile the test run and save the profile results"
        }],
        [["--gecko-profile-interval"], {
            "dest": "gecko_profile_interval",
            "type": "int",
            "help": "The interval between samples taken by the profiler (milliseconds)"
        }],
        [["--gecko-profile-entries"], {
            "dest": "gecko_profile_entries",
            "type": "int",
            "help": "How many samples to take with the profiler"
        }],
        [["--page-cycles"], {
            "dest": "page_cycles",
            "type": "int",
            "help": "How many times to repeat loading the test page (for page load tests); "
                    "for benchmark tests this is how many times the benchmark test will be run"
        }],
        [["--page-timeout"], {
            "dest": "page_timeout",
            "type": "int",
            "help": "How long to wait (ms) for one page_cycle to complete, before timing out"
        }],
        [["--browser-cycles"], {
            "dest": "browser_cycles",
            "type": "int",
            "help": "The number of times a cold load test is repeated (for cold load tests only, "
                    "where the browser is shutdown and restarted between test iterations)"
        }],
        [["--test-url-params"], {
            "action": "store",
            "dest": "test_url_params",
            "help": "Parameters to add to the test_url query string"
        }],
        [["--host"], {
            "dest": "host",
            "help": "Hostname from which to serve urls (default: 127.0.0.1). "
                    "The value HOST_IP will cause the value of host to be "
                    "to be loaded from the environment variable HOST_IP.",
        }],
        [["--power-test"], {
            "dest": "power_test",
            "action": "store_true",
            "default": False,
            "help": "Use Raptor to measure power usage. Currently only supported for Geckoview. "
                    "The host ip address must be specified either via the --host command line "
                    "argument.",
        }],
        [["--memory-test"], {
            "dest": "memory_test",
            "action": "store_true",
            "default": False,
            "help": "Use Raptor to measure memory usage.",
        }],
        [["--cpu-test"], {
            "dest": "cpu_test",
            "action": "store_true",
            "default": False,
            "help": "Use Raptor to measure CPU usage"
        }],
        [["--debug-mode"], {
            "dest": "debug_mode",
            "action": "store_true",
            "default": False,
            "help": "Run Raptor in debug mode (open browser console, limited page-cycles, etc.)",
        }],
        [["--noinstall"], {
            "dest": "noinstall",
            "action": "store_true",
            "default": False,
            "help": "Do not offer to install Android APK",
        }],
        [["--disable-e10s"], {
            "dest": "e10s",
            "action": "store_false",
            "default": True,
            "help": "Run without multiple processes (e10s).",
        }],

    ] + testing_config_options + \
        copy.deepcopy(code_coverage_config_options) + \
        browsertime_options

    def __init__(self, **kwargs):
        kwargs.setdefault('config_options', self.config_options)
        kwargs.setdefault('all_actions', ['clobber',
                                          'download-and-extract',
                                          'populate-webroot',
                                          'install-chromium-distribution',
                                          'create-virtualenv',
                                          'install',
                                          'run-tests',
                                          ])
        kwargs.setdefault('default_actions', ['clobber',
                                              'download-and-extract',
                                              'populate-webroot',
                                              'install-chromium-distribution',
                                              'create-virtualenv',
                                              'install',
                                              'run-tests',
                                              ])
        kwargs.setdefault('config', {})
        super(Raptor, self).__init__(**kwargs)

        self.workdir = self.query_abs_dirs()['abs_work_dir']  # convenience

        self.run_local = self.config.get('run_local')

        # app (browser testing on) defaults to firefox
        self.app = "firefox"

        if self.run_local:
            # raptor initiated locally, get app from command line args
            # which are passed in from mach inside 'raptor_cmd_line_args'
            # cmd line args can be in two formats depending on how user entered them
            # i.e. "--app=geckoview" or separate as "--app", "geckoview" so we have to
            # parse carefully.  It's simplest to use `argparse` to parse partially.
            self.app = "firefox"
            if 'raptor_cmd_line_args' in self.config:
                sub_parser = argparse.ArgumentParser()
                # It's not necessary to limit the allowed values: each value
                # will be parsed and verifed by raptor/raptor.py.
                sub_parser.add_argument('--app', default=None, dest='app')
                sub_parser.add_argument('-i', '--intent', default=None, dest='intent')
                sub_parser.add_argument('-a', '--activity', default=None, dest='activity')

                # We'd prefer to use `parse_known_intermixed_args`, but that's
                # new in Python 3.7.
                known, unknown = sub_parser.parse_known_args(self.config['raptor_cmd_line_args'])

                if known.app:
                    self.app = known.app
                if known.intent:
                    self.intent = known.intent
                if known.activity:
                    self.activity = known.activity
        else:
            # raptor initiated in production via mozharness
            self.test = self.config['test']
            self.app = self.config.get("app", "firefox")
            self.binary_path = self.config.get("binary_path", None)

            if self.app in ('refbrow', 'fenix'):
                self.app_name = self.binary_path

        self.installer_url = self.config.get("installer_url")
        self.raptor_json_url = self.config.get("raptor_json_url")
        self.raptor_json = self.config.get("raptor_json")
        self.raptor_json_config = self.config.get("raptor_json_config")
        self.repo_path = self.config.get("repo_path")
        self.obj_path = self.config.get("obj_path")
        self.test = None
        self.gecko_profile = self.config.get('gecko_profile') or \
            "--geckoProfile" in self.config.get("raptor_cmd_line_args", [])
        self.gecko_profile_interval = self.config.get('gecko_profile_interval')
        self.gecko_profile_entries = self.config.get('gecko_profile_entries')
        self.test_packages_url = self.config.get('test_packages_url')
        self.test_url_params = self.config.get('test_url_params')
        self.host = self.config.get('host')
        if self.host == 'HOST_IP':
            self.host = os.environ['HOST_IP']
        self.power_test = self.config.get('power_test')
        self.memory_test = self.config.get('memory_test')
        self.cpu_test = self.config.get('cpu_test')
        self.is_release_build = self.config.get('is_release_build')
        self.debug_mode = self.config.get('debug_mode', False)
        self.firefox_android_browsers = ["fennec", "geckoview", "refbrow", "fenix"]

        for (arg,), details in Raptor.browsertime_options:
            # Allow to override defaults on the `./mach raptor-test ...` command line.
            value = self.config.get(details['dest'])
            if value and arg not in self.config.get("raptor_cmd_line_args", []):
                setattr(self, details['dest'], value)

    # We accept some configuration options from the try commit message in the
    # format mozharness: <options>. Example try commit message: mozharness:
    # --geckoProfile try: <stuff>
    def query_gecko_profile_options(self):
        gecko_results = []
        # if gecko_profile is set, we add that to the raptor options
        if self.gecko_profile:
            gecko_results.append('--geckoProfile')
            if self.gecko_profile_interval:
                gecko_results.extend(
                    ['--geckoProfileInterval', str(self.gecko_profile_interval)]
                )
            if self.gecko_profile_entries:
                gecko_results.extend(
                    ['--geckoProfileEntries', str(self.gecko_profile_entries)]
                )
        return gecko_results

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(Raptor, self).query_abs_dirs()
        abs_dirs['abs_blob_upload_dir'] = os.path.join(abs_dirs['abs_work_dir'],
                                                       'blobber_upload_dir')
        abs_dirs['abs_test_install_dir'] = os.path.join(abs_dirs['abs_work_dir'], 'tests')

        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def install_chromium_distribution(self):
        '''install Google Chromium distribution in production; installation
        requirements depend on the platform'''
        linux, mac, win = 'linux', 'mac', 'win'
        chrome, chromium = 'chrome', 'chromium'

        available_chromium_dists = [chrome, chromium]
        binary_location = {
            chromium: {
                linux: ['chrome-linux', 'chrome'],
                mac: ['chrome-mac', 'Chromium.app', 'Contents', 'MacOS', 'Chromium'],
                win: ['chrome-win', 'Chrome.exe']
            },
        }

        if self.app not in available_chromium_dists:
            self.info("Google Chrome or Chromium distributions are not required.")
            return

        chromium_dist = self.app

        if self.config.get("run_local"):
            self.info("expecting %s to be pre-installed locally" % chromium_dist)
            return

        self.info("Getting fetched %s build" % chromium_dist)
        self.chromium_dist_dest = os.path.normpath(os.path.abspath(os.environ['MOZ_FETCHES_DIR']))

        if mac in self.platform_name():
            self.chromium_dist_path = os.path.join(self.chromium_dist_dest,
                                                   *binary_location[chromium_dist][mac])

        elif linux in self.platform_name():
            self.chromium_dist_path = os.path.join(self.chromium_dist_dest,
                                                   *binary_location[chromium_dist][linux])

        else:
            self.chromium_dist_path = os.path.join(self.chromium_dist_dest,
                                                   *binary_location[chromium_dist][win])

        self.info("%s dest is: %s" % (chromium_dist, self.chromium_dist_dest))
        self.info("%s path is: %s" % (chromium_dist, self.chromium_dist_path))

        # now ensure chrome binary exists
        if os.path.exists(self.chromium_dist_path):
            self.info("successfully installed %s to: %s"
                      % (chromium_dist, self.chromium_dist_path))
        else:
            self.info("abort: failed to install %s" % chromium_dist)

    def raptor_options(self, args=None, **kw):
        """return options to raptor"""
        options = []
        kw_options = {}

        # binary path; if testing on firefox the binary path already came from mozharness/pro;
        # otherwise the binary path is forwarded from cmd line arg (raptor_cmd_line_args)
        kw_options['app'] = self.app
        if self.app == "firefox" or (self.app in self.firefox_android_browsers and
                                     not self.run_local):
            binary_path = self.binary_path or self.config.get('binary_path')
            if not binary_path:
                self.fatal("Raptor requires a path to the binary.")
            kw_options['binary'] = binary_path
            if self.app in self.firefox_android_browsers:
                # in production ensure we have correct app name,
                # i.e. fennec_aurora or fennec_release etc.
                kw_options['binary'] = self.query_package_name()
                self.info("set binary to %s instead of %s" % (kw_options['binary'], binary_path))
        else:  # running on google chrome
            if not self.run_local:
                # when running locally we already set the chrome binary above in init; here
                # in production we aready installed chrome, so set the binary path to our install
                kw_options['binary'] = self.chromium_dist_path

        # options overwritten from **kw
        if 'test' in self.config:
            kw_options['test'] = self.config['test']
        if self.symbols_path:
            kw_options['symbolsPath'] = self.symbols_path
        if self.config.get('obj_path', None) is not None:
            kw_options['obj-path'] = self.config['obj_path']
        if self.test_url_params:
            kw_options['test-url-params'] = self.test_url_params

        kw_options.update(kw)
        if self.host:
            kw_options['host'] = self.host
        # configure profiling options
        options.extend(self.query_gecko_profile_options())
        # extra arguments
        if args is not None:
            options += args
        if self.config.get('run_local', False):
            options.extend(['--run-local'])
        if 'raptor_cmd_line_args' in self.config:
            options += self.config['raptor_cmd_line_args']
        if self.config.get('code_coverage', False):
            options.extend(['--code-coverage'])
        if self.config.get('is_release_build', False):
            options.extend(['--is-release-build'])
        if self.config.get('power_test', False):
            options.extend(['--power-test'])
        if self.config.get('memory_test', False):
            options.extend(['--memory-test'])
        if self.config.get('cpu_test', False):
            options.extend(['--cpu-test'])
        if self.config.get('enable_webrender', False):
            options.extend(['--enable-webrender'])

        for (arg,), details in Raptor.browsertime_options:
            # Allow to override defaults on the `./mach raptor-test ...` command line.
            value = self.config.get(details['dest'])
            if value and arg not in self.config.get("raptor_cmd_line_args", []):
                if isinstance(value, basestring):
                    options.extend([arg, os.path.expandvars(value)])
                else:
                    options.extend([arg])

        for key, value in kw_options.items():
            options.extend(['--%s' % key, value])

        return options

    def populate_webroot(self):
        """Populate the production test slaves' webroots"""
        self.raptor_path = os.path.join(
            self.query_abs_dirs()['abs_test_install_dir'], 'raptor'
        )
        if self.config.get('run_local'):
            self.raptor_path = os.path.join(self.repo_path, 'testing', 'raptor')

    # Action methods. {{{1
    # clobber defined in BaseScript

    def clobber(self):
        # Recreate the upload directory for storing the logcat collected
        # during apk installation.
        super(Raptor, self).clobber()
        upload_dir = self.query_abs_dirs()['abs_blob_upload_dir']
        if not os.path.isdir(upload_dir):
            self.mkdir_p(upload_dir)

    def install_apk(self, apk):
        # Override AnroidMixin's install_apk in order to capture
        # logcat during the installation. If the installation fails,
        # the logcat file will be left in the upload directory.
        self.logcat_start()
        try:
            super(Raptor, self).install_apk(apk)
        finally:
            self.logcat_stop()

    def download_and_extract(self, extract_dirs=None, suite_categories=None):
        return super(Raptor, self).download_and_extract(
            suite_categories=['common', 'raptor']
        )

    def create_virtualenv(self, **kwargs):
        """VirtualenvMixin.create_virtualenv() assuemes we're using
        self.config['virtualenv_modules']. Since we are installing
        raptor from its source, we have to wrap that method here."""
        # if virtualenv already exists, just add to path and don't re-install, need it
        # in path so can import jsonschema later when validating output for perfherder
        _virtualenv_path = self.config.get("virtualenv_path")

        if self.run_local and os.path.exists(_virtualenv_path):
            self.info("Virtualenv already exists, skipping creation")
            _python_interp = self.config.get('exes')['python']

            if 'win' in self.platform_name():
                _path = os.path.join(_virtualenv_path,
                                     'Lib',
                                     'site-packages')
            else:
                _path = os.path.join(_virtualenv_path,
                                     'lib',
                                     os.path.basename(_python_interp),
                                     'site-packages')

            # if  running gecko profiling  install the requirements
            if self.gecko_profile:
                self._install_view_gecko_profile_req()

            sys.path.append(_path)
            return

        # virtualenv doesn't already exist so create it
        # install mozbase first, so we use in-tree versions
        if not self.run_local:
            mozbase_requirements = os.path.join(
                self.query_abs_dirs()['abs_test_install_dir'],
                'config',
                'mozbase_requirements.txt'
            )
        else:
            mozbase_requirements = os.path.join(
                os.path.dirname(self.raptor_path),
                'config',
                'mozbase_source_requirements.txt'
            )
        self.register_virtualenv_module(
            requirements=[mozbase_requirements],
            two_pass=True,
            editable=True,
        )
        # require pip >= 1.5 so pip will prefer .whl files to install
        super(Raptor, self).create_virtualenv(
            modules=['pip>=1.5']
        )
        # raptor in harness requires what else is
        # listed in raptor requirements.txt file.
        self.install_module(
            requirements=[os.path.join(self.raptor_path,
                                       'requirements.txt')]
        )

        # if  running gecko profiling  install the requirements
        if self.gecko_profile:
            self._install_view_gecko_profile_req()

    def install(self):
        if self.app in self.firefox_android_browsers:
            self.device.uninstall_app(self.binary_path)
            self.install_apk(self.installer_path)
        else:
            super(Raptor, self).install()

    def _install_view_gecko_profile_req(self):
        # if running locally and gecko profiing is on, we will be using the
        # view-gecko-profile tool which has its own requirements too
        if self.gecko_profile and self.run_local:
            tools = os.path.join(self.config['repo_path'], 'testing', 'tools')
            view_gecko_profile_req = os.path.join(tools,
                                                  'view_gecko_profile',
                                                  'requirements.txt')
            self.info("installing requirements for the view-gecko-profile tool")
            self.install_module(requirements=[view_gecko_profile_req])

    def _artifact_perf_data(self, src, dest):
        if not os.path.isdir(os.path.dirname(dest)):
            # create upload dir if it doesn't already exist
            self.info("creating dir: %s" % os.path.dirname(dest))
            os.makedirs(os.path.dirname(dest))
        self.info('copying raptor results from %s to %s' % (src, dest))
        try:
            copyfile(src, dest)
        except Exception as e:
            self.critical("Error copying results %s to upload dir %s" % (src, dest))
            self.info(str(e))

    def run_tests(self, args=None, **kw):
        """run raptor tests"""

        # get raptor options
        options = self.raptor_options(args=args, **kw)

        # python version check
        python = self.query_python_path()
        self.run_command([python, "--version"])
        parser = RaptorOutputParser(config=self.config, log_obj=self.log_obj,
                                    error_list=RaptorErrorList)
        env = {}
        env['MOZ_UPLOAD_DIR'] = self.query_abs_dirs()['abs_blob_upload_dir']
        if not self.run_local:
            env['MINIDUMP_STACKWALK'] = self.query_minidump_stackwalk()
        env['MINIDUMP_SAVE_PATH'] = self.query_abs_dirs()['abs_blob_upload_dir']
        env['RUST_BACKTRACE'] = 'full'
        if not os.path.isdir(env['MOZ_UPLOAD_DIR']):
            self.mkdir_p(env['MOZ_UPLOAD_DIR'])
        env = self.query_env(partial_env=env, log_level=INFO)
        # adjust PYTHONPATH to be able to use raptor as a python package
        if 'PYTHONPATH' in env:
            env['PYTHONPATH'] = self.raptor_path + os.pathsep + env['PYTHONPATH']
        else:
            env['PYTHONPATH'] = self.raptor_path

        # mitmproxy needs path to mozharness when installing the cert, and tooltool
        env['SCRIPTSPATH'] = scripts_path
        env['EXTERNALTOOLSPATH'] = external_tools_path

        # disable "GC poisoning" Bug# 1499043
        env['JSGC_DISABLE_POISONING'] = '1'

        # needed to load unsigned raptor webext on release builds.
        if self.is_release_build:
            env['MOZ_DISABLE_NONLOCAL_CONNECTIONS'] = '1'

        if self.repo_path is not None:
            env['MOZ_DEVELOPER_REPO_DIR'] = self.repo_path
        if self.obj_path is not None:
            env['MOZ_DEVELOPER_OBJ_DIR'] = self.obj_path

        # sets a timeout for how long raptor should run without output
        output_timeout = self.config.get('raptor_output_timeout', 3600)
        # run raptor tests
        run_tests = os.path.join(self.raptor_path, 'raptor', 'raptor.py')

        mozlog_opts = ['--log-tbpl-level=debug']
        if not self.run_local and 'suite' in self.config:
            fname_pattern = '%s_%%s.log' % self.config['test']
            mozlog_opts.append('--log-errorsummary=%s'
                               % os.path.join(env['MOZ_UPLOAD_DIR'],
                                              fname_pattern % 'errorsummary'))
            mozlog_opts.append('--log-raw=%s'
                               % os.path.join(env['MOZ_UPLOAD_DIR'],
                                              fname_pattern % 'raw'))

        def launch_in_debug_mode(cmdline):
            cmdline = set(cmdline)
            debug_opts = {'--debug', '--debugger', '--debugger_args'}

            return bool(debug_opts.intersection(cmdline))

        if self.app in self.firefox_android_browsers:
            self.logcat_start()

        command = [python, run_tests] + options + mozlog_opts
        if launch_in_debug_mode(command):
            raptor_process = subprocess.Popen(command, cwd=self.workdir, env=env)
            raptor_process.wait()
        else:
            self.return_code = self.run_command(command, cwd=self.workdir,
                                                output_timeout=output_timeout,
                                                output_parser=parser,
                                                env=env)

        if self.app in self.firefox_android_browsers:
            self.logcat_stop()

        if parser.minidump_output:
            self.info("Looking at the minidump files for debugging purposes...")
            for item in parser.minidump_output:
                self.run_command(["ls", "-l", item])

        elif not self.run_local:
            # copy results to upload dir so they are included as an artifact
            self.info("copying raptor results to upload dir:")

            src = os.path.join(self.query_abs_dirs()['abs_work_dir'], 'raptor.json')
            dest = os.path.join(env['MOZ_UPLOAD_DIR'], 'perfherder-data.json')
            self.info(str(dest))
            self._artifact_perf_data(src, dest)

            # make individual perfherder data json's for each supporting data type
            for file in glob.glob(os.path.join(self.query_abs_dirs()['abs_work_dir'], '*')):
                path, filename = os.path.split(file)

                if not filename.startswith('raptor-'):
                    continue

                # filename is expected to contain a unique data name
                # i.e. raptor-os-baseline-power.json would result in
                # the data name os-baseline-power
                data_name = '-'.join(filename.split('-')[1:])
                data_name = '.'.join(data_name.split('.')[:-1])

                src = file
                dest = os.path.join(
                    env['MOZ_UPLOAD_DIR'],
                    'perfherder-data-%s.json' % data_name
                )
                self._artifact_perf_data(src, dest)

            src = os.path.join(self.query_abs_dirs()['abs_work_dir'], 'screenshots.html')
            if os.path.exists(src):
                dest = os.path.join(env['MOZ_UPLOAD_DIR'], 'screenshots.html')
                self.info(str(dest))
                self._artifact_perf_data(src, dest)


class RaptorOutputParser(OutputParser):
    minidump_regex = re.compile(r'''raptorError: "error executing: '(\S+) (\S+) (\S+)'"''')
    RE_PERF_DATA = re.compile(r'.*PERFHERDER_DATA:\s+(\{.*\})')

    def __init__(self, **kwargs):
        super(RaptorOutputParser, self).__init__(**kwargs)
        self.minidump_output = None
        self.found_perf_data = []

    def parse_single_line(self, line):
        m = self.minidump_regex.search(line)
        if m:
            self.minidump_output = (m.group(1), m.group(2), m.group(3))

        m = self.RE_PERF_DATA.match(line)
        if m:
            self.found_perf_data.append(m.group(1))
        super(RaptorOutputParser, self).parse_single_line(line)
