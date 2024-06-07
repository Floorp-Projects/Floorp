/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "nsComponentManagerUtils.h"
#include "nsNetUtil.h"
#include "nsIFile.h"
#include "nsIObserverService.h"
#include "nsIURI.h"
#include "nsIURIMutator.h"
#include "ContentAnalysis.h"
#include "SpecialSystemDirectory.h"
#include "TestContentAnalysisUtils.h"
#include <processenv.h>
#include <synchapi.h>
#include <vector>

const char* kAllowUrlPref = "browser.contentanalysis.allow_url_regex_list";
const char* kDenyUrlPref = "browser.contentanalysis.deny_url_regex_list";
const char* kPipePathNamePref = "browser.contentanalysis.pipe_path_name";
const char* kIsDLPEnabledPref = "browser.contentanalysis.enabled";
const char* kTimeoutPref = "browser.contentanalysis.agent_timeout";

using namespace mozilla;
using namespace mozilla::contentanalysis;

class ContentAnalysisTest : public testing::Test {
 protected:
  ContentAnalysisTest() {
    auto* logmodule = LogModule::Get("contentanalysis");
    logmodule->SetLevel(LogLevel::Verbose);
    MOZ_ALWAYS_SUCCEEDS(
        Preferences::SetString(kPipePathNamePref, mPipeName.get()));
    MOZ_ALWAYS_SUCCEEDS(Preferences::SetBool(kIsDLPEnabledPref, true));

    nsCOMPtr<nsIContentAnalysis> caSvc =
        do_GetService("@mozilla.org/contentanalysis;1");
    MOZ_ASSERT(caSvc);
    mContentAnalysis = static_cast<ContentAnalysis*>(caSvc.get());

    // Tests run earlier could have altered these values
    mContentAnalysis->mParsedUrlLists = false;
    mContentAnalysis->mAllowUrlList = {};
    mContentAnalysis->mDenyUrlList = {};

    MOZ_ALWAYS_SUCCEEDS(mContentAnalysis->TestOnlySetCACmdLineArg(true));

    MOZ_ALWAYS_SUCCEEDS(Preferences::SetCString(kAllowUrlPref, ""));
    MOZ_ALWAYS_SUCCEEDS(Preferences::SetCString(kDenyUrlPref, ""));

    bool isActive = false;
    MOZ_ALWAYS_SUCCEEDS(mContentAnalysis->GetIsActive(&isActive));
    EXPECT_TRUE(isActive);
  }

  // Note that the constructor (and SetUp() method) get called once per test,
  // not once for the whole fixture. Because Firefox does not currently
  // reconnect to an agent after the DLP pipe is closed (bug 1888293), we only
  // want to create the agent once and make sure the same process stays alive
  // through all of these tests.
  static void SetUpTestSuite() {
    GeneratePipeName(L"contentanalysissdk-gtest-", mPipeName);
    mAgentInfo = LaunchAgentNormal(L"block", mPipeName);
  }

  static void TearDownTestSuite() { mAgentInfo.TerminateProcess(); }

  void TearDown() override {
    mContentAnalysis->mParsedUrlLists = false;
    mContentAnalysis->mAllowUrlList = {};
    mContentAnalysis->mDenyUrlList = {};

    MOZ_ALWAYS_SUCCEEDS(mContentAnalysis->TestOnlySetCACmdLineArg(false));

    MOZ_ALWAYS_SUCCEEDS(Preferences::SetCString(kAllowUrlPref, ""));
    MOZ_ALWAYS_SUCCEEDS(Preferences::SetCString(kDenyUrlPref, ""));
    MOZ_ALWAYS_SUCCEEDS(Preferences::ClearUser(kPipePathNamePref));
    MOZ_ALWAYS_SUCCEEDS(Preferences::ClearUser(kIsDLPEnabledPref));
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
  static nsString mPipeName;
  static MozAgentInfo mAgentInfo;

  // Proxies for private members of ContentAnalysis.  TEST_F
  // creates new subclasses -- they do not inherit `friend`s.
  // (FRIEND_TEST is another more verbose solution.)
  using UrlFilterResult = ContentAnalysis::UrlFilterResult;
  UrlFilterResult FilterByUrlLists(nsIContentAnalysisRequest* aReq) {
    return mContentAnalysis->FilterByUrlLists(aReq);
  }
};
nsString ContentAnalysisTest::mPipeName;
MozAgentInfo ContentAnalysisTest::mAgentInfo;

TEST_F(ContentAnalysisTest, AllowUrlList) {
  MOZ_ALWAYS_SUCCEEDS(
      Preferences::SetCString(kAllowUrlPref, ".*\\.org/match.*"));
  RefPtr<nsIContentAnalysisRequest> car =
      CreateRequest("https://example.org/matchme/");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eAllow);
  car = CreateRequest("https://example.com/matchme/");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eCheck);
}

TEST_F(ContentAnalysisTest, DefaultAllowUrlList) {
  MOZ_ALWAYS_SUCCEEDS(Preferences::ClearUser(kAllowUrlPref));
  RefPtr<nsIContentAnalysisRequest> car = CreateRequest("about:home");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eAllow);
  car = CreateRequest("about:blank");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eCheck);
  car = CreateRequest("about:srcdoc");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eCheck);
  car = CreateRequest("https://example.com/");
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

