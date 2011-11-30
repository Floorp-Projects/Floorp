#!/usr/bin/env python

# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozprocess.
#
# The Initial Developer of the Original Code is
#  The Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Clint Talbert <ctalbert@mozilla.com>
#  Jonathan Griffin <jgriffin@mozilla.com>
#  Jeff Hammel <jhammel@mozilla.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

import os
import mozinfo
import shlex
import subprocess
import sys

# determine the platform-specific invocation of `ps`
if mozinfo.isMac:
    psarg = '-Acj'
elif mozinfo.isLinux:
    psarg = 'axwww'
else:
    psarg = 'ax'

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
        command = process['COMMAND']
        command = shlex.split(command)
        if command[-1] == '<defunct>':
            command = command[:-1]
            if not command or not defunct:
                continue
        if 'STAT' in process and not defunct:
            if process['STAT'] == 'Z+':
                continue
        prog = command[0]
        basename = os.path.basename(prog)
        if basename == name:
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
