// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "agent/src/event_win.h"
#include "common/utils_win.h"
#include "gtest/gtest.h"

namespace content_analysis {
namespace sdk {
namespace testing {

std::unique_ptr<ContentAnalysisEventWin> CreateEvent(
    HANDLE handle,
    const BrowserInfo& browser_info,
    ContentAnalysisRequest request) {
  return std::make_unique<ContentAnalysisEventWin>(
      handle, browser_info, std::move(request));
}

TEST(EventTest, Create_BrowserInfo) {
  const BrowserInfo bi{12345, "/path/to/binary"};
  ContentAnalysisRequest request;
  *request.add_tags() = "foo";
  request.set_request_token("req-token");

  auto event = CreateEvent(INVALID_HANDLE_VALUE, bi, request);
  ASSERT_TRUE(event);

  ASSERT_EQ(bi.pid, event->GetBrowserInfo().pid);
  ASSERT_EQ(bi.binary_path, event->GetBrowserInfo().binary_path);
}

TEST(EventTest, Create_Request) {
  const BrowserInfo bi{ 12345, "/path/to/binary" };
  ContentAnalysisRequest request;
  *request.add_tags() = "foo";
  request.set_request_token("req-token");

  auto event = CreateEvent(INVALID_HANDLE_VALUE, bi, request);
  ASSERT_TRUE(event);

  ASSERT_EQ(1u, event->GetRequest().tags_size());
  ASSERT_EQ(request.tags(0), event->GetRequest().tags(0));
  ASSERT_TRUE(event->GetRequest().has_request_token());
  ASSERT_EQ(request.request_token(), event->GetRequest().request_token());
}

TEST(EventTest, Create_Init) {
  const BrowserInfo bi{ 12345, "/path/to/binary" };
  ContentAnalysisRequest request;
  *request.add_tags() = "foo";
  request.set_request_token("req-token");

  auto event = CreateEvent(INVALID_HANDLE_VALUE, bi, request);
  ASSERT_TRUE(event);

  ASSERT_EQ(ResultCode::OK, event->Init());

  // Initializing an event should initialize the contained response for a
  // success verdict that matches the request.
  ASSERT_EQ(request.request_token(), event->GetResponse().request_token());
  ASSERT_EQ(1u, event->GetResponse().results_size());
  ASSERT_EQ(ContentAnalysisResponse::Result::SUCCESS,
            event->GetResponse().results(0).status());
  ASSERT_TRUE(event->GetResponse().results(0).has_tag());
  ASSERT_EQ(request.tags(0), event->GetResponse().results(0).tag());
  ASSERT_EQ(0u, event->GetResponse().results(0).triggered_rules_size());
}

// Initializing an event whose request has no request token is an error.
TEST(EventTest, Create_Init_RequestNoRequestToken) {
  const BrowserInfo bi{ 12345, "/path/to/binary" };
  ContentAnalysisRequest request;
  *request.add_tags() = "foo";

  auto event = CreateEvent(INVALID_HANDLE_VALUE, bi, request);
  ASSERT_TRUE(event);

  ASSERT_EQ(ResultCode::ERR_MISSING_REQUEST_TOKEN, event->Init());
}

TEST(EventTest, Write_BadPipe) {
  HANDLE pipe;
  DWORD err = internal::CreatePipe(
      internal::GetPipeNameForAgent("testpipe", false), false, true, &pipe);
  ASSERT_EQ(ERROR_SUCCESS, err);
  ASSERT_NE(INVALID_HANDLE_VALUE, pipe);

  // Create an event with the dummy pipe and initilalize it.
  const BrowserInfo bi{ 12345, "/path/to/binary" };
  ContentAnalysisRequest request;
  request.set_request_token("req-token");
  *request.add_tags() = "dlp";
  request.set_text_content("test");
  auto event = std::make_unique<ContentAnalysisEventWin>(
      pipe, bi, std::move(request));
  ASSERT_TRUE(event);
  ResultCode rc = event->Init();
  ASSERT_EQ(ResultCode::OK, rc);

  // Close the handle before trying to send the response.
  // This simulates an error with the pipe.
  CloseHandle(pipe);
  ASSERT_EQ(ERROR_SUCCESS, GetLastError());

  // The following call should not hang.
  event->Send();
}

}  // namespace testing
}  // namespace sdk
}  // namespace content_analysis

