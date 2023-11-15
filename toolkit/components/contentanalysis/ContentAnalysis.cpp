/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentAnalysis.h"
#include "ContentAnalysisIPCTypes.h"
#include "content_analysis/sdk/analysis_client.h"

#include "base/process_util.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/Logging.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_browser.h"
#include "nsAppRunner.h"
#include "nsComponentManagerUtils.h"
#include "nsIClassInfoImpl.h"
#include "nsIFile.h"
#include "nsIGlobalObject.h"
#include "nsIObserverService.h"
#include "ScopedNSSTypes.h"
#include "xpcpublic.h"

#include <algorithm>
#include <sstream>

#ifdef XP_WIN
#  include <windows.h>
#  define SECURITY_WIN32 1
#  include <security.h>
#endif  // XP_WIN

namespace {

const char* kIsDLPEnabledPref = "browser.contentanalysis.enabled";
const char* kIsPerUserPref = "browser.contentanalysis.is_per_user";
const char* kPipePathNamePref = "browser.contentanalysis.pipe_path_name";

static constexpr uint32_t kAnalysisTimeoutSecs = 30;  // 30 sec

nsresult MakePromise(JSContext* aCx, RefPtr<mozilla::dom::Promise>* aPromise) {
  nsIGlobalObject* go = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!go)) {
    return NS_ERROR_UNEXPECTED;
  }
  mozilla::ErrorResult result;
  *aPromise = mozilla::dom::Promise::Create(go, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }
  return NS_OK;
}

nsCString GenerateRequestToken() {
  nsID id = nsID::GenerateUUID();
  return nsCString(id.ToString().get());
}

static nsresult GetFileDisplayName(const nsString& aFilePath,
                                   nsString& aFileDisplayName) {
  nsresult rv;
  nsCOMPtr<nsIFile> file = do_CreateInstance("@mozilla.org/file/local;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = file->InitWithPath(aFilePath);
  NS_ENSURE_SUCCESS(rv, rv);
  return file->GetDisplayName(aFileDisplayName);
}

}  // anonymous namespace

