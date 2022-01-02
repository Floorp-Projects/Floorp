/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProxyAutoConfig.h"
#include "nsICancelable.h"
#include "nsIDNSListener.h"
#include "nsIDNSRecord.h"
#include "nsIDNSService.h"
#include "nsINamed.h"
#include "nsThreadUtils.h"
#include "nsIConsoleService.h"
#include "nsIURLParser.h"
#include "nsJSUtils.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"          // JS_CallFunctionName
#include "js/CompilationAndEvaluation.h"  // JS::Compile
#include "js/ContextOptions.h"
#include "js/Initialization.h"
#include "js/PropertyAndElement.h"  // JS_DefineFunctions, JS_GetProperty
#include "js/PropertySpec.h"
#include "js/SourceText.h"  // JS::Source{Ownership,Text}
#include "js/Utility.h"
#include "js/Warnings.h"  // JS::SetWarningReporter
#include "prnetdb.h"
#include "nsITimer.h"
#include "mozilla/Atomics.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/net/DNS.h"
#include "mozilla/net/SocketProcessChild.h"
#include "mozilla/net/SocketProcessParent.h"
#include "mozilla/net/ProxyAutoConfigChild.h"
#include "mozilla/net/ProxyAutoConfigParent.h"
#include "mozilla/Utf8.h"  // mozilla::Utf8Unit
#include "nsServiceManagerUtils.h"
#include "nsNetCID.h"

#if defined(XP_MACOSX)
#  include "nsMacUtilsImpl.h"
#endif

#include "XPCSelfHostedShmem.h"

