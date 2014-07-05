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
#include "nsThreadUtils.h"
#include "nsIConsoleService.h"
#include "nsJSUtils.h"
#include "jsfriendapi.h"
#include "prnetdb.h"
#include "nsITimer.h"
#include "mozilla/net/DNS.h"
#include "nsServiceManagerUtils.h"
#include "nsNetCID.h"

namespace mozilla {
namespace net {

// These are some global helper symbols the PAC format requires that we provide that
// are initialized as part of the global javascript context used for PAC evaluations.
// Additionally dnsResolve(host) and myIpAddress() are supplied in the same context
// but are implemented as c++ helpers. alert(msg) is similarly defined.

static const char *sPacUtils =
  "function dnsDomainIs(host, domain) {\n"
  "    return (host.length >= domain.length &&\n"
  "            host.substring(host.length - domain.length) == domain);\n"
  "}\n"
  ""
  "function dnsDomainLevels(host) {\n"
  "    return host.split('.').length - 1;\n"
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
  "    var test = /^(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})$/.exec(ipaddr);\n"
  "    if (test == null) {\n"
  "        ipaddr = dnsResolve(ipaddr);\n"
  "        if (ipaddr == null)\n"
  "            return false;\n"
  "    } else if (test[1] > 255 || test[2] > 255 || \n"
  "               test[3] > 255 || test[4] > 255) {\n"
  "        return false;    // not an IP address\n"
  "    }\n"
  "    var host = convert_addr(ipaddr);\n"
  "    var pat  = convert_addr(pattern);\n"
  "    var mask = convert_addr(maskstr);\n"
  "    return ((host & mask) == (pat & mask));\n"
  "    \n"
  "}\n"
  ""
  "function isPlainHostName(host) {\n"
  "    return (host.search('\\\\.') == -1);\n"
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
  "var months = {JAN: 0, FEB: 1, MAR: 2, APR: 3, MAY: 4, JUN: 5, JUL: 6, AUG: 7, SEP: 8, OCT: 9, NOV: 10, DEC: 11};\n"
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
  "                                    : (wd1 <= wday && wday <= wd2);\n"
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
  "            return ((isGMT ? date.getUTCDate() : date.getDate()) == tmp);\n"
  "        } else { \n"
  "            return ((isGMT ? date.getUTCFullYear() : date.getFullYear()) ==\n"
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
  "    return ((date1 <= date) && (date <= date2));\n"
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
  "    return ((date1 <= date) && (date <= date2));\n"
  "}\n"
  "";

// sRunning is defined for the helper functions only while the
// Javascript engine is running and the PAC object cannot be deleted
// or reset.
static ProxyAutoConfig *sRunning = nullptr;

// The PACResolver is used for dnsResolve()
class PACResolver MOZ_FINAL : public nsIDNSListener
                            , public nsITimerCallback
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  PACResolver()
    : mStatus(NS_ERROR_FAILURE)
  {
  }

  // nsIDNSListener
  NS_IMETHODIMP OnLookupComplete(nsICancelable *request,
                                 nsIDNSRecord *record,
                                 nsresult status)
  {
    if (mTimer) {
      mTimer->Cancel();
      mTimer = nullptr;
    }

    mRequest = nullptr;
    mStatus = status;
    mResponse = record;
    return NS_OK;
  }

  // nsITimerCallback
  NS_IMETHODIMP Notify(nsITimer *timer) 
  {
    if (mRequest)
      mRequest->Cancel(NS_ERROR_NET_TIMEOUT);
    mTimer = nullptr;
    return NS_OK;
  }

