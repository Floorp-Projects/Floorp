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
#include <windows.h>

using namespace content_analysis::sdk;

namespace {
MozAgentInfo LaunchAgentMisbehaving(const wchar_t* mode) {
  nsString cmdLineArguments;
  cmdLineArguments.Append(L" --misbehave=");
  cmdLineArguments.Append(mode);
  cmdLineArguments.Append(L" --user");
  cmdLineArguments.Append(L" --path=");
  nsString pipeName;
  GeneratePipeName(L"contentanalysissdk-gtest-", pipeName);
  cmdLineArguments.Append(pipeName);
  MozAgentInfo agentInfo;
  LaunchAgentWithCommandLineArguments(cmdLineArguments, pipeName, agentInfo);
  return agentInfo;
}
}  // namespace

// Disabled for now
/*TEST(ContentAnalysisMisbehaving, LargeResponse)
{
  auto MozAgentInfo = LaunchAgentMisbehaving(L"largeResponse");
  // Exit the test early if the process failed to launch
  ASSERT_NE(MozAgentInfo.processInfo.dwProcessId, 0UL);
  ASSERT_NE(nullptr, MozAgentInfo.client.get());

  ContentAnalysisRequest request;
  request.set_request_token("request token");
  request.set_text_content("unused");
  ContentAnalysisResponse response;
  ASSERT_EQ(0, MozAgentInfo.client->Send(request, &response));
  ASSERT_STREQ("request token", response.request_token().c_str());
  ASSERT_EQ(1001, response.results().size());

  BOOL terminateResult = ::TerminateProcess(MozAgentInfo.processInfo.hProcess,
0); ASSERT_NE(FALSE, terminateResult)
      << "Failed to terminate content_analysis_sdk_agent process";
}*/

TEST(ContentAnalysisMisbehaving, InvalidUtf8StringStartByteIsContinuationByte)
{
  auto MozAgentInfo =
      LaunchAgentMisbehaving(L"invalidUtf8StringStartByteIsContinuationByte");
  // Exit the test early if the process failed to launch
  ASSERT_NE(MozAgentInfo.processInfo.dwProcessId, 0UL);
  ASSERT_NE(nullptr, MozAgentInfo.client.get());

  ContentAnalysisRequest request;
  request.set_request_token("request token");
  request.set_text_content("unused");
  ContentAnalysisResponse response;
  ASSERT_EQ(0, MozAgentInfo.client->Send(request, &response));
  // The protobuf spec says that strings must be valid UTF-8. So it's OK if
  // this gets mangled, just want to make sure it doesn't cause a crash
  // or invalid memory access or something.
  ASSERT_STREQ("\x80\x41\x41\x41", response.request_token().c_str());

  BOOL terminateResult =
      ::TerminateProcess(MozAgentInfo.processInfo.hProcess, 0);
  ASSERT_NE(FALSE, terminateResult)
      << "Failed to terminate content_analysis_sdk_agent process";
}

TEST(ContentAnalysisMisbehaving,
     InvalidUtf8StringEndsInMiddleOfMultibyteSequence)
{
  auto MozAgentInfo = LaunchAgentMisbehaving(
      L"invalidUtf8StringEndsInMiddleOfMultibyteSequence");
  // Exit the test early if the process failed to launch
  ASSERT_NE(MozAgentInfo.processInfo.dwProcessId, 0UL);
  ASSERT_NE(nullptr, MozAgentInfo.client.get());

  ContentAnalysisRequest request;
  request.set_request_token("request token");
  request.set_text_content("unused");
  ContentAnalysisResponse response;
  ASSERT_EQ(0, MozAgentInfo.client->Send(request, &response));
  // The protobuf spec says that strings must be valid UTF-8. So it's OK if
  // this gets mangled, just want to make sure it doesn't cause a crash
  // or invalid memory access or something.
  ASSERT_STREQ("\x41\xf0\x90\x8d", response.request_token().c_str());

  BOOL terminateResult =
      ::TerminateProcess(MozAgentInfo.processInfo.hProcess, 0);
  ASSERT_NE(FALSE, terminateResult)
      << "Failed to terminate content_analysis_sdk_agent process";
}