namespace mozilla {
namespace net {

// These are some global helper symbols the PAC format requires that we provide
// that are initialized as part of the global javascript context used for PAC
// evaluations. Additionally dnsResolve(host) and myIpAddress() are supplied in
// the same context but are implemented as c++ helpers. alert(msg) is similarly
// defined.
//
// Per ProxyAutoConfig::Init, this data must be ASCII.

static const char sAsciiPacUtils[] =
    "function dnsDomainIs(host, domain) {\n"
    "    return (host.length >= domain.length &&\n"
    "            host.substring(host.length - domain.length) == domain);\n"
    "}\n"
    ""
    "function dnsDomainLevels(host) {\n"
    "    return host.split('.').length - 1;\n"
    "}\n"
    ""
    "function isValidIpAddress(ipchars) {\n"
    "    var matches = "
    "/^(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})$/.exec(ipchars);\n"
    "    if (matches == null) {\n"
    "        return false;\n"
    "    } else if (matches[1] > 255 || matches[2] > 255 || \n"
    "               matches[3] > 255 || matches[4] > 255) {\n"
    "        return false;\n"
    "    }\n"
    "    return true;\n"
    "}\n"
    ""
    "function convert_addr(ipchars) {\n"
    "    var bytes = ipchars.split('.');\n"
    "    var result = ((bytes[0] & 0xff) << 24) |\n"
    "                 ((bytes[1] & 0xff) << 16) |\n"
    "                 ((bytes[2] & 0xff) <<  8) |\n"
    "                  (bytes[3] & 0xff);\n"
    "    return result;\n"
    "}\n"
    ""
    "function isInNet(ipaddr, pattern, maskstr) {\n"
    "    if (!isValidIpAddress(pattern) || !isValidIpAddress(maskstr)) {\n"
    "        return false;\n"
    "    }\n"
    "    if (!isValidIpAddress(ipaddr)) {\n"
    "        ipaddr = dnsResolve(ipaddr);\n"
    "        if (ipaddr == null) {\n"
    "            return false;\n"
    "        }\n"
    "    }\n"
    "    var host = convert_addr(ipaddr);\n"
    "    var pat  = convert_addr(pattern);\n"
    "    var mask = convert_addr(maskstr);\n"
    "    return ((host & mask) == (pat & mask));\n"
    "    \n"
    "}\n"
    ""
    "function isPlainHostName(host) {\n"
    "    return (host.search('(\\\\.)|:') == -1);\n"
    "}\n"
    ""
    "function isResolvable(host) {\n"
    "    var ip = dnsResolve(host);\n"
    "    return (ip != null);\n"
    "}\n"
    ""
    "function localHostOrDomainIs(host, hostdom) {\n"
    "    return (host == hostdom) ||\n"
    "           (hostdom.lastIndexOf(host + '.', 0) == 0);\n"
    "}\n"
    ""
    "function shExpMatch(url, pattern) {\n"
    "   pattern = pattern.replace(/\\./g, '\\\\.');\n"
    "   pattern = pattern.replace(/\\*/g, '.*');\n"
    "   pattern = pattern.replace(/\\?/g, '.');\n"
    "   var newRe = new RegExp('^'+pattern+'$');\n"
    "   return newRe.test(url);\n"
    "}\n"
    ""
    "var wdays = {SUN: 0, MON: 1, TUE: 2, WED: 3, THU: 4, FRI: 5, SAT: 6};\n"
    "var months = {JAN: 0, FEB: 1, MAR: 2, APR: 3, MAY: 4, JUN: 5, JUL: 6, "
    "AUG: 7, SEP: 8, OCT: 9, NOV: 10, DEC: 11};\n"
    ""
    "function weekdayRange() {\n"
    "    function getDay(weekday) {\n"
    "        if (weekday in wdays) {\n"
    "            return wdays[weekday];\n"
    "        }\n"
    "        return -1;\n"
    "    }\n"
    "    var date = new Date();\n"
    "    var argc = arguments.length;\n"
    "    var wday;\n"
    "    if (argc < 1)\n"
    "        return false;\n"
    "    if (arguments[argc - 1] == 'GMT') {\n"
    "        argc--;\n"
    "        wday = date.getUTCDay();\n"
    "    } else {\n"
    "        wday = date.getDay();\n"
    "    }\n"
    "    var wd1 = getDay(arguments[0]);\n"
    "    var wd2 = (argc == 2) ? getDay(arguments[1]) : wd1;\n"
    "    return (wd1 == -1 || wd2 == -1) ? false\n"
    "                                    : (wd1 <= wd2) ? (wd1 <= wday && wday "
    "<= wd2)\n"
    "                                                   : (wd2 >= wday || wday "
    ">= wd1);\n"
    "}\n"
    ""
    "function dateRange() {\n"
    "    function getMonth(name) {\n"
    "        if (name in months) {\n"
    "            return months[name];\n"
    "        }\n"
    "        return -1;\n"
    "    }\n"
    "    var date = new Date();\n"
    "    var argc = arguments.length;\n"
    "    if (argc < 1) {\n"
    "        return false;\n"
    "    }\n"
    "    var isGMT = (arguments[argc - 1] == 'GMT');\n"
    "\n"
    "    if (isGMT) {\n"
    "        argc--;\n"
    "    }\n"
    "    // function will work even without explict handling of this case\n"
    "    if (argc == 1) {\n"
    "        var tmp = parseInt(arguments[0]);\n"
    "        if (isNaN(tmp)) {\n"
    "            return ((isGMT ? date.getUTCMonth() : date.getMonth()) ==\n"
    "                     getMonth(arguments[0]));\n"
    "        } else if (tmp < 32) {\n"
    "            return ((isGMT ? date.getUTCDate() : date.getDate()) == "
    "tmp);\n"
    "        } else { \n"
    "            return ((isGMT ? date.getUTCFullYear() : date.getFullYear()) "
    "==\n"
    "                     tmp);\n"
    "        }\n"
    "    }\n"
    "    var year = date.getFullYear();\n"
    "    var date1, date2;\n"
    "    date1 = new Date(year,  0,  1,  0,  0,  0);\n"
    "    date2 = new Date(year, 11, 31, 23, 59, 59);\n"
    "    var adjustMonth = false;\n"
    "    for (var i = 0; i < (argc >> 1); i++) {\n"
    "        var tmp = parseInt(arguments[i]);\n"
    "        if (isNaN(tmp)) {\n"
    "            var mon = getMonth(arguments[i]);\n"
    "            date1.setMonth(mon);\n"
    "        } else if (tmp < 32) {\n"
    "            adjustMonth = (argc <= 2);\n"
    "            date1.setDate(tmp);\n"
    "        } else {\n"
    "            date1.setFullYear(tmp);\n"
    "        }\n"
    "    }\n"
    "    for (var i = (argc >> 1); i < argc; i++) {\n"
    "        var tmp = parseInt(arguments[i]);\n"
    "        if (isNaN(tmp)) {\n"
    "            var mon = getMonth(arguments[i]);\n"
    "            date2.setMonth(mon);\n"
    "        } else if (tmp < 32) {\n"
    "            date2.setDate(tmp);\n"
    "        } else {\n"
    "            date2.setFullYear(tmp);\n"
    "        }\n"
    "    }\n"
    "    if (adjustMonth) {\n"
    "        date1.setMonth(date.getMonth());\n"
    "        date2.setMonth(date.getMonth());\n"
    "    }\n"
    "    if (isGMT) {\n"
    "    var tmp = date;\n"
    "        tmp.setFullYear(date.getUTCFullYear());\n"
    "        tmp.setMonth(date.getUTCMonth());\n"
    "        tmp.setDate(date.getUTCDate());\n"
    "        tmp.setHours(date.getUTCHours());\n"
    "        tmp.setMinutes(date.getUTCMinutes());\n"
    "        tmp.setSeconds(date.getUTCSeconds());\n"
    "        date = tmp;\n"
    "    }\n"
    "    return (date1 <= date2) ? (date1 <= date) && (date <= date2)\n"
    "                            : (date2 >= date) || (date >= date1);\n"
    "}\n"
    ""
    "function timeRange() {\n"
    "    var argc = arguments.length;\n"
    "    var date = new Date();\n"
    "    var isGMT= false;\n"
    ""
    "    if (argc < 1) {\n"
    "        return false;\n"
    "    }\n"
    "    if (arguments[argc - 1] == 'GMT') {\n"
    "        isGMT = true;\n"
    "        argc--;\n"
    "    }\n"
    "\n"
    "    var hour = isGMT ? date.getUTCHours() : date.getHours();\n"
    "    var date1, date2;\n"
    "    date1 = new Date();\n"
    "    date2 = new Date();\n"
    "\n"
    "    if (argc == 1) {\n"
    "        return (hour == arguments[0]);\n"
    "    } else if (argc == 2) {\n"
    "        return ((arguments[0] <= hour) && (hour <= arguments[1]));\n"
    "    } else {\n"
    "        switch (argc) {\n"
    "        case 6:\n"
    "            date1.setSeconds(arguments[2]);\n"
    "            date2.setSeconds(arguments[5]);\n"
    "        case 4:\n"
    "            var middle = argc >> 1;\n"
    "            date1.setHours(arguments[0]);\n"
    "            date1.setMinutes(arguments[1]);\n"
    "            date2.setHours(arguments[middle]);\n"
    "            date2.setMinutes(arguments[middle + 1]);\n"
    "            if (middle == 2) {\n"
    "                date2.setSeconds(59);\n"
    "            }\n"
    "            break;\n"
    "        default:\n"
    "          throw 'timeRange: bad number of arguments'\n"
    "        }\n"
    "    }\n"
    "\n"
    "    if (isGMT) {\n"
    "        date.setFullYear(date.getUTCFullYear());\n"
    "        date.setMonth(date.getUTCMonth());\n"
    "        date.setDate(date.getUTCDate());\n"
    "        date.setHours(date.getUTCHours());\n"
    "        date.setMinutes(date.getUTCMinutes());\n"
    "        date.setSeconds(date.getUTCSeconds());\n"
    "    }\n"
    "    return (date1 <= date2) ? (date1 <= date) && (date <= date2)\n"
    "                            : (date2 >= date) || (date >= date1);\n"
    "\n"
    "}\n"
    "";

// sRunning is defined for the helper functions only while the
// Javascript engine is running and the PAC object cannot be deleted
// or reset.
static Atomic<uint32_t, Relaxed>& RunningIndex() {
  static Atomic<uint32_t, Relaxed> sRunningIndex(0xdeadbeef);
  return sRunningIndex;
}
static ProxyAutoConfig* GetRunning() {
  MOZ_ASSERT(RunningIndex() != 0xdeadbeef);
  return static_cast<ProxyAutoConfig*>(PR_GetThreadPrivate(RunningIndex()));
}

static void SetRunning(ProxyAutoConfig* arg) {
  MOZ_ASSERT(RunningIndex() != 0xdeadbeef);
  PR_SetThreadPrivate(RunningIndex(), arg);
}

// The PACResolver is used for dnsResolve()
class PACResolver final : public nsIDNSListener,
                          public nsITimerCallback,
                          public nsINamed {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit PACResolver(nsIEventTarget* aTarget)
      : mStatus(NS_ERROR_FAILURE),
        mMainThreadEventTarget(aTarget),
        mMutex("PACResolver::Mutex") {}

  // nsIDNSListener
  NS_IMETHOD OnLookupComplete(nsICancelable* request, nsIDNSRecord* record,
                              nsresult status) override {
    nsCOMPtr<nsITimer> timer;
    {
      MutexAutoLock lock(mMutex);
      timer.swap(mTimer);
      mRequest = nullptr;
    }

    if (timer) {
      timer->Cancel();
    }

    mStatus = status;
    mResponse = record;
    return NS_OK;
  }

  // nsITimerCallback
  NS_IMETHOD Notify(nsITimer* timer) override {
    nsCOMPtr<nsICancelable> request;
    {
      MutexAutoLock lock(mMutex);
      request.swap(mRequest);
      mTimer = nullptr;
    }
    if (request) {
      request->Cancel(NS_ERROR_NET_TIMEOUT);
    }
    return NS_OK;
  }

  // nsINamed
  NS_IMETHOD GetName(nsACString& aName) override {
    aName.AssignLiteral("PACResolver");
    return NS_OK;
  }

  nsresult mStatus;
  nsCOMPtr<nsICancelable> mRequest;
  nsCOMPtr<nsIDNSRecord> mResponse;
  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<nsIEventTarget> mMainThreadEventTarget;
  Mutex mMutex;

 private:
  ~PACResolver() = default;
};
NS_IMPL_ISUPPORTS(PACResolver, nsIDNSListener, nsITimerCallback, nsINamed)

static void PACLogToConsole(nsString& aMessage) {
  if (XRE_IsSocketProcess()) {
    auto task = [message(aMessage)]() {
      SocketProcessChild* child = SocketProcessChild::GetSingleton();
      if (child) {
        Unused << child->SendOnConsoleMessage(message);
      }
    };
    if (NS_IsMainThread()) {
      task();
    } else {
      NS_DispatchToMainThread(NS_NewRunnableFunction("PACLogToConsole", task));
    }
    return;
  }

  nsCOMPtr<nsIConsoleService> consoleService =
      do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  if (!consoleService) return;

  consoleService->LogStringMessage(aMessage.get());
}

// Javascript errors and warnings are logged to the main error console
static void PACLogErrorOrWarning(const nsAString& aKind,
                                 JSErrorReport* aReport) {
  nsString formattedMessage(u"PAC Execution "_ns);
  formattedMessage += aKind;
  formattedMessage += u": "_ns;
  if (aReport->message()) {
    formattedMessage.Append(NS_ConvertUTF8toUTF16(aReport->message().c_str()));
  }
  formattedMessage += u" ["_ns;
  formattedMessage.Append(aReport->linebuf(), aReport->linebufLength());
  formattedMessage += u"]"_ns;
  PACLogToConsole(formattedMessage);
}

static void PACWarningReporter(JSContext* aCx, JSErrorReport* aReport) {
  MOZ_ASSERT(aReport);
  MOZ_ASSERT(aReport->isWarning());

  PACLogErrorOrWarning(u"Warning"_ns, aReport);
}

class MOZ_STACK_CLASS AutoPACErrorReporter {
  JSContext* mCx;

