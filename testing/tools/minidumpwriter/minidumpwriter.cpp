/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/*
 * Given a PID and a path to a target file, write a minidump of the
 * corresponding process in that file. This is taken more or less
 * verbatim from mozcrash and translated to C++ to avoid problems
 * writing a minidump of 64 bit Firefox from a 32 bit python.
 */

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <dbghelp.h>

int wmain(int argc, wchar_t** argv)
{
  if (argc != 3) {
    fprintf(stderr, "Usage: minidumpwriter <PID> <DUMP_FILE>\n");
    return 1;
  }

  DWORD pid = (DWORD) _wtoi(argv[1]);

  if (pid <= 0) {
    fprintf(stderr, "Usage: minidumpwriter <PID> <DUMP_FILE>\n");
    return 1;
  }

  wchar_t* dumpfile = argv[2];
  int rv = 1;
  HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                0, pid);
  if (!hProcess) {
    fprintf(stderr, "Couldn't get handle for %d\n", pid);
    return rv;
  }

  HANDLE file = CreateFileW(dumpfile, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    fprintf(stderr, "Couldn't open dump file at %S\n", dumpfile);
    CloseHandle(hProcess);
    return rv;
  }

  rv = 0;
  if (!MiniDumpWriteDump(hProcess, pid, file, MiniDumpNormal,
                         nullptr, nullptr, nullptr)) {
    fprintf(stderr, "Error 0x%X in MiniDumpWriteDump\n", GetLastError());
    rv = 1;
  }

  CloseHandle(file);
  CloseHandle(hProcess);
  return rv;
}
