#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
