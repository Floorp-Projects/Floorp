/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <shlwapi.h>
#include <objbase.h>
#include <string.h>
#include <filesystem>

#include "../ScheduledTask.h"
#include "mozilla/CmdLineAndEnvUtils.h"

using namespace mozilla::default_agent;

// See BackgroundTask_defaultagent.sys.mjs for arguments.
int wmain(int argc, wchar_t** argv) {
  // Firefox deescalates process permissions, so handle task unscheduling step
  // here instead of the Firefox Background Tasks to ensure cleanup for other
  // users. See Bug 1710143.
  if (!wcscmp(argv[1], L"uninstall")) {
    if (argc < 3 || !argv[2]) {
      return E_INVALIDARG;
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
      return hr;
    }

    RemoveTasks(argv[2], WhichTasks::AllTasksForInstallation);

    CoUninitialize();

    // Background Task handles remainder of uninstall.
  }

  std::vector<wchar_t> path(MAX_PATH, 0);
  DWORD charsWritten = GetModuleFileNameW(nullptr, path.data(), path.size());

  // GetModuleFileNameW returns the count of characters written including null
  // when truncated, excluding null otherwise. Therefore the count will always
  // be less than the buffer size when not truncated.
  while (charsWritten == path.size()) {
    path.resize(path.size() * 2, 0);
    charsWritten = GetModuleFileNameW(nullptr, path.data(), path.size());
  }

  if (charsWritten == 0) {
    return E_UNEXPECTED;
  }

  std::filesystem::path programPath = path.data();
  programPath = programPath.parent_path();
  programPath += L"\\" MOZ_APP_NAME L".exe";

  std::vector<const wchar_t*> childArgv;
  childArgv.push_back(programPath.c_str());
  childArgv.push_back(L"--backgroundtask");
  childArgv.push_back(L"defaultagent");
  // Skip argv[0], path to this exectuable.
  for (int i = 1; i < argc; i++) {
    childArgv.push_back(argv[i]);
  }

  auto cmdLine = mozilla::MakeCommandLine(childArgv.size(), childArgv.data());

  STARTUPINFOW si = {};
  si.cb = sizeof(STARTUPINFOW);
  PROCESS_INFORMATION pi = {};

  // Runs `{program path} --backgoundtask defaultagent`.
  CreateProcessW(programPath.c_str(), cmdLine.get(), nullptr, nullptr, false,
                 DETACHED_PROCESS | NORMAL_PRIORITY_CLASS, nullptr, nullptr,
                 &si, &pi);

  // Wait until process exists so uninstalling doesn't interrupt the background
  // task cleaning registry entries.
  DWORD exitCode;
  if (WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_OBJECT_0 &&
      ::GetExitCodeProcess(pi.hProcess, &exitCode)) {
    // Match EXIT_CODE in BackgroundTasksManager.sys.mjs and
    // BackgroundTask_defaultagent.sys.mjs.
    enum EXIT_CODE {
      SUCCESS = 0,
      NOT_FOUND = 2,
      EXCEPTION = 3,
      TIMEOUT = 4,
      DISABLED_BY_POLICY = 11,
      INVALID_ARGUMENT = 12,
    };

    switch (exitCode) {
      case SUCCESS:
        return S_OK;
      case NOT_FOUND:
        return E_UNEXPECTED;
      case EXCEPTION:
        return E_FAIL;
      case TIMEOUT:
        return E_FAIL;
      case DISABLED_BY_POLICY:
        return HRESULT_FROM_WIN32(ERROR_ACCESS_DISABLED_BY_POLICY);
      case INVALID_ARGUMENT:
        return E_INVALIDARG;
      default:
        return E_UNEXPECTED;
    }
  }

  return E_UNEXPECTED;
}