namespace mozilla::contentanalysis {

LazyLogModule gContentAnalysisLog("contentanalysis");
#define LOGD(...) MOZ_LOG(gContentAnalysisLog, LogLevel::Debug, (__VA_ARGS__))

NS_IMETHODIMP
ContentAnalysisRequest::GetAnalysisType(uint32_t* aAnalysisType) {
  *aAnalysisType = mAnalysisType;
  return NS_OK;
}

NS_IMETHODIMP
ContentAnalysisRequest::GetTextContent(nsAString& aTextContent) {
  aTextContent = mTextContent;
  return NS_OK;
}

NS_IMETHODIMP
ContentAnalysisRequest::GetFilePath(nsAString& aFilePath) {
  aFilePath = mFilePath;
  return NS_OK;
}

NS_IMETHODIMP
ContentAnalysisRequest::GetUrl(nsAString& aUrl) {
  aUrl = mUrl;
  return NS_OK;
}

NS_IMETHODIMP
ContentAnalysisRequest::GetEmail(nsAString& aEmail) {
  aEmail = mEmail;
  return NS_OK;
}

NS_IMETHODIMP
ContentAnalysisRequest::GetSha256Digest(nsACString& aSha256Digest) {
  aSha256Digest = mSha256Digest;
  return NS_OK;
}

NS_IMETHODIMP
ContentAnalysisRequest::GetResources(
    nsTArray<RefPtr<nsIClientDownloadResource>>& aResources) {
  aResources = mResources.Clone();
  return NS_OK;
}

NS_IMETHODIMP
ContentAnalysisRequest::GetRequestToken(nsACString& aRequestToken) {
  aRequestToken = mRequestToken;
  return NS_OK;
}

NS_IMETHODIMP
ContentAnalysisRequest::GetOperationTypeForDisplay(uint32_t* aOperationType) {
  *aOperationType = mOperationTypeForDisplay;
  return NS_OK;
}

NS_IMETHODIMP
ContentAnalysisRequest::GetOperationDisplayString(
    nsAString& aOperationDisplayString) {
  aOperationDisplayString = mOperationDisplayString;
  return NS_OK;
}

NS_IMETHODIMP
ContentAnalysisRequest::GetWindowGlobalParent(
    dom::WindowGlobalParent** aWindowGlobalParent) {
  NS_IF_ADDREF(*aWindowGlobalParent = mWindowGlobalParent);
  return NS_OK;
}

/* static */
StaticDataMutex<UniquePtr<content_analysis::sdk::Client>>
    ContentAnalysis::sCaClient("ContentAnalysisClient");

nsresult ContentAnalysis::EnsureContentAnalysisClient() {
  auto caClientRef = sCaClient.Lock();
  auto& caClient = caClientRef.ref();
  if (caClient) {
    return NS_OK;
  }

  nsAutoCString pipePathName;
  Preferences::GetCString(kPipePathNamePref, pipePathName);
  caClient.reset(
      content_analysis::sdk::Client::Create(
          {pipePathName.Data(), Preferences::GetBool(kIsPerUserPref)})
          .release());
  LOGD("Content analysis is %s", caClient ? "connected" : "not available");
  return caClient ? NS_OK : NS_ERROR_NOT_AVAILABLE;
}

ContentAnalysisRequest::ContentAnalysisRequest(
    unsigned long aAnalysisType, nsString&& aString, bool aStringIsFilePath,
    nsCString&& aSha256Digest, nsString&& aUrl, unsigned long aResourceNameType,
    dom::WindowGlobalParent* aWindowGlobalParent)
    : mAnalysisType(aAnalysisType),
      mUrl(std::move(aUrl)),
      mSha256Digest(std::move(aSha256Digest)),
      mWindowGlobalParent(aWindowGlobalParent) {
  if (aStringIsFilePath) {
    mFilePath = std::move(aString);
  } else {
    mTextContent = std::move(aString);
  }
  mOperationTypeForDisplay = aResourceNameType;
  if (mOperationTypeForDisplay == OPERATION_CUSTOMDISPLAYSTRING) {
    MOZ_ASSERT(aStringIsFilePath);
    nsresult rv = GetFileDisplayName(mFilePath, mOperationDisplayString);
    if (NS_FAILED(rv)) {
      mOperationDisplayString = u"file";
    }
  }

  mRequestToken = GenerateRequestToken();
}

static nsresult ConvertToProtobuf(
    nsIClientDownloadResource* aIn,
    content_analysis::sdk::ClientDownloadRequest_Resource* aOut) {
  nsString url;
  nsresult rv = aIn->GetUrl(url);
  NS_ENSURE_SUCCESS(rv, rv);
  aOut->set_url(NS_ConvertUTF16toUTF8(url).get());

  uint32_t resourceType;
  rv = aIn->GetType(&resourceType);
  NS_ENSURE_SUCCESS(rv, rv);
  aOut->set_type(
      static_cast<content_analysis::sdk::ClientDownloadRequest_ResourceType>(
          resourceType));

  return NS_OK;
}

static nsresult ConvertToProtobuf(
    nsIContentAnalysisRequest* aIn,
    content_analysis::sdk::ContentAnalysisRequest* aOut) {
  aOut->set_expires_at(time(nullptr) + kAnalysisTimeoutSecs);  // TODO:

  uint32_t analysisType;
  nsresult rv = aIn->GetAnalysisType(&analysisType);
  NS_ENSURE_SUCCESS(rv, rv);
  auto connector =
      static_cast<content_analysis::sdk::AnalysisConnector>(analysisType);
  aOut->set_analysis_connector(connector);

  nsCString requestToken;
  rv = aIn->GetRequestToken(requestToken);
  NS_ENSURE_SUCCESS(rv, rv);
  aOut->set_request_token(requestToken.get(), requestToken.Length());

  const std::string tag = "dlp";  // TODO:
  *aOut->add_tags() = tag;

  auto* requestData = aOut->mutable_request_data();

  nsString url;
  rv = aIn->GetUrl(url);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!url.IsEmpty()) {
    requestData->set_url(NS_ConvertUTF16toUTF8(url).get());
  }