  nsresult                mStatus;
  nsCOMPtr<nsICancelable> mRequest;
  nsCOMPtr<nsIDNSRecord>  mResponse;
  nsCOMPtr<nsITimer>      mTimer;

private:
  ~PACResolver() {}
};
NS_IMPL_ISUPPORTS(PACResolver, nsIDNSListener, nsITimerCallback)

static
void PACLogToConsole(nsString &aMessage)
{
  nsCOMPtr<nsIConsoleService> consoleService =
    do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  if (!consoleService)
    return;

  consoleService->LogStringMessage(aMessage.get());
}

// Javascript errors are logged to the main error console
static void
PACErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
  nsString formattedMessage(NS_LITERAL_STRING("PAC Execution Error: "));
  formattedMessage += report->ucmessage;
  formattedMessage += NS_LITERAL_STRING(" [");
  formattedMessage += report->uclinebuf;
  formattedMessage += NS_LITERAL_STRING("]");
  PACLogToConsole(formattedMessage);
}

// timeout of 0 means the normal necko timeout strategy, otherwise the dns request
// will be canceled after aTimeout milliseconds
static
bool PACResolve(const nsCString &aHostName, NetAddr *aNetAddr,
                unsigned int aTimeout)
{
  if (!sRunning) {
    NS_WARNING("PACResolve without a running ProxyAutoConfig object");
    return false;
  }

  return sRunning->ResolveAddress(aHostName, aNetAddr, aTimeout);
}

ProxyAutoConfig::ProxyAutoConfig()
  : mJSRuntime(nullptr)
  , mJSNeedsSetup(false)
  , mShutdown(false)
{
  MOZ_COUNT_CTOR(ProxyAutoConfig);
}

bool
ProxyAutoConfig::ResolveAddress(const nsCString &aHostName,
                                NetAddr *aNetAddr,
                                unsigned int aTimeout)
{
  nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID);
  if (!dns)
    return false;

  nsRefPtr<PACResolver> helper = new PACResolver();

  if (NS_FAILED(dns->AsyncResolve(aHostName,
                                  nsIDNSService::RESOLVE_PRIORITY_MEDIUM,
                                  helper,
                                  NS_GetCurrentThread(),
                                  getter_AddRefs(helper->mRequest))))
    return false;

  if (aTimeout && helper->mRequest) {
    if (!mTimer)
      mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
    if (mTimer) {
      mTimer->InitWithCallback(helper, aTimeout, nsITimer::TYPE_ONE_SHOT);
      helper->mTimer = mTimer;
    }
  }

  // Spin the event loop of the pac thread until lookup is complete.
  // nsPACman is responsible for keeping a queue and only allowing
  // one PAC execution at a time even when it is called re-entrantly.
  while (helper->mRequest)
    NS_ProcessNextEvent(NS_GetCurrentThread());

  if (NS_FAILED(helper->mStatus) ||
      NS_FAILED(helper->mResponse->GetNextAddr(0, aNetAddr)))
    return false;
  return true;
}

static
bool PACResolveToString(const nsCString &aHostName,
                        nsCString &aDottedDecimal,
                        unsigned int aTimeout)
{
  NetAddr netAddr;
  if (!PACResolve(aHostName, &netAddr, aTimeout))
    return false;

  char dottedDecimal[128];
  if (!NetAddrToString(&netAddr, dottedDecimal, sizeof(dottedDecimal)))
    return false;

  aDottedDecimal.Assign(dottedDecimal);
  return true;
}

// dnsResolve(host) javascript implementation
static
bool PACDnsResolve(JSContext *cx, unsigned int argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);

  if (NS_IsMainThread()) {
    NS_WARNING("DNS Resolution From PAC on Main Thread. How did that happen?");
    return false;
  }

  JS::Rooted<JSString*> arg1(cx);
  if (!JS_ConvertArguments(cx, args, "S", arg1.address()))
    return false;

  nsAutoJSString hostName;
  nsAutoCString dottedDecimal;

  if (!hostName.init(cx, arg1))
    return false;
  if (PACResolveToString(NS_ConvertUTF16toUTF8(hostName), dottedDecimal, 0)) {
    JSString *dottedDecimalString = JS_NewStringCopyZ(cx, dottedDecimal.get());
    if (!dottedDecimalString) {
      return false;
    }

    args.rval().setString(dottedDecimalString);
  }
  else {
    args.rval().setNull();
  }

  return true;
}

