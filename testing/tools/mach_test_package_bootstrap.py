# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import os
import platform
import sys


SEARCH_PATHS = [
    'marionette',
    'marionette/marionette/runner/mixins/browsermob-proxy-py',
    'marionette/client',
    'mochitest',
    'mozbase/mozcrash',
    'mozbase/mozdebug',
    'mozbase/mozdevice',
    'mozbase/mozfile',
    'mozbase/mozhttpd',
    'mozbase/mozleak',
    'mozbase/mozlog',
    'mozbase/moznetwork',
    'mozbase/mozprocess',
    'mozbase/mozprofile',
    'mozbase/mozrunner',
    'mozbase/mozsystemmonitor',
    'mozbase/mozinfo',
    'mozbase/mozscreenshot',
    'mozbase/moztest',
    'mozbase/mozversion',
    'mozbase/manifestparser',
    'tools/mach',
    'tools/wptserve',
]

# Individual files providing mach commands.
MACH_MODULES = [
    'mochitest/mach_test_package_commands.py',
    'tools/mach/mach/commands/commandinfo.py',
]


CATEGORIES = {
    'testing': {
        'short': 'Testing',
        'long': 'Run tests.',
        'priority': 30,
    },
    'devenv': {
        'short': 'Development Environment',
        'long': 'Set up and configure your development environment.',
        'priority': 20,
    },
    'misc': {
        'short': 'Potpourri',
        'long': 'Potent potables and assorted snacks.',
        'priority': 10,
    },
    'disabled': {
        'short': 'Disabled',
        'long': 'The disabled commands are hidden by default. Use -v to display them. These commands are unavailable for your current context, run "mach <command>" to see why.',
        'priority': 0,
    }
}


def bootstrap(test_package_root):
    test_package_root = os.path.abspath(test_package_root)

    # Ensure we are running Python 2.7+. We put this check here so we generate a
    # user-friendly error message rather than a cryptic stack trace on module
    # import.
    if sys.version_info[0] != 2 or sys.version_info[1] < 7:
        print('Python 2.7 or above (but not Python 3) is required to run mach.')
        print('You are running Python', platform.python_version())
        sys.exit(1)

    sys.path[0:0] = [os.path.join(test_package_root, path) for path in SEARCH_PATHS]
    import mach.main

    def populate_context(context, key=None):
        if key is not None:
            return

        context.package_root = test_package_root
        context.certs_dir = os.path.join(test_package_root, 'certs')
        context.bin_dir = os.path.join(test_package_root, 'bin')
        context.modules_dir = os.path.join(test_package_root, 'modules')

    mach = mach.main.Mach(os.getcwd())
    mach.populate_context_handler = populate_context

    for category, meta in CATEGORIES.items():
        mach.define_category(category, meta['short'], meta['long'],
                             meta['priority'])

    for path in MACH_MODULES:
        cmdfile = os.path.join(test_package_root, path)

        # Depending on which test zips were extracted,
        # the command module might not exist
        if os.path.isfile(cmdfile):
            mach.load_commands_from_file(cmdfile)

    return mach
