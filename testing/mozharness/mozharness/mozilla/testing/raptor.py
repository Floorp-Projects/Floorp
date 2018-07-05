# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import copy
import json
import os
import re
import sys
import subprocess
import time

from shutil import copyfile

import mozharness

from mozharness.base.errors import PythonErrorList
from mozharness.base.log import OutputParser, DEBUG, ERROR, CRITICAL, INFO
from mozharness.mozilla.testing.testbase import TestingMixin, testing_config_options
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.testing.codecoverage import (
    CodeCoverageMixin,
    code_coverage_config_options
)

scripts_path = os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__)))
external_tools_path = os.path.join(scripts_path, 'external_tools')
here = os.path.abspath(os.path.dirname(__file__))

RaptorErrorList = PythonErrorList + [
    {'regex': re.compile(r'''run-as: Package '.*' is unknown'''), 'level': DEBUG},
    {'substr': r'''FAIL: Busted:''', 'level': CRITICAL},
    {'substr': r'''FAIL: failed to cleanup''', 'level': ERROR},
    {'substr': r'''erfConfigurator.py: Unknown error''', 'level': CRITICAL},
    {'substr': r'''raptorError''', 'level': CRITICAL},
    {'regex': re.compile(r'''No machine_name called '.*' can be found'''), 'level': CRITICAL},
    {'substr': r"""No such file or directory: 'browser_output.txt'""",
     'level': CRITICAL,
     'explanation': "Most likely the browser failed to launch, or the test was otherwise "
     "unsuccessful in even starting."},
]