 public:
  explicit AutoPACErrorReporter(JSContext* aCx) : mCx(aCx) {}
  ~AutoPACErrorReporter() {
    if (!JS_IsExceptionPending(mCx)) {
      return;
    }
    JS::ExceptionStack exnStack(mCx);
    if (!JS::StealPendingExceptionStack(mCx, &exnStack)) {
      return;
    }

    JS::ErrorReportBuilder report(mCx);
    if (!report.init(mCx, exnStack, JS::ErrorReportBuilder::WithSideEffects)) {
      JS_ClearPendingException(mCx);
      return;
    }

    PACLogErrorOrWarning(u"Error"_ns, report.report());
  }
};

// timeout of 0 means the normal necko timeout strategy, otherwise the dns
// request will be canceled after aTimeout milliseconds
static bool PACResolve(const nsCString& aHostName, NetAddr* aNetAddr,
                       unsigned int aTimeout) {
  if (!GetRunning()) {
    NS_WARNING("PACResolve without a running ProxyAutoConfig object");
    return false;
  }

  return GetRunning()->ResolveAddress(aHostName, aNetAddr, aTimeout);
}

ProxyAutoConfig::ProxyAutoConfig()

{
  MOZ_COUNT_CTOR(ProxyAutoConfig);
}

bool ProxyAutoConfig::ResolveAddress(const nsCString& aHostName,
                                     NetAddr* aNetAddr, unsigned int aTimeout) {
  nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID);
  if (!dns) return false;

