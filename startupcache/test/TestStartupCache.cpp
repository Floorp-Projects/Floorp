/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestHarness.h"

#include "nsThreadUtils.h"
#include "nsIClassInfo.h"
#include "nsIOutputStream.h"
#include "nsIObserver.h"
#include "nsISerializable.h"
#include "nsISupports.h"
#include "nsIStartupCache.h"
#include "nsIStringStream.h"
#include "nsIStorageStream.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIURI.h"
#include "nsStringAPI.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsITelemetry.h"
#include "nsIXPConnect.h"
#include "jsapi.h"
#include "prio.h"

using namespace JS;

namespace mozilla {
namespace scache {

NS_IMPORT nsresult
NewObjectInputStreamFromBuffer(char* buffer, uint32_t len, 
                               nsIObjectInputStream** stream);

// We can't retrieve the wrapped stream from the objectOutputStream later,
// so we return it here.
NS_IMPORT nsresult
NewObjectOutputWrappedStorageStream(nsIObjectOutputStream **wrapperStream,
                                    nsIStorageStream** stream);

NS_IMPORT nsresult
NewBufferFromStorageStream(nsIStorageStream *storageStream, 
                           char** buffer, uint32_t* len);
}
}

using namespace mozilla::scache;

#define NS_ENSURE_STR_MATCH(str1, str2, testname)  \
PR_BEGIN_MACRO                                     \
if (0 != strcmp(str1, str2)) {                     \
  fail("failed " testname);                        \
  return NS_ERROR_FAILURE;                         \
}                                                  \
passed("passed " testname);                        \
PR_END_MACRO

nsresult
WaitForStartupTimer() {
  nsresult rv;
  nsCOMPtr<nsIStartupCache> sc
    = do_GetService("@mozilla.org/startupcache/cache;1");
  PR_Sleep(10 * PR_TicksPerSecond());
  
  bool complete;
  while (true) {
    
    NS_ProcessPendingEvents(nullptr);
    rv = sc->StartupWriteComplete(&complete);
    if (NS_FAILED(rv) || complete)
      break;
    PR_Sleep(1 * PR_TicksPerSecond());
  }
  return rv;
}

nsresult
TestStartupWriteRead() {
  nsresult rv;
  nsCOMPtr<nsIStartupCache> sc
    = do_GetService("@mozilla.org/startupcache/cache;1", &rv);
  if (!sc) {
    fail("didn't get a pointer...");
    return NS_ERROR_FAILURE;
  } else {
    passed("got a pointer?");
  }
  sc->InvalidateCache();
  
  const char* buf = "Market opportunities for BeardBook";
  const char* id = "id";
  char* outbufPtr = nullptr;
  nsAutoArrayPtr<char> outbuf;  
  uint32_t len;
  
  rv = sc->PutBuffer(id, buf, strlen(buf) + 1);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = sc->GetBuffer(id, &outbufPtr, &len);
  NS_ENSURE_SUCCESS(rv, rv);
  outbuf = outbufPtr;
  NS_ENSURE_STR_MATCH(buf, outbuf, "pre-write read");

  rv = sc->ResetStartupWriteTimer();
  rv = WaitForStartupTimer();
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = sc->GetBuffer(id, &outbufPtr, &len);
  NS_ENSURE_SUCCESS(rv, rv);
  outbuf = outbufPtr;
  NS_ENSURE_STR_MATCH(buf, outbuf, "simple write/read");

  return NS_OK;
}

nsresult
TestWriteInvalidateRead() {
  nsresult rv;
  const char* buf = "BeardBook competitive analysis";
  const char* id = "id";
  char* outbuf = nullptr;
  uint32_t len;
  nsCOMPtr<nsIStartupCache> sc
    = do_GetService("@mozilla.org/startupcache/cache;1", &rv);
  sc->InvalidateCache();

  rv = sc->PutBuffer(id, buf, strlen(buf) + 1);
  NS_ENSURE_SUCCESS(rv, rv);

  sc->InvalidateCache();

  rv = sc->GetBuffer(id, &outbuf, &len);
  delete[] outbuf;
  if (rv == NS_ERROR_NOT_AVAILABLE) {
    passed("buffer not available after invalidate");
  } else if (NS_SUCCEEDED(rv)) {
    fail("GetBuffer succeeded unexpectedly after invalidate");
    return NS_ERROR_UNEXPECTED;
  } else {
    fail("GetBuffer gave an unexpected failure, expected NOT_AVAILABLE");
    return rv;
  }

  sc->InvalidateCache();
  return NS_OK;
}

