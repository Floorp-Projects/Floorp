/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <ras.h>
#include <wininet.h>
#include <functional>

#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/Atomics.h"
#include "nsISystemProxySettings.h"
#include "mozilla/Components.h"
#include "mozilla/Mutex.h"
#include "mozilla/Services.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/Tokenizer.h"
#include "mozilla/Unused.h"
#include "nsComponentManagerUtils.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIWindowsRegKey.h"
#include "nsPrintfCString.h"
#include "nsNetCID.h"
#include "nsThreadUtils.h"
#include "prnetdb.h"
#include "ProxyUtils.h"
#include "ProxyConfig.h"

using namespace mozilla::net;

class nsWindowsSystemProxySettings : public nsISystemProxySettings {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISYSTEMPROXYSETTINGS

  nsWindowsSystemProxySettings(){};
  virtual nsresult Init() { return NS_OK; }

 protected:
  virtual ~nsWindowsSystemProxySettings() = default;

  bool MatchOverride(const nsACString& aHost);
  bool MatchOverrideInternal(const nsACString& aHost,
                             const nsACString& aOverrideRule);
  bool PatternMatch(const nsACString& aHost, const nsACString& aOverride);
  nsresult ReadProxyRules(
      uint32_t aOptions,
      const std::function<bool(uint32_t aFlags)>& aFlagsHandler,
      const std::function<void(const nsACString& aRule, bool& aContinue)>&
          aRuleHandler);
};

NS_IMPL_ISUPPORTS(nsWindowsSystemProxySettings, nsISystemProxySettings)

NS_IMETHODIMP
nsWindowsSystemProxySettings::GetMainThreadOnly(bool* aMainThreadOnly) {
  // bug 1366133: if you change this to main thread only, please handle
  // nsProtocolProxyService::Resolve_Internal carefully to avoid hang on main
  // thread.
  *aMainThreadOnly = false;
  return NS_OK;
}

static nsresult ReadInternetOption(uint32_t aOption, uint32_t& aFlags,
                                   nsAString& aValue) {
  // Bug 1366133: InternetGetConnectedStateExW() may cause hangs
  MOZ_ASSERT(!NS_IsMainThread());

  DWORD connFlags = 0;
  WCHAR connName[RAS_MaxEntryName + 1];
  MOZ_SEH_TRY {
    InternetGetConnectedStateExW(&connFlags, connName,
                                 mozilla::ArrayLength(connName), 0);
  }
  MOZ_SEH_EXCEPT(EXCEPTION_EXECUTE_HANDLER) { return NS_ERROR_FAILURE; }

  INTERNET_PER_CONN_OPTIONW options[2];
  options[0].dwOption = INTERNET_PER_CONN_FLAGS_UI;
  options[1].dwOption = aOption;

  INTERNET_PER_CONN_OPTION_LISTW list;
  list.dwSize = sizeof(INTERNET_PER_CONN_OPTION_LISTW);
  list.pszConnection =
      connFlags & INTERNET_CONNECTION_MODEM ? connName : nullptr;
  list.dwOptionCount = mozilla::ArrayLength(options);
  list.dwOptionError = 0;
  list.pOptions = options;

  unsigned long size = sizeof(INTERNET_PER_CONN_OPTION_LISTW);
  if (!InternetQueryOptionW(nullptr, INTERNET_OPTION_PER_CONNECTION_OPTION,
                            &list, &size)) {
    return NS_ERROR_FAILURE;
  }

  aFlags = options[0].Value.dwValue;
  aValue.Assign(options[1].Value.pszValue);
  GlobalFree(options[1].Value.pszValue);

  return NS_OK;
}

bool nsWindowsSystemProxySettings::MatchOverrideInternal(
    const nsACString& aHost, const nsACString& aOverrideRule) {
  // Windows formats its proxy override list in the form:
  // server;server;server where 'server' is a server name pattern or IP
  // address, or "<local>". "<local>" must be translated to
  // "localhost;127.0.0.1".
  // In a server name pattern, a '*' character matches any substring and
  // all other characters must match themselves; the whole pattern must match
  // the whole hostname.
  nsAutoCString host(aHost);
  if (aOverrideRule.EqualsLiteral("<local>")) {
    PRNetAddr addr;
    bool isIpAddr = (PR_StringToNetAddr(host.get(), &addr) == PR_SUCCESS);

    // Don't use proxy for local hosts (plain hostname, no dots)
    if (!isIpAddr && !host.Contains('.')) {
      return true;
    }

    if (host.EqualsLiteral("127.0.0.1") || host.EqualsLiteral("::1")) {
      return true;
    }
  } else if (PatternMatch(host, aOverrideRule)) {
    return true;
  }

  return false;
}

