/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "chromium/safebrowsing.pb.h"
#include "nsEscape.h"
#include "nsString.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsUrlClassifierUtils.h"
#include "nsTArray.h"
#include "nsReadableUtils.h"
#include "plbase64.h"
#include "nsPrintfCString.h"
#include "mozilla/Sprintf.h"
#include "mozilla/Mutex.h"
#include "nsIRedirectHistoryEntry.h"
#include "nsIHttpChannelInternal.h"
#include "mozIThirdPartyUtil.h"
#include "nsIDocShell.h"

#define DEFAULT_PROTOCOL_VERSION "2.2"

static char int_to_hex_digit(int32_t i)
{
  NS_ASSERTION((i >= 0) && (i <= 15), "int too big in int_to_hex_digit");
  return static_cast<char>(((i < 10) ? (i + '0') : ((i - 10) + 'A')));
}

static bool
IsDecimal(const nsACString & num)
{
  for (uint32_t i = 0; i < num.Length(); i++) {
    if (!isdigit(num[i])) {
      return false;
    }
  }

  return true;
}

static bool
IsHex(const nsACString & num)
{
  if (num.Length() < 3) {
    return false;
  }

  if (num[0] != '0' || !(num[1] == 'x' || num[1] == 'X')) {
    return false;
  }

  for (uint32_t i = 2; i < num.Length(); i++) {
    if (!isxdigit(num[i])) {
      return false;
    }
  }

  return true;
}

static bool
IsOctal(const nsACString & num)
{
  if (num.Length() < 2) {
    return false;
  }

  if (num[0] != '0') {
    return false;
  }

  for (uint32_t i = 1; i < num.Length(); i++) {
    if (!isdigit(num[i]) || num[i] == '8' || num[i] == '9') {
      return false;
    }
  }

  return true;
}

/////////////////////////////////////////////////////////////////
// SafeBrowsing V4 related utits.

namespace mozilla {
namespace safebrowsing {

static PlatformType
GetPlatformType()
{
#if defined(ANDROID)
  return ANDROID_PLATFORM;
#elif defined(XP_MACOSX)
  return OSX_PLATFORM;
#elif defined(XP_LINUX)
  return LINUX_PLATFORM;
#elif defined(XP_WIN)
  return WINDOWS_PLATFORM;
#else
  // Default to Linux for other platforms (see bug 1362501).
  return LINUX_PLATFORM;
#endif
}

typedef FetchThreatListUpdatesRequest_ListUpdateRequest ListUpdateRequest;
typedef FetchThreatListUpdatesRequest_ListUpdateRequest_Constraints Constraints;

static void
InitListUpdateRequest(ThreatType aThreatType,
                      const char* aStateBase64,
                      ListUpdateRequest* aListUpdateRequest)
{
  aListUpdateRequest->set_threat_type(aThreatType);
  aListUpdateRequest->set_platform_type(GetPlatformType());
  aListUpdateRequest->set_threat_entry_type(URL);

  Constraints* contraints = new Constraints();
  contraints->add_supported_compressions(RICE);
  aListUpdateRequest->set_allocated_constraints(contraints);

  // Only set non-empty state.
  if (aStateBase64[0] != '\0') {
    nsCString stateBinary;
    nsresult rv = Base64Decode(nsDependentCString(aStateBase64), stateBinary);
    if (NS_SUCCEEDED(rv)) {
      aListUpdateRequest->set_state(stateBinary.get(), stateBinary.Length());
    }
  }
}

static ClientInfo*
CreateClientInfo()
{
  ClientInfo* c = new ClientInfo();

  nsCOMPtr<nsIPrefBranch> prefBranch =
    do_GetService(NS_PREFSERVICE_CONTRACTID);

  nsAutoCString clientId;
  nsresult rv = prefBranch->GetCharPref("browser.safebrowsing.id", clientId);

  if (NS_FAILED(rv)) {
    clientId = "Firefox"; // Use "Firefox" as fallback.
  }

  c->set_client_id(clientId.get());

  return c;
}

static bool
IsAllowedOnCurrentPlatform(uint32_t aThreatType)
{
  PlatformType platform = GetPlatformType();

  switch (aThreatType) {
  case POTENTIALLY_HARMFUL_APPLICATION:
    // Bug 1388582 - Google server would respond 404 error if the request
    // contains PHA on non-mobile platform.
    return ANDROID_PLATFORM == platform;
  case MALICIOUS_BINARY:
  case CSD_DOWNLOAD_WHITELIST:
    // Bug 1392204 - 'goog-downloadwhite-proto' and 'goog-badbinurl-proto'
    // are not available on android.
    return ANDROID_PLATFORM != platform;
  }
  // We allow every threat type not listed in the switch cases.
  return true;
}

} // end of namespace safebrowsing.
} // end of namespace mozilla.

