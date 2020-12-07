#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

"""
A script to set up startup profiling with the Firefox Profiler. See
https://profiler.firefox.com/docs/#/./guide-remote-profiling?id=startup-profiling
for more information.

TODO: This script is a little janky and could be improved. For example, we
should probably avoid having a separate config directory.
"""

import os
import pathlib
import sys
from subprocess import run

SCRIPT_NAME=os.path.basename(__file__)
SCRIPT_DIR=pathlib.Path(__file__).parent.absolute()
CONFIG_DIR=os.path.join(SCRIPT_DIR, 'startup-profiling-configs')

PATH_PREFIX='/data/local/tmp'

def print_usage_and_exit():
    print('USAGE: ./{} [push|remove] <app-id>'.format(SCRIPT_NAME), file=sys.stderr)
    print('example: ./{} push org.mozilla.fenix'.format(SCRIPT_NAME), file=sys.stderr)
    sys.exit(1)

def push(id, filename):
    run(['adb', 'push', os.path.join(CONFIG_DIR, filename), PATH_PREFIX])
    run(['adb', 'shell', 'am', 'set-debug-app', '--persistent', id])
    print('Startup profiling enabled on all future start ups, possibly even after reinstall. Call script with `remove` to disable it.')

def remove(filename):
    run(['adb', 'shell', 'rm', PATH_PREFIX + '/' + filename])
    run(['adb', 'shell', 'am', 'clear-debug-app'])

try:
    cmd = sys.argv[1]
    id = sys.argv[2]
except IndexError as e:
    print_usage_and_exit()

filename = id + '-geckoview-config.yaml'

if cmd == 'push':
    push(id, filename)
elif cmd == 'remove':
    remove(filename)
else:
    print_usage_and_exit()
