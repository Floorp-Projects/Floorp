#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

"""
A script to set up startup profiling with the Firefox Profiler. See
https://profiler.firefox.com/docs/#/./guide-remote-profiling?id=startup-profiling
for more information.
"""

import os
import sys
import tempfile
from subprocess import run

SCRIPT_NAME=os.path.basename(__file__)

PATH_PREFIX='/data/local/tmp'

GV_CONFIG=b'''env:
  MOZ_PROFILER_STARTUP: 1
  MOZ_PROFILER_STARTUP_INTERVAL: 5
  MOZ_PROFILER_STARTUP_FEATURES: threads,js,stackwalk,leaf,screenshots,ipcmessages,java
  MOZ_PROFILER_STARTUP_FILTERS: GeckoMain,Compositor,Renderer,IPDL Background
'''

def print_usage_and_exit():
    print('USAGE: ./{} [push|remove] <app-id>'.format(SCRIPT_NAME), file=sys.stderr)
    print('example: ./{} push org.mozilla.fenix'.format(SCRIPT_NAME), file=sys.stderr)
    sys.exit(1)

def push(id, filename):
    config = tempfile.NamedTemporaryFile(delete=False)
    try:
        # I think the file needs to be closed to save its contents for adb push to
        # work correctly so we close it here and later delete it manually.
        with config.file as f:
            f.write(GV_CONFIG)

        run(['adb', 'push', config.name, os.path.join(PATH_PREFIX, filename)])
        run(['adb', 'shell', 'am', 'set-debug-app', '--persistent', id])
        print('Startup profiling enabled on all future start ups, possibly even after reinstall. Call script with `remove` to disable it.')
    finally:
        os.remove(config.name)

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
