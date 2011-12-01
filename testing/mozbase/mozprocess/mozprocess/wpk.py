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

from ctypes import sizeof, windll, addressof, c_wchar, create_unicode_buffer
from ctypes.wintypes import DWORD, HANDLE

PROCESS_TERMINATE = 0x0001
PROCESS_QUERY_INFORMATION = 0x0400
PROCESS_VM_READ = 0x0010

def get_pids(process_name):
    BIG_ARRAY = DWORD * 4096
    processes = BIG_ARRAY()
    needed = DWORD()

    pids = []
    result = windll.psapi.EnumProcesses(processes,
                                        sizeof(processes),
                                        addressof(needed))
    if not result:
        return pids

    num_results = needed.value / sizeof(DWORD)

    for i in range(num_results):
        pid = processes[i]
        process = windll.kernel32.OpenProcess(PROCESS_QUERY_INFORMATION |
                                              PROCESS_VM_READ,
                                              0, pid)
        if process:
            module = HANDLE()
            result = windll.psapi.EnumProcessModules(process,
                                                     addressof(module),
                                                     sizeof(module),
                                                     addressof(needed))
            if result:
                name = create_unicode_buffer(1024)
                result = windll.psapi.GetModuleBaseNameW(process, module,
                                                         name, len(name))
                # TODO: This might not be the best way to
                # match a process name; maybe use a regexp instead.
                if name.value.startswith(process_name):
                    pids.append(pid)
                windll.kernel32.CloseHandle(module)
            windll.kernel32.CloseHandle(process)

    return pids

def kill_pid(pid):
    process = windll.kernel32.OpenProcess(PROCESS_TERMINATE, 0, pid)
    if process:
        windll.kernel32.TerminateProcess(process, 0)
        windll.kernel32.CloseHandle(process)

if __name__ == '__main__':
    import subprocess
    import time

    # This test just opens a new notepad instance and kills it.

    name = 'notepad'

    old_pids = set(get_pids(name))
    subprocess.Popen([name])
    time.sleep(0.25)
    new_pids = set(get_pids(name)).difference(old_pids)

    if len(new_pids) != 1:
        raise Exception('%s was not opened or get_pids() is '
                        'malfunctioning' % name)

    kill_pid(tuple(new_pids)[0])

    newest_pids = set(get_pids(name)).difference(old_pids)

    if len(newest_pids) != 0:
        raise Exception('kill_pid() is malfunctioning')

    print "Test passed."
