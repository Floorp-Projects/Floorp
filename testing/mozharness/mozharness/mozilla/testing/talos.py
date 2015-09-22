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
import pprint
import copy
import re

from mozharness.base.config import parse_config_file
from mozharness.base.errors import PythonErrorList
from mozharness.base.log import OutputParser, DEBUG, ERROR, CRITICAL, FATAL
from mozharness.base.log import INFO, WARNING
from mozharness.mozilla.blob_upload import BlobUploadMixin, blobupload_config_options
from mozharness.mozilla.testing.testbase import TestingMixin, testing_config_options, INSTALLER_SUFFIXES
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.mozilla.testing.errors import TinderBoxPrintRe
from mozharness.mozilla.buildbot import TBPL_SUCCESS, TBPL_WORST_LEVEL_TUPLE
from mozharness.mozilla.buildbot import TBPL_RETRY, TBPL_FAILURE, TBPL_WARNING

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
    RE_TALOSDATA = re.compile(r'.*?TALOSDATA:\s+(\[.*\])')
    worst_tbpl_status = TBPL_SUCCESS

    def __init__(self, **kwargs):
        super(TalosOutputParser, self).__init__(**kwargs)
        self.minidump_output = None
        self.num_times_found_talosdata = 0

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

        if self.RE_TALOSDATA.match(line):
            self.num_times_found_talosdata += 1

        # now let's check if buildbot should retry
        harness_retry_re = TinderBoxPrintRe['harness_error']['retry_regex']
        if harness_retry_re.search(line):
            self.critical(' %s' % line)
            self.update_worst_log_and_tbpl_levels(CRITICAL, TBPL_RETRY)
            return  # skip base parse_single_line
        super(TalosOutputParser, self).parse_single_line(line)


