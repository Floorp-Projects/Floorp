#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""
run talos tests in a virtualenv
"""

import os
import sys
import pprint
import copy
import re
import shutil
import json

import mozharness
from mozharness.base.config import parse_config_file
from mozharness.base.errors import PythonErrorList
from mozharness.base.log import OutputParser, DEBUG, ERROR, CRITICAL
from mozharness.base.log import INFO, WARNING
from mozharness.base.python import Python3Virtualenv
from mozharness.mozilla.blob_upload import BlobUploadMixin, blobupload_config_options
from mozharness.mozilla.testing.testbase import TestingMixin, testing_config_options
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.testing.errors import TinderBoxPrintRe
from mozharness.mozilla.buildbot import TBPL_SUCCESS, TBPL_WORST_LEVEL_TUPLE
from mozharness.mozilla.buildbot import TBPL_RETRY, TBPL_FAILURE, TBPL_WARNING
from mozharness.mozilla.tooltool import TooltoolMixin
from mozharness.mozilla.testing.codecoverage import (
    CodeCoverageMixin,
    code_coverage_config_options
)


scripts_path = os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__)))
external_tools_path = os.path.join(scripts_path, 'external_tools')

TalosErrorList = PythonErrorList + [
    {'regex': re.compile(r'''run-as: Package '.*' is unknown'''), 'level': DEBUG},
    {'substr': r'''FAIL: Graph server unreachable''', 'level': CRITICAL},
    {'substr': r'''FAIL: Busted:''', 'level': CRITICAL},
    {'substr': r'''FAIL: failed to cleanup''', 'level': ERROR},
    {'substr': r'''erfConfigurator.py: Unknown error''', 'level': CRITICAL},
    {'substr': r'''talosError''', 'level': CRITICAL},
    {'regex': re.compile(r'''No machine_name called '.*' can be found'''), 'level': CRITICAL},
    {'substr': r"""No such file or directory: 'browser_output.txt'""",
     'level': CRITICAL,
     'explanation': r"""Most likely the browser failed to launch, or the test was otherwise unsuccessful in even starting."""},
]

# TODO: check for running processes on script invocation

class TalosOutputParser(OutputParser):
    minidump_regex = re.compile(r'''talosError: "error executing: '(\S+) (\S+) (\S+)'"''')
    RE_PERF_DATA = re.compile(r'.*PERFHERDER_DATA:\s+(\{.*\})')
    worst_tbpl_status = TBPL_SUCCESS

    def __init__(self, **kwargs):
        super(TalosOutputParser, self).__init__(**kwargs)
        self.minidump_output = None
        self.found_perf_data = []

    def update_worst_log_and_tbpl_levels(self, log_level, tbpl_level):
        self.worst_log_level = self.worst_level(log_level,
                                                self.worst_log_level)
        self.worst_tbpl_status = self.worst_level(
            tbpl_level, self.worst_tbpl_status,
            levels=TBPL_WORST_LEVEL_TUPLE
        )

    def parse_single_line(self, line):
        """ In Talos land, every line that starts with RETURN: needs to be
        printed with a TinderboxPrint:"""
        if line.startswith("RETURN:"):
            line.replace("RETURN:", "TinderboxPrint:")
        m = self.minidump_regex.search(line)
        if m:
            self.minidump_output = (m.group(1), m.group(2), m.group(3))

        m = self.RE_PERF_DATA.match(line)
        if m:
            self.found_perf_data.append(m.group(1))

        # now let's check if buildbot should retry
        harness_retry_re = TinderBoxPrintRe['harness_error']['retry_regex']
        if harness_retry_re.search(line):
            self.critical(' %s' % line)
            self.update_worst_log_and_tbpl_levels(CRITICAL, TBPL_RETRY)
            return  # skip base parse_single_line
        super(TalosOutputParser, self).parse_single_line(line)


class Talos(TestingMixin, MercurialScript, BlobUploadMixin, TooltoolMixin,
            Python3Virtualenv, CodeCoverageMixin):
    """
    install and run Talos tests:
    https://wiki.mozilla.org/Buildbot/Talos
    """
    config_options = [
        [["--use-talos-json"],
         {"action": "store_true",
          "dest": "use_talos_json",
          "default": False,
          "help": "Use talos config from talos.json"
          }],
        [["--suite"],
         {"action": "store",
          "dest": "suite",
          "help": "Talos suite to run (from talos json)"
          }],
        [["--branch-name"],
         {"action": "store",
          "dest": "branch",
          "help": "Graphserver branch to report to"
          }],
        [["--system-bits"],
         {"action": "store",
          "dest": "system_bits",
          "type": "choice",
          "default": "32",
          "choices": ['32', '64'],
          "help": "Testing 32 or 64 (for talos json plugins)"
          }],
        [["--add-option"],
         {"action": "extend",
          "dest": "talos_extra_options",
          "default": None,
          "help": "extra options to talos"
          }],
        [["--geckoProfile"], {
            "dest": "gecko_profile",
            "action": "store_true",
            "default": False,
            "help": "Whether or not to profile the test run and save the profile results"
        }],
        [["--geckoProfileInterval"], {
            "dest": "gecko_profile_interval",
            "type": "int",
            "default": 0,
            "help": "The interval between samples taken by the profiler (milliseconds)"
        }],
    ] + testing_config_options + copy.deepcopy(blobupload_config_options) \
                               + copy.deepcopy(code_coverage_config_options)

    def __init__(self, **kwargs):
        kwargs.setdefault('config_options', self.config_options)
        kwargs.setdefault('all_actions', ['clobber',
                                          'read-buildbot-config',
                                          'download-and-extract',
                                          'populate-webroot',
                                          'create-virtualenv',
                                          'install',
                                          'setup-mitmproxy',
                                          'run-tests',
                                          ])
        kwargs.setdefault('default_actions', ['clobber',
                                              'download-and-extract',
                                              'populate-webroot',
                                              'create-virtualenv',
                                              'install',
                                              'setup-mitmproxy',
                                              'run-tests',
                                              ])
        kwargs.setdefault('config', {})
        super(Talos, self).__init__(**kwargs)

        self.workdir = self.query_abs_dirs()['abs_work_dir']  # convenience

        self.run_local = self.config.get('run_local')
        self.installer_url = self.config.get("installer_url")
        self.talos_json_url = self.config.get("talos_json_url")
        self.talos_json = self.config.get("talos_json")
        self.talos_json_config = self.config.get("talos_json_config")
        self.repo_path = self.config.get("repo_path")
        self.obj_path = self.config.get("obj_path")
        self.tests = None
        self.gecko_profile = self.config.get('gecko_profile')
        self.gecko_profile_interval = self.config.get('gecko_profile_interval')
        self.pagesets_name = None
        self.mitmproxy_recording_set = None # zip file found on tooltool that contains all of the mitmproxy recordings
        self.mitmproxy_recordings_file_list = self.config.get('mitmproxy', None) # files inside the recording set
        self.mitmdump = None # path to mitdump tool itself, in py3 venv

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
        # finally, if gecko_profile is set, we add that to the talos options
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
        abs_dirs = super(Talos, self).query_abs_dirs()
        abs_dirs['abs_blob_upload_dir'] = os.path.join(abs_dirs['abs_work_dir'], 'blobber_upload_dir')
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def query_talos_json_config(self):
        """Return the talos json config."""
        if self.talos_json_config:
            return self.talos_json_config
        if not self.talos_json:
            self.talos_json = os.path.join(self.talos_path, 'talos.json')
        self.talos_json_config = parse_config_file(self.talos_json)
        self.info(pprint.pformat(self.talos_json_config))
        return self.talos_json_config

    def query_pagesets_name(self):
        """Certain suites require external pagesets to be downloaded and
        extracted.
        """
        if self.pagesets_name:
            return self.pagesets_name
        if self.query_talos_json_config() and self.suite is not None:
            self.pagesets_name = self.talos_json_config['suites'][self.suite].get('pagesets_name')
            return self.pagesets_name

    def query_mitmproxy_recordings_file_list(self):
        """ When using mitmproxy we also need the name of the playback files that are included
        inside the playback archive.
        """
        if self.mitmproxy_recordings_file_list:
            return self.mitmproxy_recordings_file_list
        if self.query_talos_json_config() and self.suite is not None:
            talos_opts = self.talos_json_config['suites'][self.suite].get('talos_options', None)
            for index, val in enumerate(talos_opts):
                if val == '--mitmproxy':
                    self.mitmproxy_recordings_file_list = talos_opts[index + 1]
            return self.mitmproxy_recordings_file_list

    def get_suite_from_test(self):
        """ Retrieve the talos suite name from a given talos test name."""
        # running locally, single test name provided instead of suite; go through tests and find suite name
        suite_name = None
        if self.query_talos_json_config():
            if '-a' in self.config['talos_extra_options']:
                test_name_index = self.config['talos_extra_options'].index('-a') + 1
            if '--activeTests' in self.config['talos_extra_options']:
                test_name_index = self.config['talos_extra_options'].index('--activeTests') + 1
            if test_name_index < len(self.config['talos_extra_options']):
                test_name = self.config['talos_extra_options'][test_name_index]
                for talos_suite in self.talos_json_config['suites']:
                    if test_name in self.talos_json_config['suites'][talos_suite].get('tests'):
                        suite_name = talos_suite
            if not suite_name:
                # no suite found to contain the specified test, error out
                self.fatal("Test name is missing or invalid")
        else:
            self.fatal("Talos json config not found, cannot verify suite")
        return suite_name

    def validate_suite(self):
        """ Ensure suite name is a valid talos suite. """
        if self.query_talos_json_config() and self.suite is not None:
            if not self.suite in self.talos_json_config.get('suites'):
                self.fatal("Suite '%s' is not valid (not found in talos json config)" % self.suite)

    def talos_options(self, args=None, **kw):
        """return options to talos"""
        # binary path
        binary_path = self.binary_path or self.config.get('binary_path')
        if not binary_path:
            self.fatal("Talos requires a path to the binary.  You can specify binary_path or add download-and-extract to your action list.")

        # talos options
        options = []
        # talos can't gather data if the process name ends with '.exe'
        if binary_path.endswith('.exe'):
            binary_path = binary_path[:-4]
        # options overwritten from **kw
        kw_options = {'executablePath': binary_path}
        if 'suite' in self.config:
            kw_options['suite'] = self.config['suite']
        if self.config.get('title'):
            kw_options['title'] = self.config['title']
        if self.config.get('branch'):
            kw_options['branchName'] = self.config['branch']
        if self.symbols_path:
            kw_options['symbolsPath'] = self.symbols_path
        # if using mitmproxy, we've already created a py3 venv just
        # for it; need to add the path to that env/mitdump tool
        if self.mitmdump:
            kw_options['mitmdumpPath'] = self.mitmdump
            # also need to have recordings list; get again here from talos.json, in case talos was
            # invoked via '-a' and therefore the --mitmproxy param wasn't used on command line
            if not self.config.get('mitmproxy', None):
                file_list = self.query_mitmproxy_recordings_file_list()
                if file_list is not None:
                    kw_options['mitmproxy'] = file_list
                else:
                    self.fatal("Talos requires list of mitmproxy playback files, use --mitmproxy")
        kw_options.update(kw)
        # talos expects tests to be in the format (e.g.) 'ts:tp5:tsvg'
        tests = kw_options.get('activeTests')
        if tests and not isinstance(tests, basestring):
            tests = ':'.join(tests)  # Talos expects this format
            kw_options['activeTests'] = tests
        for key, value in kw_options.items():
            options.extend(['--%s' % key, value])
        # configure profiling options
        options.extend(self.query_gecko_profile_options())
        # extra arguments
        if args is not None:
            options += args
        if 'talos_extra_options' in self.config:
            options += self.config['talos_extra_options']
        return options

    def populate_webroot(self):
        """Populate the production test slaves' webroots"""
        self.talos_path = os.path.join(
            self.query_abs_dirs()['abs_work_dir'], 'tests', 'talos'
        )

        # need to determine if talos pageset is required to be downloaded
        if self.config.get('run_local'):
            # talos initiated locally, get and verify test/suite from cmd line
            self.talos_path = os.path.dirname(self.talos_json)
            if '-a' in self.config['talos_extra_options'] or '--activeTests' in self.config['talos_extra_options']:
                # test name (-a or --activeTests) specified, find out what suite it is a part of
                self.suite = self.get_suite_from_test()
            elif '--suite' in self.config['talos_extra_options']:
                # --suite specified, get suite from cmd line and ensure is valid
                suite_name_index = self.config['talos_extra_options'].index('--suite') + 1
                if suite_name_index < len(self.config['talos_extra_options']):
                    self.suite = self.config['talos_extra_options'][suite_name_index]
                    self.validate_suite()
                else:
                    self.fatal("Suite name not provided")
        else:
            # talos initiated in production via mozharness
            self.suite = self.config['suite']

        # now that have the suite name, check if pageset is required, if so download it
        # the --no-download option will override this
        if self.query_pagesets_name():
            if '--no-download' not in self.config['talos_extra_options']:
                self.info("Downloading pageset with tooltool...")
                self.src_talos_webdir = os.path.join(self.talos_path, 'talos')
                src_talos_pageset = os.path.join(self.src_talos_webdir, 'tests')
                manifest_file = os.path.join(self.talos_path, 'tp5n-pageset.manifest')
                self.tooltool_fetch(
                    manifest_file,
                    output_dir=src_talos_pageset,
                    cache=self.config.get('tooltool_cache')
                )
                archive = os.path.join(src_talos_pageset, self.pagesets_name)
                unzip = self.query_exe('unzip')
                unzip_cmd = [unzip, '-q', '-o', archive, '-d', src_talos_pageset]
                self.run_command(unzip_cmd, halt_on_failure=True)
            else:
                self.info("Not downloading pageset because the no-download option was specified")

    def setup_mitmproxy(self):
        """Some talos tests require the use of mitmproxy to playback the pages,
        set it up here.
        """
        if not self.query_mitmproxy_recording_set():
            self.info("Skipping: mitmproxy is not required")
            return

        # setup python 3.x virtualenv
        self.setup_py3_virtualenv()

        # install mitmproxy
        self.install_mitmproxy()

        # download the recording set; will be overridden by the --no-download
        if '--no-download' not in self.config['talos_extra_options']:
            self.download_mitmproxy_recording_set()
        else:
            self.info("Not downloading mitmproxy recording set because no-download was specified")

    def setup_py3_virtualenv(self):
        """Mitmproxy needs Python 3.x; set up a separate py 3.x env here"""
        self.info("Setting up python 3.x virtualenv, required for mitmproxy")
        # first download the py3 package
        self.py3_path = self.fetch_python3()
        # now create the py3 venv
        self.py3_venv_configuration(python_path=self.py3_path, venv_path='py3venv')
        self.py3_create_venv()
        requirements = [os.path.join(self.talos_path, 'talos', 'mitmproxy', 'mitmproxy_requirements.txt')]
        self.py3_install_requirement_files(requirements)
        # add py3 executables path to system path
        sys.path.insert(1, self.py3_path_to_executables())

    def install_mitmproxy(self):
        """Install the mitmproxy tool into the Python 3.x env"""
        self.info("Installing mitmproxy")
        self.py3_install_modules(modules=['mitmproxy'])
        self.mitmdump = os.path.join(self.py3_path_to_executables(), 'mitmdump')
        self.run_command([self.mitmdump, '--version'], env=self.query_env())

    def query_mitmproxy_recording_set(self):
        """Mitmproxy requires external playback archives to be downloaded and extracted"""
        if self.mitmproxy_recording_set:
            return self.mitmproxy_recording_set
        if self.query_talos_json_config() and self.suite is not None:
            self.mitmproxy_recording_set = self.talos_json_config['suites'][self.suite].get('mitmproxy_recording_set', False)
            return self.mitmproxy_recording_set

    def download_mitmproxy_recording_set(self):
        """Download the set of mitmproxy recording files that will be played back"""
        self.info("Downloading the mitmproxy recording set using tooltool")
        dest = os.path.join(self.talos_path, 'talos', 'mitmproxy')
        manifest_file = os.path.join(self.talos_path, 'talos', 'mitmproxy', 'mitmproxy-playback-set.manifest')
        self.tooltool_fetch(
            manifest_file,
            output_dir=dest,
            cache=self.config.get('tooltool_cache')
        )
        archive = os.path.join(dest, self.mitmproxy_recording_set)
        unzip = self.query_exe('unzip')
        unzip_cmd = [unzip, '-q', '-o', archive, '-d', dest]
        self.run_command(unzip_cmd, halt_on_failure=True)

    # Action methods. {{{1
    # clobber defined in BaseScript
    # read_buildbot_config defined in BuildbotMixin

    def download_and_extract(self, extract_dirs=None, suite_categories=None):
        return super(Talos, self).download_and_extract(
            suite_categories=['common', 'talos']
        )

    def create_virtualenv(self, **kwargs):
        """VirtualenvMixin.create_virtualenv() assuemes we're using
        self.config['virtualenv_modules']. Since we are installing
        talos from its source, we have to wrap that method here."""
        # install mozbase first, so we use in-tree versions
        if not self.run_local:
            mozbase_requirements = os.path.join(
                self.query_abs_dirs()['abs_work_dir'],
                'tests',
                'config',
                'mozbase_requirements.txt'
            )
        else:
            mozbase_requirements = os.path.join(
                os.path.dirname(self.talos_path),
                'config',
                'mozbase_requirements.txt'
            )
        self.register_virtualenv_module(
            requirements=[mozbase_requirements],
            two_pass=True,
            editable=True,
        )
        # require pip >= 1.5 so pip will prefer .whl files to install
        super(Talos, self).create_virtualenv(
            modules=['pip>=1.5']
        )
        # talos in harness requires what else is
        # listed in talos requirements.txt file.
        self.install_module(
            requirements=[os.path.join(self.talos_path,
                                       'requirements.txt')]
        )
        # install jsonschema for perfherder validation
        self.install_module(module="jsonschema")

    def _validate_treeherder_data(self, parser):
        # late import is required, because install is done in create_virtualenv
        import jsonschema

        if len(parser.found_perf_data) != 1:
            self.critical("PERFHERDER_DATA was seen %d times, expected 1."
                          % len(parser.found_perf_data))
            parser.update_worst_log_and_tbpl_levels(WARNING, TBPL_WARNING)
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
            parser.update_worst_log_and_tbpl_levels(WARNING, TBPL_WARNING)

    def _artifact_perf_data(self, dest):
        src = os.path.join(self.query_abs_dirs()['abs_work_dir'], 'local.json')
        try:
            shutil.copyfile(src, dest)
        except:
            self.critical("Error copying results %s to upload dir %s" % (src, dest))
            parser.update_worst_log_and_tbpl_levels(CRITICAL, TBPL_FAILURE)

    def run_tests(self, args=None, **kw):
        """run Talos tests"""

        # get talos options
        options = self.talos_options(args=args, **kw)

        # XXX temporary python version check
        python = self.query_python_path()
        self.run_command([python, "--version"])
        parser = TalosOutputParser(config=self.config, log_obj=self.log_obj,
                                   error_list=TalosErrorList)
        env = {}
        env['MOZ_UPLOAD_DIR'] = self.query_abs_dirs()['abs_blob_upload_dir']
        if not self.run_local:
            env['MINIDUMP_STACKWALK'] = self.query_minidump_stackwalk()
        env['MINIDUMP_SAVE_PATH'] = self.query_abs_dirs()['abs_blob_upload_dir']
        env['RUST_BACKTRACE'] = '1'
        if not os.path.isdir(env['MOZ_UPLOAD_DIR']):
            self.mkdir_p(env['MOZ_UPLOAD_DIR'])
        env = self.query_env(partial_env=env, log_level=INFO)
        # adjust PYTHONPATH to be able to use talos as a python package
        if 'PYTHONPATH' in env:
            env['PYTHONPATH'] = self.talos_path + os.pathsep + env['PYTHONPATH']
        else:
            env['PYTHONPATH'] = self.talos_path

        # mitmproxy needs path to mozharness when installing the cert
        env['SCRIPTSPATH'] = scripts_path

        if self.repo_path is not None:
            env['MOZ_DEVELOPER_REPO_DIR'] = self.repo_path
        if self.obj_path is not None:
            env['MOZ_DEVELOPER_OBJ_DIR'] = self.obj_path

        # sets a timeout for how long talos should run without output
        output_timeout = self.config.get('talos_output_timeout', 3600)
        # run talos tests
        run_tests = os.path.join(self.talos_path, 'talos', 'run_tests.py')

        mozlog_opts = ['--log-tbpl-level=debug']
        if not self.run_local and 'suite' in self.config:
            fname_pattern = '%s_%%s.log' % self.config['suite']
            mozlog_opts.append('--log-errorsummary=%s'
                               % os.path.join(env['MOZ_UPLOAD_DIR'],
                                              fname_pattern % 'errorsummary'))
            mozlog_opts.append('--log-raw=%s'
                               % os.path.join(env['MOZ_UPLOAD_DIR'],
                                              fname_pattern % 'raw'))

        command = [python, run_tests] + options + mozlog_opts
        self.return_code = self.run_command(command, cwd=self.workdir,
                                            output_timeout=output_timeout,
                                            output_parser=parser,
                                            env=env)
        if parser.minidump_output:
            self.info("Looking at the minidump files for debugging purposes...")
            for item in parser.minidump_output:
                self.run_command(["ls", "-l", item])

        if self.return_code not in [0]:
            # update the worst log level and tbpl status
            log_level = ERROR
            tbpl_level = TBPL_FAILURE
            if self.return_code == 1:
                log_level = WARNING
                tbpl_level = TBPL_WARNING
            if self.return_code == 4:
                log_level = WARNING
                tbpl_level = TBPL_RETRY

            parser.update_worst_log_and_tbpl_levels(log_level, tbpl_level)
        elif '--no-upload-results' not in options:
            if not self.gecko_profile:
                self._validate_treeherder_data(parser)
                if not self.run_local:
                    # copy results to upload dir so they are included as an artifact
                    dest = os.path.join(env['MOZ_UPLOAD_DIR'], 'perfherder-data.json')
                    self._artifact_perf_data(dest)

        self.buildbot_status(parser.worst_tbpl_status,
                             level=parser.worst_log_level)

    def fetch_python3(self):
        manifest_file = os.path.join(
            self.talos_path,
            'talos',
            'mitmproxy',
            self.config.get('python3_manifest')[self.platform_name()])
        output_dir = self.query_abs_dirs()['abs_work_dir']
        # Slowdown: The unzipped Python3 installation gets deleted every time
        self.tooltool_fetch(
            manifest_file,
            output_dir=output_dir,
            cache=self.config.get('tooltool_cache')
        )
        python3_path = os.path.join(output_dir, 'python3.6', 'python')
        self.run_command([python3_path, '--version'], env=self.query_env())
        return python3_path
