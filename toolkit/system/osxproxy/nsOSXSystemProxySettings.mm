/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>
#import <SystemConfiguration/SystemConfiguration.h>

#include "nsISystemProxySettings.h"
#include "mozilla/ModuleUtils.h"
#include "nsIServiceManager.h"
#include "nsPrintfCString.h"
#include "nsNetUtil.h"
#include "nsISupportsPrimitives.h"
#include "nsIURI.h"
#include "nsObjCExceptions.h"
#include "mozilla/Attributes.h"

class nsOSXSystemProxySettings MOZ_FINAL : public nsISystemProxySettings {
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISYSTEMPROXYSETTINGS

  nsOSXSystemProxySettings();
  nsresult Init();

  // called by OSX when the proxy settings have changed
  void ProxyHasChanged();

  // is there a PAC url specified in the system configuration
  bool IsAutoconfigEnabled() const;
  // retrieve the pac url
  nsresult GetAutoconfigURL(nsAutoCString& aResult) const;

  // Find the SystemConfiguration proxy & port for a given URI
  nsresult FindSCProxyPort(const nsACString &aScheme, nsACString& aResultHost, int32_t& aResultPort, bool& aResultSocksProxy);

  // is host:port on the proxy exception list?
  bool IsInExceptionList(const nsACString& aHost) const;

private:
  ~nsOSXSystemProxySettings();

  SCDynamicStoreContext mContext;
  SCDynamicStoreRef mSystemDynamicStore;
  NSDictionary* mProxyDict;


  // Mapping of URI schemes to SystemConfiguration keys
  struct SchemeMapping {
    const char* mScheme;
    CFStringRef mEnabled;
    CFStringRef mHost;
    CFStringRef mPort;
    bool mIsSocksProxy;
  };
  static const SchemeMapping gSchemeMappingList[];
};

NS_IMPL_ISUPPORTS1(nsOSXSystemProxySettings, nsISystemProxySettings)

NS_IMETHODIMP
nsOSXSystemProxySettings::GetMainThreadOnly(bool *aMainThreadOnly)
{
  *aMainThreadOnly = true;
  return NS_OK;
}

// Mapping of URI schemes to SystemConfiguration keys
const nsOSXSystemProxySettings::SchemeMapping nsOSXSystemProxySettings::gSchemeMappingList[] = {
  {"http", kSCPropNetProxiesHTTPEnable, kSCPropNetProxiesHTTPProxy, kSCPropNetProxiesHTTPPort, false},
  {"https", kSCPropNetProxiesHTTPSEnable, kSCPropNetProxiesHTTPSProxy, kSCPropNetProxiesHTTPSPort, false},
  {"ftp", kSCPropNetProxiesFTPEnable, kSCPropNetProxiesFTPProxy, kSCPropNetProxiesFTPPort, false},
  {"socks", kSCPropNetProxiesSOCKSEnable, kSCPropNetProxiesSOCKSProxy, kSCPropNetProxiesSOCKSPort, true},
  {NULL, NULL, NULL, NULL, false},
};

static void
ProxyHasChangedWrapper(SCDynamicStoreRef aStore, CFArrayRef aChangedKeys, void* aInfo)
{
  static_cast<nsOSXSystemProxySettings*>(aInfo)->ProxyHasChanged();
}


nsOSXSystemProxySettings::nsOSXSystemProxySettings()
  : mSystemDynamicStore(NULL), mProxyDict(NULL)
{
  mContext = (SCDynamicStoreContext){0, this, NULL, NULL, NULL};
}

