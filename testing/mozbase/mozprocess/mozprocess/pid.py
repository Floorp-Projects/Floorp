#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import mozinfo
import shlex
import subprocess
import sys

# determine the platform-specific invocation of `ps`
if mozinfo.isWin:
    psarg='ax'
else:
    psarg = 'axwww'

def ps(arg=psarg):
    """
    python front-end to `ps`
    http://en.wikipedia.org/wiki/Ps_%28Unix%29
    returns a list of process dicts based on the `ps` header
    """
    retval = []
    process = subprocess.Popen(['ps', arg], stdout=subprocess.PIPE)
    stdout, _ = process.communicate()
    header = None
    for line in stdout.splitlines():
        line = line.strip()
        if header is None:
            # first line is the header
            header = line.split()
            continue
        split = line.split(None, len(header)-1)
        process_dict = dict(zip(header, split))
        retval.append(process_dict)
    return retval

def running_processes(name, psarg=psarg, defunct=True):
    """
    returns a list of
    {'PID': PID of process (int)
     'command': command line of process (list)}
     with the executable named `name`.
     - defunct: whether to return defunct processes
    """
    retval = []
    for process in ps(psarg):
        # Support for both BSD and UNIX syntax
        # `ps aux` returns COMMAND, `ps -ef` returns CMD
        try:
            command = process['COMMAND']
        except KeyError:
            command = process['CMD']

        command = shlex.split(command)
        if command[-1] == '<defunct>':
            command = command[:-1]
            if not command or not defunct:
                continue
        if 'STAT' in process and not defunct:
            if process['STAT'] == 'Z+':
                continue
        command = subprocess.list2cmdline(command)
        if name in command:
            retval.append((int(process['PID']), command))
    return retval

def get_pids(name):
    """Get all the pids matching name"""

    if mozinfo.isWin:
        # use the windows-specific implementation
        import wpk
        return wpk.get_pids(name)
    else:
        return [pid for pid,_ in running_processes(name)]

if __name__ == '__main__':
    pids = set()
    for i in sys.argv[1:]:
        pids.update(get_pids(i))
    for i in sorted(pids):
        print i
