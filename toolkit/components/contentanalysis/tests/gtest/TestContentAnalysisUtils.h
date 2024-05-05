/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_testcontentanalysis_h
#define mozilla_testcontentanalysis_h

#include <processthreadsapi.h>

#include "content_analysis/sdk/analysis_client.h"
#include "gtest/gtest.h"
#include "nsString.h"

struct MozAgentInfo {
  PROCESS_INFORMATION processInfo;
  std::unique_ptr<content_analysis::sdk::Client> client;
  void TerminateProcess() {
    BOOL terminateResult = ::TerminateProcess(processInfo.hProcess, 0);
    ASSERT_NE(FALSE, terminateResult)
        << "Failed to terminate content_analysis_sdk_agent process";
  }
};

void GeneratePipeName(const wchar_t* prefix, nsString& pipeName);
void LaunchAgentWithCommandLineArguments(const nsString& cmdLineArguments,
                                         const nsString& pipeName,
                                         MozAgentInfo& agentInfo);
MozAgentInfo LaunchAgentNormal(const wchar_t* aToBlock);
MozAgentInfo LaunchAgentNormal(const wchar_t* aToBlock,
                               const nsString& pipeName);
#endif
