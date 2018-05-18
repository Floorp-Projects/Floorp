# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import copy
import json
import os
import re
import sys

import mozharness

from mozharness.base.config import parse_config_file
from mozharness.base.errors import PythonErrorList
from mozharness.base.log import OutputParser, DEBUG, ERROR, CRITICAL, INFO, WARNING
from mozharness.base.python import Python3Virtualenv
from mozharness.mozilla.testing.testbase import TestingMixin, testing_config_options
from mozharness.mozilla.tooltool import TooltoolMixin
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.testing.codecoverage import (
    CodeCoverageMixin,
    code_coverage_config_options
)

scripts_path = os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__)))
external_tools_path = os.path.join(scripts_path, 'external_tools')

RaptorErrorList = PythonErrorList + [
    {'regex': re.compile(r'''run-as: Package '.*' is unknown'''), 'level': DEBUG},
    {'substr': r'''FAIL: Busted:''', 'level': CRITICAL},
    {'substr': r'''FAIL: failed to cleanup''', 'level': ERROR},
    {'substr': r'''erfConfigurator.py: Unknown error''', 'level': CRITICAL},
    {'substr': r'''raptorError''', 'level': CRITICAL},
    {'regex': re.compile(r'''No machine_name called '.*' can be found'''), 'level': CRITICAL},
    {'substr': r"""No such file or directory: 'browser_output.txt'""",
     'level': CRITICAL,
     'explanation': r"""Most likely the browser failed to launch, or the test was otherwise unsuccessful in even starting."""},
]

