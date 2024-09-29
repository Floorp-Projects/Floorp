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
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/Logging.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPrefs_browser.h"
#include "nsAppRunner.h"
#include "nsBaseClipboard.h"
#include "nsComponentManagerUtils.h"
#include "nsIClassInfoImpl.h"
#include "nsIFile.h"
#include "nsIGlobalObject.h"
#include "nsIObserverService.h"
#include "nsIOutputStream.h"
#include "nsIPrintSettings.h"
#include "nsIStorageStream.h"
#include "nsISupportsPrimitives.h"
#include "nsITransferable.h"
#include "ScopedNSSTypes.h"
#include "xpcpublic.h"

#include <algorithm>
#include <sstream>
#include <string>

#ifdef XP_WIN
#  include <windows.h>
#  define SECURITY_WIN32 1
#  include <security.h>
#  include "mozilla/NativeNt.h"
#  include "mozilla/WinDllServices.h"
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

const char* kPipePathNamePref = "browser.contentanalysis.pipe_path_name";
const char* kClientSignature = "browser.contentanalysis.client_signature";
const char* kAllowUrlPref = "browser.contentanalysis.allow_url_regex_list";
const char* kDenyUrlPref = "browser.contentanalysis.deny_url_regex_list";

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

/* static */ bool nsIContentAnalysis::MightBeActive() {
  // A DLP connection is not permitted to be added/removed while the
  // browser is running, so we can cache this.
  // Furthermore, if this is set via enterprise policy the pref will be locked
  // so users won't be able to change it.
  // Ideally we would make this a mirror: once pref, but this interacts in
  // some weird ways with the enterprise policy for testing purposes.
  static bool sIsEnabled =
      mozilla::StaticPrefs::browser_contentanalysis_enabled();
  // Note that we can't check gAllowContentAnalysis here because it
  // only gets set in the parent process.
  return sIsEnabled;
}

