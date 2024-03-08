/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "TestContentAnalysis.h"
#include <combaseapi.h>
#include <pathcch.h>
#include <shlwapi.h>
#include <rpc.h>
#include <windows.h>

void GeneratePipeName(const wchar_t* prefix, nsString& pipeName) {
  pipeName = u""_ns;
  pipeName.Append(prefix);
  UUID uuid;
  ASSERT_EQ(RPC_S_OK, UuidCreate(&uuid));
  // 39 == length of a UUID string including braces and NUL.
  wchar_t guidBuf[39] = {};
  ASSERT_EQ(39, StringFromGUID2(uuid, guidBuf, 39));
  // omit opening and closing braces (and trailing null)
  pipeName.Append(&guidBuf[1], 36);
}

void LaunchAgentWithCommandLineArguments(const nsString& cmdLineArguments,
                                         const nsString& pipeName,
                                         MozAgentInfo& agentInfo) {
  wchar_t progName[MAX_PATH] = {};
  // content_analysis_sdk_agent.exe is either next to firefox.exe (for local
  // builds), or in ../../tests/bin/ (for try/treeherder builds)
  DWORD nameSize = ::GetModuleFileNameW(nullptr, progName, MAX_PATH);
  ASSERT_NE(DWORD{0}, nameSize);
  ASSERT_EQ(S_OK, PathCchRemoveFileSpec(progName, nameSize));
  wchar_t normalizedPath[MAX_PATH] = {};
  nsString test1 = nsString(progName) + u"\\content_analysis_sdk_agent.exe"_ns;
  ASSERT_EQ(S_OK, PathCchCanonicalize(normalizedPath, MAX_PATH, test1.get()));
  nsString agentPath;
  if (::PathFileExistsW(normalizedPath)) {
    agentPath = nsString(normalizedPath);
  }
  if (agentPath.IsEmpty()) {
    nsString unNormalizedPath =
        nsString(progName) +
        u"\\..\\..\\tests\\bin\\content_analysis_sdk_agent.exe"_ns;
    ASSERT_EQ(S_OK, PathCchCanonicalize(normalizedPath, MAX_PATH,
                                        unNormalizedPath.get()));
    if (::PathFileExistsW(normalizedPath)) {
      agentPath = nsString(normalizedPath);
    }
  }
  ASSERT_FALSE(agentPath.IsEmpty());
  nsString localCmdLine = nsString(agentPath) + u" "_ns + cmdLineArguments;
  STARTUPINFOW startupInfo = {sizeof(startupInfo)};
  PROCESS_INFORMATION processInfo;
  BOOL ok =
      ::CreateProcessW(nullptr, localCmdLine.get(), nullptr, nullptr, FALSE, 0,
                       nullptr, nullptr, &startupInfo, &processInfo);
  // The documentation for CreateProcessW() says that any non-zero value is a
  // success
  if (!ok) {
    // Show the last error
    ASSERT_EQ(0UL, GetLastError())
        << "Failed to launch content_analysis_sdk_agent";
  }
  // Allow time for the agent to set up the pipe
  ::Sleep(2000);
  content_analysis::sdk::Client::Config config;
  config.name = NS_ConvertUTF16toUTF8(pipeName);
  config.user_specific = true;
  auto clientPtr = content_analysis::sdk::Client::Create(config);
  ASSERT_NE(nullptr, clientPtr.get());

  agentInfo.processInfo = processInfo;
  agentInfo.client = std::move(clientPtr);
}
