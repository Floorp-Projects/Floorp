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
from mozlog import structured


class CommandLineError(Exception):
    pass


def validate_options(options):
    if not (options.b2gPath or options.b2gDesktopPath):
        raise CommandLineError('You must specify --b2gpath or ' +
                               '--b2g-desktop-path')
    if options.b2gPath and options.b2gDesktopPath:
        raise CommandLineError('You may only use one of --b2gpath or ' +
                               '--b2g-desktop-path')
    if options.gaiaProfile and options.b2gPath:
        raise CommandLineError('You may not use --gaia-profile with ' +
                               '--b2gpath')
    if not options.browserPath:
        raise CommandLineError('You must specify --browser-path')
    if not os.path.isfile(options.manifest):
        raise CommandLineError('The manifest at "%s" does not exist!'
                               % options.manifest)


# BaseMarionetteOptions has a lot of stuff we don't care about, and
# it seems hard to apply directly to this problem. We can revisit this
# decision later if necessary.
def parse_args(in_args):
    parser = argparse.ArgumentParser(description='Run Luciddream tests.')
    parser.add_argument('--emulator', dest='emulator', action='store',
                        default='arm',
                        help='Architecture of emulator to use: x86 or arm')
    parser.add_argument('--b2gpath', dest='b2gPath', action='store',
                        help='path to B2G repo or qemu dir')
    parser.add_argument('--b2g-desktop-path', dest='b2gDesktopPath',
                        action='store',
                        help='path to B2G desktop binary')
    parser.add_argument('--browser-path', dest='browserPath', action='store',
                        help='path to Firefox binary')
    parser.add_argument('--gaia-profile', dest='gaiaProfile', action='store',
                        help='path to Gaia profile')
    parser.add_argument('manifest', metavar='MANIFEST', action='store',
                        help='path to manifest of tests to run')
    structured.commandline.add_logging_group(parser)

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


def start_browser(browserPath):
    '''
    Start a Firefox browser and return a Marionette instance that
    can talk to it.
    '''
    marionette = Marionette(
        bin=browserPath,
        # Need to avoid the browser and emulator's ports stepping
        # on each others' toes.
        port=2929,
    )
    runner = marionette.runner
    if runner:
        runner.start()
    marionette.wait_for_port()
    marionette.start_session()
    marionette.set_context(marionette.CONTEXT_CHROME)
    return marionette


#TODO: make marionette/client/marionette/runtests.py importable so we can
# just use cli from there. A lot of this is copy/paste from that function.
def main():
    try:
        args = parse_args(sys.argv[1:])
    except CommandLineError as e:
        return 1

    logger = structured.commandline.setup_logging(
        'luciddream', args, {"tbpl": sys.stdout})

    # It's sort of debatable here whether the marionette instance managed
    # by the test runner should be the browser or the emulator. Right now
    # it's the emulator because it feels like there's more fiddly setup around
    # that, but longer-term if we want to run tests against different
    # (non-B2G) targets this won't match up very well, so maybe it ought to
    # be the browser?
    browser = start_browser(args.browserPath)
    kwargs = {
        'browser': browser,
        'logger': logger,
    }
    if args.b2gPath:
        kwargs['homedir'] = args.b2gPath
        kwargs['emulator'] = args.emulator
    elif args.b2gDesktopPath:
        # Work around bug 859952
        if '-bin' not in args.b2gDesktopPath:
            if args.b2gDesktopPath.endswith('.exe'):
                newpath = args.b2gDesktopPath[:-4] + '-bin.exe'
            else:
                newpath = args.b2gDesktopPath + '-bin'
            if os.path.exists(newpath):
                args.b2gDesktopPath = newpath
        kwargs['binary'] = args.b2gDesktopPath
        kwargs['app'] = 'b2gdesktop'
        if args.gaiaProfile:
            kwargs['profile'] = args.gaiaProfile
        else:
            kwargs['profile'] = os.path.join(
                os.path.dirname(args.b2gDesktopPath),
                'gaia',
                'profile'
            )
    runner = LucidDreamTestRunner(**kwargs)
    runner.run_tests([args.manifest])
    if runner.failed > 0:
        sys.exit(10)
    sys.exit(0)


if __name__ == '__main__':
    main()