bool nsWindowsSystemProxySettings::MatchOverride(const nsACString& aHost) {
  nsAutoCString host(aHost);
  bool foundMatch = false;

  auto flagHandler = [](uint32_t aFlags) { return true; };
  auto ruleHandler = [&](const nsACString& aOverrideRule, bool& aContinue) {
    foundMatch = MatchOverrideInternal(aHost, aOverrideRule);
    if (foundMatch) {
      aContinue = false;
    }
  };

  ReadProxyRules(INTERNET_PER_CONN_PROXY_BYPASS, flagHandler, ruleHandler);
  return foundMatch;
}

nsresult nsWindowsSystemProxySettings::ReadProxyRules(
    uint32_t aOptions,
    const std::function<bool(uint32_t aFlags)>& aFlagsHandler,
    const std::function<void(const nsACString& aRule, bool& aContinue)>&
        aRuleHandler) {
  uint32_t flags = 0;
  nsAutoString buf;

  nsresult rv = ReadInternetOption(aOptions, flags, buf);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!aFlagsHandler(flags)) {
    return NS_ERROR_FAILURE;
  }

  NS_ConvertUTF16toUTF8 cbuf(buf);

  int32_t start = 0;
  int32_t end = cbuf.Length();
  while (true) {
    int32_t delimiter = cbuf.FindCharInSet(" ;", start);
    if (delimiter == -1) delimiter = end;

    if (delimiter != start) {
      const nsAutoCString rule(Substring(cbuf, start, delimiter - start));
      bool continueProcessing = false;
      aRuleHandler(rule, continueProcessing);
      if (!continueProcessing) {
        return NS_OK;
      }
    }

    if (delimiter == end) break;
    start = ++delimiter;
  }

  return NS_OK;
}

bool nsWindowsSystemProxySettings::PatternMatch(const nsACString& aHost,
                                                const nsACString& aOverride) {
  return mozilla::toolkit::system::IsHostProxyEntry(aHost, aOverride);
}

nsresult nsWindowsSystemProxySettings::GetPACURI(nsACString& aResult) {
  AUTO_PROFILER_LABEL("nsWindowsSystemProxySettings::GetPACURI", OTHER);
  nsresult rv;
  uint32_t flags = 0;
  nsAutoString buf;

  rv = ReadInternetOption(INTERNET_PER_CONN_AUTOCONFIG_URL, flags, buf);
  if (!(flags & PROXY_TYPE_AUTO_PROXY_URL)) {
    aResult.Truncate();
    return rv;
  }

  if (NS_SUCCEEDED(rv)) aResult = NS_ConvertUTF16toUTF8(buf);
  return rv;
}

nsresult nsWindowsSystemProxySettings::GetProxyForURI(const nsACString& aSpec,
                                                      const nsACString& aScheme,
                                                      const nsACString& aHost,
                                                      const int32_t aPort,
                                                      nsACString& aResult) {
  auto flagHandler = [&](uint32_t aFlags) {
    if (!(aFlags & PROXY_TYPE_PROXY)) {
      ProxyConfig::SetProxyResultDirect(aResult);
      return false;
    }

    if (MatchOverride(aHost)) {
      ProxyConfig::SetProxyResultDirect(aResult);
      return false;
    }

    return true;
  };

  constexpr auto kSocksPrefix = "socks="_ns;
  nsAutoCString prefix;
  ToLowerCase(aScheme, prefix);
  prefix.Append('=');

  nsAutoCString specificProxy;
  nsAutoCString defaultProxy;
  nsAutoCString socksProxy;

  auto ruleHandler = [&](const nsACString& aRule, bool& aContinue) {
    const nsCString proxy(aRule);
    aContinue = true;
    if (proxy.FindChar('=') == -1) {
      // If a proxy name is listed by itself, it is used as the
      // default proxy for any protocols that do not have a specific
      // proxy specified.
      // (http://msdn.microsoft.com/en-us/library/aa383996%28VS.85%29.aspx)
      defaultProxy = proxy;
    } else if (proxy.Find(prefix) == 0) {
      // To list a proxy for a specific protocol, the string must
      // follow the format "<protocol>=<protocol>://<proxy_name>".
      // (http://msdn.microsoft.com/en-us/library/aa383996%28VS.85%29.aspx)
      specificProxy = Substring(proxy, prefix.Length());
      aContinue = false;
    } else if (proxy.Find(kSocksPrefix) == 0) {
      // SOCKS proxy.
      socksProxy = Substring(proxy, kSocksPrefix.Length());  // "socks=" length.
    }
  };

  nsresult rv =
      ReadProxyRules(INTERNET_PER_CONN_PROXY_SERVER, flagHandler, ruleHandler);
  if (NS_FAILED(rv)) {
    ProxyConfig::SetProxyResultDirect(aResult);
    return rv;
  }

  ProxyConfig::ProxyStrToResult(specificProxy, defaultProxy, socksProxy,
                                aResult);
  return NS_OK;
}