nsUrlClassifierUtils::nsUrlClassifierUtils()
  : mProviderDictLock("nsUrlClassifierUtils.mProviderDictLock")
{
}

nsresult
nsUrlClassifierUtils::Init()
{
  // nsIUrlClassifierUtils is a thread-safe service so it's
  // allowed to use on non-main threads. However, building
  // the provider dictionary must be on the main thread.
  // We forcefully load nsUrlClassifierUtils in
  // nsUrlClassifierDBService::Init() to ensure we must
  // now be on the main thread.
  nsresult rv = ReadProvidersFromPrefs(mProviderDict);
  NS_ENSURE_SUCCESS(rv, rv);

  // Add an observer for shutdown
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (!observerService)
    return NS_ERROR_FAILURE;

  observerService->AddObserver(this, "xpcom-shutdown-threads", false);
  Preferences::AddStrongObserver(this, "browser.safebrowsing");

  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsUrlClassifierUtils,
                  nsIUrlClassifierUtils,
                  nsIObserver)

/////////////////////////////////////////////////////////////////////////////
// nsIUrlClassifierUtils

NS_IMETHODIMP
nsUrlClassifierUtils::GetKeyForURI(nsIURI * uri, nsACString & _retval)
{
  nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(uri);
  if (!innerURI)
    innerURI = uri;

  nsAutoCString host;
  innerURI->GetAsciiHost(host);

  if (host.IsEmpty()) {
    return NS_ERROR_MALFORMED_URI;
  }

  nsresult rv = CanonicalizeHostname(host, _retval);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString path;
  rv = innerURI->GetPathQueryRef(path);
  NS_ENSURE_SUCCESS(rv, rv);

  // strip out anchors
  int32_t ref = path.FindChar('#');
  if (ref != kNotFound)
    path.SetLength(ref);

  nsAutoCString temp;
  rv = CanonicalizePath(path, temp);
  NS_ENSURE_SUCCESS(rv, rv);

  _retval.Append(temp);

  return NS_OK;
}

// We use "goog-*-proto" as the list name for v4, where "proto" indicates
// it's updated (as well as hash completion) via protobuf.
//
// In the mozilla official build, we are allowed to use the
// private phishing list (goog-phish-proto). See Bug 1288840.
static const struct {
  const char* mListName;
  uint32_t mThreatType;
} THREAT_TYPE_CONV_TABLE[] = {
  { "goog-malware-proto",  MALWARE_THREAT},                  // 1
  { "googpub-phish-proto", SOCIAL_ENGINEERING_PUBLIC},       // 2
  { "goog-unwanted-proto", UNWANTED_SOFTWARE},               // 3
  { "goog-harmful-proto",  POTENTIALLY_HARMFUL_APPLICATION}, // 4
  { "goog-phish-proto",    SOCIAL_ENGINEERING},              // 5

  // For application reputation
  { "goog-badbinurl-proto", MALICIOUS_BINARY},            // 7
  { "goog-downloadwhite-proto", CSD_DOWNLOAD_WHITELIST},  // 9

  // For login reputation
  { "goog-passwordwhite-proto", CSD_WHITELIST}, // 8

  // For testing purpose.
  { "test-phish-proto",    SOCIAL_ENGINEERING_PUBLIC}, // 2
  { "test-unwanted-proto", UNWANTED_SOFTWARE},         // 3
  { "test-passwordwhite-proto", CSD_WHITELIST},        // 8
};