  RefPtr<PACResolver> helper = new PACResolver(mMainThreadEventTarget);
  OriginAttributes attrs;

  // When the PAC script attempts to resolve a domain, we must make sure we
  // don't use TRR, otherwise the TRR channel might also attempt to resolve
  // a name and we'll have a deadlock.
  uint32_t flags =
      nsIDNSService::RESOLVE_PRIORITY_MEDIUM |
      nsIDNSService::GetFlagsFromTRRMode(nsIRequest::TRR_DISABLED_MODE);

  if (NS_FAILED(dns->AsyncResolveNative(
          aHostName, nsIDNSService::RESOLVE_TYPE_DEFAULT, flags, nullptr,
          helper, GetCurrentEventTarget(), attrs,
          getter_AddRefs(helper->mRequest)))) {
    return false;
  }

  if (aTimeout && helper->mRequest) {
    if (!mTimer) mTimer = NS_NewTimer();
    if (mTimer) {
      mTimer->SetTarget(mMainThreadEventTarget);
      mTimer->InitWithCallback(helper, aTimeout, nsITimer::TYPE_ONE_SHOT);
      helper->mTimer = mTimer;
    }
  }

  mWaitingForDNSResolve = true;
  // Spin the event loop of the pac thread until lookup is complete.
  // nsPACman is responsible for keeping a queue and only allowing
  // one PAC execution at a time even when it is called re-entrantly.
  SpinEventLoopUntil("ProxyAutoConfig::ResolveAddress"_ns, [&, helper, this]() {
    if (!helper->mRequest) {
      mWaitingForDNSResolve = false;
      return true;
    }
    if (this->mShutdown) {
      NS_WARNING("mShutdown set with PAC request not cancelled");
      MOZ_ASSERT(NS_FAILED(helper->mStatus));
      mWaitingForDNSResolve = false;
      return true;
    }
    return false;
  });

  if (NS_FAILED(helper->mStatus)) {
    return false;
  }

  nsCOMPtr<nsIDNSAddrRecord> rec = do_QueryInterface(helper->mResponse);
  return !(!rec || NS_FAILED(rec->GetNextAddr(0, aNetAddr)));
}

static bool PACResolveToString(const nsCString& aHostName,
                               nsCString& aDottedDecimal,
                               unsigned int aTimeout) {
  NetAddr netAddr;
  if (!PACResolve(aHostName, &netAddr, aTimeout)) return false;

  char dottedDecimal[128];
  if (!netAddr.ToStringBuffer(dottedDecimal, sizeof(dottedDecimal))) {
    return false;
  }

  aDottedDecimal.Assign(dottedDecimal);
  return true;
}

// dnsResolve(host) javascript implementation
static bool PACDnsResolve(JSContext* cx, unsigned int argc, JS::Value* vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);

  if (NS_IsMainThread()) {
    NS_WARNING("DNS Resolution From PAC on Main Thread. How did that happen?");
    return false;
  }

  if (!args.requireAtLeast(cx, "dnsResolve", 1)) return false;

  // Previously we didn't check the type of the argument, so just converted it
  // to string. A badly written PAC file oculd pass null or undefined here
  // which could lead to odd results if there are any hosts called "null"
  // on the network. See bug 1724345 comment 6.
  if (!args[0].isString()) {
    args.rval().setNull();
    return true;
  }

  JS::RootedString arg1(cx);
  arg1 = args[0].toString();

  nsAutoJSString hostName;
  nsAutoCString dottedDecimal;

  if (!hostName.init(cx, arg1)) return false;
  if (PACResolveToString(NS_ConvertUTF16toUTF8(hostName), dottedDecimal, 0)) {
    JSString* dottedDecimalString = JS_NewStringCopyZ(cx, dottedDecimal.get());
    if (!dottedDecimalString) {
      return false;
    }

    args.rval().setString(dottedDecimalString);
  } else {
    args.rval().setNull();
  }

  return true;
}

