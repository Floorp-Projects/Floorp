/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>
#import <SystemConfiguration/SystemConfiguration.h>

#include "nsISystemProxySettings.h"
#include "mozilla/Components.h"
#include "nsPrintfCString.h"
#include "nsNetCID.h"
#include "nsObjCExceptions.h"
#include "mozilla/Attributes.h"
#include "mozilla/StaticPrefs_network.h"
#include "ProxyUtils.h"
#include "ProxyConfig.h"

class nsOSXSystemProxySettings : public nsISystemProxySettings {
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
  nsresult FindSCProxyPort(const nsACString& aScheme, nsACString& aResultHost,
                           int32_t& aResultPort, bool& aResultSocksProxy);

  // is host:port on the proxy exception list?
  bool IsInExceptionList(const nsACString& aHost) const;

 protected:
  virtual ~nsOSXSystemProxySettings();
  virtual void InitDone() {}
  virtual void OnProxyConfigChangedInternal() {}

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

NS_IMPL_ISUPPORTS(nsOSXSystemProxySettings, nsISystemProxySettings)

NS_IMETHODIMP
nsOSXSystemProxySettings::GetMainThreadOnly(bool* aMainThreadOnly) {
  *aMainThreadOnly = true;
  return NS_OK;
}

// Mapping of URI schemes to SystemConfiguration keys
const nsOSXSystemProxySettings::SchemeMapping
    nsOSXSystemProxySettings::gSchemeMappingList[] = {
        {"http", kSCPropNetProxiesHTTPEnable, kSCPropNetProxiesHTTPProxy,
         kSCPropNetProxiesHTTPPort, false},
        {"https", kSCPropNetProxiesHTTPSEnable, kSCPropNetProxiesHTTPSProxy,
         kSCPropNetProxiesHTTPSPort, false},
        {"ftp", kSCPropNetProxiesFTPEnable, kSCPropNetProxiesFTPProxy,
         kSCPropNetProxiesFTPPort, false},
        {"socks", kSCPropNetProxiesSOCKSEnable, kSCPropNetProxiesSOCKSProxy,
         kSCPropNetProxiesSOCKSPort, true},
        {NULL, NULL, NULL, NULL, false},
};

static void ProxyHasChangedWrapper(SCDynamicStoreRef aStore,
                                   CFArrayRef aChangedKeys, void* aInfo) {
  static_cast<nsOSXSystemProxySettings*>(aInfo)->ProxyHasChanged();
}

nsOSXSystemProxySettings::nsOSXSystemProxySettings()
    : mSystemDynamicStore(NULL), mProxyDict(NULL) {
  mContext = (SCDynamicStoreContext){0, this, NULL, NULL, NULL};
}

nsresult nsOSXSystemProxySettings::Init() {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  // Register for notification of proxy setting changes
  // See:
  // http://developer.apple.com/documentation/Networking/Conceptual/CFNetwork/CFStreamTasks/chapter_4_section_5.html
  mSystemDynamicStore = SCDynamicStoreCreate(NULL, CFSTR("Mozilla"),
                                             ProxyHasChangedWrapper, &mContext);
  if (!mSystemDynamicStore) return NS_ERROR_FAILURE;

  // Set up the store to monitor any changes to the proxies
  CFStringRef proxiesKey = SCDynamicStoreKeyCreateProxies(NULL);
  if (!proxiesKey) return NS_ERROR_FAILURE;

  CFArrayRef keyArray = CFArrayCreate(NULL, (const void**)(&proxiesKey), 1,
                                      &kCFTypeArrayCallBacks);
  CFRelease(proxiesKey);
  if (!keyArray) return NS_ERROR_FAILURE;

  SCDynamicStoreSetNotificationKeys(mSystemDynamicStore, keyArray, NULL);
  CFRelease(keyArray);

  // Add the dynamic store to the run loop
  CFRunLoopSourceRef storeRLSource =
      SCDynamicStoreCreateRunLoopSource(NULL, mSystemDynamicStore, 0);
  if (!storeRLSource) return NS_ERROR_FAILURE;
  CFRunLoopAddSource(CFRunLoopGetCurrent(), storeRLSource,
                     kCFRunLoopCommonModes);
  CFRelease(storeRLSource);

  // Load the initial copy of proxy info
  mProxyDict = (NSDictionary*)SCDynamicStoreCopyProxies(mSystemDynamicStore);
  if (!mProxyDict) return NS_ERROR_FAILURE;

  InitDone();

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

nsOSXSystemProxySettings::~nsOSXSystemProxySettings() {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  [mProxyDict release];

  if (mSystemDynamicStore) {
    // Invalidate the dynamic store's run loop source
    // to get the store out of the run loop
    CFRunLoopSourceRef rls =
        SCDynamicStoreCreateRunLoopSource(NULL, mSystemDynamicStore, 0);
    if (rls) {
      CFRunLoopSourceInvalidate(rls);
      CFRelease(rls);
    }
    CFRelease(mSystemDynamicStore);
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsOSXSystemProxySettings::ProxyHasChanged() {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  [mProxyDict release];
  mProxyDict = (NSDictionary*)SCDynamicStoreCopyProxies(mSystemDynamicStore);

  NS_OBJC_END_TRY_IGNORE_BLOCK;

  OnProxyConfigChangedInternal();
}

nsresult nsOSXSystemProxySettings::FindSCProxyPort(const nsACString& aScheme,
                                                   nsACString& aResultHost,
                                                   int32_t& aResultPort,
                                                   bool& aResultSocksProxy) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  NS_ENSURE_TRUE(mProxyDict != NULL, NS_ERROR_FAILURE);

  for (const SchemeMapping* keys = gSchemeMappingList; keys->mScheme != NULL;
       ++keys) {
    // Check for matching scheme (when appropriate)
    if (strcasecmp(keys->mScheme, PromiseFlatCString(aScheme).get()) &&
        !keys->mIsSocksProxy)
      continue;

    // Check the proxy is enabled
    NSNumber* enabled = [mProxyDict objectForKey:(NSString*)keys->mEnabled];
    NS_ENSURE_TRUE(enabled == NULL || [enabled isKindOfClass:[NSNumber class]],
                   NS_ERROR_FAILURE);
    if ([enabled intValue] == 0) continue;

    // Get the proxy host
    NSString* host = [mProxyDict objectForKey:(NSString*)keys->mHost];
    if (host == NULL) break;
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

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

bool nsOSXSystemProxySettings::IsAutoconfigEnabled() const {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  NSNumber* value = [mProxyDict
      objectForKey:(NSString*)kSCPropNetProxiesProxyAutoConfigEnable];
  NS_ENSURE_TRUE(value == NULL || [value isKindOfClass:[NSNumber class]],
                 false);
  return ([value intValue] != 0);

  NS_OBJC_END_TRY_BLOCK_RETURN(false);
}

nsresult nsOSXSystemProxySettings::GetAutoconfigURL(
    nsAutoCString& aResult) const {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  NSString* value = [mProxyDict
      objectForKey:(NSString*)kSCPropNetProxiesProxyAutoConfigURLString];
  if (value != NULL) {
    NS_ENSURE_TRUE([value isKindOfClass:[NSString class]], NS_ERROR_FAILURE);
    aResult.Assign([value UTF8String]);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

bool nsOSXSystemProxySettings::IsInExceptionList(
    const nsACString& aHost) const {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  NS_ENSURE_TRUE(mProxyDict != NULL, false);

  NSArray* exceptionList =
      [mProxyDict objectForKey:(NSString*)kSCPropNetProxiesExceptionsList];
  NS_ENSURE_TRUE(
      exceptionList == NULL || [exceptionList isKindOfClass:[NSArray class]],
      false);

  NSEnumerator* exceptionEnumerator = [exceptionList objectEnumerator];
  NSString* currentValue = NULL;
  while ((currentValue = [exceptionEnumerator nextObject])) {
    NS_ENSURE_TRUE([currentValue isKindOfClass:[NSString class]], false);
    nsAutoCString overrideStr([currentValue UTF8String]);
    if (mozilla::toolkit::system::IsHostProxyEntry(aHost, overrideStr))
      return true;
  }

  NS_OBJC_END_TRY_BLOCK_RETURN(false);
}

nsresult nsOSXSystemProxySettings::GetPACURI(nsACString& aResult) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  NS_ENSURE_TRUE(mProxyDict != NULL, NS_ERROR_FAILURE);

  nsAutoCString pacUrl;
  if (IsAutoconfigEnabled() && NS_SUCCEEDED(GetAutoconfigURL(pacUrl))) {
    aResult.Assign(pacUrl);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

nsresult nsOSXSystemProxySettings::GetProxyForURI(const nsACString& aSpec,
                                                  const nsACString& aScheme,
                                                  const nsACString& aHost,
                                                  const int32_t aPort,
                                                  nsACString& aResult) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  int32_t proxyPort;
  nsAutoCString proxyHost;
  bool proxySocks;
  nsresult rv = FindSCProxyPort(aScheme, proxyHost, proxyPort, proxySocks);

  if (NS_FAILED(rv) || IsInExceptionList(aHost)) {
    aResult.AssignLiteral("DIRECT");
  } else if (proxySocks) {
    aResult.Assign("SOCKS "_ns + proxyHost + nsPrintfCString(":%d", proxyPort));
  } else {
    aResult.Assign("PROXY "_ns + proxyHost + nsPrintfCString(":%d", proxyPort));
  }

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

using namespace mozilla::net;

class OSXSystemProxySettingsAsync final : public nsOSXSystemProxySettings {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(OSXSystemProxySettingsAsync,
                                       nsOSXSystemProxySettings)
  NS_DECL_NSISYSTEMPROXYSETTINGS

  OSXSystemProxySettingsAsync();

 protected:
  virtual void OnProxyConfigChangedInternal() override;
  void InitDone() override;

 private:
  virtual ~OSXSystemProxySettingsAsync();

  ProxyConfig mConfig;
};

OSXSystemProxySettingsAsync::OSXSystemProxySettingsAsync() = default;

OSXSystemProxySettingsAsync::~OSXSystemProxySettingsAsync() = default;

void OSXSystemProxySettingsAsync::InitDone() { OnProxyConfigChangedInternal(); }

void OSXSystemProxySettingsAsync::OnProxyConfigChangedInternal() {
  ProxyConfig config;

  // PAC
  nsAutoCString pacUrl;
  if (IsAutoconfigEnabled() && NS_SUCCEEDED(GetAutoconfigURL(pacUrl))) {
    config.SetPACUrl(pacUrl);
  }

  // proxies (for now: PROXY and SOCKS)
  for (const SchemeMapping* keys = gSchemeMappingList; keys->mScheme != NULL;
       ++keys) {
    // Check the proxy is enabled
    NSNumber* enabled = [mProxyDict objectForKey:(NSString*)keys->mEnabled];
    if (!(enabled == NULL || [enabled isKindOfClass:[NSNumber class]])) {
      continue;
    }

    if ([enabled intValue] == 0) {
      continue;
    }

    // Get the proxy host
    NSString* host = [mProxyDict objectForKey:(NSString*)keys->mHost];
    if (host == NULL) break;
    if (!([host isKindOfClass:[NSString class]])) {
      continue;
    }

    nsCString resultHost;
    resultHost.Assign([host UTF8String]);

    // Get the proxy port
    NSNumber* port = [mProxyDict objectForKey:(NSString*)keys->mPort];
    if (!([port isKindOfClass:[NSNumber class]])) {
      continue;
    }

    int32_t resultPort = [port intValue];
    ProxyServer server(ProxyConfig::ToProxyType(keys->mScheme), resultHost,
                       resultPort);
    config.Rules().mProxyServers[server.Type()] = std::move(server);
  }

  // exceptions
  NSArray* exceptionList =
      [mProxyDict objectForKey:(NSString*)kSCPropNetProxiesExceptionsList];
  if (exceptionList != NULL && [exceptionList isKindOfClass:[NSArray class]]) {
    NSEnumerator* exceptionEnumerator = [exceptionList objectEnumerator];
    NSString* currentValue = NULL;
    while ((currentValue = [exceptionEnumerator nextObject])) {
      if (currentValue != NULL &&
          [currentValue isKindOfClass:[NSString class]]) {
        nsCString overrideStr([currentValue UTF8String]);
        config.ByPassRules().mExceptions.AppendElement(std::move(overrideStr));
      }
    }
  }

  mConfig = std::move(config);
}

NS_IMETHODIMP
OSXSystemProxySettingsAsync::GetMainThreadOnly(bool* aMainThreadOnly) {
  return nsOSXSystemProxySettings::GetMainThreadOnly(aMainThreadOnly);
}

NS_IMETHODIMP
OSXSystemProxySettingsAsync::GetPACURI(nsACString& aResult) {
  aResult.Assign(mConfig.PACUrl());
  return NS_OK;
}

NS_IMETHODIMP
OSXSystemProxySettingsAsync::GetProxyForURI(const nsACString& aSpec,
                                            const nsACString& aScheme,
                                            const nsACString& aHost,
                                            const int32_t aPort,
                                            nsACString& aResult) {
  for (const auto& bypassRule : mConfig.ByPassRules().mExceptions) {
    if (mozilla::toolkit::system::IsHostProxyEntry(aHost, bypassRule)) {
      aResult.AssignLiteral("DIRECT");
      return NS_OK;
    }
  }

  mConfig.GetProxyString(aScheme, aResult);
  return NS_OK;
}

NS_IMPL_COMPONENT_FACTORY(nsOSXSystemProxySettings) {
  auto settings =
      mozilla::StaticPrefs::network_proxy_detect_system_proxy_changes()
          ? mozilla::MakeRefPtr<OSXSystemProxySettingsAsync>()
          : mozilla::MakeRefPtr<nsOSXSystemProxySettings>();
  if (NS_SUCCEEDED(settings->Init())) {
    return settings.forget().downcast<nsISupports>();
  }
  return nullptr;
}
