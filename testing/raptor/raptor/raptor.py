#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import

import json
import os
import sys

import mozinfo

from mozlog import commandline, get_default_logger
from mozprofile import create_profile
from mozrunner import runners

# need this so raptor imports work both from /raptor and via mach
here = os.path.abspath(os.path.dirname(__file__))
webext_dir = os.path.join(os.path.dirname(here), 'webext')
sys.path.insert(0, here)

try:
    from mozbuild.base import MozbuildObject
    build = MozbuildObject.from_environment(cwd=here)
except ImportError:
    build = None

from benchmark import Benchmark
from cmdline import parse_args
from control_server import RaptorControlServer
from gen_test_config import gen_test_config
from outputhandler import OutputHandler
from manifest import get_raptor_test_list
from playback import get_playback
from results import RaptorResultsHandler


class Raptor(object):
    """Container class for Raptor"""

    def __init__(self, app, binary, run_local=False, obj_path=None):
        self.config = {}
        self.config['app'] = app
        self.config['binary'] = binary
        self.config['platform'] = mozinfo.os
        self.config['run_local'] = run_local
        self.config['obj_path'] = obj_path
        self.raptor_venv = os.path.join(os.getcwd(), 'raptor-venv')
        self.log = get_default_logger(component='raptor-main')
        self.control_server = None
        self.playback = None
        self.benchmark = None

        # Create the profile
        self.profile = create_profile(self.config['app'])

        # Merge in base profiles
        with open(os.path.join(self.profile_data_dir, 'profiles.json'), 'r') as fh:
            base_profiles = json.load(fh)['raptor']

        for name in base_profiles:
            path = os.path.join(self.profile_data_dir, name)
            self.log.info("Merging profile: {}".format(path))
            self.profile.merge(path)

        # create results holder
        self.results_handler = RaptorResultsHandler()

        # Create the runner
        self.output_handler = OutputHandler()
        process_args = {
            'processOutputLine': [self.output_handler],
        }
        runner_cls = runners[app]
        self.runner = runner_cls(
            binary, profile=self.profile, process_args=process_args)

        self.log.info("raptor config: %s" % str(self.config))

    @property
    def profile_data_dir(self):
        if 'MOZ_DEVELOPER_REPO_DIR' in os.environ:
            return os.path.join(os.environ['MOZ_DEVELOPER_REPO_DIR'], 'testing', 'profiles')
        if build:
            return os.path.join(build.topsrcdir, 'testing', 'profiles')
        return os.path.join(here, 'profile_data')

    def start_control_server(self):
        self.control_server = RaptorControlServer(self.results_handler)
        self.control_server.start()

    def get_playback_config(self, test):
        self.config['playback_tool'] = test.get('playback')
        self.log.info("test uses playback tool: %s " % self.config['playback_tool'])
        self.config['playback_binary_manifest'] = test.get('playback_binary_manifest', None)
        _key = 'playback_binary_zip_%s' % self.config['platform']
        self.config['playback_binary_zip'] = test.get(_key, None)
        self.config['playback_pageset_manifest'] = test.get('playback_pageset_manifest', None)
        _key = 'playback_pageset_zip_%s' % self.config['platform']
        self.config['playback_pageset_zip'] = test.get(_key, None)
        self.config['playback_recordings'] = test.get('playback_recordings', None)

    def run_test(self, test, timeout=None):
        self.log.info("starting raptor test: %s" % test['name'])
        self.log.info("test settings: %s" % str(test))
        self.log.info("raptor config: %s" % str(self.config))

        # benchmark-type tests require the benchmark test to be served out
        if test.get('type') == "benchmark":
            self.benchmark = Benchmark(self.config, test)
            benchmark_port = int(self.benchmark.port)
        else:
            benchmark_port = 0

        gen_test_config(self.config['app'],
                        test['name'],
                        self.control_server.port,
                        benchmark_port)

        # must intall raptor addon each time because we dynamically update some content
        raptor_webext = os.path.join(webext_dir, 'raptor')
        self.log.info("installing webext %s" % raptor_webext)
        self.profile.addons.install(raptor_webext)
        # on firefox we can get an addon id; chrome addon actually is just cmd line arg
        if self.config['app'] == "firefox":
            webext_id = self.profile.addons.addon_details(raptor_webext)['id']

        # some tests require tools to playback the test pages
        if test.get('playback', None) is not None:
            self.get_playback_config(test)
            # startup the playback tool
            self.playback = get_playback(self.config)

        self.runner.start()

        proc = self.runner.process_handler
        self.output_handler.proc = proc
        self.control_server.browser_proc = proc

        try:
            self.runner.wait(timeout)
        finally:
            try:
                self.runner.check_for_crashes()
            except NotImplementedError:  # not implemented for Chrome
                pass

        if self.playback is not None:
            self.playback.stop()

        # remove the raptor webext; as it must be reloaded with each subtest anyway
        # applies to firefox only; chrome the addon is actually just cmd line arg
        if self.config['app'] == "firefox":
            self.log.info("removing webext %s" % raptor_webext)
            self.profile.addons.remove_addon(webext_id)

        if self.runner.is_running():
            self.log("Application timed out after {} seconds".format(timeout))
            self.runner.stop()

    def process_results(self):
        # when running locally output results in build/raptor.json; when running
        # in production output to a local.json to be turned into tc job artifact
        if self.config.get('run_local', False):
            if 'MOZ_DEVELOPER_REPO_DIR' in os.environ:
                raptor_json_path = os.path.join(os.environ['MOZ_DEVELOPER_REPO_DIR'],
                                                'testing', 'mozharness', 'build', 'raptor.json')
            else:
                raptor_json_path = os.path.join(here, 'raptor.json')
        else:
            raptor_json_path = os.path.join(os.getcwd(), 'local.json')

        self.config['raptor_json_path'] = raptor_json_path
        return self.results_handler.summarize_and_output(self.config)

    def clean_up(self):
        self.control_server.stop()
        self.runner.stop()
        self.log.info("finished")


def main(args=sys.argv[1:]):
    args = parse_args()
    commandline.setup_logging('raptor', args, {'tbpl': sys.stdout})
    LOG = get_default_logger(component='raptor-main')

    LOG.info("received command line arguments: %s" % str(args))

    # if a test name specified on command line, and it exists, just run that one
    # otherwise run all available raptor tests that are found for this browser
    raptor_test_list = get_raptor_test_list(args, mozinfo.os)

    # ensure we have at least one valid test to run
    if len(raptor_test_list) == 0:
        LOG.critical("abort: no tests found")
        sys.exit(1)

    LOG.info("raptor tests scheduled to run:")
    for next_test in raptor_test_list:
        LOG.info(next_test['name'])

    raptor = Raptor(args.app, args.binary, args.run_local, args.obj_path)

    raptor.start_control_server()

    for next_test in raptor_test_list:
        raptor.run_test(next_test)

    success = raptor.process_results()
    raptor.clean_up()

    if not success:
        # didn't get test results; test timed out or crashed, etc. we want job to fail
        LOG.critical("error: no raptor test results were found")
        os.sys.exit(1)


if __name__ == "__main__":
    main()
