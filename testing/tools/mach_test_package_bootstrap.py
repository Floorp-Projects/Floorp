# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import json
import os
import platform
import sys
import types


SEARCH_PATHS = [
    'marionette/harness',
    'marionette/harness/marionette_harness/runner/mixins/browsermob-proxy-py',
    'marionette/client',
    'mochitest',
    'mozbase/manifestparser',
    'mozbase/mozcrash',
    'mozbase/mozdebug',
    'mozbase/mozdevice',
    'mozbase/mozfile',
    'mozbase/mozhttpd',
    'mozbase/mozinfo',
    'mozbase/mozinstall',
    'mozbase/mozleak',
    'mozbase/mozlog',
    'mozbase/moznetwork',
    'mozbase/mozprocess',
    'mozbase/mozprofile',
    'mozbase/mozrunner',
    'mozbase/mozscreenshot',
    'mozbase/mozsystemmonitor',
    'mozbase/moztest',
    'mozbase/mozversion',
    'reftest',
    'tools/mach',
    'tools/wptserve',
    'xpcshell',
]

# Individual files providing mach commands.
MACH_MODULES = [
    'marionette/mach_test_package_commands.py',
    'mochitest/mach_test_package_commands.py',
    'reftest/mach_test_package_commands.py',
    'tools/mach/mach/commands/commandinfo.py',
    'xpcshell/mach_test_package_commands.py',
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
        'long': 'The disabled commands are hidden by default. Use -v to display them. '
                'These commands are unavailable for your current context, '
                'run "mach <command>" to see why.',
        'priority': 0,
    }
}


def ancestors(path, depth=0):
    """Emit the parent directories of a path."""
    count = 1
    while path and count != depth:
        yield path
        newpath = os.path.dirname(path)
        if newpath == path:
            break
        path = newpath
        count += 1


def find_firefox(context):
    """Try to automagically find the firefox binary."""
    import mozinstall
    search_paths = []

    # Check for a mozharness setup
    config = context.mozharness_config
    if config and 'binary_path' in config:
        return config['binary_path']
    elif config:
        search_paths.append(os.path.join(context.mozharness_workdir, 'application'))

    # Check for test-stage setup
    dist_bin = os.path.join(os.path.dirname(context.package_root), 'bin')
    if os.path.isdir(dist_bin):
        search_paths.append(dist_bin)

    for path in search_paths:
        try:
            return mozinstall.get_binary(path, 'firefox')
        except mozinstall.InvalidBinary:
            continue


def find_hostutils(context):
    workdir = context.mozharness_workdir
    hostutils = os.path.join(workdir, 'hostutils')
    for fname in os.listdir(hostutils):
        fpath = os.path.join(hostutils, fname)
        if os.path.isdir(fpath) and fname.startswith('host-utils'):
            return fpath


def normalize_test_path(test_root, path):
    if os.path.isabs(path) or os.path.exists(path):
        return os.path.normpath(os.path.abspath(path))

    for parent in ancestors(test_root):
        test_path = os.path.join(parent, path)
        if os.path.exists(test_path):
            return os.path.normpath(os.path.abspath(test_path))


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
        if key is None:
            context.package_root = test_package_root
            context.bin_dir = os.path.join(test_package_root, 'bin')
            context.certs_dir = os.path.join(test_package_root, 'certs')
            context.module_dir = os.path.join(test_package_root, 'modules')
            context.ancestors = ancestors
            context.normalize_test_path = normalize_test_path
            return

        # The values for the following 'key's will be set lazily, and cached
        # after first being invoked.
        if key == 'firefox_bin':
            return find_firefox(context)

        if key == 'hostutils':
            return find_hostutils(context)

        if key == 'mozharness_config':
            for dir_path in ancestors(context.package_root):
                mozharness_config = os.path.join(dir_path, 'logs', 'localconfig.json')
                if os.path.isfile(mozharness_config):
                    with open(mozharness_config, 'rb') as f:
                        return json.load(f)
            return {}

        if key == 'mozharness_workdir':
            config = context.mozharness_config
            if config:
                return os.path.join(config['base_work_dir'], config['work_dir'])

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
