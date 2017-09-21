# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import argparse
import os
import sys

from mozbuild.base import (
    MachCommandBase,
    MachCommandConditions as conditions,
)

from mach.decorators import (
    CommandArgument,
    CommandArgumentGroup,
    CommandProvider,
    Command,
)

def setup_awsy_argument_parser():
    from marionette_harness.runtests import MarionetteArguments
    from mozlog.structured import commandline

    parser = MarionetteArguments()
    commandline.add_logging_group(parser)

    return parser


@CommandProvider
class MachCommands(MachCommandBase):
    AWSY_PATH = os.path.dirname(os.path.realpath(__file__))
    if AWSY_PATH not in sys.path:
        sys.path.append(AWSY_PATH)
    from awsy import ITERATIONS, PER_TAB_PAUSE, SETTLE_WAIT_TIME, MAX_TABS

    def run_awsy(self, tests, binary=None, **kwargs):
        import json
        from mozlog.structured import commandline

        from marionette_harness.runtests import (
            MarionetteTestRunner,
            MarionetteHarness
        )

        parser = setup_awsy_argument_parser()

        awsy_source_dir = os.path.join(self.topsrcdir, 'testing', 'awsy')
        if not tests:
            tests = [os.path.join(awsy_source_dir,
                                  'awsy',
                                  'test_memory_usage.py')]

        args = argparse.Namespace(tests=tests)

        args.binary = binary

        if kwargs['quick']:
            kwargs['entities'] = 3
            kwargs['iterations'] = 1
            kwargs['perTabPause'] = 1
            kwargs['settleWaitTime'] = 1

        if 'disable_stylo' in kwargs and kwargs['disable_stylo']:
            if 'single_stylo_traversal' in kwargs and kwargs['single_stylo_traversal']:
                print("--disable-stylo conflicts with --single-stylo-traversal")
                return 1
            if 'enable_stylo' in kwargs and kwargs['enable_stylo']:
                print("--disable-stylo conflicts with --enable-stylo")
                return 1

        if 'single_stylo_traversal' in kwargs and kwargs['single_stylo_traversal']:
            os.environ['STYLO_THREADS'] = '1'
        else:
            os.environ['STYLO_THREADS'] = '4'

        if 'enable_stylo' in kwargs and kwargs['enable_stylo']:
            os.environ['STYLO_FORCE_ENABLED'] = '1'
        if 'disable_stylo' in kwargs and kwargs['disable_stylo']:
            os.environ['STYLO_FORCE_DISABLED'] = '1'

        runtime_testvars = {}
        for arg in ('webRootDir', 'pageManifest', 'resultsDir', 'entities', 'iterations',
                    'perTabPause', 'settleWaitTime', 'maxTabs', 'dmd'):
            if kwargs[arg]:
                runtime_testvars[arg] = kwargs[arg]

        if 'webRootDir' not in runtime_testvars:
            awsy_tests_dir = os.path.join(self.topobjdir, '_tests', 'awsy')
            web_root_dir = os.path.join(awsy_tests_dir, 'html')
            runtime_testvars['webRootDir'] = web_root_dir
        else:
            web_root_dir = runtime_testvars['webRootDir']
            awsy_tests_dir = os.path.dirname(web_root_dir)

        if 'resultsDir' not in runtime_testvars:
            runtime_testvars['resultsDir'] = os.path.join(awsy_tests_dir,
                                                          'results')
        page_load_test_dir = os.path.join(web_root_dir, 'page_load_test')
        if not os.path.isdir(page_load_test_dir):
            os.makedirs(page_load_test_dir)

        if not os.path.isdir(runtime_testvars['resultsDir']):
            os.makedirs(runtime_testvars['resultsDir'])

        runtime_testvars_path = os.path.join(awsy_tests_dir, 'runtime-testvars.json')
        if kwargs['testvars']:
            kwargs['testvars'].append(runtime_testvars_path)
        else:
            kwargs['testvars'] = [runtime_testvars_path]

        runtime_testvars_file = open(runtime_testvars_path, 'wb')
        runtime_testvars_file.write(json.dumps(runtime_testvars, indent=2))
        runtime_testvars_file.close()

        manifest_file = os.path.join(awsy_source_dir,
                                     'tp5n-pageset.manifest')
        tooltool_args = {'args': [
            sys.executable,
            os.path.join(self.topsrcdir, 'mach'),
            'artifact', 'toolchain', '-v',
            '--tooltool-manifest=%s' % manifest_file,
            '--cache-dir=%s' % os.path.join(self.topsrcdir, 'tooltool-cache'),
        ]}
        self.run_process(cwd=page_load_test_dir, **tooltool_args)
        tp5nzip = os.path.join(page_load_test_dir, 'tp5n.zip')
        tp5nmanifest = os.path.join(page_load_test_dir, 'tp5n', 'tp5n.manifest')
        if not os.path.exists(tp5nmanifest):
            unzip_args = {'args': [
                'unzip',
                '-q',
                '-o',
                tp5nzip,
                '-d',
                page_load_test_dir]}
            self.run_process(**unzip_args)

        # If '--preferences' was not specified supply our default set.
        if not kwargs['prefs_files']:
            kwargs['prefs_files'] = [os.path.join(awsy_source_dir, 'conf', 'prefs.json')]

        # Setup DMD env vars if necessary.
        if kwargs['dmd']:
            dmd_params = []

            bin_dir = os.path.dirname(binary)
            lib_name = self.substs['DLL_PREFIX'] + 'dmd' + self.substs['DLL_SUFFIX']
            dmd_lib = os.path.join(bin_dir, lib_name)
            if not os.path.exists(dmd_lib):
                print("Please build with |--enable-dmd| to use DMD.")
                return 1

            env_vars = {
                "Darwin": {
                    "DYLD_INSERT_LIBRARIES": dmd_lib,
                    "LD_LIBRARY_PATH": bin_dir,
                },
                "Linux": {
                    "LD_PRELOAD": dmd_lib,
                    "LD_LIBRARY_PATH": bin_dir,
                },
                "WINNT": {
                    "MOZ_REPLACE_MALLOC_LIB": dmd_lib,
                },
            }

            arch = self.substs['OS_ARCH']
            for k, v in env_vars[arch].iteritems():
                os.environ[k] = v

            # Also add the bin dir to the python path so we can use dmd.py
            if bin_dir not in sys.path:
                sys.path.append(bin_dir)

        for k, v in kwargs.iteritems():
            setattr(args, k, v)

        parser.verify_usage(args)

        args.logger = commandline.setup_logging('Are We Slim Yet Tests',
                                                args,
                                                {'mach': sys.stdout})
        failed = MarionetteHarness(MarionetteTestRunner, args=vars(args)).run()
        if failed > 0:
            return 1
        else:
            return 0

    @Command('awsy-test', category='testing',
        description='Run Are We Slim Yet (AWSY) memory usage testing using marionette.',
        parser=setup_awsy_argument_parser,
    )
    @CommandArgumentGroup('AWSY')
    @CommandArgument('--web-root', group='AWSY', action='store', type=str,
                     dest='webRootDir',
                     help='Path to web server root directory. If not specified, '
                     'defaults to topobjdir/_tests/awsy/html.')
    @CommandArgument('--page-manifest', group='AWSY', action='store', type=str,
                     dest='pageManifest',
                     help='Path to page manifest text file containing a list '
                     'of urls to test. The urls must be served from localhost. If not '
                     'specified, defaults to page_load_test/tp5n/tp5n.manifest under '
                     'the web root.')
    @CommandArgument('--results', group='AWSY', action='store', type=str,
                     dest='resultsDir',
                     help='Path to results directory. If not specified, defaults '
                     'to the parent directory of the web root.')
    @CommandArgument('--quick', group='AWSY', action='store_true',
                     dest='quick', default=False,
                     help='Set --entities=3, --iterations=1, --per-tab-pause=1, '
                     '--settle-wait-time=1 for a quick test. Overrides any explicit '
                     'argument settings.')
    @CommandArgument('--entities', group='AWSY', action='store', type=int,
                     dest='entities',
                     help='Number of urls to load. Defaults to the total number of '
                     'urls.')
    @CommandArgument('--max-tabs', group='AWSY', action='store', type=int,
                     dest='maxTabs',
                     help='Maximum number of tabs to open. '
                     'Defaults to %s.' % MAX_TABS)
    @CommandArgument('--iterations', group='AWSY', action='store', type=int,
                     dest='iterations',
                     help='Number of times to run through the test suite. '
                     'Defaults to %s.' % ITERATIONS)
    @CommandArgument('--per-tab-pause', group='AWSY', action='store', type=int,
                     dest='perTabPause',
                     help='Seconds to wait in between opening tabs. '
                     'Defaults to %s.' % PER_TAB_PAUSE)
    @CommandArgument('--settle-wait-time', group='AWSY', action='store', type=int,
                     dest='settleWaitTime',
                     help='Seconds to wait for things to settled down. '
                     'Defaults to %s.' % SETTLE_WAIT_TIME)
    @CommandArgument('--enable-stylo', group='AWSY', action='store_true',
                     dest='enable_stylo', default=False,
                     help='Enable Stylo.')
    @CommandArgument('--disable-stylo', group='AWSY', action='store_true',
                     dest='disable_stylo', default=False,
                     help='Disable Stylo.')
    @CommandArgument('--single-stylo-traversal', group='AWSY', action='store_true',
                     dest='single_stylo_traversal', default=False,
                     help='Set STYLO_THREADS=1.')
    @CommandArgument('--dmd', group='AWSY', action='store_true',
                     dest='dmd', default=False,
                     help='Enable DMD during testing. Requires a DMD-enabled build.')
    def run_awsy_test(self, tests, **kwargs):
        """mach awsy-test runs the in-tree version of the Are We Slim Yet
        (AWSY) tests.

        awsy-test is implemented as a marionette test and marionette
        test arguments also apply although they are not necessary
        since reasonable defaults will be chosen.

        The AWSY specific arguments can be found in the Command
        Arguments for AWSY section below.

        awsy-test will automatically download the tp5n.zip talos
        pageset from tooltool and install it under
        topobjdir/_tests/awsy/html. You can specify your own page set
        by specifying --web-root and --page-manifest.

        The results of the test will be placed in the results
        directory specified by the --results argument.

        On Windows, you may experience problems due to path length
        errors when extracting the tp5n.zip file containing the
        test pages or when attempting to write checkpoints to the
        results directory. In that case, you should specify both
        the --web-root and --results arguments pointing to a location
        with a short path. For example:

        --web-root=c:\\\\tmp\\\\html --results=c:\\\\tmp\\\\results

        Note that the double backslashes are required.
        """
        kwargs['logger_name'] = 'Awsy Tests'
        if 'test_objects' in kwargs:
            tests = []
            for obj in kwargs['test_objects']:
                tests.append(obj['file_relpath'])
            del kwargs['test_objects']

        if not kwargs.get('binary') and conditions.is_firefox(self):
            kwargs['binary'] = self.get_binary_path('app')
        return self.run_awsy(tests, **kwargs)
