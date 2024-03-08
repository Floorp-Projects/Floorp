/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "content_analysis/sdk/analysis_client.h"
#include "TestContentAnalysis.h"
#include <processenv.h>
#include <synchapi.h>

using namespace content_analysis::sdk;

MozAgentInfo LaunchAgentNormal(const wchar_t* aToBlock) {
  nsString cmdLineArguments;
  if (aToBlock && aToBlock[0] != 0) {
    cmdLineArguments.Append(L" --toblock=.*");
    cmdLineArguments.Append(aToBlock);
    cmdLineArguments.Append(L".*");
  }
  cmdLineArguments.Append(L" --user");
  cmdLineArguments.Append(L" --path=");
  nsString pipeName;
  GeneratePipeName(L"contentanalysissdk-gtest-", pipeName);
  cmdLineArguments.Append(pipeName);
  MozAgentInfo agentInfo;
  LaunchAgentWithCommandLineArguments(cmdLineArguments, pipeName, agentInfo);
  return agentInfo;
}

TEST(ContentAnalysis, TextShouldNotBeBlocked)
{
  auto MozAgentInfo = LaunchAgentNormal(L"block");
  // Exit the test early if the process failed to launch
  ASSERT_NE(MozAgentInfo.processInfo.dwProcessId, 0UL);
  ASSERT_NE(nullptr, MozAgentInfo.client.get());

  ContentAnalysisRequest request;
  request.set_request_token("request token");
  request.set_text_content("should succeed");
  ContentAnalysisResponse response;
  ASSERT_EQ(0, MozAgentInfo.client->Send(request, &response));
  ASSERT_STREQ("request token", response.request_token().c_str());
  ASSERT_EQ(1, response.results().size());
  ASSERT_EQ(ContentAnalysisResponse_Result_Status_SUCCESS,
            response.results().Get(0).status());
  ASSERT_EQ(0, response.results().Get(0).triggered_rules_size());

  BOOL terminateResult =
      ::TerminateProcess(MozAgentInfo.processInfo.hProcess, 0);
  ASSERT_NE(FALSE, terminateResult)
      << "Failed to terminate content_analysis_sdk_agent process";
}

TEST(ContentAnalysis, TextShouldBeBlocked)
{
  auto MozAgentInfo = LaunchAgentNormal(L"block");
  // Exit the test early if the process failed to launch
  ASSERT_NE(MozAgentInfo.processInfo.dwProcessId, 0UL);
  ASSERT_NE(nullptr, MozAgentInfo.client.get());

  ContentAnalysisRequest request;
  request.set_request_token("request token");
  request.set_text_content("should be blocked");
  ContentAnalysisResponse response;
  ASSERT_EQ(0, MozAgentInfo.client->Send(request, &response));
  ASSERT_STREQ("request token", response.request_token().c_str());
  ASSERT_EQ(1, response.results().size());
  ASSERT_EQ(ContentAnalysisResponse_Result_Status_SUCCESS,
            response.results().Get(0).status());
  ASSERT_EQ(1, response.results().Get(0).triggered_rules_size());
  ASSERT_EQ(ContentAnalysisResponse_Result_TriggeredRule_Action_BLOCK,
            response.results().Get(0).triggered_rules(0).action());

  BOOL terminateResult =
      ::TerminateProcess(MozAgentInfo.processInfo.hProcess, 0);
  ASSERT_NE(FALSE, terminateResult)
      << "Failed to terminate content_analysis_sdk_agent process";
}

TEST(ContentAnalysis, FileShouldNotBeBlocked)
{
  auto MozAgentInfo = LaunchAgentNormal(L"block");
  // Exit the test early if the process failed to launch
  ASSERT_NE(MozAgentInfo.processInfo.dwProcessId, 0UL);
  ASSERT_NE(nullptr, MozAgentInfo.client.get());

  ContentAnalysisRequest request;
  request.set_request_token("request token");
  request.set_file_path("..\\..\\_tests\\gtest\\allowedFile.txt");
  ContentAnalysisResponse response;
  ASSERT_EQ(0, MozAgentInfo.client->Send(request, &response));
  ASSERT_STREQ("request token", response.request_token().c_str());
  ASSERT_EQ(1, response.results().size());
  ASSERT_EQ(ContentAnalysisResponse_Result_Status_SUCCESS,
            response.results().Get(0).status());
  ASSERT_EQ(0, response.results().Get(0).triggered_rules_size());

  BOOL terminateResult =
      ::TerminateProcess(MozAgentInfo.processInfo.hProcess, 0);
  ASSERT_NE(FALSE, terminateResult)
      << "Failed to terminate content_analysis_sdk_agent process";
}

TEST(ContentAnalysis, FileShouldBeBlocked)
{
  auto MozAgentInfo = LaunchAgentNormal(L"block");
  // Exit the test early if the process failed to launch
  ASSERT_NE(MozAgentInfo.processInfo.dwProcessId, 0UL);
  ASSERT_NE(nullptr, MozAgentInfo.client.get());

  ContentAnalysisRequest request;
  request.set_request_token("request token");
  request.set_file_path("..\\..\\_tests\\gtest\\blockedFile.txt");
  ContentAnalysisResponse response;
  ASSERT_EQ(0, MozAgentInfo.client->Send(request, &response));
  ASSERT_STREQ("request token", response.request_token().c_str());
  ASSERT_EQ(1, response.results().size());
  ASSERT_EQ(ContentAnalysisResponse_Result_Status_SUCCESS,
            response.results().Get(0).status());
  ASSERT_EQ(1, response.results().Get(0).triggered_rules_size());
  ASSERT_EQ(ContentAnalysisResponse_Result_TriggeredRule_Action_BLOCK,
            response.results().Get(0).triggered_rules(0).action());

  BOOL terminateResult =
      ::TerminateProcess(MozAgentInfo.processInfo.hProcess, 0);
  ASSERT_NE(FALSE, terminateResult)
      << "Failed to terminate content_analysis_sdk_agent process";
}