class WindowsSystemProxySettingsAsync final
    : public nsWindowsSystemProxySettings,
      public nsIObserver {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSISYSTEMPROXYSETTINGS
  NS_DECL_NSIOBSERVER

  WindowsSystemProxySettingsAsync();
  nsresult Init() override;

 private:
  virtual ~WindowsSystemProxySettingsAsync();
  void ThreadFunc();
  void OnProxyConfigChangedInternal();

  ProxyConfig mConfig;
  nsCOMPtr<nsIThread> mBackgroundThread;
  mozilla::Mutex mLock{"WindowsSystemProxySettingsAsync"};
  mozilla::Atomic<bool> mInited{false};
  mozilla::Atomic<bool> mTerminated{false};
};

NS_IMPL_ISUPPORTS_INHERITED(WindowsSystemProxySettingsAsync,
                            nsWindowsSystemProxySettings, nsIObserver);

WindowsSystemProxySettingsAsync::WindowsSystemProxySettingsAsync() = default;

WindowsSystemProxySettingsAsync::~WindowsSystemProxySettingsAsync() = default;

nsresult WindowsSystemProxySettingsAsync::Init() {
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (!observerService) {
    return NS_ERROR_FAILURE;
  }
  observerService->AddObserver(this, "xpcom-shutdown-threads", false);

  nsCOMPtr<nsIThread> thread;
  if (NS_FAILED(NS_NewNamedThread("System Proxy", getter_AddRefs(thread)))) {
    NS_WARNING("NS_NewNamedThread failed!");
    return NS_ERROR_FAILURE;
  }

  mBackgroundThread = std::move(thread);

  nsCOMPtr<nsIRunnable> event = mozilla::NewRunnableMethod(
      "WindowsSystemProxySettingsAsync::ThreadFunc", this,
      &WindowsSystemProxySettingsAsync::ThreadFunc);
  return mBackgroundThread->Dispatch(event, nsIEventTarget::DISPATCH_NORMAL);
}

NS_IMETHODIMP
WindowsSystemProxySettingsAsync::Observe(nsISupports* aSubject,
                                         const char* aTopic,
                                         const char16_t* aData) {
  if (!strcmp(aTopic, "xpcom-shutdown-threads")) {
    if (mBackgroundThread) {
      mTerminated = true;
      nsCOMPtr<nsIThread> thread;
      {
        mozilla::MutexAutoLock lock(mLock);
        thread = mBackgroundThread.get();
        mBackgroundThread = nullptr;
      }
      MOZ_ALWAYS_SUCCEEDS(thread->Shutdown());
    }
  }
  return NS_OK;
}

void WindowsSystemProxySettingsAsync::ThreadFunc() {
  nsresult rv;
  nsCOMPtr<nsIWindowsRegKey> regKey =
      do_CreateInstance("@mozilla.org/windows-registry-key;1", &rv);
  if (NS_FAILED(rv)) {
    return;
  }

  rv = regKey->Open(
      nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
      u"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Internet Settings"_ns,
      nsIWindowsRegKey::ACCESS_READ);
  if (NS_FAILED(rv)) {
    return;
  }

  OnProxyConfigChangedInternal();

  rv = regKey->StartWatching(true);
  if (NS_FAILED(rv)) {
    return;
  }

  mInited = true;

  while (!mTerminated) {
    bool changed = false;
    regKey->HasChanged(&changed);
    if (changed) {
      OnProxyConfigChangedInternal();
    }
  }
}

