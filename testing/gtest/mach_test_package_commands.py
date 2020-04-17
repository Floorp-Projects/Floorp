# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import sys
from argparse import Namespace

from mach.decorators import (
    CommandProvider,
    Command,
)

here = os.path.abspath(os.path.dirname(__file__))
parser = None
logger = None


def run_gtest(context, **kwargs):
    from mozlog.commandline import setup_logging

    if not kwargs.get('log'):
        kwargs['log'] = setup_logging('gtest', kwargs, {'mach': sys.stdout})
    global logger
    logger = kwargs['log']

    args = Namespace(**kwargs)

    import mozinfo
    if mozinfo.info.get('buildapp') == 'mobile/android':
        return run_gtest_android(context, args)
    return run_gtest_desktop(context, args)


def run_gtest_desktop(context, args):
    prog = context.firefox_bin
    xre_path = os.path.dirname(context.firefox_bin)
    if sys.platform == 'darwin':
        xre_path = os.path.join(xre_path, 'Resources')
    utility_path = context.bin_dir
    cwd = os.path.join(context.package_root, 'gtest')

    logger.info("mach calling run_gtest with prog=%s xre_path=%s cwd=%s utility_path=%s" %
                (prog, xre_path, cwd, utility_path))
    # The gtest option parser ignores some options normally passed to the mozharness
    # command, so some hacking is required, for now:
    extra_args = [arg for arg in args.args if not arg.startswith('-')]
    if extra_args:
        os.environ['GTEST_FILTER'] = extra_args[0]
        logger.info("GTEST_FILTER=%s" % extra_args[0])

    import rungtests
    tester = rungtests.GTests()
    return tester.run_gtest(prog,
                            xre_path,
                            cwd,
                            utility_path=utility_path,
                            enable_webrender=args.enable_webrender)


def run_gtest_android(context, args):
    config = context.mozharness_config
    if config:
        args.adb_path = config['exes']['adb'] % {'abs_work_dir': context.mozharness_workdir}
    cwd = os.path.join(context.package_root, 'gtest')
    libxul_path = os.path.join(cwd, 'gtest_bin', 'gtest', 'libxul.so')

    logger.info("mach calling android run_gtest with package=%s cwd=%s libxul=%s" %
                (args.package, cwd, libxul_path))
    # The remote gtest option parser ignores some options normally passed to the mozharness
    # command, so some hacking is required, for now:
    extra_args = [arg for arg in args.args if not arg.startswith('-')]
    test_filter = extra_args[0] if extra_args else None
    logger.info("test filter=%s" % test_filter)

    import remotegtests
    tester = remotegtests.RemoteGTests()
    return tester.run_gtest(cwd,
                            args.shuffle, test_filter, args.package,
                            args.adb_path, args.device_serial,
                            args.remote_test_root, libxul_path,
                            args.symbols_path, args.enable_webrender)


def setup_argument_parser():
    import mozinfo
    mozinfo.find_and_update_from_json(here)
    global parser
    if mozinfo.info.get('buildapp') == 'mobile/android':
        import remotegtests
        parser = remotegtests.remoteGtestOptions()
    else:
        import rungtests
        parser = rungtests.gtestOptions()
    return parser


@CommandProvider
class GtestCommands(object):

    def __init__(self, context):
        self.context = context

    @Command('gtest', category='testing',
             description='Run the gtest harness.',
             parser=setup_argument_parser)
    def gtest(self, **kwargs):
        self.context.activate_mozharness_venv()
        result = run_gtest(self.context, **kwargs)
        return 0 if result else 1