class Raptor(TestingMixin, MercurialScript, CodeCoverageMixin):
    """
    install and run raptor tests
    """
    config_options = [
        [["--test"],
         {"action": "store",
          "dest": "test",
          "help": "Raptor test to run"
          }],
        [["--app"],
         {"default": "firefox",
          "choices": ["firefox", "chrome"],
          "dest": "app",
          "help": "name of the application we are testing (default: firefox)"
          }],
        [["--branch-name"],
         {"action": "store",
          "dest": "branch",
          "help": "branch running against"
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
            "help": "Tries to enable the WebRender compositor.",
        }],
    ] + testing_config_options + copy.deepcopy(code_coverage_config_options)

    def __init__(self, **kwargs):
        kwargs.setdefault('config_options', self.config_options)
        kwargs.setdefault('all_actions', ['clobber',
                                          'download-and-extract',
                                          'populate-webroot',
                                          'install-chrome',
                                          'create-virtualenv',
                                          'install',
                                          'run-tests',
                                          ])
        kwargs.setdefault('default_actions', ['clobber',
                                              'download-and-extract',
                                              'populate-webroot',
                                              'install-chrome',
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
            self.app = "firefox"
            if 'raptor_cmd_line_args' in self.config:
                for next_arg in self.config['raptor_cmd_line_args']:
                    if "chrome" in next_arg:
                        self.app = "chrome"
                        break
        else:
            # raptor initiated in production via mozharness
            self.test = self.config['test']
            self.app = self.config.get("app", "firefox")

        self.installer_url = self.config.get("installer_url")
        self.raptor_json_url = self.config.get("raptor_json_url")
        self.raptor_json = self.config.get("raptor_json")
        self.raptor_json_config = self.config.get("raptor_json_config")
        self.repo_path = self.config.get("repo_path")
        self.obj_path = self.config.get("obj_path")
        self.test = None
        self.gecko_profile = self.config.get('gecko_profile')
        self.gecko_profile_interval = self.config.get('gecko_profile_interval')

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

    def install_chrome(self):
        # temporary hack to install google chrome in production; until chrome is in our CI
        if self.app != "chrome":
            self.info("Google Chrome is not required")
            return

        if self.config.get("run_local"):
            self.info("expecting Google Chrome to be pre-installed locally")
            return

        # in production we can put the chrome build in mozharness/mozilla/testing/chrome
        self.chrome_dest = os.path.join(here, 'chrome')

        # mozharness/base/script.py.self.platform_name will return one of:
        # 'linux64', 'linux', 'macosx', 'win64', 'win32'

        if 'mac' in self.platform_name():
            chrome_archive_file = "googlechrome.dmg"
            chrome_url = "https://dl.google.com/chrome/mac/stable/GGRO/%s" % chrome_archive_file
            self.chrome_path = os.path.join(self.chrome_dest, 'Google Chrome.app',
                                            'Contents', 'MacOS', 'Google Chrome')

        elif 'linux' in self.platform_name():
            chrome_archive_file = "google-chrome-stable_current_amd64.deb"
            chrome_url = "https://dl.google.com/linux/direct/%s" % chrome_archive_file
            self.chrome_path = os.path.join(self.chrome_dest, 'opt', 'google',
                                            'chrome', 'google-chrome')

        else:
            # windows 7/10
            if '64' in self.platform_name():
                chrome_archive_file = "standalonesetup64.exe"
            else:
                chrome_archive_file = "standalonesetup.exe"
            chrome_url = "https://dl.google.com/chrome/install/%s" % chrome_archive_file

        chrome_archive = os.path.join(self.chrome_dest, chrome_archive_file)

        self.info("installing google chrome - temporary install hack")
        self.info("chrome archive is: %s" % chrome_archive)
        self.info("chrome dest is: %s" % self.chrome_dest)

        if os.path.exists(self.chrome_path):
            self.info("google chrome binary already exists at: %s" % self.chrome_path)
            return

        if not os.path.exists(chrome_archive):
            # download the chrome installer
            self.download_file(chrome_url, parent_dir=self.chrome_dest)

        commands = []

        if 'mac' in self.platform_name():
            # open the chrome dmg to have it mounted
            commands.append(["open", chrome_archive_file])

            # then extract/copy app from mnt to our folder
            commands.append(["cp", "-r", "/Volumes/Google Chrome/Google Chrome.app", "."])

        elif 'linux' in self.platform_name():
            # on linux in order to avoid needing sudo, we unpack the google chrome deb
            # manually using ar, and then unpack the contents of the ar after that
            commands.append(["ar", "x", chrome_archive_file])

            # now we have the google chrome .deb file unpacked using ar, we need to
            # unpack the tar.xz file that was inside the .deb
            commands.append(['tar', '-xJf',
                            os.path.join(self.chrome_dest, 'data.tar.xz'),
                            '-C', self.chrome_dest])

        else:
            # TODO: Bug 1473389 - finish this for windows 7 (x32) and winows 10 (x64)
            pass

        # now run the commands to unpack / install google chrome
        for next_command in commands:
            return_code = self.run_command(next_command, cwd=self.chrome_dest)
            time.sleep(30)
            if return_code not in [0]:
                self.info("abort: failed to install %s to %s with command: %s"
                          % (chrome_archive_file, self.chrome_dest, next_command))

        # now ensure chrome binary exists
        if os.path.exists(self.chrome_path):
            self.info("successfully installed Google Chrome to: %s" % self.chrome_path)
        else:
            self.info("abort: failed to install Google Chrome")

    def raptor_options(self, args=None, **kw):
        """return options to raptor"""
        options = []
        kw_options = {}

        # binary path; if testing on firefox the binary path already came from mozharness/pro;
        # otherwise the binary path is forwarded from cmd line arg (raptor_cmd_line_args)
        kw_options['app'] = self.app
        if self.app == "firefox":
            binary_path = self.binary_path or self.config.get('binary_path')
            if not binary_path:
                self.fatal("Raptor requires a path to the binary.")
            kw_options['binary'] = binary_path
        else:
            if not self.run_local:
                # in production we aready installed google chrome, so set the binary path for arg
                # when running locally a --binary arg as passed in, already in raptor_cmd_line_args
                kw_options['binary'] = self.chrome_path

        # options overwritten from **kw
        if 'test' in self.config:
            kw_options['test'] = self.config['test']
        if self.config.get('branch'):
            kw_options['branchName'] = self.config['branch']
        if self.symbols_path:
            kw_options['symbolsPath'] = self.symbols_path
        if self.config.get('obj_path', None) is not None:
            kw_options['obj-path'] = self.config['obj_path']
        kw_options.update(kw)
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

    def _validate_treeherder_data(self, parser):
        # late import is required, because install is done in create_virtualenv
        import jsonschema

        if len(parser.found_perf_data) != 1:
            self.critical("PERFHERDER_DATA was seen %d times, expected 1."
                          % len(parser.found_perf_data))
            return

        schema_path = os.path.join(external_tools_path,
                                   'performance-artifact-schema.json')
        self.info("Validating PERFHERDER_DATA against %s" % schema_path)
        try:
            with open(schema_path) as f:
                schema = json.load(f)
            data = json.loads(parser.found_perf_data[0])
            jsonschema.validate(data, schema)
        except Exception as e:
            self.exception("Error while validating PERFHERDER_DATA")
            self.info(e)

    def _artifact_perf_data(self, dest):
        src = os.path.join(self.query_abs_dirs()['abs_work_dir'], 'raptor.json')
        if not os.path.isdir(os.path.dirname(dest)):
            # create upload dir if it doesn't already exist
            self.info("creating dir: %s" % os.path.dirname(dest))
            os.makedirs(os.path.dirname(dest))
        self.info('copying raptor results from %s to %s' % (src, dest))
        try:
            copyfile(src, dest)
        except Exception as e:
            self.critical("Error copying results %s to upload dir %s" % (src, dest))
            self.info(e)

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

        # if running in production on a quantum_render build
        if self.config['enable_webrender']:
            self.info("webrender is enabled so setting MOZ_WEBRENDER=1 and MOZ_ACCELERATED=1")
            env['MOZ_WEBRENDER'] = '1'
            env['MOZ_ACCELERATED'] = '1'

        # mitmproxy needs path to mozharness when installing the cert, and tooltool
        env['SCRIPTSPATH'] = scripts_path
        env['EXTERNALTOOLSPATH'] = external_tools_path

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

        command = [python, run_tests] + options + mozlog_opts
        if launch_in_debug_mode(command):
            raptor_process = subprocess.Popen(command, cwd=self.workdir, env=env)
            raptor_process.wait()
        else:
            self.return_code = self.run_command(command, cwd=self.workdir,
                                                output_timeout=output_timeout,
                                                output_parser=parser,
                                                env=env)
        if parser.minidump_output:
            self.info("Looking at the minidump files for debugging purposes...")
            for item in parser.minidump_output:
                self.run_command(["ls", "-l", item])

        elif '--no-upload-results' not in options:
            if not self.gecko_profile:
                self._validate_treeherder_data(parser)
                if not self.run_local:
                    # copy results to upload dir so they are included as an artifact
                    self.info("copying raptor results to upload dir:")
                    dest = os.path.join(env['MOZ_UPLOAD_DIR'], 'perfherder-data.json')
                    self.info(str(dest))
                    self._artifact_perf_data(dest)


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