// myIpAddress() javascript implementation
static
bool PACMyIpAddress(JSContext *cx, unsigned int argc, JS::Value *vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

  if (NS_IsMainThread()) {
    NS_WARNING("DNS Resolution From PAC on Main Thread. How did that happen?");
    return false;
  }

  if (!sRunning) {
    NS_WARNING("PAC myIPAddress without a running ProxyAutoConfig object");
    return false;
  }

  return sRunning->MyIPAddress(args);
}

// proxyAlert(msg) javascript implementation
static
bool PACProxyAlert(JSContext *cx, unsigned int argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);

  JS::Rooted<JSString*> arg1(cx);
  if (!JS_ConvertArguments(cx, args, "S", arg1.address()))
    return false;

  nsAutoJSString message;
  if (!message.init(cx, arg1))
    return false;

  nsAutoString alertMessage;
  alertMessage.SetCapacity(32 + message.Length());
  alertMessage += NS_LITERAL_STRING("PAC-alert: ");
  alertMessage += message;
  PACLogToConsole(alertMessage);

  args.rval().setUndefined();  /* return undefined */
  return true;
}

static const JSFunctionSpec PACGlobalFunctions[] = {
  JS_FS("dnsResolve", PACDnsResolve, 1, 0),
  JS_FS("myIpAddress", PACMyIpAddress, 0, 0),
  JS_FS("alert", PACProxyAlert, 1, 0),
  JS_FS_END
};

// JSRuntimeWrapper is a c++ object that manages the runtime and context
// for the JS engine used on the PAC thread. It is initialized and destroyed
// on the PAC thread.
class JSRuntimeWrapper
{
 public:
  static JSRuntimeWrapper *Create()
  {
    JSRuntimeWrapper *entry = new JSRuntimeWrapper();

    if (NS_FAILED(entry->Init())) {
      delete entry;
      return nullptr;
    }

    return entry;
  }

  JSContext *Context() const
  {
    return mContext;
  }

  JSObject *Global() const
  {
    return mGlobal;
  }

  ~JSRuntimeWrapper()
  {
    MOZ_COUNT_DTOR(JSRuntimeWrapper);
    if (mContext) {
      JS_DestroyContext(mContext);
    }

    if (mRuntime) {
      JS_DestroyRuntime(mRuntime);
    }
  }

  void SetOK()
  {
    mOK = true;
  }

  bool IsOK()
  {
    return mOK;
  }

private:
  static const unsigned sRuntimeHeapSize = 2 << 20;

  JSRuntime *mRuntime;
  JSContext *mContext;
  JSObject  *mGlobal;
  bool      mOK;

  static const JSClass sGlobalClass;

  JSRuntimeWrapper()
    : mRuntime(nullptr), mContext(nullptr), mGlobal(nullptr), mOK(false)
  {
      MOZ_COUNT_CTOR(JSRuntimeWrapper);
  }

  nsresult Init()
  {
    mRuntime = JS_NewRuntime(sRuntimeHeapSize);
    NS_ENSURE_TRUE(mRuntime, NS_ERROR_OUT_OF_MEMORY);

    /*
     * Not setting this will cause JS_CHECK_RECURSION to report false
     * positives
     */
    JS_SetNativeStackQuota(mRuntime, 128 * sizeof(size_t) * 1024); 

    mContext = JS_NewContext(mRuntime, 0);
    NS_ENSURE_TRUE(mContext, NS_ERROR_OUT_OF_MEMORY);

    JSAutoRequest ar(mContext);

    JS::CompartmentOptions options;
    options.setZone(JS::SystemZone)
           .setVersion(JSVERSION_LATEST);
    mGlobal = JS_NewGlobalObject(mContext, &sGlobalClass, nullptr,
                                 JS::DontFireOnNewGlobalHook, options);
    NS_ENSURE_TRUE(mGlobal, NS_ERROR_OUT_OF_MEMORY);
    JS::Rooted<JSObject*> global(mContext, mGlobal);

    JSAutoCompartment ac(mContext, global);
    js::SetDefaultObjectForContext(mContext, global);
    JS_InitStandardClasses(mContext, global);

    JS_SetErrorReporter(mContext, PACErrorReporter);

    if (!JS_DefineFunctions(mContext, global, PACGlobalFunctions))
      return NS_ERROR_FAILURE;

    JS_FireOnNewGlobalObject(mContext, global);

    return NS_OK;
  }
};

