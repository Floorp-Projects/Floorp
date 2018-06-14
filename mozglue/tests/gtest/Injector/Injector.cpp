/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char** argv)
{
  if (argc < 4) {
    fprintf(stderr,
            "Not enough command line arguments.\n"
            "Command line syntax:\n"
            "Injector.exe [pid] [startAddr] [threadParam]\n");
    return 1;
  }

  DWORD pid = strtoul(argv[1], 0, 0);
#ifdef HAVE_64BIT_BUILD
  void* startAddr = (void*)strtoull(argv[2], 0, 0);
  void* threadParam = (void*)strtoull(argv[3], 0, 0);
#else
  void* startAddr = (void*)strtoul(argv[2], 0, 0);
  void* threadParam = (void*)strtoul(argv[3], 0, 0);
#endif
  HANDLE targetProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                                  PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                                  FALSE,
                                  pid);
  if (targetProc == nullptr) {
    fprintf(stderr, "Error %lu opening target process, PID %lu \n", GetLastError(), pid);
    return 1;
  }

  HANDLE hThread = CreateRemoteThread(targetProc, nullptr, 0,
                                      (LPTHREAD_START_ROUTINE)startAddr,
                                      threadParam, 0, nullptr);
  if (hThread == nullptr) {
    fprintf(stderr, "Error %lu in CreateRemoteThread\n", GetLastError());
    CloseHandle(targetProc);
    return 1;
  }

  CloseHandle(hThread);
  CloseHandle(targetProc);

  return 0;
}