// myIpAddress() javascript implementation
static bool PACMyIpAddress(JSContext* cx, unsigned int argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

  if (NS_IsMainThread()) {
    NS_WARNING("DNS Resolution From PAC on Main Thread. How did that happen?");
    return false;
  }

  if (!GetRunning()) {
    NS_WARNING("PAC myIPAddress without a running ProxyAutoConfig object");
    return false;
  }

  return GetRunning()->MyIPAddress(args);
}

// proxyAlert(msg) javascript implementation
static bool PACProxyAlert(JSContext* cx, unsigned int argc, JS::Value* vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);

  if (!args.requireAtLeast(cx, "alert", 1)) return false;

  JS::Rooted<JSString*> arg1(cx, JS::ToString(cx, args[0]));
  if (!arg1) return false;

  nsAutoJSString message;
  if (!message.init(cx, arg1)) return false;

  nsAutoString alertMessage;
  alertMessage.AssignLiteral(u"PAC-alert: ");
  alertMessage.Append(message);
  PACLogToConsole(alertMessage);

  args.rval().setUndefined(); /* return undefined */
  return true;
}

static const JSFunctionSpec PACGlobalFunctions[] = {
    JS_FN("dnsResolve", PACDnsResolve, 1, 0),

    // a global "var pacUseMultihomedDNS = true;" will change behavior
    // of myIpAddress to actively use DNS
    JS_FN("myIpAddress", PACMyIpAddress, 0, 0),
    JS_FN("alert", PACProxyAlert, 1, 0), JS_FS_END};

// JSContextWrapper is a c++ object that manages the context for the JS engine
// used on the PAC thread. It is initialized and destroyed on the PAC thread.
class JSContextWrapper {
 public:
  static JSContextWrapper* Create(uint32_t aExtraHeapSize) {
    JSContext* cx = JS_NewContext(JS::DefaultHeapMaxBytes + aExtraHeapSize);
    if (NS_WARN_IF(!cx)) return nullptr;

    JS::ContextOptionsRef(cx).setDisableIon().setDisableEvalSecurityChecks();

    JSContextWrapper* entry = new JSContextWrapper(cx);
    if (NS_FAILED(entry->Init())) {
      delete entry;
      return nullptr;
    }

    return entry;
  }

  JSContext* Context() const { return mContext; }

  JSObject* Global() const { return mGlobal; }

  ~JSContextWrapper() {
    mGlobal = nullptr;

    MOZ_COUNT_DTOR(JSContextWrapper);

    if (mContext) {
      JS_DestroyContext(mContext);
    }
  }

  void SetOK() { mOK = true; }

  bool IsOK() { return mOK; }

 private:
  JSContext* mContext;
  JS::PersistentRooted<JSObject*> mGlobal;
  bool mOK;

  static const JSClass sGlobalClass;

  explicit JSContextWrapper(JSContext* cx)
      : mContext(cx), mGlobal(cx, nullptr), mOK(false) {
    MOZ_COUNT_CTOR(JSContextWrapper);
  }

