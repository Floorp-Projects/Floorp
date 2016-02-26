#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

from __future__ import print_function
import argparse
import os
import sys

from luciddream import LucidDreamTestCase
from marionette import Marionette
from marionette.runner import BaseMarionetteTestRunner
import marionette
import mozlog


class CommandLineError(Exception):
    pass


def validate_options(options):
    if not (options.emulator_path or options.b2g_desktop_path):
        raise CommandLineError('You must specify --emulator-path or ' +
                               '--b2g-desktop-path')
    if options.emulator_path and options.b2g_desktop_path:
        raise CommandLineError('You may only use one of --emulator-path or ' +
                               '--b2g-desktop-path')
    if options.gaia_profile and options.emulator_path:
        raise CommandLineError('You may not use --gaia-profile with ' +
                               '--emulator-path')
    if not options.browser_path:
        raise CommandLineError('You must specify --browser-path')
    if not os.path.isfile(options.manifest):
        raise CommandLineError('The manifest at "%s" does not exist!'
                               % options.manifest)


# BaseMarionetteOptions has a lot of stuff we don't care about, and
# it seems hard to apply directly to this problem. We can revisit this
# decision later if necessary.
def parse_args(in_args):
    parser = argparse.ArgumentParser(description='Run Luciddream tests.')
    parser.add_argument('--emulator-arch', dest='emulator_arch', action='store',
                        default='arm',
                        help='Architecture of emulator to use: x86 or arm')
    parser.add_argument('--emulator-path', dest='emulator_path', action='store',
                        help='path to B2G repo or qemu dir')
    parser.add_argument('--b2g-desktop-path', dest='b2g_desktop_path',
                        action='store',
                        help='path to B2G desktop binary')
    parser.add_argument('--browser-path', dest='browser_path', action='store',
                        help='path to Firefox binary')
    parser.add_argument('--gaia-profile', dest='gaia_profile', action='store',
                        help='path to Gaia profile')
    parser.add_argument('--startup-timeout', dest='startup_timeout', action='store',
                        default=60,  type=int,
                        help='max time to wait for Marionette to be available after launching binary')
    parser.add_argument('manifest', metavar='MANIFEST', action='store',
                        help='path to manifest of tests to run')
    mozlog.commandline.add_logging_group(parser)

    args = parser.parse_args(in_args)
    try:
        validate_options(args)
        return args
    except CommandLineError as e:
        print('Error: ', e.args[0], file=sys.stderr)
        parser.print_help()
        raise


class LucidDreamTestRunner(BaseMarionetteTestRunner):
    def __init__(self, **kwargs):
        BaseMarionetteTestRunner.__init__(self, **kwargs)
        #TODO: handle something like MarionetteJSTestCase
        self.test_handlers = [LucidDreamTestCase]


def start_browser(browser_path, app_args, startup_timeout):
    '''
    Start a Firefox browser and return a Marionette instance that
    can talk to it.
    '''
    marionette = Marionette(
        bin=browser_path,
        # Need to avoid the browser and emulator's ports stepping
        # on each others' toes.
        port=2929,
        app_args=app_args,
        gecko_log="firefox.log",
        startup_timeout=startup_timeout
    )
    runner = marionette.runner
    if runner:
        runner.start()
    marionette.wait_for_port()
    marionette.start_session()
    marionette.set_context(marionette.CONTEXT_CHROME)
    return marionette


# TODO: make ../../marionette/harness/marionette/runtests.py importable
# so we can just use cli from there.  A lot of this is copy/paste from that function.
def run(browser_path=None, b2g_desktop_path=None, emulator_path=None,
        emulator_arch=None, gaia_profile=None, manifest=None, browser_args=None,
        **kwargs):
    # It's sort of debatable here whether the marionette instance managed
    # by the test runner should be the browser or the emulator. Right now
    # it's the emulator because it feels like there's more fiddly setup around
    # that, but longer-term if we want to run tests against different
    # (non-B2G) targets this won't match up very well, so maybe it ought to
    # be the browser?
    browser = start_browser(browser_path, browser_args, kwargs['startup_timeout'])

    kwargs["browser"] = browser
    if not "logger" in kwargs:
        logger = mozlog.commandline.setup_logging(
            "luciddream", kwargs, {"tbpl": sys.stdout})
        kwargs["logger"] = logger

    if emulator_path:
        kwargs['homedir'] = emulator_path
        kwargs['emulator'] = emulator_arch
    elif b2g_desktop_path:
        # Work around bug 859952
        if '-bin' not in b2g_desktop_path:
            if b2g_desktop_path.endswith('.exe'):
                newpath = b2g_desktop_path[:-4] + '-bin.exe'
            else:
                newpath = b2g_desktop_path + '-bin'
            if os.path.exists(newpath):
                b2g_desktop_path = newpath
        kwargs['binary'] = b2g_desktop_path
        kwargs['app'] = 'b2gdesktop'
        if gaia_profile:
            kwargs['profile'] = gaia_profile
        else:
            kwargs['profile'] = os.path.join(
                os.path.dirname(b2g_desktop_path),
                'gaia',
                'profile'
            )
    runner = LucidDreamTestRunner(**kwargs)
    runner.run_tests([manifest])
    if runner.failed > 0:
        sys.exit(10)
    sys.exit(0)

def main():
    args = parse_args(sys.argv[1:])
    run(**vars(args))

if __name__ == '__main__':
    main()