NS_IMETHODIMP
nsUrlClassifierUtils::ConvertThreatTypeToListNames(uint32_t aThreatType,
                                                   nsACString& aListNames)
{
  for (uint32_t i = 0; i < ArrayLength(THREAT_TYPE_CONV_TABLE); i++) {
    if (aThreatType == THREAT_TYPE_CONV_TABLE[i].mThreatType) {
      if (!aListNames.IsEmpty()) {
        aListNames.AppendLiteral(",");
      }
      aListNames += THREAT_TYPE_CONV_TABLE[i].mListName;
    }
  }

  return aListNames.IsEmpty() ? NS_ERROR_FAILURE : NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierUtils::ConvertListNameToThreatType(const nsACString& aListName,
                                                  uint32_t* aThreatType)
{
  for (uint32_t i = 0; i < ArrayLength(THREAT_TYPE_CONV_TABLE); i++) {
    if (aListName.EqualsASCII(THREAT_TYPE_CONV_TABLE[i].mListName)) {
      *aThreatType = THREAT_TYPE_CONV_TABLE[i].mThreatType;
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsUrlClassifierUtils::GetProvider(const nsACString& aTableName,
                                  nsACString& aProvider)
{
  MutexAutoLock lock(mProviderDictLock);
  nsCString* provider = nullptr;
  if (StringBeginsWith(aTableName, NS_LITERAL_CSTRING("test"))) {
    aProvider = NS_LITERAL_CSTRING(TESTING_TABLE_PROVIDER_NAME);
  } else if (mProviderDict.Get(aTableName, &provider)) {
    aProvider = provider ? *provider : EmptyCString();
  } else {
    aProvider = EmptyCString();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierUtils::GetTelemetryProvider(const nsACString& aTableName,
                                  nsACString& aProvider)
{
  GetProvider(aTableName, aProvider);
  // Whitelist known providers to avoid reporting on private ones.
  // An empty provider is treated as "other"
  if (!NS_LITERAL_CSTRING("mozilla").Equals(aProvider) &&
      !NS_LITERAL_CSTRING("google").Equals(aProvider) &&
      !NS_LITERAL_CSTRING("google4").Equals(aProvider) &&
      !NS_LITERAL_CSTRING("baidu").Equals(aProvider) &&
      !NS_LITERAL_CSTRING("mozcn").Equals(aProvider) &&
      !NS_LITERAL_CSTRING("yandex").Equals(aProvider) &&
      !NS_LITERAL_CSTRING(TESTING_TABLE_PROVIDER_NAME).Equals(aProvider)) {
    aProvider.AssignLiteral("other");
  }

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierUtils::GetProtocolVersion(const nsACString& aProvider,
                                         nsACString& aVersion)
{
  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefBranch) {
      nsPrintfCString prefName("browser.safebrowsing.provider.%s.pver",
                               nsCString(aProvider).get());
      nsAutoCString version;
      nsresult rv = prefBranch->GetCharPref(prefName.get(), version);

      aVersion = NS_SUCCEEDED(rv) ? version.get() : DEFAULT_PROTOCOL_VERSION;
  } else {
      aVersion = DEFAULT_PROTOCOL_VERSION;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierUtils::MakeUpdateRequestV4(const char** aListNames,
                                          const char** aStatesBase64,
                                          uint32_t aCount,
                                          nsACString &aRequest)
{
  using namespace mozilla::safebrowsing;

  FetchThreatListUpdatesRequest r;
  r.set_allocated_client(CreateClientInfo());

  for (uint32_t i = 0; i < aCount; i++) {
    nsCString listName(aListNames[i]);
    uint32_t threatType;
    nsresult rv = ConvertListNameToThreatType(listName, &threatType);
    if (NS_FAILED(rv)) {
      continue; // Unknown list name.
    }
    if (!IsAllowedOnCurrentPlatform(threatType)) {
      NS_WARNING(nsPrintfCString("Threat type %d (%s) is unsupported on current platform: %d",
                                 threatType,
                                 aListNames[i],
                                 GetPlatformType()).get());
      continue; // Some threat types are not available on some platforms.
    }
    auto lur = r.mutable_list_update_requests()->Add();
    InitListUpdateRequest(static_cast<ThreatType>(threatType), aStatesBase64[i], lur);
  }

  // Then serialize.
  std::string s;
  r.SerializeToString(&s);

  nsCString out;
  nsresult rv = Base64URLEncode(s.size(),
                                (const uint8_t*)s.c_str(),
                                Base64URLEncodePaddingPolicy::Include,
                                out);
  NS_ENSURE_SUCCESS(rv, rv);

  aRequest = out;

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierUtils::MakeFindFullHashRequestV4(const char** aListNames,
                                                const char** aListStatesBase64,
                                                const char** aPrefixesBase64,
                                                uint32_t aListCount,
                                                uint32_t aPrefixCount,
                                                nsACString &aRequest)
{
  FindFullHashesRequest r;
  r.set_allocated_client(CreateClientInfo());

  nsresult rv;

  //-------------------------------------------------------------------
  // Set up FindFullHashesRequest.threat_info.
  auto threatInfo = r.mutable_threat_info();

  // 1) Set threat types.
  for (uint32_t i = 0; i < aListCount; i++) {
    // Add threat types.
    uint32_t threatType;
    rv = ConvertListNameToThreatType(nsDependentCString(aListNames[i]), &threatType);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!IsAllowedOnCurrentPlatform(threatType)) {
      NS_WARNING(nsPrintfCString("Threat type %d (%s) is unsupported on current platform: %d",
                                 threatType,
                                 aListNames[i],
                                 GetPlatformType()).get());
      continue;
    }
    threatInfo->add_threat_types((ThreatType)threatType);

    // Add client states for index 'i' only when the threat type is available
    // on current platform.
    nsCString stateBinary;
    rv = Base64Decode(nsDependentCString(aListStatesBase64[i]), stateBinary);
    NS_ENSURE_SUCCESS(rv, rv);
    r.add_client_states(stateBinary.get(), stateBinary.Length());
  }

  // 2) Set platform type.
  threatInfo->add_platform_types(GetPlatformType());

  // 3) Set threat entry type.
  threatInfo->add_threat_entry_types(URL);

  // 4) Set threat entries.
  for (uint32_t i = 0; i < aPrefixCount; i++) {
    nsCString prefixBinary;
    rv = Base64Decode(nsDependentCString(aPrefixesBase64[i]), prefixBinary);
    threatInfo->add_threat_entries()->set_hash(prefixBinary.get(),
                                               prefixBinary.Length());
  }
  //-------------------------------------------------------------------

  // Then serialize.
  std::string s;
  r.SerializeToString(&s);

  nsCString out;
  rv = Base64URLEncode(s.size(),
                       (const uint8_t*)s.c_str(),
                       Base64URLEncodePaddingPolicy::Include,
                       out);
  NS_ENSURE_SUCCESS(rv, rv);

  aRequest = out;

  return NS_OK;
}

// Remove ref, query, userpass, anypart which may contain sensitive data
static nsresult
GetSpecWithoutSensitiveData(nsIURI* aUri, nsACString &aSpec)
{
  if (NS_WARN_IF(!aUri)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIURI> clone;
  // Clone to make the uri mutable
  nsresult rv = aUri->CloneIgnoringRef(getter_AddRefs(clone));
  nsCOMPtr<nsIURL> url(do_QueryInterface(clone));
  if (url) {
    rv = url->SetQuery(EmptyCString());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = url->SetRef(EmptyCString());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = url->SetUserPass(EmptyCString());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = url->GetAsciiSpec(aSpec);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

static nsresult
AddThreatSourceFromChannel(ThreatHit& aHit, nsIChannel *aChannel,
                           ThreatHit_ThreatSourceType aType)
{
  if (NS_WARN_IF(!aChannel)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;

  auto matchingSource = aHit.add_resources();
  matchingSource->set_type(aType);

  nsCOMPtr<nsIURI> uri;
  rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString spec;
  rv = GetSpecWithoutSensitiveData(uri, spec);
  NS_ENSURE_SUCCESS(rv, rv);
  matchingSource->set_url(spec.get());

  nsCOMPtr<nsIHttpChannel> httpChannel =
    do_QueryInterface(aChannel);
  if (httpChannel) {
    nsCOMPtr<nsIURI> referrer;
    rv = httpChannel->GetReferrer(getter_AddRefs(referrer));
    if (NS_SUCCEEDED(rv) && referrer) {
      nsCString referrerSpec;
      rv = GetSpecWithoutSensitiveData(referrer, referrerSpec);
      NS_ENSURE_SUCCESS(rv, rv);
      matchingSource->set_referrer(referrerSpec.get());
    }
  }

  nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal =
    do_QueryInterface(aChannel);
  if (httpChannelInternal) {
    nsCString remoteIp;
    rv = httpChannelInternal->GetRemoteAddress(remoteIp);
    if (NS_SUCCEEDED(rv) && !remoteIp.IsEmpty()) {
      matchingSource->set_remote_ip(remoteIp.get());
    }
  }
  return NS_OK;
}
static nsresult
AddThreatSourceFromRedirectEntry(ThreatHit& aHit,
                                 nsIRedirectHistoryEntry *aRedirectEntry,
                                 ThreatHit_ThreatSourceType aType)
{
  if (NS_WARN_IF(!aRedirectEntry)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;

  nsCOMPtr<nsIPrincipal> principal;
  rv = aRedirectEntry->GetPrincipal(getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> uri;
  rv = principal->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString spec;
  rv = GetSpecWithoutSensitiveData(uri, spec);
  NS_ENSURE_SUCCESS(rv, rv);
  auto source = aHit.add_resources();
  source->set_url(spec.get());
  source->set_type(aType);

  nsCOMPtr<nsIURI> referrer;
  rv = aRedirectEntry->GetReferrerURI(getter_AddRefs(referrer));
  if (NS_SUCCEEDED(rv) && referrer) {
    nsCString referrerSpec;
    rv = GetSpecWithoutSensitiveData(referrer, referrerSpec);
    NS_ENSURE_SUCCESS(rv, rv);
    source->set_referrer(referrerSpec.get());
  }

  nsCString remoteIp;
  rv = aRedirectEntry->GetRemoteAddress(remoteIp);
  if (NS_SUCCEEDED(rv) && !remoteIp.IsEmpty()) {
    source->set_remote_ip(remoteIp.get());
  }
  return NS_OK;
}

// Add top level tab url and redirect threatsources to threatHit message
static nsresult
AddTabThreatSources(ThreatHit& aHit, nsIChannel *aChannel)
{
  if (NS_WARN_IF(!aChannel)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;
  nsCOMPtr<mozIDOMWindowProxy> win;
  nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil =
    do_GetService(THIRDPARTYUTIL_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = thirdPartyUtil->GetTopWindowForChannel(aChannel, getter_AddRefs(win));
  NS_ENSURE_SUCCESS(rv, rv);

  auto* pwin = nsPIDOMWindowOuter::From(win);
  nsCOMPtr<nsIDocShell> docShell = pwin->GetDocShell();
  if (!docShell) {
    return NS_OK;
  }

  nsCOMPtr<nsIChannel> topChannel;
  docShell->GetCurrentDocumentChannel(getter_AddRefs(topChannel));
  if (!topChannel) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> uri;
  rv = aChannel->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> topUri;
  rv = topChannel->GetURI(getter_AddRefs(topUri));
  NS_ENSURE_SUCCESS(rv, rv);

  bool isTopUri = false;
  rv = topUri->Equals(uri, &isTopUri);
  if (NS_SUCCEEDED(rv) && !isTopUri) {
    nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
    if (loadInfo && loadInfo->RedirectChain().Length()) {
      AddThreatSourceFromRedirectEntry(aHit, loadInfo->RedirectChain()[0],
                                       ThreatHit_ThreatSourceType_TAB_RESOURCE);
    }
  }

  // Set top level tab_url threat source
  rv = AddThreatSourceFromChannel(aHit, topChannel,
                                  ThreatHit_ThreatSourceType_TAB_URL);
  Unused << NS_WARN_IF(NS_FAILED(rv));

  // Set tab_redirect threat sources if there's any
  nsCOMPtr<nsILoadInfo> topLoadInfo = topChannel->GetLoadInfo();
  if (!topLoadInfo) {
    return NS_OK;
  }

  nsIRedirectHistoryEntry* redirectEntry;
  size_t length = topLoadInfo->RedirectChain().Length();
  for (size_t i = 0; i < length; i++) {
    redirectEntry = topLoadInfo->RedirectChain()[i];
    AddThreatSourceFromRedirectEntry(aHit, redirectEntry,
                                     ThreatHit_ThreatSourceType_TAB_REDIRECT);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierUtils::MakeThreatHitReport(nsIChannel *aChannel,
                                          const nsACString& aListName,
                                          const nsACString& aHashBase64,
                                          nsACString &aRequest)
{
  if (NS_WARN_IF(aListName.IsEmpty()) ||
      NS_WARN_IF(aHashBase64.IsEmpty()) ||
      NS_WARN_IF(!aChannel)) {
    return NS_ERROR_INVALID_ARG;
  }

  ThreatHit hit;
  nsresult rv;

  uint32_t threatType;
  rv = ConvertListNameToThreatType(aListName, &threatType);
  NS_ENSURE_SUCCESS(rv, rv);
  hit.set_threat_type(static_cast<ThreatType>(threatType));

  hit.set_platform_type(GetPlatformType());

  nsCString hash;
  rv = Base64Decode(aHashBase64, hash);
  if (NS_FAILED(rv) || hash.Length() != COMPLETE_SIZE) {
    return NS_ERROR_FAILURE;
  }

  auto threatEntry = hit.mutable_entry();
  threatEntry->set_hash(hash.get(), hash.Length());

  // Set matching source
  rv = AddThreatSourceFromChannel(hit, aChannel,
                                  ThreatHit_ThreatSourceType_MATCHING_URL);
  Unused << NS_WARN_IF(NS_FAILED(rv));
  // Set tab url, tab resource url and redirect sources
  rv = AddTabThreatSources(hit, aChannel);
  Unused << NS_WARN_IF(NS_FAILED(rv));

  hit.set_allocated_client_info(CreateClientInfo());

  std::string s;
  hit.SerializeToString(&s);

  nsCString out;
  rv = Base64URLEncode(s.size(),
                       reinterpret_cast<const uint8_t*>(s.c_str()),
                       Base64URLEncodePaddingPolicy::Include,
                       out);
  NS_ENSURE_SUCCESS(rv, rv);

  aRequest = out;

  return NS_OK;
}

static uint32_t
DurationToMs(const Duration& aDuration)
{
  // Seconds precision is good enough. Ignore nanoseconds like Chrome does.
  return aDuration.seconds() * 1000;
}

NS_IMETHODIMP
nsUrlClassifierUtils::ParseFindFullHashResponseV4(const nsACString& aResponse,
                                                  nsIUrlClassifierParseFindFullHashCallback *aCallback)
{
  enum CompletionErrorType {
    SUCCESS = 0,
    PARSING_FAILURE = 1,
    UNKNOWN_THREAT_TYPE = 2,
  };

  FindFullHashesResponse r;
  if (!r.ParseFromArray(aResponse.BeginReading(), aResponse.Length())) {
    NS_WARNING("Invalid response");
    Telemetry::Accumulate(Telemetry::URLCLASSIFIER_COMPLETION_ERROR,
                          PARSING_FAILURE);
    return NS_ERROR_FAILURE;
  }

  bool hasUnknownThreatType = false;

  for (auto& m : r.matches()) {
    nsCString tableNames;
    nsresult rv = ConvertThreatTypeToListNames(m.threat_type(), tableNames);
    if (NS_FAILED(rv)) {
      hasUnknownThreatType = true;
      continue; // Ignore un-convertable threat type.
    }
    auto& hash = m.threat().hash();
    auto cacheDurationSec = m.cache_duration().seconds();
    aCallback->OnCompleteHashFound(nsDependentCString(hash.c_str(), hash.length()),
                                   tableNames, cacheDurationSec);

    Telemetry::Accumulate(Telemetry::URLCLASSIFIER_POSITIVE_CACHE_DURATION,
                          cacheDurationSec * PR_MSEC_PER_SEC);
  }

  auto minWaitDuration = DurationToMs(r.minimum_wait_duration());
  auto negCacheDurationSec = r.negative_cache_duration().seconds();

  aCallback->OnResponseParsed(minWaitDuration, negCacheDurationSec);

  Telemetry::Accumulate(Telemetry::URLCLASSIFIER_COMPLETION_ERROR,
                        hasUnknownThreatType ? UNKNOWN_THREAT_TYPE : SUCCESS);

  Telemetry::Accumulate(Telemetry::URLCLASSIFIER_NEGATIVE_CACHE_DURATION,
                        negCacheDurationSec * PR_MSEC_PER_SEC);

  return NS_OK;
}

//////////////////////////////////////////////////////////
// nsIObserver

NS_IMETHODIMP
nsUrlClassifierUtils::Observe(nsISupports *aSubject, const char *aTopic,
                              const char16_t *aData)
{
  if (0 == strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    MutexAutoLock lock(mProviderDictLock);
    return ReadProvidersFromPrefs(mProviderDict);
  }

  if (0 == strcmp(aTopic, "xpcom-shutdown-threads")) {
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    NS_ENSURE_TRUE(prefs, NS_ERROR_FAILURE);
    return prefs->RemoveObserver("browser.safebrowsing", this);
  }

  return NS_ERROR_UNEXPECTED;
}

/////////////////////////////////////////////////////////////////////////////
// non-interface methods

nsresult
nsUrlClassifierUtils::ReadProvidersFromPrefs(ProviderDictType& aDict)
{
  MOZ_ASSERT(NS_IsMainThread(), "ReadProvidersFromPrefs must be on main thread");

  nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(prefs, NS_ERROR_FAILURE);
  nsCOMPtr<nsIPrefBranch> prefBranch;
  nsresult rv = prefs->GetBranch("browser.safebrowsing.provider.",
                                  getter_AddRefs(prefBranch));
  NS_ENSURE_SUCCESS(rv, rv);

  // We've got a pref branch for "browser.safebrowsing.provider.".
  // Enumerate all children prefs and parse providers.
  uint32_t childCount;
  char** childArray;
  rv = prefBranch->GetChildList("", &childCount, &childArray);
  NS_ENSURE_SUCCESS(rv, rv);

  // Collect providers from childArray.
  nsTHashtable<nsCStringHashKey> providers;
  for (uint32_t i = 0; i < childCount; i++) {
    nsCString child(childArray[i]);
    auto dotPos = child.FindChar('.');
    if (dotPos < 0) {
      continue;
    }

    nsDependentCSubstring provider = Substring(child, 0, dotPos);

    providers.PutEntry(provider);
  }
  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(childCount, childArray);

  // Now we have all providers. Check which one owns |aTableName|.
  // e.g. The owning lists of provider "google" is defined in
  // "browser.safebrowsing.provider.google.lists".
  for (auto itr = providers.Iter(); !itr.Done(); itr.Next()) {
    auto entry = itr.Get();
    nsCString provider(entry->GetKey());
    nsPrintfCString owninListsPref("%s.lists", provider.get());

    nsAutoCString owningLists;
    nsresult rv = prefBranch->GetCharPref(owninListsPref.get(), owningLists);
    if (NS_FAILED(rv)) {
      continue;
    }

    // We've got the owning lists (represented as string) of |provider|.
    // Build the dictionary for the owning list and the current provider.
    nsTArray<nsCString> tables;
    Classifier::SplitTables(owningLists, tables);
    for (auto tableName : tables) {
      aDict.Put(tableName, new nsCString(provider));
    }
  }

  return NS_OK;
}

nsresult
nsUrlClassifierUtils::CanonicalizeHostname(const nsACString & hostname,
                                           nsACString & _retval)
{
  nsAutoCString unescaped;
  if (!NS_UnescapeURL(PromiseFlatCString(hostname).get(),
                      PromiseFlatCString(hostname).Length(),
                      0, unescaped)) {
    unescaped.Assign(hostname);
  }

  nsAutoCString cleaned;
  CleanupHostname(unescaped, cleaned);

  nsAutoCString temp;
  ParseIPAddress(cleaned, temp);
  if (!temp.IsEmpty()) {
    cleaned.Assign(temp);
  }

  ToLowerCase(cleaned);
  SpecialEncode(cleaned, false, _retval);

  return NS_OK;
}


nsresult
nsUrlClassifierUtils::CanonicalizePath(const nsACString & path,
                                       nsACString & _retval)
{
  _retval.Truncate();

  nsAutoCString decodedPath(path);
  nsAutoCString temp;
  while (NS_UnescapeURL(decodedPath.get(), decodedPath.Length(), 0, temp)) {
    decodedPath.Assign(temp);
    temp.Truncate();
  }

  SpecialEncode(decodedPath, true, _retval);
  // XXX: lowercase the path?

  return NS_OK;
}

void
nsUrlClassifierUtils::CleanupHostname(const nsACString & hostname,
                                      nsACString & _retval)
{
  _retval.Truncate();

  const char* curChar = hostname.BeginReading();
  const char* end = hostname.EndReading();
  char lastChar = '\0';
  while (curChar != end) {
    unsigned char c = static_cast<unsigned char>(*curChar);
    if (c == '.' && (lastChar == '\0' || lastChar == '.')) {
      // skip
    } else {
      _retval.Append(*curChar);
    }
    lastChar = c;
    ++curChar;
  }

  // cut off trailing dots
  while (_retval.Length() > 0 && _retval[_retval.Length() - 1] == '.') {
    _retval.SetLength(_retval.Length() - 1);
  }
}

void
nsUrlClassifierUtils::ParseIPAddress(const nsACString & host,
                                     nsACString & _retval)
{
  _retval.Truncate();
  nsACString::const_iterator iter, end;
  host.BeginReading(iter);
  host.EndReading(end);

  if (host.Length() <= 15) {
    // The Windows resolver allows a 4-part dotted decimal IP address to
    // have a space followed by any old rubbish, so long as the total length
    // of the string doesn't get above 15 characters. So, "10.192.95.89 xy"
    // is resolved to 10.192.95.89.
    // If the string length is greater than 15 characters, e.g.
    // "10.192.95.89 xy.wildcard.example.com", it will be resolved through
    // DNS.

    if (FindCharInReadable(' ', iter, end)) {
      end = iter;
    }
  }

  for (host.BeginReading(iter); iter != end; iter++) {
    if (!(isxdigit(*iter) || *iter == 'x' || *iter == 'X' || *iter == '.')) {
      // not an IP
      return;
    }
  }

  host.BeginReading(iter);
  nsTArray<nsCString> parts;
  ParseString(PromiseFlatCString(Substring(iter, end)), '.', parts);
  if (parts.Length() > 4) {
    return;
  }

  // If any potentially-octal numbers (start with 0 but not hex) have
  // non-octal digits, no part of the ip can be in octal
  // XXX: this came from the old javascript implementation, is it really
  // supposed to be like this?
  bool allowOctal = true;
  uint32_t i;

  for (i = 0; i < parts.Length(); i++) {
    const nsCString& part = parts[i];
    if (part[0] == '0') {
      for (uint32_t j = 1; j < part.Length(); j++) {
        if (part[j] == 'x') {
          break;
        }
        if (part[j] == '8' || part[j] == '9') {
          allowOctal = false;
          break;
        }
      }
    }
  }

  for (i = 0; i < parts.Length(); i++) {
    nsAutoCString canonical;

    if (i == parts.Length() - 1) {
      CanonicalNum(parts[i], 5 - parts.Length(), allowOctal, canonical);
    } else {
      CanonicalNum(parts[i], 1, allowOctal, canonical);
    }

    if (canonical.IsEmpty()) {
      _retval.Truncate();
      return;
    }

    if (_retval.IsEmpty()) {
      _retval.Assign(canonical);
    } else {
      _retval.Append('.');
      _retval.Append(canonical);
    }
  }
}

void
nsUrlClassifierUtils::CanonicalNum(const nsACString& num,
                                   uint32_t bytes,
                                   bool allowOctal,
                                   nsACString& _retval)
{
  _retval.Truncate();

  if (num.Length() < 1) {
    return;
  }

  uint32_t val;
  if (allowOctal && IsOctal(num)) {
    if (PR_sscanf(PromiseFlatCString(num).get(), "%o", &val) != 1) {
      return;
    }
  } else if (IsDecimal(num)) {
    if (PR_sscanf(PromiseFlatCString(num).get(), "%u", &val) != 1) {
      return;
    }
  } else if (IsHex(num)) {
  if (PR_sscanf(PromiseFlatCString(num).get(), num[1] == 'X' ? "0X%x" : "0x%x",
                &val) != 1) {
      return;
    }
  } else {
    return;
  }

  while (bytes--) {
    char buf[20];
    SprintfLiteral(buf, "%u", val & 0xff);
    if (_retval.IsEmpty()) {
      _retval.Assign(buf);
    } else {
      _retval = nsDependentCString(buf) + NS_LITERAL_CSTRING(".") + _retval;
    }
    val >>= 8;
  }
}

// This function will encode all "special" characters in typical url
// encoding, that is %hh where h is a valid hex digit.  It will also fold
// any duplicated slashes.
bool
nsUrlClassifierUtils::SpecialEncode(const nsACString & url,
                                    bool foldSlashes,
                                    nsACString & _retval)
{
  bool changed = false;
  const char* curChar = url.BeginReading();
  const char* end = url.EndReading();

  unsigned char lastChar = '\0';
  while (curChar != end) {
    unsigned char c = static_cast<unsigned char>(*curChar);
    if (ShouldURLEscape(c)) {
      _retval.Append('%');
      _retval.Append(int_to_hex_digit(c / 16));
      _retval.Append(int_to_hex_digit(c % 16));

      changed = true;
    } else if (foldSlashes && (c == '/' && lastChar == '/')) {
      // skip
    } else {
      _retval.Append(*curChar);
    }
    lastChar = c;
    curChar++;
  }
  return changed;
}

bool
nsUrlClassifierUtils::ShouldURLEscape(const unsigned char c) const
{
  return c <= 32 || c == '%' || c >=127;
}