  nsresult Init() {
    /*
     * Not setting this will cause JS_CHECK_RECURSION to report false
     * positives
     */
    JS_SetNativeStackQuota(mContext, 128 * sizeof(size_t) * 1024);

    JS::SetWarningReporter(mContext, PACWarningReporter);

    // When available, set the self-hosted shared memory to be read, so that
    // we can decode the self-hosted content instead of parsing it.
    {
      auto& shm = xpc::SelfHostedShmem::GetSingleton();
      JS::SelfHostedCache selfHostedContent = shm.Content();

      if (!JS::InitSelfHostedCode(mContext, selfHostedContent)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }

    JS::RealmOptions options;
    options.creationOptions().setNewCompartmentInSystemZone();
    options.behaviors().setClampAndJitterTime(false);
    mGlobal = JS_NewGlobalObject(mContext, &sGlobalClass, nullptr,
                                 JS::DontFireOnNewGlobalHook, options);
    if (!mGlobal) {
      JS_ClearPendingException(mContext);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    JS::Rooted<JSObject*> global(mContext, mGlobal);

    JSAutoRealm ar(mContext, global);
    AutoPACErrorReporter aper(mContext);
    if (!JS_DefineFunctions(mContext, global, PACGlobalFunctions)) {
      return NS_ERROR_FAILURE;
    }

    JS_FireOnNewGlobalObject(mContext, global);

    return NS_OK;
  }
};

const JSClass JSContextWrapper::sGlobalClass = {"PACResolutionThreadGlobal",
                                                JSCLASS_GLOBAL_FLAGS,
                                                &JS::DefaultGlobalClassOps};

void ProxyAutoConfig::SetThreadLocalIndex(uint32_t index) {
  RunningIndex() = index;
}

nsresult ProxyAutoConfig::ConfigurePAC(const nsCString& aPACURI,
                                       const nsCString& aPACScriptData,
                                       bool aIncludePath,
                                       uint32_t aExtraHeapSize,
                                       nsIEventTarget* aEventTarget) {
  mShutdown = false;  // Shutdown needs to be called prior to destruction

  mPACURI = aPACURI;

  // The full PAC script data is the concatenation of 1) the various functions
  // exposed to PAC scripts in |sAsciiPacUtils| and 2) the user-provided PAC
  // script data.  Historically this was single-byte Latin-1 text (usually just
  // ASCII, but bug 296163 has a real-world Latin-1 example).  We now support
  // UTF-8 if the full data validates as UTF-8, before falling back to Latin-1.
  // (Technically this is a breaking change: intentional Latin-1 scripts that
  // happen to be valid UTF-8 may have different behavior.  We assume such cases
  // are vanishingly rare.)
  //
  // Supporting both UTF-8 and Latin-1 requires that the functions exposed to
  // PAC scripts be both UTF-8- and Latin-1-compatible: that is, they must be
  // ASCII.
  mConcatenatedPACData = sAsciiPacUtils;
  mConcatenatedPACData.Append(aPACScriptData);

  mIncludePath = aIncludePath;
  mExtraHeapSize = aExtraHeapSize;
  mMainThreadEventTarget = aEventTarget;

  if (!GetRunning()) return SetupJS();

  mJSNeedsSetup = true;
  return NS_OK;
}

nsresult ProxyAutoConfig::SetupJS() {
  mJSNeedsSetup = false;
  MOZ_ASSERT(!GetRunning(), "JIT is running");

#if defined(XP_MACOSX)
  nsMacUtilsImpl::EnableTCSMIfAvailable();
#endif

  delete mJSContext;
  mJSContext = nullptr;

  if (mConcatenatedPACData.IsEmpty()) return NS_ERROR_FAILURE;

  NS_GetCurrentThread()->SetCanInvokeJS(true);

  mJSContext = JSContextWrapper::Create(mExtraHeapSize);
  if (!mJSContext) return NS_ERROR_FAILURE;

  JSContext* cx = mJSContext->Context();
  JSAutoRealm ar(cx, mJSContext->Global());
  AutoPACErrorReporter aper(cx);

  // check if this is a data: uri so that we don't spam the js console with
  // huge meaningless strings. this is not on the main thread, so it can't
  // use nsIURI scheme methods
  bool isDataURI =
      nsDependentCSubstring(mPACURI, 0, 5).LowerCaseEqualsASCII("data:", 5);

  SetRunning(this);

  JS::Rooted<JSObject*> global(cx, mJSContext->Global());

  auto CompilePACScript = [this](JSContext* cx) -> JSScript* {
    JS::CompileOptions options(cx);
    options.setSkipFilenameValidation(true);
    options.setFileAndLine(this->mPACURI.get(), 1);

    // Per ProxyAutoConfig::Init, compile as UTF-8 if the full data is UTF-8,
    // and otherwise inflate Latin-1 to UTF-16 and compile that.
    const char* scriptData = this->mConcatenatedPACData.get();
    size_t scriptLength = this->mConcatenatedPACData.Length();
    if (mozilla::IsUtf8(mozilla::Span(scriptData, scriptLength))) {
      JS::SourceText<Utf8Unit> srcBuf;
      if (!srcBuf.init(cx, scriptData, scriptLength,
                       JS::SourceOwnership::Borrowed)) {
        return nullptr;
      }

      return JS::Compile(cx, options, srcBuf);
    }

    // nsReadableUtils.h says that "ASCII" is a misnomer "for legacy reasons",
    // and this handles not just ASCII but Latin-1 too.
    NS_ConvertASCIItoUTF16 inflated(this->mConcatenatedPACData);

    JS::SourceText<char16_t> source;
    if (!source.init(cx, inflated.get(), inflated.Length(),
                     JS::SourceOwnership::Borrowed)) {
      return nullptr;
    }

    return JS::Compile(cx, options, source);
  };

  JS::Rooted<JSScript*> script(cx, CompilePACScript(cx));
  if (!script || !JS_ExecuteScript(cx, script)) {
    nsString alertMessage(u"PAC file failed to install from "_ns);
    if (isDataURI) {
      alertMessage += u"data: URI"_ns;
    } else {
      alertMessage += NS_ConvertUTF8toUTF16(mPACURI);
    }
    PACLogToConsole(alertMessage);
    SetRunning(nullptr);
    return NS_ERROR_FAILURE;
  }
  SetRunning(nullptr);

  mJSContext->SetOK();
  nsString alertMessage(u"PAC file installed from "_ns);
  if (isDataURI) {
    alertMessage += u"data: URI"_ns;
  } else {
    alertMessage += NS_ConvertUTF8toUTF16(mPACURI);
  }
  PACLogToConsole(alertMessage);

  // we don't need these now
  mConcatenatedPACData.Truncate();
  mPACURI.Truncate();

  return NS_OK;
}

void ProxyAutoConfig::GetProxyForURIWithCallback(
    const nsCString& aTestURI, const nsCString& aTestHost,
    std::function<void(nsresult aStatus, const nsACString& aResult)>&&
        aCallback) {
  nsAutoCString result;
  nsresult status = GetProxyForURI(aTestURI, aTestHost, result);
  aCallback(status, result);
}

nsresult ProxyAutoConfig::GetProxyForURI(const nsCString& aTestURI,
                                         const nsCString& aTestHost,
                                         nsACString& result) {
  if (mJSNeedsSetup) SetupJS();

  if (!mJSContext || !mJSContext->IsOK()) return NS_ERROR_NOT_AVAILABLE;

  JSContext* cx = mJSContext->Context();
  JSAutoRealm ar(cx, mJSContext->Global());
  AutoPACErrorReporter aper(cx);

  // the sRunning flag keeps a new PAC file from being installed
  // while the event loop is spinning on a DNS function. Don't early return.
  SetRunning(this);
  mRunningHost = aTestHost;

  nsresult rv = NS_ERROR_FAILURE;
  nsCString clensedURI = aTestURI;

  if (!mIncludePath) {
    nsCOMPtr<nsIURLParser> urlParser =
        do_GetService(NS_STDURLPARSER_CONTRACTID);
    int32_t pathLen = 0;
    if (urlParser) {
      uint32_t schemePos;
      int32_t schemeLen;
      uint32_t authorityPos;
      int32_t authorityLen;
      uint32_t pathPos;
      rv = urlParser->ParseURL(aTestURI.get(), aTestURI.Length(), &schemePos,
                               &schemeLen, &authorityPos, &authorityLen,
                               &pathPos, &pathLen);
    }
    if (NS_SUCCEEDED(rv)) {
      if (pathLen) {
        // cut off the path but leave the initial slash
        pathLen--;
      }
      aTestURI.Left(clensedURI, aTestURI.Length() - pathLen);
    }
  }

  JS::RootedString uriString(cx, JS_NewStringCopyZ(cx, clensedURI.get()));
  JS::RootedString hostString(cx, JS_NewStringCopyZ(cx, aTestHost.get()));

  if (uriString && hostString) {
    JS::RootedValueArray<2> args(cx);
    args[0].setString(uriString);
    args[1].setString(hostString);

    JS::Rooted<JS::Value> rval(cx);
    JS::Rooted<JSObject*> global(cx, mJSContext->Global());
    bool ok = JS_CallFunctionName(cx, global, "FindProxyForURL", args, &rval);

    if (ok && rval.isString()) {
      nsAutoJSString pacString;
      if (pacString.init(cx, rval.toString())) {
        CopyUTF16toUTF8(pacString, result);
        rv = NS_OK;
      }
    }
  }

  mRunningHost.Truncate();
  SetRunning(nullptr);
  return rv;
}

void ProxyAutoConfig::GC() {
  if (!mJSContext || !mJSContext->IsOK()) return;

  JSAutoRealm ar(mJSContext->Context(), mJSContext->Global());
  JS_MaybeGC(mJSContext->Context());
}

ProxyAutoConfig::~ProxyAutoConfig() {
  MOZ_COUNT_DTOR(ProxyAutoConfig);
  MOZ_ASSERT(mShutdown, "Shutdown must be called before dtor.");
  NS_ASSERTION(!mJSContext,
               "~ProxyAutoConfig leaking JS context that "
               "should have been deleted on pac thread");
}

void ProxyAutoConfig::Shutdown() {
  MOZ_ASSERT(!NS_IsMainThread(), "wrong thread for shutdown");

  if (NS_WARN_IF(GetRunning()) || mShutdown) {
    return;
  }

  mShutdown = true;
  delete mJSContext;
  mJSContext = nullptr;
}

bool ProxyAutoConfig::SrcAddress(const NetAddr* remoteAddress,
                                 nsCString& localAddress) {
  PRFileDesc* fd;
  fd = PR_OpenUDPSocket(remoteAddress->raw.family);
  if (!fd) return false;

  PRNetAddr prRemoteAddress;
  NetAddrToPRNetAddr(remoteAddress, &prRemoteAddress);
  if (PR_Connect(fd, &prRemoteAddress, 0) != PR_SUCCESS) {
    PR_Close(fd);
    return false;
  }

  PRNetAddr localName;
  if (PR_GetSockName(fd, &localName) != PR_SUCCESS) {
    PR_Close(fd);
    return false;
  }

  PR_Close(fd);

  char dottedDecimal[128];
  if (PR_NetAddrToString(&localName, dottedDecimal, sizeof(dottedDecimal)) !=
      PR_SUCCESS) {
    return false;
  }

  localAddress.Assign(dottedDecimal);

  return true;
}

// hostName is run through a dns lookup and then a udp socket is connected
// to the result. If that all works, the local IP address of the socket is
// returned to the javascript caller and |*aResult| is set to true. Otherwise
// |*aResult| is set to false.
bool ProxyAutoConfig::MyIPAddressTryHost(const nsCString& hostName,
                                         unsigned int timeout,
                                         const JS::CallArgs& aArgs,
                                         bool* aResult) {
  *aResult = false;

  NetAddr remoteAddress;
  nsAutoCString localDottedDecimal;
  JSContext* cx = mJSContext->Context();

  if (PACResolve(hostName, &remoteAddress, timeout) &&
      SrcAddress(&remoteAddress, localDottedDecimal)) {
    JSString* dottedDecimalString =
        JS_NewStringCopyZ(cx, localDottedDecimal.get());
    if (!dottedDecimalString) {
      return false;
    }

    *aResult = true;
    aArgs.rval().setString(dottedDecimalString);
  }
  return true;
}

bool ProxyAutoConfig::MyIPAddress(const JS::CallArgs& aArgs) {
  nsAutoCString remoteDottedDecimal;
  nsAutoCString localDottedDecimal;
  JSContext* cx = mJSContext->Context();
  JS::RootedValue v(cx);
  JS::Rooted<JSObject*> global(cx, mJSContext->Global());

  bool useMultihomedDNS =
      JS_GetProperty(cx, global, "pacUseMultihomedDNS", &v) &&
      !v.isUndefined() && ToBoolean(v);

  // first, lookup the local address of a socket connected
  // to the host of uri being resolved by the pac file. This is
  // v6 safe.. but is the last step like that
  bool rvalAssigned = false;
  if (useMultihomedDNS) {
    if (!MyIPAddressTryHost(mRunningHost, kTimeout, aArgs, &rvalAssigned) ||
        rvalAssigned) {
      return rvalAssigned;
    }
  } else {
    // we can still do the fancy multi homing thing if the host is a literal
    if (HostIsIPLiteral(mRunningHost) &&
        (!MyIPAddressTryHost(mRunningHost, kTimeout, aArgs, &rvalAssigned) ||
         rvalAssigned)) {
      return rvalAssigned;
    }
  }

  // next, look for a route to a public internet address that doesn't need DNS.
  // This is the google anycast dns address, but it doesn't matter if it
  // remains operable (as we don't contact it) as long as the address stays
  // in commonly routed IP address space.
  remoteDottedDecimal.AssignLiteral("8.8.8.8");
  if (!MyIPAddressTryHost(remoteDottedDecimal, 0, aArgs, &rvalAssigned) ||
      rvalAssigned) {
    return rvalAssigned;
  }

  // finally, use the old algorithm based on the local hostname
  nsAutoCString hostName;
  nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID);
  // without multihomedDNS use such a short timeout that we are basically
  // just looking at the cache for raw dotted decimals
  uint32_t timeout = useMultihomedDNS ? kTimeout : 1;
  if (dns && NS_SUCCEEDED(dns->GetMyHostName(hostName)) &&
      PACResolveToString(hostName, localDottedDecimal, timeout)) {
    JSString* dottedDecimalString =
        JS_NewStringCopyZ(cx, localDottedDecimal.get());
    if (!dottedDecimalString) {
      return false;
    }

    aArgs.rval().setString(dottedDecimalString);
    return true;
  }

  // next try a couple RFC 1918 variants.. maybe there is a
  // local route
  remoteDottedDecimal.AssignLiteral("192.168.0.1");
  if (!MyIPAddressTryHost(remoteDottedDecimal, 0, aArgs, &rvalAssigned) ||
      rvalAssigned) {
    return rvalAssigned;
  }

  // more RFC 1918
  remoteDottedDecimal.AssignLiteral("10.0.0.1");
  if (!MyIPAddressTryHost(remoteDottedDecimal, 0, aArgs, &rvalAssigned) ||
      rvalAssigned) {
    return rvalAssigned;
  }

  // who knows? let's fallback to localhost
  localDottedDecimal.AssignLiteral("127.0.0.1");
  JSString* dottedDecimalString =
      JS_NewStringCopyZ(cx, localDottedDecimal.get());
  if (!dottedDecimalString) {
    return false;
  }

  aArgs.rval().setString(dottedDecimalString);
  return true;
}

RemoteProxyAutoConfig::RemoteProxyAutoConfig() = default;

RemoteProxyAutoConfig::~RemoteProxyAutoConfig() = default;

nsresult RemoteProxyAutoConfig::Init(nsIThread* aPACThread) {
  MOZ_ASSERT(NS_IsMainThread());

  SocketProcessParent* socketProcessParent =
      SocketProcessParent::GetSingleton();
  if (!socketProcessParent) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  ipc::Endpoint<PProxyAutoConfigParent> parent;
  ipc::Endpoint<PProxyAutoConfigChild> child;
  nsresult rv = PProxyAutoConfig::CreateEndpoints(
      base::GetCurrentProcId(), socketProcessParent->OtherPid(), &parent,
      &child);
  if (NS_FAILED(rv)) {
    return rv;
  }

  Unused << socketProcessParent->SendInitProxyAutoConfigChild(std::move(child));
  mProxyAutoConfigParent = new ProxyAutoConfigParent();
  return aPACThread->Dispatch(
      NS_NewRunnableFunction("ProxyAutoConfigParent::ProxyAutoConfigParent",
                             [proxyAutoConfigParent(mProxyAutoConfigParent),
                              endpoint{std::move(parent)}]() mutable {
                               proxyAutoConfigParent->Init(std::move(endpoint));
                             }));
}

nsresult RemoteProxyAutoConfig::ConfigurePAC(const nsCString& aPACURI,
                                             const nsCString& aPACScriptData,
                                             bool aIncludePath,
                                             uint32_t aExtraHeapSize,
                                             nsIEventTarget*) {
  Unused << mProxyAutoConfigParent->SendConfigurePAC(
      aPACURI, aPACScriptData, aIncludePath, aExtraHeapSize);
  return NS_OK;
}

void RemoteProxyAutoConfig::Shutdown() { mProxyAutoConfigParent->Close(); }

void RemoteProxyAutoConfig::GC() { Unused << mProxyAutoConfigParent->SendGC(); }

void RemoteProxyAutoConfig::GetProxyForURIWithCallback(
    const nsCString& aTestURI, const nsCString& aTestHost,
    std::function<void(nsresult aStatus, const nsACString& aResult)>&&
        aCallback) {
  if (!mProxyAutoConfigParent->CanSend()) {
    return;
  }

  mProxyAutoConfigParent->SendGetProxyForURI(
      aTestURI, aTestHost,
      [aCallback](Tuple<nsresult, nsCString>&& aResult) {
        nsresult status;
        nsCString result;
        Tie(status, result) = aResult;
        aCallback(status, result);
      },
      [aCallback](mozilla::ipc::ResponseRejectReason&& aReason) {
        aCallback(NS_ERROR_FAILURE, ""_ns);
      });
}

}  // namespace net
}  // namespace mozilla