void WindowsSystemProxySettingsAsync::OnProxyConfigChangedInternal() {
  ProxyConfig config;

  // PAC
  nsAutoCString pacUrl;
  if (NS_SUCCEEDED(GetPACURI(pacUrl))) {
    config.SetPACUrl(pacUrl);
  }

  // proxies
  auto flagHandler = [&](uint32_t aFlags) {
    if (!(aFlags & PROXY_TYPE_PROXY)) {
      return false;
    }
    return true;
  };

  // The format of input string is like: scheme=host:port, e.g.
  // "http=127.0.0.1:3128".
  auto processProxyStr = [](const nsCString& aInput,
                            ProxyServer::ProxyType& aOutType,
                            nsACString& aOutHost, int32_t& aOutPort) {
    aOutType = ProxyServer::ProxyType::DEFAULT;
    aOutHost = EmptyCString();
    aOutPort = -1;

    mozilla::Tokenizer t(aInput);
    mozilla::Tokenizer::Token token;
    // skip over spaces
    t.SkipWhites();
    t.Record();

    bool parsingIPv6 = false;
    bool parsingPort = false;
    while (t.Next(token)) {
      if (token.Equals(mozilla::Tokenizer::Token::EndOfFile())) {
        if (aOutHost.IsEmpty()) {
          t.Claim(aOutHost);
        }
        break;
      }

      if (token.Equals(mozilla::Tokenizer::Token::Char('='))) {
        nsAutoCString typeStr;
        t.Claim(typeStr);
        aOutType = ProxyConfig::ToProxyType(typeStr.get());
        t.Record();
      }

      if (token.Equals(mozilla::Tokenizer::Token::Char('['))) {
        parsingIPv6 = true;
        continue;
      }

      if (!parsingIPv6 && token.Equals(mozilla::Tokenizer::Token::Char(':'))) {
        // Port is starting. Claim the previous as host.
        t.Claim(aOutHost);
        t.Record();
        parsingPort = true;
        continue;
      }

      if (token.Equals(mozilla::Tokenizer::Token::Char(']'))) {
        parsingIPv6 = false;
        continue;
      }
    }

    if (parsingPort) {
      nsAutoCString portStr;
      t.Claim(portStr);
      nsresult rv = NS_OK;
      aOutPort = portStr.ToInteger(&rv);
      if (NS_FAILED(rv)) {
        aOutPort = -1;
      }
    }
  };

  auto ruleHandler = [&](const nsACString& aRule, bool& aContinue) {
    const nsCString proxy(aRule);
    aContinue = true;
    ProxyServer::ProxyType type;
    nsCString host;
    int32_t port = -1;
    processProxyStr(proxy, type, host, port);
    if (!host.IsEmpty()) {
      ProxyServer server(type, host, port);
      config.Rules().mProxyServers[server.Type()] = std::move(server);
    }
  };

  // Note that reading the proxy settings from registry directly is not
  // documented by Microsoft, so doing it could be risky. We still use system
  // API to read proxy settings for safe.
  ReadProxyRules(INTERNET_PER_CONN_PROXY_SERVER, flagHandler, ruleHandler);

  auto bypassRuleHandler = [&](const nsACString& aOverrideRule,
                               bool& aContinue) {
    aContinue = true;
    config.ByPassRules().mExceptions.AppendElement(aOverrideRule);
  };

  auto dummyHandler = [](uint32_t aFlags) { return true; };
  ReadProxyRules(INTERNET_PER_CONN_PROXY_BYPASS, dummyHandler,
                 bypassRuleHandler);

  {
    mozilla::MutexAutoLock lock(mLock);
    mConfig = std::move(config);
  }
}

NS_IMETHODIMP
WindowsSystemProxySettingsAsync::GetMainThreadOnly(bool* aMainThreadOnly) {
  return nsWindowsSystemProxySettings::GetMainThreadOnly(aMainThreadOnly);
}

NS_IMETHODIMP WindowsSystemProxySettingsAsync::GetPACURI(nsACString& aResult) {
  AUTO_PROFILER_LABEL("WindowsSystemProxySettingsAsync::GetPACURI", OTHER);
  mozilla::MutexAutoLock lock(mLock);
  aResult.Assign(mConfig.PACUrl());
  return NS_OK;
}

NS_IMETHODIMP WindowsSystemProxySettingsAsync::GetProxyForURI(
    const nsACString& aSpec, const nsACString& aScheme, const nsACString& aHost,
    const int32_t aPort, nsACString& aResult) {
  // Fallback to nsWindowsSystemProxySettings::GetProxyForURI if we failed to
  // monitor the change of registry keys.
  if (!mInited) {
    return nsWindowsSystemProxySettings::GetProxyForURI(aSpec, aScheme, aHost,
                                                        aPort, aResult);
  }

  mozilla::MutexAutoLock lock(mLock);

  for (const auto& bypassRule : mConfig.ByPassRules().mExceptions) {
    if (MatchOverrideInternal(aHost, bypassRule)) {
      ProxyConfig::SetProxyResultDirect(aResult);
      return NS_OK;
    }
  }

  mConfig.GetProxyString(aScheme, aResult);
  return NS_OK;
}

NS_IMPL_COMPONENT_FACTORY(nsWindowsSystemProxySettings) {
  auto settings =
      mozilla::StaticPrefs::network_proxy_detect_system_proxy_changes()
          ? mozilla::MakeRefPtr<WindowsSystemProxySettingsAsync>()
          : mozilla::MakeRefPtr<nsWindowsSystemProxySettings>();
  if (NS_SUCCEEDED(settings->Init())) {
    return settings.forget().downcast<nsISupports>();
  }
  return nullptr;
}