nsresult
TestWriteObject() {
  nsresult rv;

  nsCOMPtr<nsIURI> obj
    = do_CreateInstance("@mozilla.org/network/simple-uri;1");
  if (!obj) {
    fail("did not create object in test write object");
    return NS_ERROR_UNEXPECTED;
  }
  NS_NAMED_LITERAL_CSTRING(spec, "http://www.mozilla.org");
  obj->SetSpec(spec);
  nsCOMPtr<nsIStartupCache> sc = do_GetService("@mozilla.org/startupcache/cache;1", &rv);

  sc->InvalidateCache();
  
  // Create an object stream. Usually this is done with
  // NewObjectOutputWrappedStorageStream, but that uses
  // StartupCache::GetSingleton in debug builds, and we
  // don't have access to that here. Obviously.
  const char* id = "id";
  nsCOMPtr<nsIStorageStream> storageStream
    = do_CreateInstance("@mozilla.org/storagestream;1");
  NS_ENSURE_ARG_POINTER(storageStream);
  
  rv = storageStream->Init(256, (uint32_t) -1, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIObjectOutputStream> objectOutput
    = do_CreateInstance("@mozilla.org/binaryoutputstream;1");
  if (!objectOutput)
    return NS_ERROR_OUT_OF_MEMORY;
  
  nsCOMPtr<nsIOutputStream> outputStream
    = do_QueryInterface(storageStream);
  
  rv = objectOutput->SetOutputStream(outputStream);

  if (NS_FAILED(rv)) {
    fail("failed to create output stream");
    return rv;
  }
  nsCOMPtr<nsISupports> objQI(do_QueryInterface(obj));
  rv = objectOutput->WriteObject(objQI, true);
  if (NS_FAILED(rv)) {
    fail("failed to write object");
    return rv;
  }

  char* bufPtr = nullptr;
  nsAutoArrayPtr<char> buf;
  uint32_t len;
  NewBufferFromStorageStream(storageStream, &bufPtr, &len);
  buf = bufPtr;

  // Since this is a post-startup write, it should be written and
  // available.
  rv = sc->PutBuffer(id, buf, len);
  if (NS_FAILED(rv)) {
    fail("failed to insert input stream");
    return rv;
  }
    
  char* buf2Ptr = nullptr;
  nsAutoArrayPtr<char> buf2;
  uint32_t len2;
  nsCOMPtr<nsIObjectInputStream> objectInput;
  rv = sc->GetBuffer(id, &buf2Ptr, &len2);
  if (NS_FAILED(rv)) {
    fail("failed to retrieve buffer");
    return rv;
  }
  buf2 = buf2Ptr;

  rv = NewObjectInputStreamFromBuffer(buf2, len2, getter_AddRefs(objectInput));
  if (NS_FAILED(rv)) {
    fail("failed to created input stream");
    return rv;
  }  
  buf2.forget();

  nsCOMPtr<nsISupports> deserialized;
  rv = objectInput->ReadObject(true, getter_AddRefs(deserialized));
  if (NS_FAILED(rv)) {
    fail("failed to read object");
    return rv;
  }
  
  bool match = false;
  nsCOMPtr<nsIURI> uri(do_QueryInterface(deserialized));
  if (uri) {
    nsCString outSpec;
    uri->GetSpec(outSpec);
    match = outSpec.Equals(spec);
  }
  if (!match) {
    fail("deserialized object has incorrect information");
    return rv;
  }
  
  passed("write object");
  return NS_OK;
}

nsresult
LockCacheFile(bool protect, nsIFile* profileDir) {
  NS_ENSURE_ARG(profileDir);

  nsCOMPtr<nsIFile> startupCache;
  profileDir->Clone(getter_AddRefs(startupCache));
  NS_ENSURE_STATE(startupCache);
  startupCache->AppendNative(NS_LITERAL_CSTRING("startupCache"));

  nsresult rv;
#ifndef XP_WIN
  static uint32_t oldPermissions;
#else
  static PRFileDesc* fd = nullptr;
#endif

  // To prevent deletion of the startupcache file, we change the containing
  // directory's permissions on Linux/Mac, and hold the file open on Windows
  if (protect) {
#ifndef XP_WIN
    rv = startupCache->GetPermissions(&oldPermissions);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = startupCache->SetPermissions(0555);
    NS_ENSURE_SUCCESS(rv, rv);
#else
    // Filename logic from StartupCache.cpp
    #ifdef IS_BIG_ENDIAN
    #define SC_ENDIAN "big"
    #else
    #define SC_ENDIAN "little"
    #endif

    #if PR_BYTES_PER_WORD == 4
    #define SC_WORDSIZE "4"
    #else
    #define SC_WORDSIZE "8"
    #endif
    char sStartupCacheName[] = "startupCache." SC_WORDSIZE "." SC_ENDIAN;
    startupCache->AppendNative(NS_LITERAL_CSTRING(sStartupCacheName));

    rv = startupCache->OpenNSPRFileDesc(PR_RDONLY, 0, &fd);
    NS_ENSURE_SUCCESS(rv, rv);
#endif
  } else {
#ifndef XP_WIN
    rv = startupCache->SetPermissions(oldPermissions);
    NS_ENSURE_SUCCESS(rv, rv);
#else
   PR_Close(fd);
#endif
  }

  return NS_OK;
}

nsresult
TestIgnoreDiskCache(nsIFile* profileDir) {
  nsresult rv;
  nsCOMPtr<nsIStartupCache> sc
    = do_GetService("@mozilla.org/startupcache/cache;1", &rv);
  sc->InvalidateCache();
  
  const char* buf = "Get a Beardbook app for your smartphone";
  const char* id = "id";
  char* outbuf = nullptr;
  uint32_t len;
  
  rv = sc->PutBuffer(id, buf, strlen(buf) + 1);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = sc->ResetStartupWriteTimer();
  rv = WaitForStartupTimer();
  NS_ENSURE_SUCCESS(rv, rv);

  // Prevent StartupCache::InvalidateCache from deleting the disk file
  rv = LockCacheFile(true, profileDir);
  NS_ENSURE_SUCCESS(rv, rv);

  sc->IgnoreDiskCache();

  rv = sc->GetBuffer(id, &outbuf, &len);

  nsresult r = LockCacheFile(false, profileDir);
  NS_ENSURE_SUCCESS(r, r);

  delete[] outbuf;

  if (rv == NS_ERROR_NOT_AVAILABLE) {
    passed("buffer not available after ignoring disk cache");
  } else if (NS_SUCCEEDED(rv)) {
    fail("GetBuffer succeeded unexpectedly after ignoring disk cache");
    return NS_ERROR_UNEXPECTED;
  } else {
    fail("GetBuffer gave an unexpected failure, expected NOT_AVAILABLE");
    return rv;
  }

  sc->InvalidateCache();
  return NS_OK;
}

nsresult
TestEarlyShutdown() {
  nsresult rv;
  nsCOMPtr<nsIStartupCache> sc
    = do_GetService("@mozilla.org/startupcache/cache;1", &rv);
  sc->InvalidateCache();

  const char* buf = "Find your soul beardmate on BeardBook";
  const char* id = "id";
  uint32_t len;
  char* outbuf = nullptr;
  
  sc->ResetStartupWriteTimer();
  rv = sc->PutBuffer(id, buf, strlen(buf) + 1);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIObserver> obs;
  sc->GetObserver(getter_AddRefs(obs));
  obs->Observe(nullptr, "xpcom-shutdown", nullptr);
  rv = WaitForStartupTimer();
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = sc->GetBuffer(id, &outbuf, &len);
  delete[] outbuf;

  if (NS_SUCCEEDED(rv)) {
    passed("GetBuffer succeeded after early shutdown");
  } else {
    fail("GetBuffer failed after early shutdown");
    return rv;
  }

  const char* other_id = "other_id";
  rv = sc->PutBuffer(other_id, buf, strlen(buf) + 1);

  if (rv == NS_ERROR_NOT_AVAILABLE) {
    passed("PutBuffer not available after early shutdown");
  } else if (NS_SUCCEEDED(rv)) {
    fail("PutBuffer succeeded unexpectedly after early shutdown");
    return NS_ERROR_UNEXPECTED;
  } else {
    fail("PutBuffer gave an unexpected failure, expected NOT_AVAILABLE");
    return rv;
  }
 
  return NS_OK;
}

static bool
GetHistogramCounts(const char *testmsg, const nsACString &histogram_id,
                   JSContext *cx, MutableHandle<Value> counts)
{
  nsCOMPtr<nsITelemetry> telemetry = do_GetService("@mozilla.org/base/telemetry;1");
  Rooted<Value> h(cx);
  nsresult trv = telemetry->GetHistogramById(histogram_id, cx, h.address());
  if (NS_FAILED(trv)) {
    fail("%s: couldn't get histogram %s", testmsg, ToNewCString(histogram_id));
    return false;
  }
  passed(testmsg);

  Rooted<Value> snapshot_val(cx);
  JSFunction *snapshot_fn = nullptr;
  Rooted<Value> ss(cx);
  return (JS_GetProperty(cx, JSVAL_TO_OBJECT(h), "snapshot",
                         &snapshot_val)
          && (snapshot_fn = JS_ValueToFunction(cx, snapshot_val))
          && JS::Call(cx, JSVAL_TO_OBJECT(h),
                      snapshot_fn, 0, nullptr, &ss)
          && JS_GetProperty(cx, JSVAL_TO_OBJECT(ss), "counts", counts));
}

nsresult
CompareCountArrays(JSContext *cx, JSObject *aBefore, JSObject *aAfter)
{
  uint32_t before_size, after_size;
  JS::RootedObject before(cx, aBefore);
  JS::RootedObject after(cx, aAfter);
  if (!(JS_GetArrayLength(cx, before, &before_size)
        && JS_GetArrayLength(cx, after, &after_size))) {
    return NS_ERROR_UNEXPECTED;
  }

  if (before_size != after_size) {
    return NS_ERROR_UNEXPECTED;
  }

  JS::RootedValue before_num(cx), after_num(cx);
  for (uint32_t i = 0; i < before_size; ++i) {
    if (!(JS_GetElement(cx, before, i, &before_num)
          && JS_GetElement(cx, after, i, &after_num))) {
      return NS_ERROR_UNEXPECTED;
    }

    JSBool same = true;
    if (!JS_LooselyEqual(cx, before_num, after_num, &same)) {
      return NS_ERROR_UNEXPECTED;
    } else {
      if (same) {
        continue;
      } else {
        // Some element of the histograms's count arrays differed.
        // That's a good thing!
        return NS_OK;
      }
    }
  }

  // None of the elements of the histograms's count arrays differed.
  // Not good, we should have recorded something.
  return NS_ERROR_FAILURE;
}

nsresult
TestHistogramValues(const char* type, bool use_js, JSContext *cx,
                    JSObject *before, JSObject *after)
{
  if (!use_js) {
    fail("couldn't check histogram recording");
    return NS_ERROR_FAILURE;
  }
  nsresult compare = CompareCountArrays(cx, before, after);
  if (compare == NS_ERROR_UNEXPECTED) {
    fail("count comparison error");
    return compare;
  }
  if (compare == NS_ERROR_FAILURE) {
    fail("histogram didn't record %s", type);
    return compare;
  }
  passed("histogram records %s", type);
  return NS_OK;
}

int main(int argc, char** argv)
{
  ScopedXPCOM xpcom("Startup Cache");
  if (xpcom.failed())
    return 1;

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  prefs->SetIntPref("hangmonitor.timeout", 0);
  
  int rv = 0;
  // nsITelemetry doesn't have a nice C++ interface.
  JSContext *cx = nullptr;

  // XPCOM initialization spins up XPConnect, which spins up a JSRuntime, which
  // we can only have one of per thread. So we need to get a JSContext out of
  // XPConnect here, rather than creating our own runtime. XPConnect rules
  // dictate that we push the context as well, but we're trying to make the
  // pushing/popping APIs accessible only through nsCxPusher, which isn't
  // accessible via the external linkage used by this test. We can get away with
  // using the cx here without triggering a cx stack assert, so just do that
  // for now. Eventually, the whole notion of pushing and popping will just go
  // away.
  nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID());
  if (xpc)
    cx = xpc->GetSafeJSContext();

  bool use_js = !!cx;
  JSAutoRequest req(cx);
  static JSClass global_class = {
    "global", JSCLASS_NEW_RESOLVE | JSCLASS_GLOBAL_FLAGS | JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_DeletePropertyStub,
    JS_PropertyStub,  JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub,
    JS_ConvertStub
  };
  JSObject *glob = nullptr;
  if (use_js)
    glob = JS_NewGlobalObject(cx, &global_class, nullptr, JS::FireOnNewGlobalHook);
  if (!glob)
    use_js = false;
  mozilla::Maybe<JSAutoCompartment> ac;
  if (use_js)
    ac.construct(cx, glob);
  if (use_js && !JS_InitStandardClasses(cx, glob))
    use_js = false;

  NS_NAMED_LITERAL_CSTRING(age_histogram_id, "STARTUP_CACHE_AGE_HOURS");
  NS_NAMED_LITERAL_CSTRING(invalid_histogram_id, "STARTUP_CACHE_INVALID");

  Rooted<Value> age_before_counts(cx);
  if (use_js &&
      !GetHistogramCounts("STARTUP_CACHE_AGE_HOURS histogram before test",
                          age_histogram_id, cx, &age_before_counts))
    use_js = false;
  
  Rooted<Value> invalid_before_counts(cx);
  if (use_js &&
      !GetHistogramCounts("STARTUP_CACHE_INVALID histogram before test",
                          invalid_histogram_id, cx, &invalid_before_counts))
    use_js = false;
  
  nsresult scrv;
  nsCOMPtr<nsIStartupCache> sc 
    = do_GetService("@mozilla.org/startupcache/cache;1", &scrv);
  if (NS_FAILED(scrv))
    rv = 1;
  else
    sc->RecordAgesAlways();
  if (NS_FAILED(TestStartupWriteRead()))
    rv = 1;
  if (NS_FAILED(TestWriteInvalidateRead()))
    rv = 1;
  if (NS_FAILED(TestWriteObject()))
    rv = 1;
  nsCOMPtr<nsIFile> profileDir = xpcom.GetProfileDirectory();
  if (NS_FAILED(TestIgnoreDiskCache(profileDir)))
    rv = 1;
  if (NS_FAILED(TestEarlyShutdown()))
    rv = 1;

  Rooted<Value> age_after_counts(cx);
  if (use_js &&
      !GetHistogramCounts("STARTUP_CACHE_AGE_HOURS histogram after test",
                          age_histogram_id, cx, &age_after_counts))
    use_js = false;

  if (NS_FAILED(TestHistogramValues("age samples", use_js, cx,
                                    age_before_counts.toObjectOrNull(),
                                    age_after_counts.toObjectOrNull())))
    rv = 1;
                                                    
  Rooted<Value> invalid_after_counts(cx);
  if (use_js &&
      !GetHistogramCounts("STARTUP_CACHE_INVALID histogram after test",
                          invalid_histogram_id, cx, &invalid_after_counts))
    use_js = false;

  // STARTUP_CACHE_INVALID should have been triggered by TestIgnoreDiskCache()
  if (NS_FAILED(TestHistogramValues("invalid disk cache", use_js, cx,
                                    invalid_before_counts.toObjectOrNull(),
                                    invalid_after_counts.toObjectOrNull())))
    rv = 1;

  return rv;
}
