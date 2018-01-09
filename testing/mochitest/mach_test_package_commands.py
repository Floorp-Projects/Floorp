# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import sys
from argparse import Namespace
from functools import partial

from mach.decorators import (
    CommandProvider,
    Command,
)

here = os.path.abspath(os.path.dirname(__file__))
parser = None


def run_mochitest(context, **kwargs):
    from mochitest_options import ALL_FLAVORS
    from mozlog.commandline import setup_logging

    if not kwargs.get('log'):
        kwargs['log'] = setup_logging('mochitest', kwargs, {'mach': sys.stdout})

    flavor = kwargs.get('flavor') or 'mochitest'
    if flavor not in ALL_FLAVORS:
        for fname, fobj in ALL_FLAVORS.iteritems():
            if flavor in fobj['aliases']:
                flavor = fname
                break
    fobj = ALL_FLAVORS[flavor]
    kwargs.update(fobj.get('extra_args', {}))

    args = Namespace(**kwargs)
    args.e10s = context.mozharness_config.get('e10s', args.e10s)
    args.certPath = context.certs_dir

    if args.test_paths:
        install_subdir = fobj.get('install_subdir', fobj['suite'])
        test_root = os.path.join(context.package_root, 'mochitest', install_subdir)
        normalize = partial(context.normalize_test_path, test_root)
        args.test_paths = map(normalize, args.test_paths)

    import mozinfo
    if mozinfo.info.get('buildapp') == 'mobile/android':
        return run_mochitest_android(context, args)
    return run_mochitest_desktop(context, args)


def run_mochitest_desktop(context, args):
    args.app = args.app or context.firefox_bin
    args.utilityPath = context.bin_dir
    args.extraProfileFiles.append(os.path.join(context.bin_dir, 'plugins'))

    from runtests import run_test_harness
    return run_test_harness(parser, args)


def run_mochitest_android(context, args):
    args.app = args.app or 'org.mozilla.fennec'
    args.extraProfileFiles.append(os.path.join(context.package_root, 'mochitest', 'fonts'))
    args.utilityPath = context.hostutils
    args.xrePath = context.hostutils

    config = context.mozharness_config
    if config:
        args.remoteWebServer = config['remote_webserver']
        args.httpPort = config['emulator']['http_port']
        args.sslPort = config['emulator']['ssl_port']
        args.adbPath = config['exes']['adb'] % {'abs_work_dir': context.mozharness_workdir}

    from runtestsremote import run_test_harness
    return run_test_harness(parser, args)


def setup_argument_parser():
    import mozinfo
    mozinfo.find_and_update_from_json(here)
    app = 'generic'
    if mozinfo.info.get('buildapp') == 'mobile/android':
        app = 'android'

    from mochitest_options import MochitestArgumentParser
    global parser
    parser = MochitestArgumentParser(app=app)
    return parser


@CommandProvider
class MochitestCommands(object):

    def __init__(self, context):
        self.context = context

    @Command('mochitest', category='testing',
             description='Run the mochitest harness.',
             parser=setup_argument_parser)
    def mochitest(self, **kwargs):
        self.context.activate_mozharness_venv()
        return run_mochitest(self.context, **kwargs)
