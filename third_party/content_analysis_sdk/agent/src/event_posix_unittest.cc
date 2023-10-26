// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "agent/src/event_posix.h"
#include "content_analysis/sdk/analysis_agent.h"
#include "gtest/gtest.h"

namespace content_analysis {
namespace sdk {
namespace testing {

std::unique_ptr<ContentAnalysisEventPosix> CreateEvent(
    const BrowserInfo& browser_info,
    ContentAnalysisRequest request) {
  return std::make_unique<ContentAnalysisEventPosix>(
      browser_info, std::move(request));
}

TEST(EventTest, Create_BrowserInfo) {
  const BrowserInfo bi{12345, "/path/to/binary"};
  ContentAnalysisRequest request;
  *request.add_tags() = "foo";
  request.set_request_token("req-token");

  auto event = CreateEvent(bi, request);
  ASSERT_TRUE(event);

  ASSERT_EQ(bi.pid, event->GetBrowserInfo().pid);
  ASSERT_EQ(bi.binary_path, event->GetBrowserInfo().binary_path);
}

TEST(EventTest, Create_Request) {
  const BrowserInfo bi{ 12345, "/path/to/binary" };
  ContentAnalysisRequest request;
  *request.add_tags() = "foo";
  request.set_request_token("req-token");

  auto event = CreateEvent(bi, request);
  ASSERT_TRUE(event);

  ASSERT_EQ(1u, event->GetRequest().tags_size());
  ASSERT_EQ(request.tags(0), event->GetRequest().tags(0));
  ASSERT_TRUE(event->GetRequest().has_request_token());
  ASSERT_EQ(request.request_token(), event->GetRequest().request_token());
}

}  // namespace testing
}  // namespace sdk
}  // namespace content_analysis