nsresult
nsOSXSystemProxySettings::Init()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // Register for notification of proxy setting changes
  // See: http://developer.apple.com/documentation/Networking/Conceptual/CFNetwork/CFStreamTasks/chapter_4_section_5.html
  mSystemDynamicStore = SCDynamicStoreCreate(NULL, CFSTR("Mozilla"), ProxyHasChangedWrapper, &mContext);
  if (!mSystemDynamicStore)
    return NS_ERROR_FAILURE;

  // Set up the store to monitor any changes to the proxies
  CFStringRef proxiesKey = SCDynamicStoreKeyCreateProxies(NULL);
  if (!proxiesKey)
    return NS_ERROR_FAILURE;

  CFArrayRef keyArray = CFArrayCreate(NULL, (const void**)(&proxiesKey), 1, &kCFTypeArrayCallBacks);
  CFRelease(proxiesKey);
  if (!keyArray)
    return NS_ERROR_FAILURE;

  SCDynamicStoreSetNotificationKeys(mSystemDynamicStore, keyArray, NULL);
  CFRelease(keyArray);

  // Add the dynamic store to the run loop
  CFRunLoopSourceRef storeRLSource = SCDynamicStoreCreateRunLoopSource(NULL, mSystemDynamicStore, 0);
  if (!storeRLSource)
    return NS_ERROR_FAILURE;
  CFRunLoopAddSource(CFRunLoopGetCurrent(), storeRLSource, kCFRunLoopCommonModes);
  CFRelease(storeRLSource);

  // Load the initial copy of proxy info
  mProxyDict = (NSDictionary*)SCDynamicStoreCopyProxies(mSystemDynamicStore);
  if (!mProxyDict)
    return NS_ERROR_FAILURE;

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsOSXSystemProxySettings::~nsOSXSystemProxySettings()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mProxyDict release];

  if (mSystemDynamicStore) {
    // Invalidate the dynamic store's run loop source
    // to get the store out of the run loop
    CFRunLoopSourceRef rls = SCDynamicStoreCreateRunLoopSource(NULL, mSystemDynamicStore, 0);
    if (rls) {
      CFRunLoopSourceInvalidate(rls);
      CFRelease(rls);
    }
    CFRelease(mSystemDynamicStore);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}


void
nsOSXSystemProxySettings::ProxyHasChanged()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mProxyDict release];
  mProxyDict = (NSDictionary*)SCDynamicStoreCopyProxies(mSystemDynamicStore);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsresult
nsOSXSystemProxySettings::FindSCProxyPort(const nsACString &aScheme, nsACString& aResultHost, int32_t& aResultPort, bool& aResultSocksProxy)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NS_ENSURE_TRUE(mProxyDict != NULL, NS_ERROR_FAILURE);

  for (const SchemeMapping* keys = gSchemeMappingList; keys->mScheme != NULL; ++keys) {
    // Check for matching scheme (when appropriate)
    if (strcasecmp(keys->mScheme, PromiseFlatCString(aScheme).get()) &&
        !keys->mIsSocksProxy)
      continue;

    // Check the proxy is enabled
    NSNumber* enabled = [mProxyDict objectForKey:(NSString*)keys->mEnabled];
    NS_ENSURE_TRUE(enabled == NULL || [enabled isKindOfClass:[NSNumber class]], NS_ERROR_FAILURE);
    if ([enabled intValue] == 0)
      continue;
    
    // Get the proxy host
    NSString* host = [mProxyDict objectForKey:(NSString*)keys->mHost];
    if (host == NULL)
      break;
    NS_ENSURE_TRUE([host isKindOfClass:[NSString class]], NS_ERROR_FAILURE);
    aResultHost.Assign([host UTF8String]);

    // Get the proxy port
    NSNumber* port = [mProxyDict objectForKey:(NSString*)keys->mPort];
    NS_ENSURE_TRUE([port isKindOfClass:[NSNumber class]], NS_ERROR_FAILURE);
    aResultPort = [port intValue];

    aResultSocksProxy = keys->mIsSocksProxy;

    return NS_OK;
  }

  return NS_ERROR_FAILURE;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

bool
nsOSXSystemProxySettings::IsAutoconfigEnabled() const
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NSNumber* value = [mProxyDict objectForKey:(NSString*)kSCPropNetProxiesProxyAutoConfigEnable];
  NS_ENSURE_TRUE(value == NULL || [value isKindOfClass:[NSNumber class]], false);
  return ([value intValue] != 0);

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(false);
}