nsCOMPtr<nsIURI> GetExampleDotComURI() {
  nsCOMPtr<nsIURI> uri;
  MOZ_ALWAYS_SUCCEEDS(NS_NewURI(getter_AddRefs(uri), "https://example.com"));
  return uri;
}

void SendRequestAndExpectResponse(
    RefPtr<ContentAnalysis> contentAnalysis,
    const nsCOMPtr<nsIContentAnalysisRequest>& request,
    Maybe<bool> expectedShouldAllow,
    Maybe<nsIContentAnalysisResponse::Action> expectedAction) {
  std::atomic<bool> gotResponse = false;
  std::atomic<bool> timedOut = false;
  auto callback = MakeRefPtr<ContentAnalysisCallback>(
      [&](nsIContentAnalysisResponse* response) {
        if (expectedShouldAllow.isSome()) {
          bool shouldAllow = false;
          MOZ_ALWAYS_SUCCEEDS(response->GetShouldAllowContent(&shouldAllow));
          EXPECT_EQ(*expectedShouldAllow, shouldAllow);
        }
        if (expectedAction.isSome()) {
          nsIContentAnalysisResponse::Action action;
          MOZ_ALWAYS_SUCCEEDS(response->GetAction(&action));
          EXPECT_EQ(*expectedAction, action);
        }
        nsCString requestToken, originalRequestToken;
        MOZ_ALWAYS_SUCCEEDS(response->GetRequestToken(requestToken));
        MOZ_ALWAYS_SUCCEEDS(request->GetRequestToken(originalRequestToken));
        EXPECT_EQ(originalRequestToken, requestToken);
        gotResponse = true;
      },
      [&gotResponse](nsresult error) {
        EXPECT_EQ(NS_OK, error);
        gotResponse = true;
        // Make sure that we didn't somehow get passed NS_OK
        FAIL() << "Got error response";
      });

  MOZ_ALWAYS_SUCCEEDS(
      contentAnalysis->AnalyzeContentRequestCallback(request, false, callback));
  RefPtr<CancelableRunnable> timer =
      NS_NewCancelableRunnableFunction("Content Analysis timeout", [&] {
        if (!gotResponse.load()) {
          timedOut = true;
        }
      });
  NS_DelayedDispatchToCurrentThread(do_AddRef(timer), 10000);
  mozilla::SpinEventLoopUntil("Waiting for ContentAnalysis result"_ns, [&]() {
    return gotResponse.load() || timedOut.load();
  });
  timer->Cancel();
  EXPECT_TRUE(gotResponse);
  EXPECT_FALSE(timedOut);
}

TEST_F(ContentAnalysisTest, SendAllowedTextToAgent_GetAllowedResponse) {
  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();
  nsString allow(L"allow");
  nsCOMPtr<nsIContentAnalysisRequest> request = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry, std::move(allow),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);

  SendRequestAndExpectResponse(mContentAnalysis, request, Some(true),
                               Some(nsIContentAnalysisResponse::eAllow));
}

TEST_F(ContentAnalysisTest, SendBlockedTextToAgent_GetBlockResponse) {
  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();
  nsString block(L"block");
  nsCOMPtr<nsIContentAnalysisRequest> request = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry, std::move(block),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);

  SendRequestAndExpectResponse(mContentAnalysis, request, Some(false),
                               Some(nsIContentAnalysisResponse::eBlock));
}

class RawRequestObserver final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  RawRequestObserver() {}

  const std::vector<content_analysis::sdk::ContentAnalysisRequest>&
  GetRequests() {
    return mRequests;
  }

 private:
  ~RawRequestObserver() = default;
  std::vector<content_analysis::sdk::ContentAnalysisRequest> mRequests;
};

NS_IMPL_ISUPPORTS(RawRequestObserver, nsIObserver);

NS_IMETHODIMP RawRequestObserver::Observe(nsISupports* aSubject,
                                          const char* aTopic,
                                          const char16_t* aData) {
  std::wstring dataWideString(reinterpret_cast<const wchar_t*>(aData));
  std::vector<uint8_t> dataVector(dataWideString.size());
  for (size_t i = 0; i < dataWideString.size(); ++i) {
    // Since this data is really bytes and not a null-terminated string, the
    // calling code adds 0xFF00 to every member to ensure there are no 0 values.
    dataVector[i] = static_cast<uint8_t>(dataWideString[i] - 0xFF00);
  }
  content_analysis::sdk::ContentAnalysisRequest request;
  EXPECT_TRUE(request.ParseFromArray(dataVector.data(), dataVector.size()));
  mRequests.push_back(std::move(request));
  return NS_OK;
}