const JSClass JSRuntimeWrapper::sGlobalClass = {
  "PACResolutionThreadGlobal",
  JSCLASS_GLOBAL_FLAGS,
  JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,
  nullptr, nullptr, nullptr, nullptr,
  JS_GlobalObjectTraceHook
};

nsresult
ProxyAutoConfig::Init(const nsCString &aPACURI,
                      const nsCString &aPACScript)
{
  mPACURI = aPACURI;
  mPACScript = sPacUtils;
  mPACScript.Append(aPACScript);

  if (!sRunning)
    return SetupJS();

  mJSNeedsSetup = true;
  return NS_OK;
}

nsresult
ProxyAutoConfig::SetupJS()
{
  mJSNeedsSetup = false;
  NS_ABORT_IF_FALSE(!sRunning, "JIT is running");

  delete mJSRuntime;
  mJSRuntime = nullptr;

  if (mPACScript.IsEmpty())
    return NS_ERROR_FAILURE;

  mJSRuntime = JSRuntimeWrapper::Create();
  if (!mJSRuntime)
    return NS_ERROR_FAILURE;

  JSContext* cx = mJSRuntime->Context();
  JSAutoRequest ar(cx);
  JSAutoCompartment ac(cx, mJSRuntime->Global());

  // check if this is a data: uri so that we don't spam the js console with
  // huge meaningless strings. this is not on the main thread, so it can't
  // use nsIRUI scheme methods
  bool isDataURI = nsDependentCSubstring(mPACURI, 0, 5).LowerCaseEqualsASCII("data:", 5);

  sRunning = this;
  JS::Rooted<JSObject*> global(cx, mJSRuntime->Global());
  JS::CompileOptions options(cx);
  options.setFileAndLine(mPACURI.get(), 1);
  JS::Rooted<JSScript*> script(cx);
  if (!JS_CompileScript(cx, global, mPACScript.get(),
                        mPACScript.Length(), options, &script) ||
      !JS_ExecuteScript(cx, global, script))
  {
    nsString alertMessage(NS_LITERAL_STRING("PAC file failed to install from "));
    if (isDataURI) {
      alertMessage += NS_LITERAL_STRING("data: URI");
    }
    else {
      alertMessage += NS_ConvertUTF8toUTF16(mPACURI);
    }
    PACLogToConsole(alertMessage);
    sRunning = nullptr;
    return NS_ERROR_FAILURE;
  }
  sRunning = nullptr;

  mJSRuntime->SetOK();
  nsString alertMessage(NS_LITERAL_STRING("PAC file installed from "));
  if (isDataURI) {
    alertMessage += NS_LITERAL_STRING("data: URI");
  }
  else {
    alertMessage += NS_ConvertUTF8toUTF16(mPACURI);
  }
  PACLogToConsole(alertMessage);

  // we don't need these now
  mPACScript.Truncate();
  mPACURI.Truncate();

  return NS_OK;
}

