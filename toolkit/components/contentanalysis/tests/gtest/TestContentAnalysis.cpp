/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "nsNetUtil.h"
#include "ContentAnalysis.h"
#include <processenv.h>
#include <synchapi.h>

const char* kAllowUrlPref = "browser.contentanalysis.allow_url_regex_list";
const char* kDenyUrlPref = "browser.contentanalysis.deny_url_regex_list";

using namespace mozilla;
using namespace mozilla::contentanalysis;

class ContentAnalysisTest : public testing::Test {
 protected:
  ContentAnalysisTest() {
    auto* logmodule = LogModule::Get("contentanalysis");
    logmodule->SetLevel(LogLevel::Verbose);

    nsCOMPtr<nsIContentAnalysis> caSvc =
        do_GetService("@mozilla.org/contentanalysis;1");
    MOZ_ASSERT(caSvc);
    mContentAnalysis = static_cast<ContentAnalysis*>(caSvc.get());

    // Tests run earlier could have altered these values
    mContentAnalysis->mParsedUrlLists = false;
    mContentAnalysis->mAllowUrlList = {};
    mContentAnalysis->mDenyUrlList = {};

    MOZ_ALWAYS_SUCCEEDS(Preferences::SetCString(kAllowUrlPref, ""));
    MOZ_ALWAYS_SUCCEEDS(Preferences::SetCString(kDenyUrlPref, ""));
  }

  void TearDown() override {
    mContentAnalysis->mParsedUrlLists = false;
    mContentAnalysis->mAllowUrlList = {};
    mContentAnalysis->mDenyUrlList = {};

    MOZ_ALWAYS_SUCCEEDS(Preferences::SetCString(kAllowUrlPref, ""));
    MOZ_ALWAYS_SUCCEEDS(Preferences::SetCString(kDenyUrlPref, ""));
  }

  already_AddRefed<nsIContentAnalysisRequest> CreateRequest(const char* aUrl) {
    nsCOMPtr<nsIURI> uri;
    MOZ_ALWAYS_SUCCEEDS(NS_NewURI(getter_AddRefs(uri), aUrl));
    // We will only use the URL and, implicitly, the analysisType
    // (behavior differs for download vs other types).
    return RefPtr(new ContentAnalysisRequest(
                      nsIContentAnalysisRequest::AnalysisType::eFileTransfer,
                      EmptyString(), false, EmptyCString(), uri,
                      nsIContentAnalysisRequest::OperationType::eDroppedText,
                      nullptr))
        .forget();
  }

  RefPtr<ContentAnalysis> mContentAnalysis;

  // Proxies for private members of ContentAnalysis.  TEST_F
  // creates new subclasses -- they do not inherit `friend`s.
  // (FRIEND_TEST is another more verbose solution.)
  using UrlFilterResult = ContentAnalysis::UrlFilterResult;
  UrlFilterResult FilterByUrlLists(nsIContentAnalysisRequest* aReq) {
    return mContentAnalysis->FilterByUrlLists(aReq);
  }
};

TEST_F(ContentAnalysisTest, AllowUrlList) {
  MOZ_ALWAYS_SUCCEEDS(
      Preferences::SetCString(kAllowUrlPref, ".*\\.org/match.*"));
  RefPtr<nsIContentAnalysisRequest> car =
      CreateRequest("https://example.org/matchme/");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eAllow);
  car = CreateRequest("https://example.com/matchme/");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eCheck);
}

TEST_F(ContentAnalysisTest, MultipleAllowUrlList) {
  MOZ_ALWAYS_SUCCEEDS(Preferences::SetCString(
      kAllowUrlPref, ".*\\.org/match.* .*\\.net/match.*"));
  RefPtr<nsIContentAnalysisRequest> car =
      CreateRequest("https://example.org/matchme/");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eAllow);
  car = CreateRequest("https://example.net/matchme/");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eAllow);
  car = CreateRequest("https://example.com/matchme/");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eCheck);
}

TEST_F(ContentAnalysisTest, DenyUrlList) {
  MOZ_ALWAYS_SUCCEEDS(
      Preferences::SetCString(kDenyUrlPref, ".*\\.com/match.*"));
  RefPtr<nsIContentAnalysisRequest> car =
      CreateRequest("https://example.org/matchme/");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eCheck);
  car = CreateRequest("https://example.com/matchme/");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eDeny);
}

TEST_F(ContentAnalysisTest, MultipleDenyUrlList) {
  MOZ_ALWAYS_SUCCEEDS(Preferences::SetCString(
      kDenyUrlPref, ".*\\.com/match.* .*\\.biz/match.*"));
  RefPtr<nsIContentAnalysisRequest> car =
      CreateRequest("https://example.org/matchme/");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eCheck);
  car = CreateRequest("https://example.com/matchme/");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eDeny);
  car = CreateRequest("https://example.biz/matchme/");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eDeny);
}

TEST_F(ContentAnalysisTest, DenyOverridesAllowUrlList) {
  MOZ_ALWAYS_SUCCEEDS(
      Preferences::SetCString(kAllowUrlPref, ".*\\.org/match.*"));
  MOZ_ALWAYS_SUCCEEDS(Preferences::SetCString(kDenyUrlPref, ".*.org/match.*"));
  RefPtr<nsIContentAnalysisRequest> car =
      CreateRequest("https://example.org/matchme/");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eDeny);
}