TEST(ContentAnalysisMisbehaving, InvalidUtf8StringMultibyteSequenceTooShort)
{
  auto MozAgentInfo =
      LaunchAgentMisbehaving(L"invalidUtf8StringMultibyteSequenceTooShort");
  // Exit the test early if the process failed to launch
  ASSERT_NE(MozAgentInfo.processInfo.dwProcessId, 0UL);
  ASSERT_NE(nullptr, MozAgentInfo.client.get());

  ContentAnalysisRequest request;
  request.set_request_token("request token");
  request.set_text_content("unused");
  ContentAnalysisResponse response;
  ASSERT_EQ(0, MozAgentInfo.client->Send(request, &response));
  // The protobuf spec says that strings must be valid UTF-8. So it's OK if
  // this gets mangled, just want to make sure it doesn't cause a crash
  // or invalid memory access or something.
  ASSERT_STREQ("\xf0\x90\x8d\x41", response.request_token().c_str());

  BOOL terminateResult =
      ::TerminateProcess(MozAgentInfo.processInfo.hProcess, 0);
  ASSERT_NE(FALSE, terminateResult)
      << "Failed to terminate content_analysis_sdk_agent process";
}

TEST(ContentAnalysisMisbehaving, InvalidUtf8StringDecodesToInvalidCodePoint)
{
  auto MozAgentInfo =
      LaunchAgentMisbehaving(L"invalidUtf8StringDecodesToInvalidCodePoint");
  // Exit the test early if the process failed to launch
  ASSERT_NE(MozAgentInfo.processInfo.dwProcessId, 0UL);
  ASSERT_NE(nullptr, MozAgentInfo.client.get());

  ContentAnalysisRequest request;
  request.set_request_token("request token");
  request.set_text_content("unused");
  ContentAnalysisResponse response;
  ASSERT_EQ(0, MozAgentInfo.client->Send(request, &response));
  // The protobuf spec says that strings must be valid UTF-8. So it's OK if
  // this gets mangled, just want to make sure it doesn't cause a crash
  // or invalid memory access or something.
  ASSERT_STREQ("\xf7\xbf\xbf\xbf", response.request_token().c_str());

  BOOL terminateResult =
      ::TerminateProcess(MozAgentInfo.processInfo.hProcess, 0);
  ASSERT_NE(FALSE, terminateResult)
      << "Failed to terminate content_analysis_sdk_agent process";
}

TEST(ContentAnalysisMisbehaving, InvalidUtf8StringOverlongEncoding)
{
  auto MozAgentInfo =
      LaunchAgentMisbehaving(L"invalidUtf8StringOverlongEncoding");
  // Exit the test early if the process failed to launch
  ASSERT_NE(MozAgentInfo.processInfo.dwProcessId, 0UL);
  ASSERT_NE(nullptr, MozAgentInfo.client.get());

  ContentAnalysisRequest request;
  request.set_request_token("request token");
  request.set_text_content("unused");
  ContentAnalysisResponse response;
  ASSERT_EQ(0, MozAgentInfo.client->Send(request, &response));
  // The protobuf spec says that strings must be valid UTF-8. So it's OK if
  // this gets mangled, just want to make sure it doesn't cause a crash
  // or invalid memory access or something.
  ASSERT_STREQ("\xf0\x82\x82\xac", response.request_token().c_str());

  BOOL terminateResult =
      ::TerminateProcess(MozAgentInfo.processInfo.hProcess, 0);
  ASSERT_NE(FALSE, terminateResult)
      << "Failed to terminate content_analysis_sdk_agent process";
}

TEST(ContentAnalysisMisbehaving, StringWithEmbeddedNull)
{
  auto MozAgentInfo = LaunchAgentMisbehaving(L"stringWithEmbeddedNull");
  // Exit the test early if the process failed to launch
  ASSERT_NE(MozAgentInfo.processInfo.dwProcessId, 0UL);
  ASSERT_NE(nullptr, MozAgentInfo.client.get());

  ContentAnalysisRequest request;
  request.set_request_token("request token");
  request.set_text_content("unused");
  ContentAnalysisResponse response;
  ASSERT_EQ(0, MozAgentInfo.client->Send(request, &response));
  std::string expected("\x41\x00\x41");
  ASSERT_EQ(expected, response.request_token());

  BOOL terminateResult =
      ::TerminateProcess(MozAgentInfo.processInfo.hProcess, 0);
  ASSERT_NE(FALSE, terminateResult)
      << "Failed to terminate content_analysis_sdk_agent process";
}

TEST(ContentAnalysisMisbehaving, ZeroResults)
{
  auto MozAgentInfo = LaunchAgentMisbehaving(L"zeroResults");
  // Exit the test early if the process failed to launch
  ASSERT_NE(MozAgentInfo.processInfo.dwProcessId, 0UL);
  ASSERT_NE(nullptr, MozAgentInfo.client.get());

  ContentAnalysisRequest request;
  request.set_request_token("request token");
  request.set_text_content("unused");
  ContentAnalysisResponse response;
  ASSERT_EQ(0, MozAgentInfo.client->Send(request, &response));
  ASSERT_EQ(0, response.results().size());

  BOOL terminateResult =
      ::TerminateProcess(MozAgentInfo.processInfo.hProcess, 0);
  ASSERT_NE(FALSE, terminateResult)
      << "Failed to terminate content_analysis_sdk_agent process";
}