  nsString email;
  rv = aIn->GetEmail(email);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!email.IsEmpty()) {
    requestData->set_email(NS_ConvertUTF16toUTF8(email).get());
  }

  nsCString sha256Digest;
  rv = aIn->GetSha256Digest(sha256Digest);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!sha256Digest.IsEmpty()) {
    requestData->set_digest(sha256Digest.get());
  }

  nsString filePath;
  rv = aIn->GetFilePath(filePath);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!filePath.IsEmpty()) {
    std::string filePathStr = NS_ConvertUTF16toUTF8(filePath).get();
    aOut->set_file_path(filePathStr);
    auto filename = filePathStr.substr(filePathStr.find_last_of("/\\") + 1);
    if (!filename.empty()) {
      requestData->set_filename(filename);
    }
  } else {
    nsString textContent;
    rv = aIn->GetTextContent(textContent);
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_ASSERT(!textContent.IsEmpty());
    aOut->set_text_content(NS_ConvertUTF16toUTF8(textContent).get());
  }

#ifdef XP_WIN
  ULONG userLen = 0;
  GetUserNameExW(NameSamCompatible, nullptr, &userLen);
  if (GetLastError() == ERROR_MORE_DATA && userLen > 0) {
    auto user = mozilla::MakeUnique<wchar_t[]>(userLen);
    if (GetUserNameExW(NameSamCompatible, user.get(), &userLen)) {
      auto* clientMetadata = aOut->mutable_client_metadata();
      auto* browser = clientMetadata->mutable_browser();
      browser->set_machine_user(NS_ConvertUTF16toUTF8(user.get()).get());
    }
  }