namespace mozilla::contentanalysis {
ContentAnalysisRequest::~ContentAnalysisRequest() {
#ifdef XP_WIN
  CloseHandle(mPrintDataHandle);
#endif
}

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
ContentAnalysisRequest::GetPrintDataHandle(uint64_t* aPrintDataHandle) {
#ifdef XP_WIN
  uintptr_t printDataHandle = reinterpret_cast<uintptr_t>(mPrintDataHandle);
  uint64_t printDataValue = static_cast<uint64_t>(printDataHandle);
  *aPrintDataHandle = printDataValue;
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
ContentAnalysisRequest::GetPrinterName(nsAString& aPrinterName) {
  aPrinterName = mPrinterName;
  return NS_OK;
}

NS_IMETHODIMP
ContentAnalysisRequest::GetPrintDataSize(uint64_t* aPrintDataSize) {
#ifdef XP_WIN
  *aPrintDataSize = mPrintDataSize;
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
ContentAnalysisRequest::GetUrl(nsIURI** aUrl) {
  NS_ENSURE_ARG_POINTER(aUrl);
  NS_IF_ADDREF(*aUrl = mUrl);
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

nsresult ContentAnalysis::CreateContentAnalysisClient(
    nsCString&& aPipePathName, nsString&& aClientSignatureSetting,
    bool aIsPerUser) {
  MOZ_ASSERT(!NS_IsMainThread());
  // This method should only be called once
  MOZ_ASSERT(!mCaClientPromise->IsResolved());

  std::shared_ptr<content_analysis::sdk::Client> client(
      content_analysis::sdk::Client::Create({aPipePathName.Data(), aIsPerUser})
          .release());
  LOGD("Content analysis is %s", client ? "connected" : "not available");
#ifdef XP_WIN
  if (client && !aClientSignatureSetting.IsEmpty()) {
    std::string agentPath = client->GetAgentInfo().binary_path;
    nsString agentWidePath = NS_ConvertUTF8toUTF16(agentPath);
    UniquePtr<wchar_t[]> orgName =
        mozilla::DllServices::Get()->GetBinaryOrgName(agentWidePath.Data());
    bool signatureMatches = false;
    if (orgName) {
      auto dependentOrgName = nsDependentString(orgName.get());
      LOGD("Content analysis client signed with organization name \"%S\"",
           static_cast<const wchar_t*>(dependentOrgName.get()));
      signatureMatches = aClientSignatureSetting.Equals(dependentOrgName);
    } else {
      LOGD("Content analysis client has no signature");
    }
    if (!signatureMatches) {
      LOGE(
          "Got mismatched content analysis client signature! All content "
          "analysis operations will fail.");
      mCaClientPromise->Reject(NS_ERROR_INVALID_SIGNATURE, __func__);
      return NS_OK;
    }
  }
#endif  // XP_WIN
  mCaClientPromise->Resolve(client, __func__);

  return NS_OK;
}

ContentAnalysisRequest::ContentAnalysisRequest(
    AnalysisType aAnalysisType, nsString aString, bool aStringIsFilePath,
    nsCString aSha256Digest, nsCOMPtr<nsIURI> aUrl,
    OperationType aOperationType, dom::WindowGlobalParent* aWindowGlobalParent)
    : mAnalysisType(aAnalysisType),
      mUrl(std::move(aUrl)),
      mSha256Digest(std::move(aSha256Digest)),
      mWindowGlobalParent(aWindowGlobalParent) {
  MOZ_ASSERT(aAnalysisType != AnalysisType::ePrint,
             "Print should use other ContentAnalysisRequest constructor!");
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

ContentAnalysisRequest::ContentAnalysisRequest(
    const nsTArray<uint8_t> aPrintData, nsCOMPtr<nsIURI> aUrl,
    nsString aPrinterName, dom::WindowGlobalParent* aWindowGlobalParent)
    : mAnalysisType(AnalysisType::ePrint),
      mUrl(std::move(aUrl)),
      mPrinterName(std::move(aPrinterName)),
      mWindowGlobalParent(aWindowGlobalParent) {
#ifdef XP_WIN
  LARGE_INTEGER dataContentLength;
  dataContentLength.QuadPart = static_cast<LONGLONG>(aPrintData.Length());
  mPrintDataHandle = ::CreateFileMappingW(
      INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, dataContentLength.HighPart,
      dataContentLength.LowPart, nullptr);
  if (mPrintDataHandle) {
    mozilla::nt::AutoMappedView view(mPrintDataHandle, FILE_MAP_ALL_ACCESS);
    memcpy(view.as<uint8_t>(), aPrintData.Elements(), aPrintData.Length());
    mPrintDataSize = aPrintData.Length();
  }
#else
  MOZ_ASSERT_UNREACHABLE(
      "Content Analysis is not supported on non-Windows platforms");
#endif
  mOperationTypeForDisplay = OperationType::eOperationPrint;
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

// Generate an ID that will be shared by all DLP requests.
// Used to cancel all requests on Firefox shutdown.
void ContentAnalysis::GenerateUserActionId() {
  nsID id = nsID::GenerateUUID();
  mUserActionId = nsPrintfCString("Firefox %s", id.ToString().get());
}

nsCString ContentAnalysis::GetUserActionId() { return mUserActionId; }

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
    nsIContentAnalysisRequest* aIn, nsCString&& aUserActionId,
    int64_t aRequestCount,
    content_analysis::sdk::ContentAnalysisRequest* aOut) {
  uint32_t timeout = StaticPrefs::browser_contentanalysis_agent_timeout();
  aOut->set_expires_at(time(nullptr) + timeout);

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

  aOut->set_user_action_id(aUserActionId.get());
  aOut->set_user_action_requests_count(aRequestCount);

  const std::string tag = "dlp";  // TODO:
  *aOut->add_tags() = tag;

  auto* requestData = aOut->mutable_request_data();

  nsCOMPtr<nsIURI> url;
  rv = aIn->GetUrl(getter_AddRefs(url));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCString urlString;
  rv = url->GetSpec(urlString);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!urlString.IsEmpty()) {
    requestData->set_url(urlString.get());
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

  if (analysisType == nsIContentAnalysisRequest::AnalysisType::ePrint) {
#if XP_WIN
    uint64_t printDataHandle;
    MOZ_TRY(aIn->GetPrintDataHandle(&printDataHandle));
    if (!printDataHandle) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    aOut->mutable_print_data()->set_handle(printDataHandle);

    uint64_t printDataSize;
    MOZ_TRY(aIn->GetPrintDataSize(&printDataSize));
    aOut->mutable_print_data()->set_size(printDataSize);

    nsString printerName;
    MOZ_TRY(aIn->GetPrinterName(printerName));
    requestData->mutable_print_metadata()->set_printer_name(
        NS_ConvertUTF16toUTF8(printerName).get());
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
  } else {
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

namespace {
// We don't want this overload to be called for string parameters, so
// use std::enable_if
template <typename T>
typename std::enable_if_t<!std::is_same<std::string, std::decay_t<T>>::value,
                          void>
LogWithMaxLength(std::stringstream& ss, T value, size_t maxLength) {
  ss << value;
}

// 0 indicates no max length
template <typename T>
typename std::enable_if_t<std::is_same<std::string, std::decay_t<T>>::value,
                          void>
LogWithMaxLength(std::stringstream& ss, T value, size_t maxLength) {
  if (!maxLength || value.length() < maxLength) {
    ss << value;
  } else {
    ss << value.substr(0, maxLength) << " (truncated)";
  }
}
}  // namespace

static void LogRequest(
    const content_analysis::sdk::ContentAnalysisRequest* aPbRequest) {
  // We cannot use Protocol Buffer's DebugString() because we optimize for
  // lite runtime.
  if (!static_cast<LogModule*>(gContentAnalysisLog)
           ->ShouldLog(LogLevel::Debug)) {
    return;
  }

  std::stringstream ss;
  ss << "ContentAnalysisRequest:"
     << "\n";

#define ADD_FIELD(PBUF, NAME, FUNC)            \
  ss << "  " << (NAME) << ": ";                \
  if ((PBUF)->has_##FUNC()) {                  \
    LogWithMaxLength(ss, (PBUF)->FUNC(), 500); \
    ss << "\n";                                \
  } else                                       \
    ss << "<none>"                             \
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
    : mAction(aAction), mRequestToken(aRequestToken) {}

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

NS_IMETHODIMP
ContentAnalysisResponse::GetCancelError(CancelError* aCancelError) {
  *aCancelError = mCancelError;
  return NS_OK;
}

NS_IMETHODIMP
ContentAnalysisResponse::GetIsCachedResponse(bool* aIsCachedResponse) {
  *aIsCachedResponse = mIsCachedResponse;
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

void ContentAnalysisResponse::SetCancelError(CancelError aCancelError) {
  mCancelError = aCancelError;
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

static DefaultResult GetDefaultResultFromPref() {
  uint32_t value = StaticPrefs::browser_contentanalysis_default_result();
  if (value > static_cast<uint32_t>(DefaultResult::eLastValue)) {
    LOGE(
        "Invalid value for browser.contentanalysis.default_result pref "
        "value");
    return DefaultResult::eBlock;
  }
  return static_cast<DefaultResult>(value);
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
    if (GetDefaultResultFromPref() == DefaultResult::eAllow) {
      *aShouldAllowContent =
          result != NoContentAnalysisResult::DENY_DUE_TO_CANCELED;
    } else {
      // Note that we allow content if we're unable to get it (for example, if
      // there's clipboard content that is not text or file)
      *aShouldAllowContent =
          result == NoContentAnalysisResult::
                        ALLOW_DUE_TO_CONTENT_ANALYSIS_NOT_ACTIVE ||
          result == NoContentAnalysisResult::
                        ALLOW_DUE_TO_CONTEXT_EXEMPT_FROM_CONTENT_ANALYSIS ||
          result == NoContentAnalysisResult::ALLOW_DUE_TO_SAME_TAB_SOURCE ||
          result == NoContentAnalysisResult::ALLOW_DUE_TO_COULD_NOT_GET_DATA;
    }
  } else {
    *aShouldAllowContent =
        ShouldAllowAction(mValue.as<nsIContentAnalysisResponse::Action>());
  }
  return NS_OK;
}

void ContentAnalysis::EnsureParsedUrlFilters() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mParsedUrlLists) {
    return;
  }

  mParsedUrlLists = true;
  nsAutoCString allowList;
  MOZ_ALWAYS_SUCCEEDS(Preferences::GetCString(kAllowUrlPref, allowList));
  for (const nsACString& regexSubstr : allowList.Split(u' ')) {
    if (!regexSubstr.IsEmpty()) {
      auto flatStr = PromiseFlatCString(regexSubstr);
      const char* regex = flatStr.get();
      LOGD("CA will allow URLs that match %s", regex);
      mAllowUrlList.push_back(std::regex(regex));
    }
  }

  nsAutoCString denyList;
  MOZ_ALWAYS_SUCCEEDS(Preferences::GetCString(kDenyUrlPref, denyList));
  for (const nsACString& regexSubstr : denyList.Split(u' ')) {
    if (!regexSubstr.IsEmpty()) {
      auto flatStr = PromiseFlatCString(regexSubstr);
      const char* regex = flatStr.get();
      LOGD("CA will block URLs that match %s", regex);
      mDenyUrlList.push_back(std::regex(regex));
    }
  }
}

ContentAnalysis::UrlFilterResult ContentAnalysis::FilterByUrlLists(
    nsIContentAnalysisRequest* aRequest) {
  EnsureParsedUrlFilters();

  nsCOMPtr<nsIURI> nsiUrl;
  MOZ_ALWAYS_SUCCEEDS(aRequest->GetUrl(getter_AddRefs(nsiUrl)));
  nsCString urlString;
  nsresult rv = nsiUrl->GetSpec(urlString);
  NS_ENSURE_SUCCESS(rv, UrlFilterResult::eDeny);
  MOZ_ASSERT(!urlString.IsEmpty());
  std::string url = urlString.BeginReading();
  size_t count = 0;
  for (const auto& denyFilter : mDenyUrlList) {
    if (std::regex_match(url, denyFilter)) {
      LOGD("Denying CA request : Deny filter %zu matched url %s", count,
           url.c_str());
      return UrlFilterResult::eDeny;
    }
    ++count;
  }

  count = 0;
  UrlFilterResult result = UrlFilterResult::eCheck;
  for (const auto& allowFilter : mAllowUrlList) {
    if (std::regex_match(url, allowFilter)) {
      LOGD("CA request : Allow filter %zu matched %s", count, url.c_str());
      result = UrlFilterResult::eAllow;
      break;
    }
    ++count;
  }

  // The rest only applies to download resources.
  nsIContentAnalysisRequest::AnalysisType analysisType;
  MOZ_ALWAYS_SUCCEEDS(aRequest->GetAnalysisType(&analysisType));
  if (analysisType != ContentAnalysisRequest::AnalysisType::eFileDownloaded) {
    MOZ_ASSERT(result == UrlFilterResult::eCheck ||
               result == UrlFilterResult::eAllow);
    LOGD("CA request filter result: %s",
         result == UrlFilterResult::eCheck ? "check" : "allow");
    return result;
  }

  nsTArray<RefPtr<nsIClientDownloadResource>> resources;
  MOZ_ALWAYS_SUCCEEDS(aRequest->GetResources(resources));
  for (size_t resourceIdx = 0; resourceIdx < resources.Length();
       /* noop */) {
    auto& resource = resources[resourceIdx];
    nsAutoString nsUrl;
    MOZ_ALWAYS_SUCCEEDS(resource->GetUrl(nsUrl));
    std::string url = NS_ConvertUTF16toUTF8(nsUrl).get();
    count = 0;
    for (auto& denyFilter : mDenyUrlList) {
      if (std::regex_match(url, denyFilter)) {
        LOGD(
            "Denying CA request : Deny filter %zu matched download resource "
            "at url %s",
            count, url.c_str());
        return UrlFilterResult::eDeny;
      }
      ++count;
    }

    count = 0;
    bool removed = false;
    for (auto& allowFilter : mAllowUrlList) {
      if (std::regex_match(url, allowFilter)) {
        LOGD(
            "CA request : Allow filter %zu matched download resource "
            "at url %s",
            count, url.c_str());
        resources.RemoveElementAt(resourceIdx);
        removed = true;
        break;
      }
      ++count;
    }
    if (!removed) {
      ++resourceIdx;
    }
  }

  // Check unless all were allowed.
  return resources.Length() ? UrlFilterResult::eCheck : UrlFilterResult::eAllow;
}

NS_IMPL_CLASSINFO(ContentAnalysisRequest, nullptr, 0, {0});
NS_IMPL_ISUPPORTS_CI(ContentAnalysisRequest, nsIContentAnalysisRequest);
NS_IMPL_CLASSINFO(ContentAnalysisResponse, nullptr, 0, {0});
NS_IMPL_ISUPPORTS_CI(ContentAnalysisResponse, nsIContentAnalysisResponse);
NS_IMPL_ISUPPORTS(ContentAnalysisAcknowledgement,
                  nsIContentAnalysisAcknowledgement);
NS_IMPL_ISUPPORTS(ContentAnalysisCallback, nsIContentAnalysisCallback);
NS_IMPL_ISUPPORTS(ContentAnalysisResult, nsIContentAnalysisResult);
NS_IMPL_ISUPPORTS(ContentAnalysisDiagnosticInfo,
                  nsIContentAnalysisDiagnosticInfo);
NS_IMPL_ISUPPORTS(ContentAnalysis, nsIContentAnalysis, ContentAnalysis);

ContentAnalysis::ContentAnalysis()
    : mCaClientPromise(
          new ClientPromise::Private("ContentAnalysis::ContentAnalysis")),
      mClientCreationAttempted(false),
      mSetByEnterprise(false),
      mCallbackMap("ContentAnalysis::mCallbackMap"),
      mWarnResponseDataMap("ContentAnalysis::mWarnResponseDataMap") {
  GenerateUserActionId();
}

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
  if (!StaticPrefs::browser_contentanalysis_enabled()) {
    LOGD("Local DLP Content Analysis is not active");
    return NS_OK;
  }
  // Accessing mClientCreationAttempted, mSetByEnterprise and non-static prefs
  // so need to be on the main thread
  MOZ_ASSERT(NS_IsMainThread());
  // gAllowContentAnalysisArgPresent is only set in the parent process
  MOZ_ASSERT(XRE_IsParentProcess());
  if (!gAllowContentAnalysisArgPresent && !mSetByEnterprise) {
    LOGE(
        "The content analysis pref is enabled but not by an enterprise "
        "policy and -allow-content-analysis was not present on the "
        "command-line.  Content Analysis will not be active.");
    return NS_OK;
  }

  *aIsActive = true;
  LOGD("Local DLP Content Analysis is active");
  // On the main thread so no need for synchronization here.
  if (!mClientCreationAttempted) {
    mClientCreationAttempted = true;
    LOGD("Dispatching background task to create Content Analysis client");

    nsCString pipePathName;
    nsresult rv = Preferences::GetCString(kPipePathNamePref, pipePathName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mCaClientPromise->Reject(rv, __func__);
      return rv;
    }
    bool isPerUser = StaticPrefs::browser_contentanalysis_is_per_user();
    nsString clientSignature;
    // It's OK if this fails, we will default to the empty string
    Preferences::GetString(kClientSignature, clientSignature);
    rv = NS_DispatchBackgroundTask(NS_NewCancelableRunnableFunction(
        "ContentAnalysis::CreateContentAnalysisClient",
        [owner = RefPtr{this}, pipePathName = std::move(pipePathName),
         clientSignature = std::move(clientSignature), isPerUser]() mutable {
          owner->CreateContentAnalysisClient(
              std::move(pipePathName), std::move(clientSignature), isPerUser);
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
  *aMightBeActive = nsIContentAnalysis::MightBeActive();
  return NS_OK;
}

NS_IMETHODIMP
ContentAnalysis::GetIsSetByEnterprisePolicy(bool* aSetByEnterprise) {
  *aSetByEnterprise = mSetByEnterprise;
  return NS_OK;
}

NS_IMETHODIMP
ContentAnalysis::SetIsSetByEnterprisePolicy(bool aSetByEnterprise) {
  mSetByEnterprise = aSetByEnterprise;
  return NS_OK;
}

NS_IMETHODIMP
ContentAnalysis::TestOnlySetCACmdLineArg(bool aVal) {
#ifdef ENABLE_TESTS
  gAllowContentAnalysisArgPresent = aVal;
  return NS_OK;
#else
  LOGE("ContentAnalysis::TestOnlySetCACmdLineArg is test-only");
  return NS_ERROR_UNEXPECTED;
#endif
}

nsresult ContentAnalysis::CancelWithError(nsCString aRequestToken,
                                          nsresult aResult) {
  LOGD(
      "ContentAnalysis::CancelWithError dispatching to main thread for "
      "request %s",
      aRequestToken.get());
  return NS_DispatchToMainThread(NS_NewCancelableRunnableFunction(
      "ContentAnalysis::CancelWithError",
      [aResult, aRequestToken = std::move(aRequestToken)]() mutable {
        LOGD("ContentAnalysis::CancelWithError on main thread for request %s",
             aRequestToken.get());
        RefPtr<ContentAnalysis> owner = GetContentAnalysisFromService();
        if (!owner) {
          // May be shutting down
          return;
        }
        owner->SetLastResult(aResult);
        nsCOMPtr<nsIObserverService> obsServ =
            mozilla::services::GetObserverService();
        nsIContentAnalysisResponse::Action action =
            nsIContentAnalysisResponse::Action::eCanceled;
        // If we're shutting down, ignore the default result and just leave the
        // action as canceled. This fixes a hang if the default result is warn
        // and we shutdown during a request (bug 1912245)
        if (aResult != NS_ERROR_ILLEGAL_DURING_SHUTDOWN) {
          DefaultResult defaultResponse = GetDefaultResultFromPref();
          switch (defaultResponse) {
            case DefaultResult::eAllow:
              action = nsIContentAnalysisResponse::Action::eAllow;
              break;
            case DefaultResult::eWarn:
              action = nsIContentAnalysisResponse::Action::eWarn;
              break;
            case DefaultResult::eBlock:
              action = nsIContentAnalysisResponse::Action::eCanceled;
              break;
            default:
              MOZ_ASSERT(false);
              action = nsIContentAnalysisResponse::Action::eCanceled;
          }
        }
        RefPtr<ContentAnalysisResponse> response =
            ContentAnalysisResponse::FromAction(action, aRequestToken);
        response->SetOwner(owner);
        nsIContentAnalysisResponse::CancelError cancelError;
        switch (aResult) {
          case NS_ERROR_NOT_AVAILABLE:
            cancelError = nsIContentAnalysisResponse::CancelError::eNoAgent;
            break;
          case NS_ERROR_INVALID_SIGNATURE:
            cancelError =
                nsIContentAnalysisResponse::CancelError::eInvalidAgentSignature;
            break;
          default:
            cancelError = nsIContentAnalysisResponse::CancelError::eErrorOther;
            break;
        }
        response->SetCancelError(cancelError);
        Maybe<CallbackData> maybeCallbackData;
        {
          auto lock = owner->mCallbackMap.Lock();
          maybeCallbackData = lock->Extract(aRequestToken);
          if (maybeCallbackData.isNothing()) {
            LOGD("Content analysis did not find callback for token %s",
                 aRequestToken.get());
            return;
          }
        }
        if (action == nsIContentAnalysisResponse::Action::eWarn) {
          owner->SendWarnResponse(std::move(aRequestToken),
                                  std::move(*maybeCallbackData), response);
          return;
        }
        nsMainThreadPtrHandle<nsIContentAnalysisCallback> callbackHolder =
            maybeCallbackData->TakeCallbackHolder();
        obsServ->NotifyObservers(response, "dlp-response", nullptr);
        if (callbackHolder) {
          if (action == nsIContentAnalysisResponse::Action::eCanceled) {
            callbackHolder->Error(aResult);
          } else {
            callbackHolder->ContentResult(response);
          }
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
    int64_t aRequestCount,
    const RefPtr<nsIContentAnalysisCallback>& aCallback) {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = NS_ERROR_FAILURE;
  auto callbackCopy = aCallback;
  auto se = MakeScopeExit([&] {
    if (!NS_SUCCEEDED(rv)) {
      LOGE("RunAnalyzeRequestTask failed");
      callbackCopy->Error(rv);
    }
  });

  nsCString requestToken;
  rv = aRequest->GetRequestToken(requestToken);
  NS_ENSURE_SUCCESS(rv, rv);

  nsMainThreadPtrHandle<nsIContentAnalysisCallback> callbackHolderCopy(
      new nsMainThreadPtrHolder<nsIContentAnalysisCallback>(
          "content analysis callback", aCallback));
  CallbackData callbackData(std::move(callbackHolderCopy), aAutoAcknowledge);
  {
    auto lock = mCallbackMap.Lock();
    lock->InsertOrUpdate(requestToken, std::move(callbackData));
  }

  // Check URLs of requested info against
  // browser.contentanalysis.allow_url_regex_list/deny_url_regex_list.
  // Build the list once since creating regexs is slow.
  // URLs that match the allow list are removed from the check.  There is
  // only one URL in all cases except downloads.  If all contents are removed
  // or the page URL is allowed (for downloads) then the operation is allowed.
  // URLs that match the deny list block the entire operation.
  // If the request is completely covered by this filter then flag it as
  // not needing to send an Acknowledge.
  auto filterResult = FilterByUrlLists(aRequest);
  if (filterResult == ContentAnalysis::UrlFilterResult::eDeny) {
    LOGD("Blocking request due to deny URL filter.");
    auto response = ContentAnalysisResponse::FromAction(
        nsIContentAnalysisResponse::Action::eBlock, requestToken);
    response->DoNotAcknowledge();
    IssueResponse(response);
    return NS_OK;
  }
  if (filterResult == ContentAnalysis::UrlFilterResult::eAllow) {
    LOGD("Allowing request -- all operations match allow URL filter.");
    auto response = ContentAnalysisResponse::FromAction(
        nsIContentAnalysisResponse::Action::eAllow, requestToken);
    response->DoNotAcknowledge();
    IssueResponse(response);
    return NS_OK;
  }

  content_analysis::sdk::ContentAnalysisRequest pbRequest;
  rv =
      ConvertToProtobuf(aRequest, GetUserActionId(), aRequestCount, &pbRequest);
  NS_ENSURE_SUCCESS(rv, rv);

  // This is a very simple cache to avoid the case of making multiple
  // consecutive DLP requests to the agent for the same text data. This has
  // been an issue on Google Docs and OneDrive (bug 1912384)
  nsCOMPtr<nsIContentAnalysisRequest> requestToCache;
  CachedData::CacheResult cacheMatchResult =
      mCachedData.CompareWithRequest(aRequest);
  if (cacheMatchResult == CachedData::CacheResult::Matches) {
    auto action = mCachedData.ResultAction();
    MOZ_ASSERT(action.isSome());
    LOGD("Found existing request in cache for token %s with action %d",
         requestToken.get(), *action);
    mCachedData.SetExpirationTimer();
    auto response = ContentAnalysisResponse::FromAction(*action, requestToken);
    response->DoNotAcknowledge();
    response->SetIsCachedResponse();
    IssueResponse(response);
    return NS_OK;
  }
  if (cacheMatchResult != CachedData::CacheResult::CannotBeCached) {
    // We will use the cache
    requestToCache = aRequest;
  }
  LOGD("Issuing ContentAnalysisRequest for token %s", requestToken.get());
  LogRequest(&pbRequest);
  nsCOMPtr<nsIObserverService> obsServ =
      mozilla::services::GetObserverService();
  // Avoid serializing the string here if no one is observing this message
  if (obsServ->HasObservers("dlp-request-sent-raw")) {
    std::string requestString = pbRequest.SerializeAsString();
    nsTArray<char16_t> requestArray;
    requestArray.SetLength(requestString.size() + 1);
    for (size_t i = 0; i < requestString.size(); ++i) {
      // Since NotifyObservers() expects a null-terminated string,
      // make sure none of these values are 0.
      requestArray[i] = requestString[i] + 0xFF00;
    }
    requestArray[requestString.size()] = 0;
    obsServ->NotifyObservers(this, "dlp-request-sent-raw",
                             requestArray.Elements());
  }

  mCaClientPromise->Then(
      GetCurrentSerialEventTarget(), __func__,
      [requestToken, pbRequest = std::move(pbRequest),
       requestToCache = std::move(requestToCache)](
          std::shared_ptr<content_analysis::sdk::Client> client) mutable {
        // The content analysis call is synchronous so run in the background.
        NS_DispatchBackgroundTask(
            NS_NewCancelableRunnableFunction(
                __func__,
                [requestToken, pbRequest = std::move(pbRequest),
                 requestToCache = std::move(requestToCache),
                 client = std::move(client)]() mutable {
                  DoAnalyzeRequest(requestToken, std::move(pbRequest),
                                   std::move(requestToCache), client);
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
    nsCOMPtr<nsIContentAnalysisRequest> aRequestToCache,
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
      [pbResponse = std::move(pbResponse),
       aRequestToCache = std::move(aRequestToCache)]() mutable {
        LOGD("RunAnalyzeRequestTask on main thread about to send response");
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
        if (aRequestToCache) {
          [&]() {
            nsIContentAnalysisResponse::Action action;
            if (NS_FAILED(response->GetAction(&action))) {
              LOGE("Content analysis couldn't get action from response!");
              return;
            }
            nsCString responseRequestToken;
            nsresult requestRv =
                response->GetRequestToken(responseRequestToken);
            if (NS_FAILED(requestRv)) {
              LOGE(
                  "Content analysis couldn't get request token from response!");
              return;
            }
            {
              auto callbackMap = owner->mCallbackMap.Lock();
              auto maybeCallbackData =
                  callbackMap->MaybeGet(responseRequestToken);
              // This prevents caching cancelled results to avoid issues like
              // bug 1918028.
              if (maybeCallbackData.isSome() &&
                  !maybeCallbackData->Canceled()) {
                owner->mCachedData.SetData(std::move(aRequestToCache), action);
              }
            }
          }();
        }
        owner->IssueResponse(response);
      }));
}

void ContentAnalysis::SendWarnResponse(
    nsCString&& aResponseRequestToken, CallbackData aCallbackData,
    RefPtr<ContentAnalysisResponse>& aResponse) {
  nsCOMPtr<nsIObserverService> obsServ =
      mozilla::services::GetObserverService();
  {
    auto warnResponseDataMap = mWarnResponseDataMap.Lock();
    warnResponseDataMap->InsertOrUpdate(
        aResponseRequestToken,
        WarnResponseData(std::move(aCallbackData), aResponse));
  }
  obsServ->NotifyObservers(aResponse, "dlp-response", nullptr);
}

void ContentAnalysis::IssueResponse(RefPtr<ContentAnalysisResponse>& response) {
  MOZ_ASSERT(NS_IsMainThread());
  nsCString responseRequestToken;
  nsresult requestRv = response->GetRequestToken(responseRequestToken);
  if (NS_FAILED(requestRv)) {
    LOGE("Content analysis couldn't get request token from response!");
    return;
  }
  // Successfully made a request to the agent, so mark that we succeeded
  mLastResult = NS_OK;

  Maybe<CallbackData> maybeCallbackData;
  {
    auto callbackMap = mCallbackMap.Lock();
    maybeCallbackData = callbackMap->Extract(responseRequestToken);
  }
  if (maybeCallbackData.isNothing()) {
    LOGD("Content analysis did not find callback for token %s",
         responseRequestToken.get());
    return;
  }
  response->SetOwner(this);
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

  LOGD("Content analysis resolving response promise for token %s",
       responseRequestToken.get());
  nsIContentAnalysisResponse::Action action;
  DebugOnly<nsresult> rv = response->GetAction(&action);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  nsCOMPtr<nsIObserverService> obsServ =
      mozilla::services::GetObserverService();
  if (action == nsIContentAnalysisResponse::Action::eWarn) {
    SendWarnResponse(std::move(responseRequestToken),
                     std::move(*maybeCallbackData), response);
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
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aRequest);
  NS_ENSURE_ARG(aCallback);
  nsresult rv = AnalyzeContentRequestCallbackPrivate(aRequest, aAutoAcknowledge,
                                                     aCallback);
  if (NS_FAILED(rv)) {
    nsCString requestToken;
    nsresult requestTokenRv = aRequest->GetRequestToken(requestToken);
    NS_ENSURE_SUCCESS(requestTokenRv, requestTokenRv);
    CancelWithError(requestToken, rv);
  }
  return rv;
}

nsresult ContentAnalysis::AnalyzeContentRequestCallbackPrivate(
    nsIContentAnalysisRequest* aRequest, bool aAutoAcknowledge,
    nsIContentAnalysisCallback* aCallback) {
  // Make sure we send the notification first, so if we later return
  // an error the JS will handle it correctly.
  nsCOMPtr<nsIObserverService> obsServ =
      mozilla::services::GetObserverService();
  obsServ->NotifyObservers(aRequest, "dlp-request-made", nullptr);

  bool isActive;
  nsresult rv = GetIsActive(&isActive);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!isActive) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  MOZ_ASSERT(NS_IsMainThread());
  // since we're on the main thread, don't need to synchronize this
  int64_t requestCount = ++mRequestCount;
  return RunAnalyzeRequestTask(aRequest, aAutoAcknowledge, requestCount,
                               aCallback);
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
ContentAnalysis::CancelAllRequests() {
  LOGD("CancelAllRequests running");
  mCaClientPromise->Then(
      GetCurrentSerialEventTarget(), __func__,
      [&](std::shared_ptr<content_analysis::sdk::Client> client) {
        auto owner = GetContentAnalysisFromService();
        if (!owner) {
          // May be shutting down
          return;
        }
        NS_DispatchToMainThread(NS_NewCancelableRunnableFunction(
            "ContentAnalysis::CancelAllRequests", []() {
              auto owner = GetContentAnalysisFromService();
              if (!owner) {
                // May be shutting down
                return;
              }
              {
                auto callbackMap = owner->mCallbackMap.Lock();
                auto keys = callbackMap->Keys();
                for (const auto& key : keys) {
                  owner->CancelWithError(nsCString(key),
                                         NS_ERROR_ILLEGAL_DURING_SHUTDOWN);
                }
              }
            }));
        {
          auto warnResponseDataMap = owner->mWarnResponseDataMap.Lock();
          auto keys = warnResponseDataMap->Keys();
          for (const auto& key : keys) {
            LOGD(
                "Responding to warn dialog (from CancelAllRequests) for "
                "request %s",
                nsCString(key).get());
            owner->RespondToWarnDialog(key, false);
          }
        }
        if (!client) {
          LOGE("CancelAllRequests got a null client");
          return;
        }
        content_analysis::sdk::ContentAnalysisCancelRequests requests;
        requests.set_user_action_id(owner->GetUserActionId().get());
        int err = client->CancelRequests(requests);
        if (err != 0) {
          LOGE("CancelAllRequests got error %d", err);
        } else {
          LOGD("CancelAllRequests did cancelling of requests");
        }
      },
      [&](nsresult rv) { LOGE("CancelAllRequests failed to get the client"); });
  return NS_OK;
}

NS_IMETHODIMP
ContentAnalysis::RespondToWarnDialog(const nsACString& aRequestToken,
                                     bool aAllowContent) {
  nsCString requestToken(aRequestToken);
  LOGD(
      "ContentAnalysis::RespondToWarnDialog dispatching to main thread for "
      "request %s",
      requestToken.get());
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
        nsIContentAnalysisResponse::Action action;
        DebugOnly<nsresult> rv = entry->mResponse->GetAction(&action);
        MOZ_ASSERT(NS_SUCCEEDED(rv));
        {
          auto request = self->mCachedData.Request();
          if (request) {
            nsCString cachedRequestToken;
            DebugOnly<nsresult> tokenRv =
                request->GetRequestToken(cachedRequestToken);
            MOZ_ASSERT(NS_SUCCEEDED(tokenRv));
            if (cachedRequestToken.Equals(requestToken)) {
              self->mCachedData.UpdateWarnAction(action);
            }
          }
        }
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

#if defined(XP_WIN)
RefPtr<ContentAnalysis::PrintAllowedPromise>
ContentAnalysis::PrintToPDFToDetermineIfPrintAllowed(
    dom::CanonicalBrowsingContext* aBrowsingContext,
    nsIPrintSettings* aPrintSettings) {
  // Note that the IsChrome() check here excludes a few
  // common about pages like about:config, about:preferences,
  // and about:support, but other about: pages may still
  // go through content analysis.
  if (aBrowsingContext->IsChrome()) {
    return PrintAllowedPromise::CreateAndResolve(PrintAllowedResult(true),
                                                 __func__);
  }
  nsCOMPtr<nsIPrintSettings> contentAnalysisPrintSettings;
  if (NS_WARN_IF(NS_FAILED(aPrintSettings->Clone(
          getter_AddRefs(contentAnalysisPrintSettings)))) ||
      NS_WARN_IF(!aBrowsingContext->GetCurrentWindowGlobal())) {
    return PrintAllowedPromise::CreateAndReject(
        PrintAllowedError(NS_ERROR_FAILURE), __func__);
  }
  contentAnalysisPrintSettings->SetOutputDestination(
      nsIPrintSettings::OutputDestinationType::kOutputDestinationStream);
  contentAnalysisPrintSettings->SetOutputFormat(
      nsIPrintSettings::kOutputFormatPDF);
  nsCOMPtr<nsIStorageStream> storageStream =
      do_CreateInstance("@mozilla.org/storagestream;1");
  if (!storageStream) {
    return PrintAllowedPromise::CreateAndReject(
        PrintAllowedError(NS_ERROR_FAILURE), __func__);
  }
  // Use segment size of 512K
  nsresult rv = storageStream->Init(0x80000, UINT32_MAX);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return PrintAllowedPromise::CreateAndReject(PrintAllowedError(rv),
                                                __func__);
  }

  nsCOMPtr<nsIOutputStream> outputStream;
  storageStream->QueryInterface(NS_GET_IID(nsIOutputStream),
                                getter_AddRefs(outputStream));
  MOZ_ASSERT(outputStream);

  contentAnalysisPrintSettings->SetOutputStream(outputStream.get());
  RefPtr<dom::CanonicalBrowsingContext> browsingContext = aBrowsingContext;
  auto promise = MakeRefPtr<PrintAllowedPromise::Private>(__func__);
  nsCOMPtr<nsIPrintSettings> finalPrintSettings(aPrintSettings);
  aBrowsingContext
      ->PrintWithNoContentAnalysis(contentAnalysisPrintSettings, true, nullptr)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [browsingContext, contentAnalysisPrintSettings, finalPrintSettings,
           promise](
              dom::MaybeDiscardedBrowsingContext cachedStaticBrowsingContext)
              MOZ_CAN_RUN_SCRIPT_BOUNDARY_LAMBDA mutable {
                nsCOMPtr<nsIOutputStream> outputStream;
                contentAnalysisPrintSettings->GetOutputStream(
                    getter_AddRefs(outputStream));
                nsCOMPtr<nsIStorageStream> storageStream =
                    do_QueryInterface(outputStream);
                MOZ_ASSERT(storageStream);
                nsTArray<uint8_t> printData;
                uint32_t length = 0;
                storageStream->GetLength(&length);
                if (!printData.SetLength(length, fallible)) {
                  promise->Reject(
                      PrintAllowedError(NS_ERROR_OUT_OF_MEMORY,
                                        cachedStaticBrowsingContext),
                      __func__);
                  return;
                }
                nsCOMPtr<nsIInputStream> inputStream;
                nsresult rv = storageStream->NewInputStream(
                    0, getter_AddRefs(inputStream));
                if (NS_FAILED(rv)) {
                  promise->Reject(
                      PrintAllowedError(rv, cachedStaticBrowsingContext),
                      __func__);
                  return;
                }
                uint32_t currentPosition = 0;
                while (currentPosition < length) {
                  uint32_t elementsRead = 0;
                  // Make sure the reinterpret_cast<> below is safe
                  static_assert(std::is_trivially_assignable_v<
                                decltype(*printData.Elements()), char>);
                  rv = inputStream->Read(
                      reinterpret_cast<char*>(printData.Elements()) +
                          currentPosition,
                      length - currentPosition, &elementsRead);
                  if (NS_WARN_IF(NS_FAILED(rv) || !elementsRead)) {
                    promise->Reject(
                        PrintAllowedError(NS_FAILED(rv) ? rv : NS_ERROR_FAILURE,
                                          cachedStaticBrowsingContext),
                        __func__);
                    return;
                  }
                  currentPosition += elementsRead;
                }

                nsString printerName;
                rv = contentAnalysisPrintSettings->GetPrinterName(printerName);
                if (NS_WARN_IF(NS_FAILED(rv))) {
                  promise->Reject(
                      PrintAllowedError(rv, cachedStaticBrowsingContext),
                      __func__);
                  return;
                }

                auto* windowParent = browsingContext->GetCurrentWindowGlobal();
                if (!windowParent) {
                  // The print window may have been closed by the user by now.
                  // Cancel the print.
                  promise->Reject(
                      PrintAllowedError(NS_ERROR_ABORT,
                                        cachedStaticBrowsingContext),
                      __func__);
                  return;
                }
                nsCOMPtr<nsIURI> uri = GetURIForBrowsingContext(
                    windowParent->Canonical()->GetBrowsingContext());
                if (!uri) {
                  promise->Reject(
                      PrintAllowedError(NS_ERROR_FAILURE,
                                        cachedStaticBrowsingContext),
                      __func__);
                  return;
                }
                nsCOMPtr<nsIContentAnalysisRequest> contentAnalysisRequest =
                    new contentanalysis::ContentAnalysisRequest(
                        std::move(printData), std::move(uri),
                        std::move(printerName), windowParent);
                auto callback =
                    MakeRefPtr<contentanalysis::ContentAnalysisCallback>(
                        [browsingContext, cachedStaticBrowsingContext, promise,
                         finalPrintSettings = std::move(finalPrintSettings)](
                            nsIContentAnalysisResponse* aResponse)
                            MOZ_CAN_RUN_SCRIPT_BOUNDARY_LAMBDA mutable {
                              bool shouldAllow = false;
                              DebugOnly<nsresult> rv =
                                  aResponse->GetShouldAllowContent(
                                      &shouldAllow);
                              MOZ_ASSERT(NS_SUCCEEDED(rv));
                              promise->Resolve(
                                  PrintAllowedResult(
                                      shouldAllow, cachedStaticBrowsingContext),
                                  __func__);
                            },
                        [promise,
                         cachedStaticBrowsingContext](nsresult aError) {
                          promise->Reject(
                              PrintAllowedError(aError,
                                                cachedStaticBrowsingContext),
                              __func__);
                        });
                nsCOMPtr<nsIContentAnalysis> contentAnalysis =
                    mozilla::components::nsIContentAnalysis::Service();
                if (NS_WARN_IF(!contentAnalysis)) {
                  promise->Reject(
                      PrintAllowedError(rv, cachedStaticBrowsingContext),
                      __func__);
                } else {
                  bool isActive = false;
                  nsresult rv = contentAnalysis->GetIsActive(&isActive);
                  // Should not be called if content analysis is not active
                  MOZ_ASSERT(isActive);
                  Unused << NS_WARN_IF(NS_FAILED(rv));
                  rv = contentAnalysis->AnalyzeContentRequestCallback(
                      contentAnalysisRequest, /* aAutoAcknowledge */ true,
                      callback);
                  if (NS_WARN_IF(NS_FAILED(rv))) {
                    promise->Reject(
                        PrintAllowedError(rv, cachedStaticBrowsingContext),
                        __func__);
                  }
                }
              },
          [promise](nsresult aError) {
            promise->Reject(PrintAllowedError(aError), __func__);
          });
  return promise;
}
#endif

NS_IMPL_ISUPPORTS(ContentAnalysis::SafeContentAnalysisResultCallback,
                  nsIContentAnalysisCallback);

//  - true means a content analysis request was fired
//  - false means there is no text data in the transferable
//  - NoContentAnalysisResult means there was an error
using ClipboardContentAnalysisResult =
    mozilla::Result<bool, mozilla::contentanalysis::NoContentAnalysisResult>;

NS_IMETHODIMP ContentAnalysis::SafeContentAnalysisResultCallback::ContentResult(
    nsIContentAnalysisResponse* aResponse) {
  RefPtr<ContentAnalysisResult> result =
      ContentAnalysisResult::FromContentAnalysisResponse(aResponse);
  Callback(result);
  return NS_OK;
}

NS_IMETHODIMP ContentAnalysis::SafeContentAnalysisResultCallback::Error(
    nsresult aError) {
  Callback(ContentAnalysisResult::FromNoResult(
      NoContentAnalysisResult::DENY_DUE_TO_OTHER_ERROR));
  return NS_OK;
}

ClipboardContentAnalysisResult AnalyzeText(
    uint64_t aInnerWindowId,
    ContentAnalysis::SafeContentAnalysisResultCallback* aResolver,
    nsIURI* aDocumentURI, nsIContentAnalysis* aContentAnalysis,
    nsString aText) {
  RefPtr<mozilla::dom::WindowGlobalParent> window =
      mozilla::dom::WindowGlobalParent::GetByInnerWindowId(aInnerWindowId);
  if (!window) {
    // The window has gone away in the meantime
    return mozilla::Err(NoContentAnalysisResult::DENY_DUE_TO_OTHER_ERROR);
  }
  nsCOMPtr<nsIContentAnalysisRequest> contentAnalysisRequest =
      new ContentAnalysisRequest(
          nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
          std::move(aText), false, EmptyCString(), aDocumentURI,
          nsIContentAnalysisRequest::OperationType::eClipboard, window);
  nsresult rv = aContentAnalysis->AnalyzeContentRequestCallback(
      contentAnalysisRequest, /* aAutoAcknowledge */ true, aResolver);
  if (NS_FAILED(rv)) {
    return mozilla::Err(NoContentAnalysisResult::DENY_DUE_TO_OTHER_ERROR);
  }
  return true;
}

ClipboardContentAnalysisResult CheckClipboardContentAnalysisAsCustomData(
    uint64_t aInnerWindowId,
    ContentAnalysis::SafeContentAnalysisResultCallback* aResolver,
    nsIURI* aDocumentURI, nsIContentAnalysis* aContentAnalysis,
    nsITransferable* aTrans) {
  nsCOMPtr<nsISupports> transferData;
  if (NS_FAILED(aTrans->GetTransferData(kCustomTypesMime,
                                        getter_AddRefs(transferData)))) {
    return false;
  }
  nsCOMPtr<nsISupportsCString> cStringData = do_QueryInterface(transferData);
  if (!cStringData) {
    return false;
  }
  nsCString str;
  nsresult rv = cStringData->GetData(str);
  if (NS_FAILED(rv)) {
    return false;
  }
  nsString text;
  dom::DataTransfer::ParseExternalCustomTypesString(
      mozilla::Span(str.Data(), str.Length()),
      [&](dom::DataTransfer::ParseExternalCustomTypesStringData&& aData) {
        text = std::move(std::move(aData).second);
      });
  if (text.IsEmpty()) {
    return false;
  }
  return AnalyzeText(aInnerWindowId, aResolver, aDocumentURI, aContentAnalysis,
                     std::move(text));
}

ClipboardContentAnalysisResult CheckClipboardContentAnalysisAsText(
    uint64_t aInnerWindowId,
    ContentAnalysis::SafeContentAnalysisResultCallback* aResolver,
    nsIURI* aDocumentURI, nsIContentAnalysis* aContentAnalysis,
    nsITransferable* aTextTrans, const char* aFlavor) {
  nsCOMPtr<nsISupports> transferData;
  if (NS_FAILED(
          aTextTrans->GetTransferData(aFlavor, getter_AddRefs(transferData)))) {
    return false;
  }
  nsString text;
  nsCOMPtr<nsISupportsString> textData = do_QueryInterface(transferData);
  if (MOZ_LIKELY(textData)) {
    if (NS_FAILED(textData->GetData(text))) {
      return mozilla::Err(NoContentAnalysisResult::DENY_DUE_TO_OTHER_ERROR);
    }
  }
  if (text.IsEmpty()) {
    nsCOMPtr<nsISupportsCString> cStringData = do_QueryInterface(transferData);
    if (cStringData) {
      nsCString cText;
      if (NS_FAILED(cStringData->GetData(cText))) {
        return mozilla::Err(NoContentAnalysisResult::DENY_DUE_TO_OTHER_ERROR);
      }
      text = NS_ConvertUTF8toUTF16(cText);
    }
  }
  if (text.IsEmpty()) {
    // Content Analysis doesn't expect to analyze an empty string.
    // Just approve it.
    return mozilla::Err(NoContentAnalysisResult::
                            ALLOW_DUE_TO_CONTEXT_EXEMPT_FROM_CONTENT_ANALYSIS);
  }
  return AnalyzeText(aInnerWindowId, aResolver, aDocumentURI, aContentAnalysis,
                     std::move(text));
}

ClipboardContentAnalysisResult CheckClipboardContentAnalysisAsFile(
    uint64_t aInnerWindowId,
    ContentAnalysis::SafeContentAnalysisResultCallback* aResolver,
    nsIURI* aDocumentURI, nsIContentAnalysis* aContentAnalysis,
    nsITransferable* aFileTrans) {
  nsCOMPtr<nsISupports> transferData;
  nsresult rv =
      aFileTrans->GetTransferData(kFileMime, getter_AddRefs(transferData));
  nsString filePath;
  if (NS_SUCCEEDED(rv)) {
    if (nsCOMPtr<nsIFile> file = do_QueryInterface(transferData)) {
      rv = file->GetPath(filePath);
    } else {
      MOZ_ASSERT_UNREACHABLE("clipboard data had kFileMime but no nsIFile!");
      return mozilla::Err(NoContentAnalysisResult::DENY_DUE_TO_OTHER_ERROR);
    }
  }
  if (NS_FAILED(rv) || filePath.IsEmpty()) {
    return false;
  }
  RefPtr<mozilla::dom::WindowGlobalParent> window =
      mozilla::dom::WindowGlobalParent::GetByInnerWindowId(aInnerWindowId);
  if (!window) {
    // The window has gone away in the meantime
    return mozilla::Err(NoContentAnalysisResult::DENY_DUE_TO_OTHER_ERROR);
  }
  // Let the content analysis code calculate the digest
  nsCOMPtr<nsIContentAnalysisRequest> contentAnalysisRequest =
      new ContentAnalysisRequest(
          nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
          std::move(filePath), true, EmptyCString(), aDocumentURI,
          nsIContentAnalysisRequest::OperationType::eCustomDisplayString,
          window);
  rv = aContentAnalysis->AnalyzeContentRequestCallback(
      contentAnalysisRequest,
      /* aAutoAcknowledge */ true, aResolver);
  if (NS_FAILED(rv)) {
    return mozilla::Err(NoContentAnalysisResult::DENY_DUE_TO_OTHER_ERROR);
  }
  return true;
}

void ContentAnalysis::CheckClipboardContentAnalysis(
    nsBaseClipboard* aClipboard, mozilla::dom::WindowGlobalParent* aWindow,
    nsITransferable* aTransferable, int32_t aClipboardType,
    SafeContentAnalysisResultCallback* aResolver) {
  using namespace mozilla::contentanalysis;

  // Content analysis is only needed if an outside webpage has access to
  // the data. So, skip content analysis if there is:
  //  - no associated window (for example, scripted clipboard read by system
  //  code)
  //  - the window is a chrome docshell
  //  - the window is being rendered in the parent process (for example,
  //  about:support and the like)
  if (!aWindow || aWindow->GetBrowsingContext()->IsChrome() ||
      aWindow->IsInProcess()) {
    aResolver->Callback(ContentAnalysisResult::FromNoResult(
        NoContentAnalysisResult::
            ALLOW_DUE_TO_CONTEXT_EXEMPT_FROM_CONTENT_ANALYSIS));
    return;
  }
  nsCOMPtr<nsIContentAnalysis> contentAnalysis =
      mozilla::components::nsIContentAnalysis::Service();
  if (!contentAnalysis) {
    aResolver->Callback(ContentAnalysisResult::FromNoResult(
        NoContentAnalysisResult::DENY_DUE_TO_OTHER_ERROR));
    return;
  }

  bool contentAnalysisIsActive;
  nsresult rv = contentAnalysis->GetIsActive(&contentAnalysisIsActive);
  if (MOZ_LIKELY(NS_FAILED(rv) || !contentAnalysisIsActive)) {
    aResolver->Callback(ContentAnalysisResult::FromNoResult(
        NoContentAnalysisResult::ALLOW_DUE_TO_CONTENT_ANALYSIS_NOT_ACTIVE));
    return;
  }

  uint64_t innerWindowId = aWindow->InnerWindowId();
  if (mozilla::StaticPrefs::
          browser_contentanalysis_bypass_for_same_tab_operations()) {
    mozilla::Maybe<uint64_t> cacheInnerWindowId =
        aClipboard->GetClipboardCacheInnerWindowId(aClipboardType);
    if (cacheInnerWindowId.isSome() && *cacheInnerWindowId == innerWindowId) {
      // If the same page copied this data to the clipboard (and the above
      // preference is set) we can skip content analysis and immediately allow
      // this.
      aResolver->Callback(ContentAnalysisResult::FromNoResult(
          NoContentAnalysisResult::ALLOW_DUE_TO_SAME_TAB_SOURCE));
      return;
    }
  }

  nsCOMPtr<nsIURI> currentURI =
      GetURIForBrowsingContext(aWindow->Canonical()->GetBrowsingContext());
  if (!currentURI) {
    aResolver->Callback(ContentAnalysisResult::FromNoResult(
        NoContentAnalysisResult::DENY_DUE_TO_OTHER_ERROR));
    return;
  }
  nsTArray<nsCString> flavors;
  rv = aTransferable->FlavorsTransferableCanExport(flavors);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aResolver->Callback(ContentAnalysisResult::FromNoResult(
        NoContentAnalysisResult::DENY_DUE_TO_OTHER_ERROR));
    return;
  }
  bool keepChecking = true;
  if (flavors.Contains(kFileMime)) {
    auto fileResult = CheckClipboardContentAnalysisAsFile(
        innerWindowId, aResolver, currentURI, contentAnalysis, aTransferable);

    if (fileResult.isErr()) {
      aResolver->Callback(
          ContentAnalysisResult::FromNoResult(fileResult.unwrapErr()));
      return;
    }
    keepChecking = !fileResult.unwrap();
  }
  if (!keepChecking) {
    return;
  }

  auto customResult = CheckClipboardContentAnalysisAsCustomData(
      innerWindowId, aResolver, currentURI, contentAnalysis, aTransferable);
  if (customResult.isErr()) {
    aResolver->Callback(
        ContentAnalysisResult::FromNoResult(customResult.unwrapErr()));
    return;
  }
  keepChecking = !customResult.unwrap();
  if (!keepChecking) {
    return;
  }

  // Note that on Windows, kNativeHTMLMime will return the text in the native
  // Windows clipboard CF_HTML format - see
  // https://learn.microsoft.com/en-us/windows/win32/dataxchg/html-clipboard-format
  auto textFormats = {kTextMime, kHTMLMime, kNativeHTMLMime};
  for (const auto& textFormat : textFormats) {
    auto textResult = CheckClipboardContentAnalysisAsText(
        innerWindowId, aResolver, currentURI, contentAnalysis, aTransferable,
        textFormat);
    if (textResult.isErr()) {
      aResolver->Callback(
          ContentAnalysisResult::FromNoResult(textResult.unwrapErr()));
      return;
    }
    keepChecking = !textResult.unwrap();
    if (!keepChecking) {
      break;
    }
  }

  if (keepChecking) {
    // Couldn't get any data from this
    aResolver->Callback(ContentAnalysisResult::FromNoResult(
        NoContentAnalysisResult::ALLOW_DUE_TO_COULD_NOT_GET_DATA));
    return;
  }
}

bool ContentAnalysis::CheckClipboardContentAnalysisSync(
    nsBaseClipboard* aClipboard, mozilla::dom::WindowGlobalParent* aWindow,
    const nsCOMPtr<nsITransferable>& trans, int32_t aClipboardType) {
  bool requestDone = false;
  RefPtr<nsIContentAnalysisResult> result;
  auto callback = mozilla::MakeRefPtr<SafeContentAnalysisResultCallback>(
      [&requestDone, &result](RefPtr<nsIContentAnalysisResult>&& aResult) {
        result = std::move(aResult);
        requestDone = true;
      });
  CheckClipboardContentAnalysis(aClipboard, aWindow, trans, aClipboardType,
                                callback);
  mozilla::SpinEventLoopUntil("CheckClipboardContentAnalysisSync"_ns,
                              [&requestDone]() -> bool { return requestDone; });
  return result->GetShouldAllowContent();
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

  if (mDoNotAcknowledge) {
    return NS_OK;
  }
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

bool ContentAnalysis::LastRequestSucceeded() {
  return mLastResult != NS_ERROR_NOT_AVAILABLE &&
         mLastResult != NS_ERROR_INVALID_SIGNATURE &&
         mLastResult != NS_ERROR_FAILURE;
}

NS_IMETHODIMP
ContentAnalysis::GetDiagnosticInfo(JSContext* aCx,
                                   mozilla::dom::Promise** aPromise) {
  RefPtr<mozilla::dom::Promise> promise;
  nsresult rv = MakePromise(aCx, &promise);
  NS_ENSURE_SUCCESS(rv, rv);
  mCaClientPromise->Then(
      GetCurrentSerialEventTarget(), __func__,
      [promise](std::shared_ptr<content_analysis::sdk::Client> client) mutable {
        if (!client) {
          auto info = MakeRefPtr<ContentAnalysisDiagnosticInfo>(
              false, EmptyString(), false, 0);
          promise->MaybeResolve(info);
          return;
        }
        RefPtr<ContentAnalysis> self = GetContentAnalysisFromService();
        std::string agentPath = client->GetAgentInfo().binary_path;
        nsString agentWidePath = NS_ConvertUTF8toUTF16(agentPath);
        auto info = MakeRefPtr<ContentAnalysisDiagnosticInfo>(
            self->LastRequestSucceeded(), std::move(agentWidePath), false,
            self ? self->mRequestCount : 0);
        promise->MaybeResolve(info);
      },
      [promise](nsresult rv) {
        RefPtr<ContentAnalysis> self = GetContentAnalysisFromService();
        auto info = MakeRefPtr<ContentAnalysisDiagnosticInfo>(
            false, EmptyString(), rv == NS_ERROR_INVALID_SIGNATURE,
            self ? self->mRequestCount : 0);
        promise->MaybeResolve(info);
      });
  promise.forget(aPromise);
  return NS_OK;
}

/* static */ nsCOMPtr<nsIURI> ContentAnalysis::GetURIForBrowsingContext(
    dom::CanonicalBrowsingContext* aBrowsingContext) {
  dom::WindowGlobalParent* windowGlobal =
      aBrowsingContext->GetCurrentWindowGlobal();
  if (!windowGlobal) {
    return nullptr;
  }
  nsIPrincipal* principal = windowGlobal->DocumentPrincipal();
  dom::CanonicalBrowsingContext* curBrowsingContext =
      aBrowsingContext->GetParent();
  while (curBrowsingContext) {
    dom::WindowGlobalParent* newWindowGlobal =
        curBrowsingContext->GetCurrentWindowGlobal();
    if (!newWindowGlobal) {
      break;
    }
    nsIPrincipal* newPrincipal = newWindowGlobal->DocumentPrincipal();
    if (!(newPrincipal->Subsumes(principal))) {
      break;
    }
    principal = newPrincipal;
    curBrowsingContext = curBrowsingContext->GetParent();
  }
  return principal->GetURI();
}

// IDL implementation
NS_IMETHODIMP ContentAnalysis::GetURIForBrowsingContext(
    dom::BrowsingContext* aBrowsingContext, nsIURI** aURI) {
  NS_ENSURE_ARG_POINTER(aBrowsingContext);
  NS_ENSURE_ARG_POINTER(aURI);
  nsCOMPtr<nsIURI> uri =
      GetURIForBrowsingContext(aBrowsingContext->Canonical());
  if (!uri) {
    return NS_ERROR_FAILURE;
  }
  uri.forget(aURI);
  return NS_OK;
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

NS_IMETHODIMP ContentAnalysisDiagnosticInfo::GetConnectedToAgent(
    bool* aConnectedToAgent) {
  *aConnectedToAgent = mConnectedToAgent;
  return NS_OK;
}
NS_IMETHODIMP ContentAnalysisDiagnosticInfo::GetAgentPath(
    nsAString& aAgentPath) {
  aAgentPath = mAgentPath;
  return NS_OK;
}
NS_IMETHODIMP ContentAnalysisDiagnosticInfo::GetFailedSignatureVerification(
    bool* aFailedSignatureVerification) {
  *aFailedSignatureVerification = mFailedSignatureVerification;
  return NS_OK;
}

NS_IMETHODIMP ContentAnalysisDiagnosticInfo::GetRequestCount(
    int64_t* aRequestCount) {
  *aRequestCount = mRequestCount;
  return NS_OK;
}

ContentAnalysis::CachedData::CacheResult
ContentAnalysis::CachedData::CompareWithRequest(
    const RefPtr<nsIContentAnalysisRequest>& aRequest) {
  MOZ_ASSERT(NS_IsMainThread());
  nsIContentAnalysisRequest::AnalysisType analysisType;
  if (NS_FAILED(aRequest->GetAnalysisType(&analysisType)) ||
      analysisType != nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry) {
    return CacheResult::CannotBeCached;
  }
  nsString requestTextContent;
  if (NS_FAILED(aRequest->GetTextContent(requestTextContent)) ||
      requestTextContent.IsEmpty()) {
    return CacheResult::CannotBeCached;
  }
  nsCOMPtr<nsIURI> requestUri;
  if (NS_FAILED(aRequest->GetUrl(getter_AddRefs(requestUri)))) {
    return CacheResult::CannotBeCached;
  }
  RefPtr<dom::WindowGlobalParent> windowGlobalParent;
  if (NS_FAILED(aRequest->GetWindowGlobalParent(
          getter_AddRefs(windowGlobalParent)))) {
    return CacheResult::CannotBeCached;
  }

  nsCOMPtr<nsIContentAnalysisRequest> cachedRequest = Request();
  if (!cachedRequest) {
    return CacheResult::DoesNotMatchExisting;
  }
  nsCOMPtr<nsIURI> cachedUri;
  bool uriEquals = false;
  if (NS_FAILED(cachedRequest->GetUrl(getter_AddRefs(cachedUri))) ||
      NS_FAILED(cachedUri->Equals(requestUri, &uriEquals)) || !uriEquals) {
    return CacheResult::DoesNotMatchExisting;
  }
  nsString cachedTextContent;
  if (NS_FAILED(cachedRequest->GetTextContent(cachedTextContent)) ||
      !cachedTextContent.Equals(requestTextContent)) {
    return CacheResult::DoesNotMatchExisting;
  }
  RefPtr<dom::WindowGlobalParent> cachedWindowGlobalParent;
  if (NS_FAILED(cachedRequest->GetWindowGlobalParent(
          getter_AddRefs(cachedWindowGlobalParent)))) {
    return CacheResult::DoesNotMatchExisting;
  }
  if (cachedWindowGlobalParent && windowGlobalParent &&
      cachedWindowGlobalParent->InnerWindowId() !=
          windowGlobalParent->InnerWindowId()) {
    return CacheResult::DoesNotMatchExisting;
  }
  return CacheResult::Matches;
}

void ContentAnalysis::CachedData::SetExpirationTimer() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mExpirationTimer) {
    mExpirationTimer->Cancel();
  } else {
    mExpirationTimer = NS_NewTimer();
  }
  mExpirationTimer->InitWithNamedFuncCallback(
      [](nsITimer* func, void* closure) {
        LOGD("Clearing content analysis cache (dispatching to main thread)");
        NS_DispatchToMainThread(
            NS_NewCancelableRunnableFunction("Clear ContentAnalysis cache", [] {
              LOGD("Clearing content analysis cache");
              RefPtr<ContentAnalysis> contentAnalysis =
                  ContentAnalysis::GetContentAnalysisFromService();
              if (contentAnalysis) {
                contentAnalysis->mCachedData.Clear();
              }
            }));
      },
      nullptr, mClearTimeout, nsITimer::TYPE_ONE_SHOT,
      "ContentAnalysis::CachedData::SetExpirationTimer");
  LOGD("Set content analysis cached data clear timer with timeout %d",
       mClearTimeout);
}

void ContentAnalysis::SetCachedDataTimeoutForTesting(uint32_t aNewTimeout) {
  mCachedData.mClearTimeout = aNewTimeout;
}
void ContentAnalysis::ResetCachedDataTimeoutForTesting() {
  mCachedData.mClearTimeout = kDefaultCachedDataTimeoutInMs;
}

#undef LOGD
#undef LOGE
}  // namespace mozilla::contentanalysis