TEST(ContentAnalysisMisbehaving, ResultWithInvalidStatus)
{
  auto MozAgentInfo = LaunchAgentMisbehaving(L"resultWithInvalidStatus");
  // Exit the test early if the process failed to launch
  ASSERT_NE(MozAgentInfo.processInfo.dwProcessId, 0UL);
  ASSERT_NE(nullptr, MozAgentInfo.client.get());

  ContentAnalysisRequest request;
  request.set_request_token("request token");
  request.set_text_content("unused");
  ContentAnalysisResponse response;
  ASSERT_EQ(0, MozAgentInfo.client->Send(request, &response));
  ASSERT_EQ(1, response.results().size());
  // protobuf will fail to read this because it's an invalid value.
  // (and leave status at its default value of 0)
  // just make sure we can get the value without throwing
  ASSERT_GE(static_cast<int>(response.results(0).status()), 0);

  BOOL terminateResult =
      ::TerminateProcess(MozAgentInfo.processInfo.hProcess, 0);
  ASSERT_NE(FALSE, terminateResult)
      << "Failed to terminate content_analysis_sdk_agent process";
}

TEST(ContentAnalysisMisbehaving, MessageTruncatedInMiddleOfString)
{
  auto MozAgentInfo =
      LaunchAgentMisbehaving(L"messageTruncatedInMiddleOfString");
  // Exit the test early if the process failed to launch
  ASSERT_NE(MozAgentInfo.processInfo.dwProcessId, 0UL);
  ASSERT_NE(nullptr, MozAgentInfo.client.get());

  ContentAnalysisRequest request;
  request.set_request_token("request token");
  ContentAnalysisResponse response;
  // The response is an invalid serialization of protobuf, so this should fail
  ASSERT_EQ(-1, MozAgentInfo.client->Send(request, &response));

  BOOL terminateResult =
      ::TerminateProcess(MozAgentInfo.processInfo.hProcess, 0);
  ASSERT_NE(FALSE, terminateResult)
      << "Failed to terminate content_analysis_sdk_agent process";
}

TEST(ContentAnalysisMisbehaving, MessageWithInvalidWireType)
{
  auto MozAgentInfo = LaunchAgentMisbehaving(L"messageWithInvalidWireType");
  // Exit the test early if the process failed to launch
  ASSERT_NE(MozAgentInfo.processInfo.dwProcessId, 0UL);
  ASSERT_NE(nullptr, MozAgentInfo.client.get());

  ContentAnalysisRequest request;
  request.set_request_token("request token");
  ContentAnalysisResponse response;
  // The response is an invalid serialization of protobuf, so this should fail
  ASSERT_EQ(-1, MozAgentInfo.client->Send(request, &response));

  BOOL terminateResult =
      ::TerminateProcess(MozAgentInfo.processInfo.hProcess, 0);
  ASSERT_NE(FALSE, terminateResult)
      << "Failed to terminate content_analysis_sdk_agent process";
}

TEST(ContentAnalysisMisbehaving, MessageWithUnusedFieldNumber)
{
  auto MozAgentInfo = LaunchAgentMisbehaving(L"messageWithUnusedFieldNumber");
  // Exit the test early if the process failed to launch
  ASSERT_NE(MozAgentInfo.processInfo.dwProcessId, 0UL);
  ASSERT_NE(nullptr, MozAgentInfo.client.get());

  ContentAnalysisRequest request;
  request.set_request_token("request token");
  ContentAnalysisResponse response;
  ASSERT_EQ(0, MozAgentInfo.client->Send(request, &response));
  // protobuf will read the value and store it in an unused section
  // just make sure we can get a value without throwing
  ASSERT_STREQ("", response.request_token().c_str());

  BOOL terminateResult =
      ::TerminateProcess(MozAgentInfo.processInfo.hProcess, 0);
  ASSERT_NE(FALSE, terminateResult)
      << "Failed to terminate content_analysis_sdk_agent process";
}

TEST(ContentAnalysisMisbehaving, MessageWithWrongStringWireType)
{
  auto MozAgentInfo = LaunchAgentMisbehaving(L"messageWithWrongStringWireType");
  // Exit the test early if the process failed to launch
  ASSERT_NE(MozAgentInfo.processInfo.dwProcessId, 0UL);
  ASSERT_NE(nullptr, MozAgentInfo.client.get());

  ContentAnalysisRequest request;
  request.set_request_token("request token");
  ContentAnalysisResponse response;
  // The response is an invalid serialization of protobuf, so this should fail
  ASSERT_EQ(-1, MozAgentInfo.client->Send(request, &response));

  BOOL terminateResult =
      ::TerminateProcess(MozAgentInfo.processInfo.hProcess, 0);
  ASSERT_NE(FALSE, terminateResult)
      << "Failed to terminate content_analysis_sdk_agent process";
}