class Raptor(TestingMixin, MercurialScript, Python3Virtualenv, CodeCoverageMixin):
    """
    install and run raptor tests
    """
    config_options = [
        [["--test"],
         {"action": "store",
          "dest": "test",
          "help": "Raptor test to run"
          }],
        [["--branch-name"],
         {"action": "store",
          "dest": "branch",
          "help": "branch running against"
          }],
        [["--add-option"],
         {"action": "extend",
          "dest": "raptor_extra_options",
          "default": None,
          "help": "extra options to raptor"
          }],
    ] + testing_config_options + copy.deepcopy(code_coverage_config_options)

    def __init__(self, **kwargs):
        kwargs.setdefault('config_options', self.config_options)
        kwargs.setdefault('all_actions', ['clobber',
                                          'download-and-extract',
                                          'populate-webroot',
                                          'create-virtualenv',
                                          'install',
                                          'run-tests',
                                          ])
        kwargs.setdefault('default_actions', ['clobber',
                                              'download-and-extract',
                                              'populate-webroot',
                                              'create-virtualenv',
                                              'install',
                                              'run-tests',
                                              ])
        kwargs.setdefault('config', {})
        super(Raptor, self).__init__(**kwargs)

        self.workdir = self.query_abs_dirs()['abs_work_dir']  # convenience

        self.run_local = self.config.get('run_local')
        self.installer_url = self.config.get("installer_url")
        self.raptor_json_url = self.config.get("raptor_json_url")
        self.raptor_json = self.config.get("raptor_json")
        self.raptor_json_config = self.config.get("raptor_json_config")
        self.repo_path = self.config.get("repo_path")
        self.obj_path = self.config.get("obj_path")
        self.tests = None
        self.gecko_profile = self.config.get('gecko_profile')
        self.gecko_profile_interval = self.config.get('gecko_profile_interval')
        self.mitmproxy_rel_bin = None # some platforms download a mitmproxy release binary
        self.mitmproxy_pageset = None # zip file found on tooltool that contains all of the mitmproxy recordings
        self.mitmproxy_recordings_file_list = self.config.get('mitmproxy', None) # files inside the recording set
        self.mitmdump = None # path to mitmdump tool itself, in py3 venv

    # We accept some configuration options from the try commit message in the format mozharness: <options>
    # Example try commit message:
    #   mozharness: --geckoProfile try: <stuff>
    def query_gecko_profile_options(self):
        gecko_results = []
        if self.buildbot_config:
            # this is inside automation
            # now let's see if we added GeckoProfile specs in the commit message
            try:
                junk, junk, opts = self.buildbot_config['sourcestamp']['changes'][-1]['comments'].partition('mozharness:')
            except IndexError:
                # when we don't have comments on changes (bug 1255187)
                opts = None

            if opts:
                # In the case of a multi-line commit message, only examine
                # the first line for mozharness options
                opts = opts.split('\n')[0]
                opts = re.sub(r'\w+:.*', '', opts).strip().split(' ')
                if "--geckoProfile" in opts:
                    # overwrite whatever was set here.
                    self.gecko_profile = True
                try:
                    idx = opts.index('--geckoProfileInterval')
                    if len(opts) > idx + 1:
                        self.gecko_profile_interval = opts[idx + 1]
                except ValueError:
                    pass
            else:
                # no opts, check for '--geckoProfile' in try message text directly
                if self.try_message_has_flag('geckoProfile'):
                    self.gecko_profile = True

        # finally, if gecko_profile is set, we add that to the raptor options
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
        abs_dirs['abs_blob_upload_dir'] = os.path.join(abs_dirs['abs_work_dir'], 'blobber_upload_dir')
        abs_dirs['abs_test_install_dir'] = os.path.join(abs_dirs['abs_work_dir'], 'tests')
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def raptor_options(self, args=None, **kw):
        """return options to raptor"""
        # binary path
        binary_path = self.binary_path or self.config.get('binary_path')
        if not binary_path:
            self.fatal("Raptor requires a path to the binary.  You can specify binary_path or add download-and-extract to your action list.")
        # raptor options
        if binary_path.endswith('.exe'):
            binary_path = binary_path[:-4]
        options = []
        kw_options = {'binary': binary_path}
        # options overwritten from **kw
        if 'test' in self.config:
            kw_options['test'] = self.config['test']
        if self.config.get('branch'):
            kw_options['branchName'] = self.config['branch']
        if self.symbols_path:
            kw_options['symbolsPath'] = self.symbols_path
        kw_options.update(kw)
        # configure profiling options
        options.extend(self.query_gecko_profile_options())
        # extra arguments
        if args is not None:
            options += args
        if 'raptor_extra_options' in self.config:
            options += self.config['raptor_extra_options']
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
            # raptor initiated locally, get and verify test from cmd line
            self.raptor_path = os.path.join(self.repo_path, 'testing', 'raptor')
            if 'raptor_extra_options' in self.config:
                if '--test' in self.config['raptor_extra_options']:
                    # --test specified, get test from cmd line and ensure is valid
                    test_name_index = self.config['raptor_extra_options'].index('--test') + 1
                    if test_name_index < len(self.config['raptor_extra_options']):
                        self.test = self.config['raptor_extra_options'][test_name_index]
                    else:
                        self.fatal("Test name not provided")
        else:
            # raptor initiated in production via mozharness
            self.test = self.config['test']

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
        except:
            self.exception("Error while validating PERFHERDER_DATA")

    def _artifact_perf_data(self, dest):
        src = os.path.join(self.query_abs_dirs()['abs_work_dir'], 'local.json')
        try:
            shutil.copyfile(src, dest)
        except:
            self.critical("Error copying results %s to upload dir %s" % (src, dest))

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

        if self.return_code not in [0]:
            # update the worst log level
            log_level = ERROR
            if self.return_code == 1:
                log_level = WARNING
            if self.return_code == 4:
                log_level = WARNING

        elif '--no-upload-results' not in options:
            if not self.gecko_profile:
                self._validate_treeherder_data(parser)
                if not self.run_local:
                    # copy results to upload dir so they are included as an artifact
                    dest = os.path.join(env['MOZ_UPLOAD_DIR'], 'perfherder-data.json')
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