nsresult
ProxyAutoConfig::GetProxyForURI(const nsCString &aTestURI,
                                const nsCString &aTestHost,
                                nsACString &result)
{
  if (mJSNeedsSetup)
    SetupJS();

  if (!mJSRuntime || !mJSRuntime->IsOK())
    return NS_ERROR_NOT_AVAILABLE;

  JSContext *cx = mJSRuntime->Context();
  JSAutoRequest ar(cx);
  JSAutoCompartment ac(cx, mJSRuntime->Global());

  // the sRunning flag keeps a new PAC file from being installed
  // while the event loop is spinning on a DNS function. Don't early return.
  sRunning = this;
  mRunningHost = aTestHost;

  nsresult rv = NS_ERROR_FAILURE;
  JS::RootedString uriString(cx, JS_NewStringCopyZ(cx, aTestURI.get()));
  JS::RootedString hostString(cx, JS_NewStringCopyZ(cx, aTestHost.get()));

  if (uriString && hostString) {
    JS::AutoValueArray<2> args(cx);
    args[0].setString(uriString);
    args[1].setString(hostString);

    JS::Rooted<JS::Value> rval(cx);
    JS::Rooted<JSObject*> global(cx, mJSRuntime->Global());
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
  sRunning = nullptr;
  return rv;
}

void
ProxyAutoConfig::GC()
{
  if (!mJSRuntime || !mJSRuntime->IsOK())
    return;

  JSAutoCompartment ac(mJSRuntime->Context(), mJSRuntime->Global());
  JS_MaybeGC(mJSRuntime->Context());
}

ProxyAutoConfig::~ProxyAutoConfig()
{
  MOZ_COUNT_DTOR(ProxyAutoConfig);
  NS_ASSERTION(!mJSRuntime,
               "~ProxyAutoConfig leaking JS runtime that "
               "should have been deleted on pac thread");
}

void
ProxyAutoConfig::Shutdown()
{
  NS_ABORT_IF_FALSE(!NS_IsMainThread(), "wrong thread for shutdown");

  if (sRunning || mShutdown)
    return;

  mShutdown = true;
  delete mJSRuntime;
  mJSRuntime = nullptr;
}

bool
ProxyAutoConfig::SrcAddress(const NetAddr *remoteAddress, nsCString &localAddress)
{
  PRFileDesc *fd;
  fd = PR_OpenUDPSocket(remoteAddress->raw.family);
  if (!fd)
    return false;

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
  if (PR_NetAddrToString(&localName, dottedDecimal, sizeof(dottedDecimal)) != PR_SUCCESS)
    return false;
  
  localAddress.Assign(dottedDecimal);

  return true;
}

// hostName is run through a dns lookup and then a udp socket is connected
// to the result. If that all works, the local IP address of the socket is
// returned to the javascript caller and |*aResult| is set to true. Otherwise
// |*aResult| is set to false.
bool
ProxyAutoConfig::MyIPAddressTryHost(const nsCString &hostName,
                                    unsigned int timeout,
                                    const JS::CallArgs &aArgs,
                                    bool* aResult)
{
  *aResult = false;

  NetAddr remoteAddress;
  nsAutoCString localDottedDecimal;
  JSContext *cx = mJSRuntime->Context();

  if (PACResolve(hostName, &remoteAddress, timeout) &&
      SrcAddress(&remoteAddress, localDottedDecimal)) {
    JSString *dottedDecimalString =
      JS_NewStringCopyZ(cx, localDottedDecimal.get());
    if (!dottedDecimalString) {
      return false;
    }

    *aResult = true;
    aArgs.rval().setString(dottedDecimalString);
  }
  return true;
}

bool
ProxyAutoConfig::MyIPAddress(const JS::CallArgs &aArgs)
{
  nsAutoCString remoteDottedDecimal;
  nsAutoCString localDottedDecimal;
  JSContext *cx = mJSRuntime->Context();

  // first, lookup the local address of a socket connected
  // to the host of uri being resolved by the pac file. This is
  // v6 safe.. but is the last step like that
  bool rvalAssigned = false;
  if (!MyIPAddressTryHost(mRunningHost, kTimeout, aArgs, &rvalAssigned) ||
      rvalAssigned) {
    return rvalAssigned;
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
  
  // next, use the old algorithm based on the local hostname
  nsAutoCString hostName;
  nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID);
  if (dns && NS_SUCCEEDED(dns->GetMyHostName(hostName)) &&
      PACResolveToString(hostName, localDottedDecimal, kTimeout)) {
    JSString *dottedDecimalString =
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
  JSString *dottedDecimalString =
    JS_NewStringCopyZ(cx, localDottedDecimal.get());
  if (!dottedDecimalString) {
    return false;
  }

  aArgs.rval().setString(dottedDecimalString);
  return true;
}

} // namespace mozilla
} // namespace mozilla::net
