/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentAnalysis.h"
#include "ContentAnalysisIPCTypes.h"
#include "content_analysis/sdk/analysis_client.h"

#include "base/process_util.h"
#include "GMPUtils.h"  // ToHexString
#include "mozilla/Components.h"
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

namespace mozilla::contentanalysis {

LazyLogModule gContentAnalysisLog("contentanalysis");
#define LOGD(...)                                        \
  MOZ_LOG(mozilla::contentanalysis::gContentAnalysisLog, \
          mozilla::LogLevel::Debug, (__VA_ARGS__))

#define LOGE(...)                                        \
  MOZ_LOG(mozilla::contentanalysis::gContentAnalysisLog, \
          mozilla::LogLevel::Error, (__VA_ARGS__))

}  // namespace mozilla::contentanalysis

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

nsIContentAnalysisAcknowledgement::FinalAction ConvertResult(
    nsIContentAnalysisResponse::Action aResponseResult) {
  switch (aResponseResult) {
    case nsIContentAnalysisResponse::Action::eReportOnly:
      return nsIContentAnalysisAcknowledgement::FinalAction::eReportOnly;
    case nsIContentAnalysisResponse::Action::eWarn:
      return nsIContentAnalysisAcknowledgement::FinalAction::eWarn;
    case nsIContentAnalysisResponse::Action::eBlock:
      return nsIContentAnalysisAcknowledgement::FinalAction::eBlock;
    case nsIContentAnalysisResponse::Action::eAllow:
      return nsIContentAnalysisAcknowledgement::FinalAction::eAllow;
    case nsIContentAnalysisResponse::Action::eUnspecified:
      return nsIContentAnalysisAcknowledgement::FinalAction::eUnspecified;
    default:
      LOGE(
          "ConvertResult got unexpected responseResult "
          "%d",
          static_cast<uint32_t>(aResponseResult));
      return nsIContentAnalysisAcknowledgement::FinalAction::eUnspecified;
  }
}

}  // anonymous namespace