class Talos(TestingMixin, MercurialScript, BlobUploadMixin):
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
        [["--spsProfile"], {
            "dest": "sps_profile",
            "action": "store_true",
            "default": False,
            "help": "Whether or not to profile the test run and save the profile results"
        }],
        [["--spsProfileInterval"], {
            "dest": "sps_profile_interval",
            "type": "int",
            "default": 0,
            "help": "The interval between samples taken by the profiler (milliseconds)"
        }],
    ] + testing_config_options + copy.deepcopy(blobupload_config_options)

    def __init__(self, **kwargs):
        kwargs.setdefault('config_options', self.config_options)
        kwargs.setdefault('all_actions', ['clobber',
                                          'read-buildbot-config',
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
        super(Talos, self).__init__(**kwargs)

        self.workdir = self.query_abs_dirs()['abs_work_dir']  # convenience

        self.run_local = self.config.get('run_local')
        self.installer_url = self.config.get("installer_url")
        self.talos_json_url = self.config.get("talos_json_url")
        self.talos_json = self.config.get("talos_json")
        self.talos_json_config = self.config.get("talos_json_config")
        self.tests = None
        self.pagesets_url = None
        self.pagesets_parent_dir_path = None
        self.pagesets_manifest_path = None
        self.abs_pagesets_paths = None
        self.pagesets_manifest_filename = None
        self.pagesets_manifest_parent_path = None
        self.sps_profile = self.config.get('sps_profile')
        self.sps_profile_interval = self.config.get('sps_profile_interval')

    # We accept some configuration options from the try commit message in the format mozharness: <options>
    # Example try commit message:
    #   mozharness: --spsProfile try: <stuff>
    def query_sps_profile_options(self):
        sps_results = []
        if self.buildbot_config:
            # this is inside automation
            # now let's see if we added spsProfile specs in the commit message
            junk, junk, opts = self.buildbot_config['sourcestamp']['changes'][-1]['comments'].partition('mozharness:')
            if opts:
              opts = re.sub(r'\w+:.*', '', opts).strip().split(' ')
              if "--spsProfile" in opts:
                  # overwrite whatever was set here.
                  self.sps_profile = True
              try:
                    idx = opts.index('--spsProfileInterval')
                    if len(opts) > idx + 1:
                        self.sps_profile_interval = opts[idx + 1]
              except ValueError:
                  pass
        # finally, if sps_profile is set, we add that to the talos options
        if self.sps_profile:
            sps_results.append('--spsProfile')
            if self.sps_profile_interval:
                sps_results.extend(
                    ['--spsProfileInterval', str(self.sps_profile_interval)]
                )
        return sps_results

    def query_abs_dirs(self):
        c = self.config
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(Talos, self).query_abs_dirs()
        abs_dirs['abs_blob_upload_dir'] = os.path.join(abs_dirs['abs_work_dir'], 'blobber_upload_dir')
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def query_talos_json_url(self):
        """Hacky, but I haven't figured out a better way to get the
        talos json url before we install the build.

        We can't get this information after we install the build, because
        we have to create the virtualenv to use mozinstall, and talos_url
        is specified in the talos json.
        """
        if self.talos_json_url:
            return self.talos_json_url
        self.info("Guessing talos json url...")
        if not self.installer_url:
            self.read_buildbot_config()
            self.postflight_read_buildbot_config()
            if not self.installer_url:
                self.fatal("Can't figure out talos_json_url without an installer_url!")
        for suffix in INSTALLER_SUFFIXES:
            if self.installer_url.endswith(suffix):
                build_txt_url = self.installer_url[:-len(suffix)] + '.txt'
                break
        else:
            self.fatal("Can't figure out talos_json_url from installer_url %s!" % self.installer_url)
        build_txt_file = self.download_file(build_txt_url, parent_dir=self.workdir)
        if not build_txt_file:
            self.fatal("Can't download %s to guess talos_json_url!" % build_txt_url)
        # HG hardcode?
        revision_re = re.compile(r'''([a-zA-Z]+://.+)/rev/([0-9a-fA-F]{10})''')
        contents = self.read_from_file(build_txt_file, error_level=FATAL).splitlines()
        for line in contents:
            m = revision_re.match(line)
            if m:
                break
        else:
            self.fatal("Can't figure out talos_json_url from %s!" % build_txt_file)
        self.talos_json_url = "%s/raw-file/%s/testing/talos/talos.json" % (m.group(1), m.group(2))
        return self.talos_json_url

    def download_talos_json(self):
        talos_json_url = self.query_talos_json_url()
        self.talos_json = self.download_file(talos_json_url,
                                             parent_dir=self.workdir,
                                             error_level=FATAL)

    def query_talos_json_config(self):
        """Return the talos json config; download and read from the
        talos_json_url if need be."""
        if self.talos_json_config:
            return self.talos_json_config
        if not self.talos_json:
            talos_json_url = self.query_talos_json_url()
            if not talos_json_url:
                self.fatal("Can't download talos_json without a talos_json_url!")
            self.download_talos_json()
        self.talos_json_config = parse_config_file(self.talos_json)
        self.info(pprint.pformat(self.talos_json_config))
        return self.talos_json_config

    def query_pagesets_url(self):
        """Certain suites require external pagesets to be downloaded and
        extracted.
        """
        if self.pagesets_url:
            return self.pagesets_url
        if self.query_talos_json_config() and 'suite' in self.config:
            self.pagesets_url = self.talos_json_config['suites'][self.config['suite']].get('pagesets_url')
            return self.pagesets_url

    def query_pagesets_parent_dir_path(self):
        """ We have to copy the pageset into the webroot separately.

        Helper method to avoid hardcodes.
        """
        if self.pagesets_parent_dir_path:
            return self.pagesets_parent_dir_path
        if self.query_talos_json_config():
            self.pagesets_parent_dir_path = self.talos_json_config['suites'][self.config['suite']].get('pagesets_parent_dir_path')
            return self.pagesets_parent_dir_path

    def query_pagesets_manifest_path(self):
        """ We have to copy the tp manifest from webroot to talos root when
        those two directories aren't the same, until bug 795172 is fixed.

        Helper method to avoid hardcodes.
        """
        if self.pagesets_manifest_path:
            return self.pagesets_manifest_path
        if self.query_talos_json_config():
            self.pagesets_manifest_path = self.talos_json_config['suites'][self.config['suite']].get('pagesets_manifest_path')
            return self.pagesets_manifest_path

    def query_pagesets_manifest_filename(self):
        if self.pagesets_manifest_filename:
            return self.pagesets_manifest_filename
        else:
            manifest_path = self.query_pagesets_manifest_path()
            self.pagesets_manifest_filename = os.path.basename(manifest_path)
            return self.pagesets_manifest_filename

    def query_pagesets_manifest_parent_path(self):
        if self.pagesets_manifest_parent_path:
            return self.pagesets_manifest_parent_path
        if self.query_talos_json_config():
            manifest_path = self.query_pagesets_manifest_path()
            self.pagesets_manifest_parent_path = os.path.dirname(manifest_path)
            return self.pagesets_manifest_parent_path

    def query_abs_pagesets_paths(self):
        """ Returns a bunch of absolute pagesets directory paths.
        We need this to make the dir and copy the manifest to the local dir.
        """
        if self.abs_pagesets_paths:
            return self.abs_pagesets_paths
        else:
            paths = {}
            manifest_parent_path = self.query_pagesets_manifest_parent_path()
            paths['pagesets_manifest_parent'] = os.path.join(self.talos_path, manifest_parent_path)

            manifest_path = self.query_pagesets_manifest_path()
            paths['pagesets_manifest'] = os.path.join(self.talos_path, manifest_path)

            self.abs_pagesets_paths = paths
            return self.abs_pagesets_paths

    def talos_options(self, args=None, **kw):
        """return options to talos"""
        # binary path
        binary_path = self.binary_path or self.config.get('binary_path')
        if not binary_path:
            self.fatal("Talos requires a path to the binary.  You can specify binary_path or add download-and-extract to your action list.")

        # talos options
        options = []
        if self.config.get('python_webserver', True):
            options.append('--develop')
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
        kw_options.update(kw)
        # talos expects tests to be in the format (e.g.) 'ts:tp5:tsvg'
        tests = kw_options.get('activeTests')
        if tests and not isinstance(tests, basestring):
            tests = ':'.join(tests)  # Talos expects this format
            kw_options['activeTests'] = tests
        for key, value in kw_options.items():
            options.extend(['--%s' % key, value])
        # configure profiling options
        options.extend(self.query_sps_profile_options())
        # extra arguments
        if args is not None:
            options += args
        if 'talos_extra_options' in self.config:
            options += self.config['talos_extra_options']
        return options

    def talos_conf_path(self, conf):
        """return the full path for a talos .yml configuration file"""
        if os.path.isabs(conf):
            return conf
        return os.path.join(self.workdir, conf)

    def populate_webroot(self):
        """Populate the production test slaves' webroots"""
        c = self.config
        if not c.get('webroot'):
            self.fatal("webroot need to be set to populate_webroot!")
        self.info("Populating webroot %s..." % c['webroot'])
        talos_webdir = os.path.join(c['webroot'], 'talos')
        self.mkdir_p(c['webroot'], error_level=FATAL)
        self.rmtree(talos_webdir, error_level=FATAL)

        self.talos_path = os.path.join(
            self.query_abs_dirs()['abs_work_dir'], 'tests', 'talos'
        )
        if c.get('run_local'):
            self.talos_path = os.path.dirname(self.talos_json)

        # the apache server needs the talos directory (talos/talos)
        # to be in the webroot
        src_talos_webdir = os.path.join(self.talos_path, 'talos')
        self.copytree(src_talos_webdir, talos_webdir)

        if self.query_pagesets_url():
            self.info("Downloading pageset...")
            pagesets_path = os.path.join(c['webroot'], self.query_pagesets_parent_dir_path())
            self._download_unzip(self.pagesets_url, pagesets_path)

            # mkdir for the missing manifest directory in talos_repo/talos/page_load_test directory
            abs_pagesets_paths = self.query_abs_pagesets_paths()
            abs_manifest_parent_path = abs_pagesets_paths['pagesets_manifest_parent']
            self.mkdir_p(abs_manifest_parent_path, error_level=FATAL)

            # copy all the manifest file from unzipped zip file into the manifest dir
            src_manifest_file = os.path.join(c['webroot'], self.query_pagesets_manifest_path())
            dest_manifest_file = abs_pagesets_paths['pagesets_manifest']
            self.copyfile(src_manifest_file, dest_manifest_file, error_level=FATAL)
        if 'suite' in c:
            plugins_url = self.talos_json_config['suites'][c['suite']].get('plugins', {}).get(c['system_bits'])
            if plugins_url:
                self.info("Downloading plugin...")
                # TODO add this path to talos.json ?
                self._download_unzip(plugins_url, os.path.join(talos_webdir, 'base_profile'))

    # Action methods. {{{1
    # clobber defined in BaseScript
    # read_buildbot_config defined in BuildbotMixin

    def download_and_extract(self, target_unzip_dirs=None, suite_categories=None):
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

    def postflight_create_virtualenv(self):
        """ This belongs in download_and_install() but requires the
        virtualenv to be set up :(

        The real fix here may be a --tpmanifest option for PerfConfigurator.
        """
        c = self.config
        if not c.get('python_webserver', True) and self.query_pagesets_url():
            pagesets_path = self.query_pagesets_manifest_path()
            manifest_source = os.path.join(c['webroot'], pagesets_path)
            manifest_target = os.path.join(self.query_python_site_packages_path(), pagesets_path)
            self.mkdir_p(os.path.dirname(manifest_target))
            self.copyfile(manifest_source, manifest_target)

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
        if not os.path.isdir(env['MOZ_UPLOAD_DIR']):
            self.mkdir_p(env['MOZ_UPLOAD_DIR'])
        env = self.query_env(partial_env=env, log_level=INFO)
        # adjust PYTHONPATH to be able to use talos as a python package
        if 'PYTHONPATH' in env:
            env['PYTHONPATH'] = self.talos_path + os.pathsep + env['PYTHONPATH']
        else:
            env['PYTHONPATH'] = self.talos_path

        # sets a timeout for how long talos should run without output
        output_timeout = self.config.get('talos_output_timeout', 3600)
        # run talos tests
        run_tests = os.path.join(self.talos_path, 'talos', 'run_tests.py')
        command = [python, run_tests, '--debug'] + options
        self.return_code = self.run_command(command, cwd=self.workdir,
                                            output_timeout=output_timeout,
                                            output_parser=parser,
                                            env=env)
        if parser.minidump_output:
            self.info("Looking at the minidump files for debugging purposes...")
            for item in parser.minidump_output:
                self.run_command(["ls", "-l", item])
        if parser.num_times_found_talosdata != 1:
            self.critical("TALOSDATA was seen %d times, expected 1."
                          % parser.num_times_found_talosdata)
            parser.update_worst_log_and_tbpl_levels(WARNING, TBPL_WARNING)

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

        self.buildbot_status(parser.worst_tbpl_status,
                             level=parser.worst_log_level)