TEST(ContentAnalysisMisbehaving, MessageWithZeroTag)
{
  auto MozAgentInfo = LaunchAgentMisbehaving(L"messageWithZeroTag");
  // Exit the test early if the process failed to launch
  ASSERT_NE(MozAgentInfo.processInfo.dwProcessId, 0UL);
  ASSERT_NE(nullptr, MozAgentInfo.client.get());

  ContentAnalysisRequest request;
  request.set_request_token("request token");
  ContentAnalysisResponse response;
  // The response is an invalid serialization of protobuf, so this should fail
  ASSERT_EQ(-1, MozAgentInfo.client->Send(request, &response));

  BOOL terminateResult =
      ::TerminateProcess(MozAgentInfo.processInfo.hProcess, 0);
  ASSERT_NE(FALSE, terminateResult)
      << "Failed to terminate content_analysis_sdk_agent process";
}

TEST(ContentAnalysisMisbehaving, MessageWithZeroFieldButNonzeroWireType)
{
  auto MozAgentInfo =
      LaunchAgentMisbehaving(L"messageWithZeroFieldButNonzeroWireType");
  // Exit the test early if the process failed to launch
  ASSERT_NE(MozAgentInfo.processInfo.dwProcessId, 0UL);
  ASSERT_NE(nullptr, MozAgentInfo.client.get());

  ContentAnalysisRequest request;
  request.set_request_token("request token");
  ContentAnalysisResponse response;
  // The response is an invalid serialization of protobuf, so this should fail
  ASSERT_EQ(-1, MozAgentInfo.client->Send(request, &response));

  BOOL terminateResult =
      ::TerminateProcess(MozAgentInfo.processInfo.hProcess, 0);
  ASSERT_NE(FALSE, terminateResult)
      << "Failed to terminate content_analysis_sdk_agent process";
}

TEST(ContentAnalysisMisbehaving, MessageWithGroupEnd)
{
  auto MozAgentInfo =
      LaunchAgentMisbehaving(L"messageWithZeroFieldButNonzeroWireType");
  // Exit the test early if the process failed to launch
  ASSERT_NE(MozAgentInfo.processInfo.dwProcessId, 0UL);
  ASSERT_NE(nullptr, MozAgentInfo.client.get());

  ContentAnalysisRequest request;
  request.set_request_token("request token");
  ContentAnalysisResponse response;
  // The response is an invalid serialization of protobuf, so this should fail
  ASSERT_EQ(-1, MozAgentInfo.client->Send(request, &response));

  BOOL terminateResult =
      ::TerminateProcess(MozAgentInfo.processInfo.hProcess, 0);
  ASSERT_NE(FALSE, terminateResult)
      << "Failed to terminate content_analysis_sdk_agent process";
}

TEST(ContentAnalysisMisbehaving, MessageTruncatedInMiddleOfVarint)
{
  auto MozAgentInfo =
      LaunchAgentMisbehaving(L"messageTruncatedInMiddleOfVarint");
  // Exit the test early if the process failed to launch
  ASSERT_NE(MozAgentInfo.processInfo.dwProcessId, 0UL);
  ASSERT_NE(nullptr, MozAgentInfo.client.get());

  ContentAnalysisRequest request;
  request.set_request_token("request token");
  ContentAnalysisResponse response;
  // The response is an invalid serialization of protobuf, so this should fail
  ASSERT_EQ(-1, MozAgentInfo.client->Send(request, &response));

  BOOL terminateResult =
      ::TerminateProcess(MozAgentInfo.processInfo.hProcess, 0);
  ASSERT_NE(FALSE, terminateResult)
      << "Failed to terminate content_analysis_sdk_agent process";
}

TEST(ContentAnalysisMisbehaving, MessageTruncatedInMiddleOfTag)
{
  auto MozAgentInfo = LaunchAgentMisbehaving(L"messageTruncatedInMiddleOfTag");
  // Exit the test early if the process failed to launch
  ASSERT_NE(MozAgentInfo.processInfo.dwProcessId, 0UL);
  ASSERT_NE(nullptr, MozAgentInfo.client.get());

  ContentAnalysisRequest request;
  request.set_request_token("request token");
  ContentAnalysisResponse response;
  // The response is an invalid serialization of protobuf, so this should fail
  ASSERT_EQ(-1, MozAgentInfo.client->Send(request, &response));

  BOOL terminateResult =
      ::TerminateProcess(MozAgentInfo.processInfo.hProcess, 0);
  ASSERT_NE(FALSE, terminateResult)
      << "Failed to terminate content_analysis_sdk_agent process";
}