TEST_F(ContentAnalysisTest, CheckRawRequestWithText) {
  MOZ_ALWAYS_SUCCEEDS(Preferences::SetInt(kTimeoutPref, 65));
  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();
  nsString allow(L"allow");
  nsCOMPtr<nsIContentAnalysisRequest> request = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry, std::move(allow),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);
  nsCOMPtr<nsIObserverService> obsServ =
      mozilla::services::GetObserverService();
  auto rawRequestObserver = MakeRefPtr<RawRequestObserver>();
  MOZ_ALWAYS_SUCCEEDS(
      obsServ->AddObserver(rawRequestObserver, "dlp-request-sent-raw", false));
  time_t now = time(nullptr);

  SendRequestAndExpectResponse(mContentAnalysis, request, Nothing(), Nothing());
  auto requests = rawRequestObserver->GetRequests();
  EXPECT_EQ(static_cast<size_t>(1), requests.size());
  time_t t = requests[0].expires_at();
  time_t secs_remaining = t - now;
  // There should be around 65 seconds remaining
  EXPECT_LE(abs(secs_remaining - 65), 2);
  const auto& request_url = requests[0].request_data().url();
  EXPECT_EQ(uri->GetSpecOrDefault(),
            nsCString(request_url.data(), request_url.size()));
  nsCString request_user_action_id(requests[0].user_action_id().data(),
                                   requests[0].user_action_id().size());
  // The user_action_id has a GUID appended to the end, just make sure the
  // beginning is right.
  request_user_action_id.Truncate(8);
  EXPECT_EQ(nsCString("Firefox "), request_user_action_id);
  const auto& request_text = requests[0].text_content();
  EXPECT_EQ(nsCString("allow"),
            nsCString(request_text.data(), request_text.size()));

  MOZ_ALWAYS_SUCCEEDS(
      obsServ->RemoveObserver(rawRequestObserver, "dlp-request-sent-raw"));
  MOZ_ALWAYS_SUCCEEDS(Preferences::ClearUser(kTimeoutPref));
}

TEST_F(ContentAnalysisTest, CheckRawRequestWithFile) {
  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();
  nsCOMPtr<nsIFile> file;
  MOZ_ALWAYS_SUCCEEDS(GetSpecialSystemDirectory(OS_CurrentWorkingDirectory,
                                                getter_AddRefs(file)));
  nsString allowRelativePath(L"allowedFile.txt");
  MOZ_ALWAYS_SUCCEEDS(file->AppendRelativePath(allowRelativePath));
  nsString allowPath;
  MOZ_ALWAYS_SUCCEEDS(file->GetPath(allowPath));

  nsCOMPtr<nsIContentAnalysisRequest> request = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry, allowPath, true,
      EmptyCString(), uri, nsIContentAnalysisRequest::OperationType::eClipboard,
      nullptr);
  nsCOMPtr<nsIObserverService> obsServ =
      mozilla::services::GetObserverService();
  auto rawRequestObserver = MakeRefPtr<RawRequestObserver>();
  MOZ_ALWAYS_SUCCEEDS(
      obsServ->AddObserver(rawRequestObserver, "dlp-request-sent-raw", false));

  SendRequestAndExpectResponse(mContentAnalysis, request, Nothing(), Nothing());
  auto requests = rawRequestObserver->GetRequests();
  EXPECT_EQ(static_cast<size_t>(1), requests.size());
  const auto& request_url = requests[0].request_data().url();
  EXPECT_EQ(uri->GetSpecOrDefault(),
            nsCString(request_url.data(), request_url.size()));
  nsCString request_user_action_id(requests[0].user_action_id().data(),
                                   requests[0].user_action_id().size());
  // The user_action_id has a GUID appended to the end, just make sure the
  // beginning is right.
  request_user_action_id.Truncate(8);
  EXPECT_EQ(nsCString("Firefox "), request_user_action_id);
  const auto& request_file_path = requests[0].file_path();
  EXPECT_EQ(NS_ConvertUTF16toUTF8(allowPath),
            nsCString(request_file_path.data(), request_file_path.size()));

  MOZ_ALWAYS_SUCCEEDS(
      obsServ->RemoveObserver(rawRequestObserver, "dlp-request-sent-raw"));
}

TEST_F(ContentAnalysisTest, CheckTwoRequestsHaveSameUserActionId) {
  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();
  nsString allow(L"allow");
  nsCOMPtr<nsIContentAnalysisRequest> request = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry, std::move(allow),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);
  nsCOMPtr<nsIObserverService> obsServ =
      mozilla::services::GetObserverService();
  auto rawRequestObserver = MakeRefPtr<RawRequestObserver>();
  MOZ_ALWAYS_SUCCEEDS(
      obsServ->AddObserver(rawRequestObserver, "dlp-request-sent-raw", false));

  SendRequestAndExpectResponse(mContentAnalysis, request, Nothing(), Nothing());
  SendRequestAndExpectResponse(mContentAnalysis, request, Nothing(), Nothing());
  auto requests = rawRequestObserver->GetRequests();
  EXPECT_EQ(static_cast<size_t>(2), requests.size());
  EXPECT_EQ(requests[0].user_action_id(), requests[1].user_action_id());

  MOZ_ALWAYS_SUCCEEDS(
      obsServ->RemoveObserver(rawRequestObserver, "dlp-request-sent-raw"));
}