namespace mozilla::contentanalysis {

NS_IMETHODIMP
ContentAnalysisRequest::GetAnalysisType(AnalysisType* aAnalysisType) {
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
ContentAnalysisRequest::GetOperationTypeForDisplay(
    OperationType* aOperationType) {
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

nsresult ContentAnalysis::CreateContentAnalysisClient(nsCString&& aPipePathName,
                                                      bool aIsPerUser) {
  MOZ_ASSERT(!NS_IsMainThread());
  // This method should only be called once
  MOZ_ASSERT(!mCaClientPromise->IsResolved());

  std::shared_ptr<content_analysis::sdk::Client> client(
      content_analysis::sdk::Client::Create({aPipePathName.Data(), aIsPerUser})
          .release());
  LOGD("Content analysis is %s", client ? "connected" : "not available");
  mCaClientPromise->Resolve(client, __func__);

  return NS_OK;
}

ContentAnalysisRequest::ContentAnalysisRequest(
    AnalysisType aAnalysisType, nsString aString, bool aStringIsFilePath,
    nsCString aSha256Digest, nsString aUrl, OperationType aOperationType,
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
  mOperationTypeForDisplay = aOperationType;
  if (mOperationTypeForDisplay == OperationType::eCustomDisplayString) {
    MOZ_ASSERT(aStringIsFilePath);
    nsresult rv = GetFileDisplayName(mFilePath, mOperationDisplayString);
    if (NS_FAILED(rv)) {
      mOperationDisplayString = u"file";
    }
  }

  mRequestToken = GenerateRequestToken();
}

nsresult ContentAnalysisRequest::GetFileDigest(const nsAString& aFilePath,
                                               nsCString& aDigestString) {
  MOZ_DIAGNOSTIC_ASSERT(
      !NS_IsMainThread(),
      "ContentAnalysisRequest::GetFileDigest does file IO and should "
      "not run on the main thread");
  nsresult rv;
  mozilla::Digest digest;
  digest.Begin(SEC_OID_SHA256);
  PRFileDesc* fd = nullptr;
  nsCOMPtr<nsIFile> file = do_CreateInstance("@mozilla.org/file/local;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = file->InitWithPath(aFilePath);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = file->OpenNSPRFileDesc(PR_RDONLY | nsIFile::OS_READAHEAD, 0, &fd);
  NS_ENSURE_SUCCESS(rv, rv);
  auto closeFile = MakeScopeExit([fd]() { PR_Close(fd); });
  constexpr uint32_t kBufferSize = 1024 * 1024;
  auto buffer = mozilla::MakeUnique<uint8_t[]>(kBufferSize);
  if (!buffer) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  PRInt32 bytesRead = PR_Read(fd, buffer.get(), kBufferSize);
  while (bytesRead != 0) {
    if (bytesRead == -1) {
      return NS_ErrorAccordingToNSPR();
    }
    digest.Update(mozilla::Span<const uint8_t>(buffer.get(), bytesRead));
    bytesRead = PR_Read(fd, buffer.get(), kBufferSize);
  }
  nsTArray<uint8_t> digestResults;
  rv = digest.End(digestResults);
  NS_ENSURE_SUCCESS(rv, rv);
  aDigestString = mozilla::ToHexString(digestResults);
  return NS_OK;
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

  nsIContentAnalysisRequest::AnalysisType analysisType;
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
    content_analysis::sdk::ContentAnalysisResponse&& aResponse)
    : mHasAcknowledged(false) {
  mAction = Action::eUnspecified;
  for (const auto& result : aResponse.results()) {
    if (!result.has_status() ||
        result.status() !=
            content_analysis::sdk::ContentAnalysisResponse::Result::SUCCESS) {
      mAction = Action::eUnspecified;
      return;
    }
    // The action values increase with severity, so the max is the most severe.
    for (const auto& rule : result.triggered_rules()) {
      mAction =
          static_cast<Action>(std::max(static_cast<uint32_t>(mAction),
                                       static_cast<uint32_t>(rule.action())));
    }
  }

  // If no rules blocked then we should allow.
  if (mAction == Action::eUnspecified) {
    mAction = Action::eAllow;
  }

  const auto& requestToken = aResponse.request_token();
  mRequestToken.Assign(requestToken.data(), requestToken.size());
}

ContentAnalysisResponse::ContentAnalysisResponse(
    Action aAction, const nsACString& aRequestToken)
    : mAction(aAction), mRequestToken(aRequestToken), mHasAcknowledged(false) {}

/* static */
already_AddRefed<ContentAnalysisResponse> ContentAnalysisResponse::FromProtobuf(
    content_analysis::sdk::ContentAnalysisResponse&& aResponse) {
  auto ret = RefPtr<ContentAnalysisResponse>(
      new ContentAnalysisResponse(std::move(aResponse)));

  if (ret->mAction == Action::eUnspecified) {
    return nullptr;
  }

  return ret.forget();
}

/* static */
RefPtr<ContentAnalysisResponse> ContentAnalysisResponse::FromAction(
    Action aAction, const nsACString& aRequestToken) {
  if (aAction == Action::eUnspecified) {
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

  nsIContentAnalysisAcknowledgement::Result result;
  nsresult rv = aIn->GetResult(&result);
  NS_ENSURE_SUCCESS(rv, rv);
  aOut->set_status(
      static_cast<content_analysis::sdk::ContentAnalysisAcknowledgement_Status>(
          result));

  nsIContentAnalysisAcknowledgement::FinalAction finalAction;
  rv = aIn->GetFinalAction(&finalAction);
  NS_ENSURE_SUCCESS(rv, rv);
  aOut->set_final_action(
      static_cast<
          content_analysis::sdk::ContentAnalysisAcknowledgement_FinalAction>(
          finalAction));

  return NS_OK;
}

NS_IMETHODIMP
ContentAnalysisResponse::GetAction(Action* aAction) {
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
  mOwner = std::move(aOwner);
}

void ContentAnalysisResponse::ResolveWarnAction(bool aAllowContent) {
  MOZ_ASSERT(mAction == Action::eWarn);
  mAction = aAllowContent ? Action::eAllow : Action::eBlock;
}

ContentAnalysisAcknowledgement::ContentAnalysisAcknowledgement(
    Result aResult, FinalAction aFinalAction)
    : mResult(aResult), mFinalAction(aFinalAction) {}

NS_IMETHODIMP
ContentAnalysisAcknowledgement::GetResult(Result* aResult) {
  *aResult = mResult;
  return NS_OK;
}

NS_IMETHODIMP
ContentAnalysisAcknowledgement::GetFinalAction(FinalAction* aFinalAction) {
  *aFinalAction = mFinalAction;
  return NS_OK;
}

namespace {
static bool ShouldAllowAction(
    nsIContentAnalysisResponse::Action aResponseCode) {
  return aResponseCode == nsIContentAnalysisResponse::Action::eAllow ||
         aResponseCode == nsIContentAnalysisResponse::Action::eReportOnly ||
         aResponseCode == nsIContentAnalysisResponse::Action::eWarn;
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
    *aShouldAllowContent =
        ShouldAllowAction(mValue.as<nsIContentAnalysisResponse::Action>());
  }
  return NS_OK;
}

NS_IMPL_CLASSINFO(ContentAnalysisRequest, nullptr, 0, {0});
NS_IMPL_ISUPPORTS_CI(ContentAnalysisRequest, nsIContentAnalysisRequest);
NS_IMPL_CLASSINFO(ContentAnalysisResponse, nullptr, 0, {0});
NS_IMPL_ISUPPORTS_CI(ContentAnalysisResponse, nsIContentAnalysisResponse);
NS_IMPL_ISUPPORTS(ContentAnalysisAcknowledgement,
                  nsIContentAnalysisAcknowledgement);
NS_IMPL_ISUPPORTS(ContentAnalysisCallback, nsIContentAnalysisCallback);
NS_IMPL_ISUPPORTS(ContentAnalysisResult, nsIContentAnalysisResult);
NS_IMPL_ISUPPORTS(ContentAnalysis, nsIContentAnalysis, ContentAnalysis);

ContentAnalysis::ContentAnalysis()
    : mCaClientPromise(
          new ClientPromise::Private("ContentAnalysis::ContentAnalysis")),
      mClientCreationAttempted(false),
      mCallbackMap("ContentAnalysis::mCallbackMap"),
      mWarnResponseDataMap("ContentAnalysis::mWarnResponseDataMap") {}

ContentAnalysis::~ContentAnalysis() {
  // Accessing mClientCreationAttempted so need to be on the main thread
  MOZ_ASSERT(NS_IsMainThread());
  if (!mClientCreationAttempted) {
    // Reject the promise to avoid assertions when it gets destroyed
    mCaClientPromise->Reject(NS_ERROR_ILLEGAL_DURING_SHUTDOWN, __func__);
  }
}

NS_IMETHODIMP
ContentAnalysis::GetIsActive(bool* aIsActive) {
  *aIsActive = false;
  // Need to be on the main thread to read prefs
  MOZ_ASSERT(NS_IsMainThread());
  // gAllowContentAnalysis is only set in the parent process
  MOZ_ASSERT(XRE_IsParentProcess());
  if (!gAllowContentAnalysis || !Preferences::GetBool(kIsDLPEnabledPref)) {
    LOGD("Local DLP Content Analysis is not active");
    return NS_OK;
  }
  *aIsActive = true;
  LOGD("Local DLP Content Analysis is active");
  // mClientCreationAttempted is only accessed on the main thread,
  // so no need for synchronization here.
  if (!mClientCreationAttempted) {
    mClientCreationAttempted = true;
    LOGD("Dispatching background task to create Content Analysis client");

    nsCString pipePathName;
    nsresult rv = Preferences::GetCString(kPipePathNamePref, pipePathName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mCaClientPromise->Reject(rv, __func__);
      return rv;
    }
    bool isPerUser = Preferences::GetBool(kIsPerUserPref);
    rv = NS_DispatchBackgroundTask(NS_NewCancelableRunnableFunction(
        "ContentAnalysis::CreateContentAnalysisClient",
        [owner = RefPtr{this}, pipePathName = std::move(pipePathName),
         isPerUser]() mutable {
          owner->CreateContentAnalysisClient(std::move(pipePathName),
                                             isPerUser);
        }));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mCaClientPromise->Reject(rv, __func__);
      return rv;
    }
  }
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

nsresult ContentAnalysis::CancelWithError(nsCString aRequestToken,
                                          nsresult aResult) {
  return NS_DispatchToMainThread(NS_NewCancelableRunnableFunction(
      "ContentAnalysis::RunAnalyzeRequestTask::HandleResponse",
      [aResult, aRequestToken = std::move(aRequestToken)] {
        RefPtr<ContentAnalysis> owner = GetContentAnalysisFromService();
        if (!owner) {
          // May be shutting down
          return;
        }
        nsCOMPtr<nsIObserverService> obsServ =
            mozilla::services::GetObserverService();
        RefPtr<ContentAnalysisResponse> response =
            ContentAnalysisResponse::FromAction(
                nsIContentAnalysisResponse::Action::eCanceled, aRequestToken);
        obsServ->NotifyObservers(response, "dlp-response", nullptr);
        nsMainThreadPtrHandle<nsIContentAnalysisCallback> callbackHolder;
        {
          auto lock = owner->mCallbackMap.Lock();
          auto callbackData = lock->Extract(aRequestToken);
          if (callbackData.isSome()) {
            callbackHolder = callbackData->TakeCallbackHolder();
          }
        }
        if (callbackHolder) {
          callbackHolder->Error(aResult);
        }
      }));
}

RefPtr<ContentAnalysis> ContentAnalysis::GetContentAnalysisFromService() {
  RefPtr<ContentAnalysis> contentAnalysisService =
      mozilla::components::nsIContentAnalysis::Service();
  if (!contentAnalysisService) {
    // May be shutting down
    return nullptr;
  }

  return contentAnalysisService;
}

nsresult ContentAnalysis::RunAnalyzeRequestTask(
    const RefPtr<nsIContentAnalysisRequest>& aRequest, bool aAutoAcknowledge,
    const RefPtr<nsIContentAnalysisCallback>& aCallback) {
  nsresult rv = NS_ERROR_FAILURE;
  auto callbackCopy = aCallback;
  auto se = MakeScopeExit([&] {
    if (!NS_SUCCEEDED(rv)) {
      LOGE("RunAnalyzeRequestTask failed");
      callbackCopy->Error(rv);
    }
  });

  content_analysis::sdk::ContentAnalysisRequest pbRequest;
  rv = ConvertToProtobuf(aRequest, &pbRequest);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString requestToken;
  nsMainThreadPtrHandle<nsIContentAnalysisCallback> callbackHolderCopy(
      new nsMainThreadPtrHolder<nsIContentAnalysisCallback>(
          "content analysis callback", aCallback));
  CallbackData callbackData(std::move(callbackHolderCopy), aAutoAcknowledge);
  rv = aRequest->GetRequestToken(requestToken);
  NS_ENSURE_SUCCESS(rv, rv);
  {
    auto lock = mCallbackMap.Lock();
    lock->InsertOrUpdate(requestToken, std::move(callbackData));
  }

  LOGD("Issuing ContentAnalysisRequest for token %s", requestToken.get());
  LogRequest(&pbRequest);

  mCaClientPromise->Then(
      GetCurrentSerialEventTarget(), __func__,
      [requestToken, pbRequest = std::move(pbRequest)](
          std::shared_ptr<content_analysis::sdk::Client> client) mutable {
        // The content analysis call is synchronous so run in the background.
        NS_DispatchBackgroundTask(
            NS_NewCancelableRunnableFunction(
                __func__,
                [requestToken, pbRequest = std::move(pbRequest),
                 client = std::move(client)]() mutable {
                  DoAnalyzeRequest(requestToken, std::move(pbRequest), client);
                }),
            NS_DISPATCH_EVENT_MAY_BLOCK);
      },
      [requestToken](nsresult rv) mutable {
        LOGD("RunAnalyzeRequestTask failed to get client");
        RefPtr<ContentAnalysis> owner = GetContentAnalysisFromService();
        if (!owner) {
          // May be shutting down
          return;
        }
        owner->CancelWithError(std::move(requestToken), rv);
      });

  return rv;
}

void ContentAnalysis::DoAnalyzeRequest(
    nsCString aRequestToken,
    content_analysis::sdk::ContentAnalysisRequest&& aRequest,
    const std::shared_ptr<content_analysis::sdk::Client>& aClient) {
  MOZ_ASSERT(!NS_IsMainThread());
  RefPtr<ContentAnalysis> owner =
      ContentAnalysis::GetContentAnalysisFromService();
  if (!owner) {
    // May be shutting down
    return;
  }

  if (!aClient) {
    owner->CancelWithError(std::move(aRequestToken), NS_ERROR_NOT_AVAILABLE);
    return;
  }

  if (aRequest.has_file_path() && !aRequest.file_path().empty() &&
      (!aRequest.request_data().has_digest() ||
       aRequest.request_data().digest().empty())) {
    // Calculate the digest
    nsCString digest;
    nsCString fileCPath(aRequest.file_path().data(),
                        aRequest.file_path().length());
    nsString filePath = NS_ConvertUTF8toUTF16(fileCPath);
    nsresult rv = ContentAnalysisRequest::GetFileDigest(filePath, digest);
    if (NS_FAILED(rv)) {
      owner->CancelWithError(std::move(aRequestToken), rv);
      return;
    }
    if (!digest.IsEmpty()) {
      aRequest.mutable_request_data()->set_digest(digest.get());
    }
  }

  {
    auto callbackMap = owner->mCallbackMap.Lock();
    if (!callbackMap->Contains(aRequestToken)) {
      LOGD(
          "RunAnalyzeRequestTask token %s has already been "
          "cancelled - not issuing request",
          aRequestToken.get());
      return;
    }
  }

  // Run request, then dispatch back to main thread to resolve
  // aCallback
  content_analysis::sdk::ContentAnalysisResponse pbResponse;
  int err = aClient->Send(aRequest, &pbResponse);
  if (err != 0) {
    LOGE("RunAnalyzeRequestTask client transaction failed");
    owner->CancelWithError(std::move(aRequestToken), NS_ERROR_FAILURE);
    return;
  }
  LOGD("Content analysis client transaction succeeded");
  LogResponse(&pbResponse);
  NS_DispatchToMainThread(NS_NewCancelableRunnableFunction(
      "ContentAnalysis::RunAnalyzeRequestTask::HandleResponse",
      [pbResponse = std::move(pbResponse)]() mutable {
        RefPtr<ContentAnalysis> owner = GetContentAnalysisFromService();
        if (!owner) {
          // May be shutting down
          return;
        }

        RefPtr<ContentAnalysisResponse> response =
            ContentAnalysisResponse::FromProtobuf(std::move(pbResponse));
        if (!response) {
          LOGE("Content analysis got invalid response!");
          return;
        }
        nsCString responseRequestToken;
        nsresult requestRv = response->GetRequestToken(responseRequestToken);
        if (NS_FAILED(requestRv)) {
          LOGE(
              "Content analysis couldn't get request token "
              "from response!");
          return;
        }

        Maybe<CallbackData> maybeCallbackData;
        {
          auto callbackMap = owner->mCallbackMap.Lock();
          maybeCallbackData = callbackMap->Extract(responseRequestToken);
        }
        if (maybeCallbackData.isNothing()) {
          LOGD(
              "Content analysis did not find callback for "
              "token %s",
              responseRequestToken.get());
          return;
        }
        response->SetOwner(owner);
        if (maybeCallbackData->Canceled()) {
          // request has already been cancelled, so there's
          // nothing to do
          LOGD(
              "Content analysis got response but ignoring "
              "because it was already cancelled for token %s",
              responseRequestToken.get());
          // Note that we always acknowledge here, even if
          // autoAcknowledge isn't set, since we raise an exception
          // at the caller on cancellation.
          auto acknowledgement = MakeRefPtr<ContentAnalysisAcknowledgement>(
              nsIContentAnalysisAcknowledgement::Result::eTooLate,
              nsIContentAnalysisAcknowledgement::FinalAction::eBlock);
          response->Acknowledge(acknowledgement);
          return;
        }

        LOGD(
            "Content analysis resolving response promise for "
            "token %s",
            responseRequestToken.get());
        nsIContentAnalysisResponse::Action action = response->GetAction();
        nsCOMPtr<nsIObserverService> obsServ =
            mozilla::services::GetObserverService();
        if (action == nsIContentAnalysisResponse::Action::eWarn) {
          {
            auto warnResponseDataMap = owner->mWarnResponseDataMap.Lock();
            warnResponseDataMap->InsertOrUpdate(
                responseRequestToken,
                WarnResponseData(std::move(*maybeCallbackData), response));
          }
          obsServ->NotifyObservers(response, "dlp-response", nullptr);
          return;
        }

        obsServ->NotifyObservers(response, "dlp-response", nullptr);
        if (maybeCallbackData->AutoAcknowledge()) {
          auto acknowledgement = MakeRefPtr<ContentAnalysisAcknowledgement>(
              nsIContentAnalysisAcknowledgement::Result::eSuccess,
              ConvertResult(action));
          response->Acknowledge(acknowledgement);
        }

        nsMainThreadPtrHandle<nsIContentAnalysisCallback> callbackHolder =
            maybeCallbackData->TakeCallbackHolder();
        callbackHolder->ContentResult(response);
      }));
}

NS_IMETHODIMP
ContentAnalysis::AnalyzeContentRequest(nsIContentAnalysisRequest* aRequest,
                                       bool aAutoAcknowledge, JSContext* aCx,
                                       mozilla::dom::Promise** aPromise) {
  RefPtr<mozilla::dom::Promise> promise;
  nsresult rv = MakePromise(aCx, &promise);
  NS_ENSURE_SUCCESS(rv, rv);
  RefPtr<ContentAnalysisCallback> callbackPtr =
      new ContentAnalysisCallback(promise);
  promise.forget(aPromise);
  return AnalyzeContentRequestCallback(aRequest, aAutoAcknowledge,
                                       callbackPtr.get());
}

NS_IMETHODIMP
ContentAnalysis::AnalyzeContentRequestCallback(
    nsIContentAnalysisRequest* aRequest, bool aAutoAcknowledge,
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

  rv = RunAnalyzeRequestTask(aRequest, aAutoAcknowledge, aCallback);
  return rv;
}

NS_IMETHODIMP
ContentAnalysis::CancelContentAnalysisRequest(const nsACString& aRequestToken) {
  MOZ_ASSERT(NS_IsMainThread());
  nsCString requestToken(aRequestToken);

  auto callbackMap = mCallbackMap.Lock();
  auto entry = callbackMap->Lookup(requestToken);
  LOGD("Content analysis cancelling request %s", requestToken.get());
  // Make sure the entry hasn't been cancelled already
  if (entry && !entry->Canceled()) {
    nsMainThreadPtrHandle<nsIContentAnalysisCallback> callbackHolder =
        entry->TakeCallbackHolder();
    entry->SetCanceled();
    // Should only be called once
    MOZ_ASSERT(callbackHolder);
    if (callbackHolder) {
      callbackHolder->Error(NS_ERROR_ABORT);
    }
  } else {
    LOGD("Content analysis request not found when trying to cancel %s",
         requestToken.get());
  }
  return NS_OK;
}

NS_IMETHODIMP
ContentAnalysis::RespondToWarnDialog(const nsACString& aRequestToken,
                                     bool aAllowContent) {
  nsCString requestToken(aRequestToken);
  NS_DispatchToMainThread(NS_NewCancelableRunnableFunction(
      "RespondToWarnDialog",
      [aAllowContent, requestToken = std::move(requestToken)]() {
        RefPtr<ContentAnalysis> self = GetContentAnalysisFromService();
        if (!self) {
          // May be shutting down
          return;
        }

        LOGD("Content analysis getting warn response %d for request %s",
             aAllowContent ? 1 : 0, requestToken.get());
        Maybe<WarnResponseData> entry;
        {
          auto warnResponseDataMap = self->mWarnResponseDataMap.Lock();
          entry = warnResponseDataMap->Extract(requestToken);
        }
        if (!entry) {
          LOGD(
              "Content analysis request not found when trying to send warn "
              "response for request %s",
              requestToken.get());
          return;
        }
        entry->mResponse->ResolveWarnAction(aAllowContent);
        auto action = entry->mResponse->GetAction();
        if (entry->mCallbackData.AutoAcknowledge()) {
          RefPtr<ContentAnalysisAcknowledgement> acknowledgement =
              new ContentAnalysisAcknowledgement(
                  nsIContentAnalysisAcknowledgement::Result::eSuccess,
                  ConvertResult(action));
          entry->mResponse->Acknowledge(acknowledgement);
        }
        nsMainThreadPtrHandle<nsIContentAnalysisCallback> callbackHolder =
            entry->mCallbackData.TakeCallbackHolder();
        if (callbackHolder) {
          RefPtr<ContentAnalysisResponse> response =
              ContentAnalysisResponse::FromAction(action, requestToken);
          response->SetOwner(self);
          callbackHolder.get()->ContentResult(response.get());
        } else {
          LOGD(
              "Content analysis had no callback to send warn final response "
              "to for request %s",
              requestToken.get());
        }
      }));
  return NS_OK;
}

NS_IMETHODIMP
ContentAnalysisResponse::Acknowledge(
    nsIContentAnalysisAcknowledgement* aAcknowledgement) {
  MOZ_ASSERT(mOwner);
  if (mHasAcknowledged) {
    MOZ_ASSERT(false, "Already acknowledged this ContentAnalysisResponse!");
    return NS_ERROR_FAILURE;
  }
  mHasAcknowledged = true;
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

  // The content analysis connection is synchronous so run in the background.
  LOGD("RunAcknowledgeTask dispatching acknowledge task");
  mCaClientPromise->Then(
      GetCurrentSerialEventTarget(), __func__,
      [pbAck = std::move(pbAck)](
          std::shared_ptr<content_analysis::sdk::Client> client) mutable {
        NS_DispatchBackgroundTask(
            NS_NewCancelableRunnableFunction(
                __func__,
                [pbAck = std::move(pbAck),
                 client = std::move(client)]() mutable {
                  RefPtr<ContentAnalysis> owner =
                      GetContentAnalysisFromService();
                  if (!owner) {
                    // May be shutting down
                    return;
                  }
                  if (!client) {
                    return;
                  }

                  int err = client->Acknowledge(pbAck);
                  MOZ_ASSERT(err == 0);
                  LOGD(
                      "RunAcknowledgeTask sent transaction acknowledgement, "
                      "err=%d",
                      err);
                }),
            NS_DISPATCH_EVENT_MAY_BLOCK);
      },
      [](nsresult rv) { LOGD("RunAcknowledgeTask failed to get the client"); });
  return rv;
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

#undef LOGD
#undef LOGE
}  // namespace mozilla::contentanalysis