#endif

  nsTArray<RefPtr<nsIClientDownloadResource>> resources;
  rv = aIn->GetResources(resources);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!resources.IsEmpty()) {
    auto* pbClientDownloadRequest = requestData->mutable_csd();
    for (auto& nsResource : resources) {
      rv = ConvertToProtobuf(nsResource.get(),
                             pbClientDownloadRequest->add_resources());
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

static void LogRequest(
    content_analysis::sdk::ContentAnalysisRequest* aPbRequest) {
  // We cannot use Protocol Buffer's DebugString() because we optimize for
  // lite runtime.
  if (!static_cast<LogModule*>(gContentAnalysisLog)
           ->ShouldLog(LogLevel::Debug)) {
    return;
  }

  std::stringstream ss;
  ss << "ContentAnalysisRequest:"
     << "\n";

#define ADD_FIELD(PBUF, NAME, FUNC) \
  ss << "  " << (NAME) << ": ";     \
  if ((PBUF)->has_##FUNC())         \
    ss << (PBUF)->FUNC() << "\n";   \
  else                              \
    ss << "<none>"                  \
       << "\n";

#define ADD_EXISTS(PBUF, NAME, FUNC) \
  ss << "  " << (NAME) << ": "       \
     << ((PBUF)->has_##FUNC() ? "<exists>" : "<none>") << "\n";

  ADD_FIELD(aPbRequest, "Expires", expires_at);
  ADD_FIELD(aPbRequest, "Analysis Type", analysis_connector);
  ADD_FIELD(aPbRequest, "Request Token", request_token);
  ADD_FIELD(aPbRequest, "File Path", file_path);
  ADD_FIELD(aPbRequest, "Text Content", text_content);
  // TODO: Tags
  ADD_EXISTS(aPbRequest, "Request Data Struct", request_data);
  const auto* requestData =
      aPbRequest->has_request_data() ? &aPbRequest->request_data() : nullptr;
  if (requestData) {
    ADD_FIELD(requestData, "  Url", url);
    ADD_FIELD(requestData, "  Email", email);
    ADD_FIELD(requestData, "  SHA-256 Digest", digest);
    ADD_FIELD(requestData, "  Filename", filename);
    ADD_EXISTS(requestData, "  Client Download Request struct", csd);
    const auto* csd = requestData->has_csd() ? &requestData->csd() : nullptr;
    if (csd) {
      uint32_t i = 0;
      for (const auto& resource : csd->resources()) {
        ss << "      Resource " << i << ":"
           << "\n";
        ADD_FIELD(&resource, "      Url", url);
        ADD_FIELD(&resource, "      Type", type);
        ++i;
      }
    }
  }
  ADD_EXISTS(aPbRequest, "Client Metadata Struct", client_metadata);
  const auto* clientMetadata = aPbRequest->has_client_metadata()
                                   ? &aPbRequest->client_metadata()
                                   : nullptr;
  if (clientMetadata) {
    ADD_EXISTS(clientMetadata, "  Browser Struct", browser);
    const auto* browser =
        clientMetadata->has_browser() ? &clientMetadata->browser() : nullptr;
    if (browser) {
      ADD_FIELD(browser, "    Machine User", machine_user);
    }
  }

#undef ADD_EXISTS
#undef ADD_FIELD

  LOGD("%s", ss.str().c_str());
}

ContentAnalysisResponse::ContentAnalysisResponse(
    content_analysis::sdk::ContentAnalysisResponse&& aResponse) {
  mAction = nsIContentAnalysisResponse::ACTION_UNSPECIFIED;
  for (const auto& result : aResponse.results()) {
    if (!result.has_status() ||
        result.status() !=
            content_analysis::sdk::ContentAnalysisResponse::Result::SUCCESS) {
      mAction = nsIContentAnalysisResponse::ACTION_UNSPECIFIED;
      return;
    }
    // The action values increase with severity, so the max is the most severe.
    for (const auto& rule : result.triggered_rules()) {
      mAction = std::max(mAction, static_cast<uint32_t>(rule.action()));
    }
  }

  // If no rules blocked then we should allow.
  if (mAction == nsIContentAnalysisResponse::ACTION_UNSPECIFIED) {
    mAction = nsIContentAnalysisResponse::ALLOW;
  }

  const auto& requestToken = aResponse.request_token();
  mRequestToken.Assign(requestToken.data(), requestToken.size());
}

ContentAnalysisResponse::ContentAnalysisResponse(
    unsigned long aAction, const nsACString& aRequestToken)
    : mAction(aAction), mRequestToken(aRequestToken) {}

/* static */
already_AddRefed<ContentAnalysisResponse> ContentAnalysisResponse::FromProtobuf(
    content_analysis::sdk::ContentAnalysisResponse&& aResponse) {
  auto ret = RefPtr<ContentAnalysisResponse>(
      new ContentAnalysisResponse(std::move(aResponse)));

  using PBContentAnalysisResponse =
      content_analysis::sdk::ContentAnalysisResponse;
  if (ret->mAction ==
      PBContentAnalysisResponse::Result::TriggeredRule::ACTION_UNSPECIFIED) {
    return nullptr;
  }

  return ret.forget();
}

/* static */
RefPtr<ContentAnalysisResponse> ContentAnalysisResponse::FromAction(
    unsigned long aAction, const nsACString& aRequestToken) {
  if (aAction == nsIContentAnalysisResponse::ACTION_UNSPECIFIED) {
    return nullptr;
  }
  return RefPtr<ContentAnalysisResponse>(
      new ContentAnalysisResponse(aAction, aRequestToken));
}

NS_IMETHODIMP
ContentAnalysisResponse::GetRequestToken(nsACString& aRequestToken) {
  aRequestToken = mRequestToken;
  return NS_OK;
}

static void LogResponse(
    content_analysis::sdk::ContentAnalysisResponse* aPbResponse) {
  if (!static_cast<LogModule*>(gContentAnalysisLog)
           ->ShouldLog(LogLevel::Debug)) {
    return;
  }

  std::stringstream ss;
  ss << "ContentAnalysisResponse:"
     << "\n";

#define ADD_FIELD(PBUF, NAME, FUNC) \
  ss << "  " << (NAME) << ": ";     \
  if ((PBUF)->has_##FUNC())         \
    ss << (PBUF)->FUNC() << "\n";   \
  else                              \
    ss << "<none>"                  \
       << "\n";

  ADD_FIELD(aPbResponse, "Request Token", request_token);
  uint32_t i = 0;
  for (const auto& result : aPbResponse->results()) {
    ss << "  Result " << i << ":"
       << "\n";
    ADD_FIELD(&result, "    Status", status);
    uint32_t j = 0;
    for (const auto& rule : result.triggered_rules()) {
      ss << "    Rule " << j << ":"
         << "\n";
      ADD_FIELD(&rule, "    action", action);
      ++j;
    }
    ++i;
  }

#undef ADD_FIELD

  LOGD("%s", ss.str().c_str());
}

static nsresult ConvertToProtobuf(
    nsIContentAnalysisAcknowledgement* aIn, const nsACString& aRequestToken,
    content_analysis::sdk::ContentAnalysisAcknowledgement* aOut) {
  aOut->set_request_token(aRequestToken.Data(), aRequestToken.Length());

  uint32_t result;
  nsresult rv = aIn->GetResult(&result);
  NS_ENSURE_SUCCESS(rv, rv);
  aOut->set_status(
      static_cast<content_analysis::sdk::ContentAnalysisAcknowledgement_Status>(
          result));

  uint32_t finalAction;
  rv = aIn->GetFinalAction(&finalAction);
  NS_ENSURE_SUCCESS(rv, rv);
  aOut->set_final_action(
      static_cast<
          content_analysis::sdk::ContentAnalysisAcknowledgement_FinalAction>(
          finalAction));

  return NS_OK;
}

NS_IMETHODIMP
ContentAnalysisResponse::GetAction(uint32_t* aAction) {
  *aAction = mAction;
  return NS_OK;
}

static void LogAcknowledgement(
    content_analysis::sdk::ContentAnalysisAcknowledgement* aPbAck) {
  if (!static_cast<LogModule*>(gContentAnalysisLog)
           ->ShouldLog(LogLevel::Debug)) {
    return;
  }

  std::stringstream ss;
  ss << "ContentAnalysisAcknowledgement:"
     << "\n";

#define ADD_FIELD(PBUF, NAME, FUNC) \
  ss << "  " << (NAME) << ": ";     \
  if ((PBUF)->has_##FUNC())         \
    ss << (PBUF)->FUNC() << "\n";   \
  else                              \
    ss << "<none>"                  \
       << "\n";

  ADD_FIELD(aPbAck, "Status", status);
  ADD_FIELD(aPbAck, "Final Action", final_action);

#undef ADD_FIELD

  LOGD("%s", ss.str().c_str());
}

void ContentAnalysisResponse::SetOwner(RefPtr<ContentAnalysis> aOwner) {
  mOwner = aOwner;
}

namespace {
static bool ShouldAllowAction(uint32_t aResponseCode) {
  return aResponseCode == nsIContentAnalysisResponse::ALLOW ||
         aResponseCode == nsIContentAnalysisResponse::REPORT_ONLY ||
         aResponseCode == nsIContentAnalysisResponse::WARN;
}
}  // namespace

NS_IMETHODIMP ContentAnalysisResponse::GetShouldAllowContent(
    bool* aShouldAllowContent) {
  *aShouldAllowContent = ShouldAllowAction(mAction);
  return NS_OK;
}

NS_IMETHODIMP ContentAnalysisResult::GetShouldAllowContent(
    bool* aShouldAllowContent) {
  if (mValue.is<NoContentAnalysisResult>()) {
    NoContentAnalysisResult result = mValue.as<NoContentAnalysisResult>();
    *aShouldAllowContent =
        result == NoContentAnalysisResult::AGENT_NOT_PRESENT ||
        result == NoContentAnalysisResult::NO_PARENT_BROWSER;
  } else {
    *aShouldAllowContent = ShouldAllowAction(mValue.as<uint32_t>());
  }
  return NS_OK;
}

NS_IMPL_CLASSINFO(ContentAnalysisRequest, nullptr, 0, {0});
NS_IMPL_ISUPPORTS_CI(ContentAnalysisRequest, nsIContentAnalysisRequest);
NS_IMPL_CLASSINFO(ContentAnalysisResponse, nullptr, 0, {0});
NS_IMPL_ISUPPORTS_CI(ContentAnalysisResponse, nsIContentAnalysisResponse);
NS_IMPL_ISUPPORTS(ContentAnalysisCallback, nsIContentAnalysisCallback);
NS_IMPL_ISUPPORTS(ContentAnalysisResult, nsIContentAnalysisResult);
NS_IMPL_ISUPPORTS(ContentAnalysis, nsIContentAnalysis);

ContentAnalysis::~ContentAnalysis() {
  auto caClientRef = sCaClient.Lock();
  auto& caClient = caClientRef.ref();
  caClient = nullptr;
}

NS_IMETHODIMP
ContentAnalysis::GetIsActive(bool* aIsActive) {
  *aIsActive = false;
  // gAllowContentAnalysis is only set in the parent process
  MOZ_ASSERT(XRE_IsParentProcess());
  if (!gAllowContentAnalysis || !Preferences::GetBool(kIsDLPEnabledPref)) {
    return NS_OK;
  }

  nsresult rv = EnsureContentAnalysisClient();
  *aIsActive = NS_SUCCEEDED(rv);
  LOGD("Local DLP Content Analysis is %sactive", *aIsActive ? "" : "not ");
  return NS_OK;
}

NS_IMETHODIMP
ContentAnalysis::GetMightBeActive(bool* aMightBeActive) {
  // A DLP connection is not permitted to be added/removed while the
  // browser is running, so we can cache this.
  static bool sIsEnabled = Preferences::GetBool(kIsDLPEnabledPref);
  // Note that we can't check gAllowContentAnalysis here because it
  // only gets set in the parent process.
  *aMightBeActive = sIsEnabled;
  return NS_OK;
}

nsresult ContentAnalysis::RunAnalyzeRequestTask(
    RefPtr<nsIContentAnalysisRequest> aRequest,
    RefPtr<nsIContentAnalysisCallback> aCallback) {
  nsresult rv = NS_ERROR_FAILURE;
  auto callbackCopy = aCallback;
  auto se = MakeScopeExit([&] {
    if (!NS_SUCCEEDED(rv)) {
      LOGD("RunAnalyzeRequestTask failed");
      callbackCopy->Error(rv);
    }
  });

  // The Client object from the SDK must be kept live as long as there are
  // active transactions.
  RefPtr<ContentAnalysis> owner = this;

  content_analysis::sdk::ContentAnalysisRequest pbRequest;
  rv = ConvertToProtobuf(aRequest, &pbRequest);
  NS_ENSURE_SUCCESS(rv, rv);

  LOGD("Issuing ContentAnalysisRequest");
  LogRequest(&pbRequest);

  nsCString requestToken;
  rv = aRequest->GetRequestToken(requestToken);
  NS_ENSURE_SUCCESS(rv, rv);

  // The content analysis connection is synchronous so run in the background.
  rv = NS_DispatchBackgroundTask(
      NS_NewRunnableFunction(
          "RunAnalyzeRequestTask",
          [pbRequest = std::move(pbRequest), aCallback = std::move(aCallback),
           requestToken = std::move(requestToken), owner] {
            nsresult rv = NS_ERROR_FAILURE;
            content_analysis::sdk::ContentAnalysisResponse pbResponse;

            auto resolveOnMainThread = MakeScopeExit([&] {
              NS_DispatchToMainThread(NS_NewRunnableFunction(
                  "ResolveOnMainThread",
                  [rv, owner, aCallback = std::move(aCallback),
                   pbResponse = std::move(pbResponse), requestToken]() mutable {
                    if (NS_SUCCEEDED(rv)) {
                      LOGD(
                          "Content analysis resolving response promise for "
                          "token %s",
                          requestToken.get());
                      RefPtr<ContentAnalysisResponse> response =
                          ContentAnalysisResponse::FromProtobuf(
                              std::move(pbResponse));
                      if (response) {
                        response->SetOwner(owner);
                        nsCOMPtr<nsIObserverService> obsServ =
                            mozilla::services::GetObserverService();
                        obsServ->NotifyObservers(response, "dlp-response",
                                                 nullptr);
                        aCallback->ContentResult(response);
                      } else {
                        aCallback->Error(NS_ERROR_FAILURE);
                      }
                    } else {
                      aCallback->Error(rv);
                    }
                  }));
            });

            auto caClientRef = sCaClient.Lock();
            auto& caClient = caClientRef.ref();
            if (!caClient) {
              LOGD("RunAnalyzeRequestTask failed to get client");
              rv = NS_ERROR_NOT_AVAILABLE;
              return;
            }

            // Run request, then dispatch back to main thread to resolve
            // aPromise
            int err = caClient->Send(pbRequest, &pbResponse);
            if (err != 0) {
              LOGD("RunAnalyzeRequestTask client transaction failed");
              rv = NS_ERROR_FAILURE;
              return;
            }

            LOGD("Content analysis client transaction succeeded");
            LogResponse(&pbResponse);
            rv = NS_OK;
          }),
      NS_DISPATCH_EVENT_MAY_BLOCK);

  return rv;
}

NS_IMETHODIMP
ContentAnalysis::AnalyzeContentRequest(nsIContentAnalysisRequest* aRequest,
                                       JSContext* aCx,
                                       mozilla::dom::Promise** aPromise) {
  RefPtr<mozilla::dom::Promise> promise;
  nsresult rv = MakePromise(aCx, &promise);
  NS_ENSURE_SUCCESS(rv, rv);
  RefPtr<ContentAnalysisCallback> callbackPtr =
      new ContentAnalysisCallback(promise);
  promise.forget(aPromise);
  return AnalyzeContentRequestCallback(aRequest, callbackPtr.get());
}

NS_IMETHODIMP
ContentAnalysis::AnalyzeContentRequestCallback(
    nsIContentAnalysisRequest* aRequest,
    nsIContentAnalysisCallback* aCallback) {
  NS_ENSURE_ARG(aRequest);
  NS_ENSURE_ARG(aCallback);

  bool isActive;
  nsresult rv = GetIsActive(&isActive);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!isActive) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIObserverService> obsServ =
      mozilla::services::GetObserverService();
  obsServ->NotifyObservers(aRequest, "dlp-request-made", nullptr);

  rv = RunAnalyzeRequestTask(aRequest, aCallback);
  return rv;
}

NS_IMETHODIMP
ContentAnalysisResponse::Acknowledge(
    nsIContentAnalysisAcknowledgement* aAcknowledgement) {
  MOZ_ASSERT(mOwner);
  return mOwner->RunAcknowledgeTask(aAcknowledgement, mRequestToken);
};

nsresult ContentAnalysis::RunAcknowledgeTask(
    nsIContentAnalysisAcknowledgement* aAcknowledgement,
    const nsACString& aRequestToken) {
  bool isActive;
  nsresult rv = GetIsActive(&isActive);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!isActive) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  content_analysis::sdk::ContentAnalysisAcknowledgement pbAck;
  rv = ConvertToProtobuf(aAcknowledgement, aRequestToken, &pbAck);
  NS_ENSURE_SUCCESS(rv, rv);

  LOGD("Issuing ContentAnalysisAcknowledgement");
  LogAcknowledgement(&pbAck);

  // The Client object from the SDK must be kept live as long as there are
  // active transactions.
  RefPtr<ContentAnalysis> owner = this;

  // The content analysis connection is synchronous so run in the background.
  LOGD("RunAcknowledgeTask dispatching acknowledge task");
  return NS_DispatchBackgroundTask(NS_NewRunnableFunction(
      "RunAcknowledgeTask", [owner, pbAck = std::move(pbAck)] {
        auto caClientRef = sCaClient.Lock();
        auto& caClient = caClientRef.ref();
        if (!caClient) {
          LOGD("RunAcknowledgeTask failed to get the client");
          return;
        }

        DebugOnly<int> err = caClient->Acknowledge(pbAck);
        MOZ_ASSERT(err == 0);
        LOGD("RunAcknowledgeTask sent transaction acknowledgement");
      }));
}

NS_IMETHODIMP ContentAnalysisCallback::ContentResult(
    nsIContentAnalysisResponse* aResponse) {
  if (mPromise.isSome()) {
    mPromise->get()->MaybeResolve(aResponse);
  } else {
    mContentResponseCallback(aResponse);
  }
  return NS_OK;
}

NS_IMETHODIMP ContentAnalysisCallback::Error(nsresult aError) {
  if (mPromise.isSome()) {
    mPromise->get()->MaybeReject(aError);
  } else {
    mErrorCallback(aError);
  }
  return NS_OK;
}

ContentAnalysisCallback::ContentAnalysisCallback(RefPtr<dom::Promise> aPromise)
    : mPromise(Some(new nsMainThreadPtrHolder<dom::Promise>(
          "content analysis promise", aPromise))) {}
}  // namespace mozilla::contentanalysis