nsresult
nsOSXSystemProxySettings::GetAutoconfigURL(nsAutoCString& aResult) const
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSString* value = [mProxyDict objectForKey:(NSString*)kSCPropNetProxiesProxyAutoConfigURLString];
  if (value != NULL) {
    NS_ENSURE_TRUE([value isKindOfClass:[NSString class]], NS_ERROR_FAILURE);
    aResult.Assign([value UTF8String]);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

static bool
IsHostProxyEntry(const nsACString& aHost, const nsACString& aOverride)
{
  nsAutoCString host(aHost);
  nsAutoCString override(aOverride);

  int32_t overrideLength = override.Length();
  int32_t tokenStart = 0;
  int32_t offset = 0;
  bool star = false;

  while (tokenStart < overrideLength) {
    int32_t tokenEnd = override.FindChar('*', tokenStart);
    if (tokenEnd == tokenStart) {
      // Star is the first character in the token.
      star = true;
      tokenStart++;
      // If the character following the '*' is a '.' character then skip
      // it so that "*.foo.com" allows "foo.com".
      if (override.FindChar('.', tokenStart) == tokenStart)
        tokenStart++;
    } else {
      if (tokenEnd == -1)
        tokenEnd = overrideLength; // no '*' char, match rest of string
      nsAutoCString token(Substring(override, tokenStart, tokenEnd - tokenStart));
      offset = host.Find(token, offset);
      if (offset == -1 || (!star && offset))
        return false;
      star = false;
      tokenStart = tokenEnd;
      offset += token.Length();
    }
  }

  return (star || (offset == static_cast<int32_t>(host.Length())));
}

bool
nsOSXSystemProxySettings::IsInExceptionList(const nsACString& aHost) const
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NS_ENSURE_TRUE(mProxyDict != NULL, false);

  NSArray* exceptionList = [mProxyDict objectForKey:(NSString*)kSCPropNetProxiesExceptionsList];
  NS_ENSURE_TRUE(exceptionList == NULL || [exceptionList isKindOfClass:[NSArray class]], false);

  NSEnumerator* exceptionEnumerator = [exceptionList objectEnumerator];
  NSString* currentValue = NULL;
  while ((currentValue = [exceptionEnumerator nextObject])) {
    NS_ENSURE_TRUE([currentValue isKindOfClass:[NSString class]], false);
    nsAutoCString overrideStr([currentValue UTF8String]);
    if (IsHostProxyEntry(aHost, overrideStr))
      return true;
  }

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(false);
}

nsresult
nsOSXSystemProxySettings::GetPACURI(nsACString& aResult)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NS_ENSURE_TRUE(mProxyDict != NULL, NS_ERROR_FAILURE);

  nsAutoCString pacUrl;
  if (IsAutoconfigEnabled() && NS_SUCCEEDED(GetAutoconfigURL(pacUrl))) {
    aResult.Assign(pacUrl);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult
nsOSXSystemProxySettings::GetProxyForURI(const nsACString & aSpec,
                                         const nsACString & aScheme,
                                         const nsACString & aHost,
                                         const int32_t      aPort,
                                         nsACString & aResult)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  int32_t proxyPort;
  nsAutoCString proxyHost;
  bool proxySocks;
  nsresult rv = FindSCProxyPort(aScheme, proxyHost, proxyPort, proxySocks);

  if (NS_FAILED(rv) || IsInExceptionList(aHost)) {
    aResult.AssignLiteral("DIRECT");
  } else if (proxySocks) {
    aResult.Assign(NS_LITERAL_CSTRING("SOCKS ") + proxyHost + nsPrintfCString(":%d", proxyPort));
  } else {      
    aResult.Assign(NS_LITERAL_CSTRING("PROXY ") + proxyHost + nsPrintfCString(":%d", proxyPort));
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

#define NS_OSXSYSTEMPROXYSERVICE_CID  /* 9afcd4b8-2e0f-41f4-8f1f-3bf0d3cf67de */\
    { 0x9afcd4b8, 0x2e0f, 0x41f4, \
      { 0x8f, 0x1f, 0x3b, 0xf0, 0xd3, 0xcf, 0x67, 0xde } }

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsOSXSystemProxySettings, Init);
NS_DEFINE_NAMED_CID(NS_OSXSYSTEMPROXYSERVICE_CID);

static const mozilla::Module::CIDEntry kOSXSysProxyCIDs[] = {
  { &kNS_OSXSYSTEMPROXYSERVICE_CID, false, NULL, nsOSXSystemProxySettingsConstructor },
  { NULL }
};

static const mozilla::Module::ContractIDEntry kOSXSysProxyContracts[] = {
  { NS_SYSTEMPROXYSETTINGS_CONTRACTID, &kNS_OSXSYSTEMPROXYSERVICE_CID },
  { NULL }
};

static const mozilla::Module kOSXSysProxyModule = {
  mozilla::Module::kVersion,
  kOSXSysProxyCIDs,
  kOSXSysProxyContracts
};

NSMODULE_DEFN(nsOSXProxyModule) = &kOSXSysProxyModule;
