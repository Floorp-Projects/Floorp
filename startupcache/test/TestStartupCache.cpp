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
#include "jsapi.h"

namespace mozilla {
namespace scache {

NS_IMPORT nsresult
NewObjectInputStreamFromBuffer(char* buffer, PRUint32 len, 
                               nsIObjectInputStream** stream);

// We can't retrieve the wrapped stream from the objectOutputStream later,
// so we return it here.
NS_IMPORT nsresult
NewObjectOutputWrappedStorageStream(nsIObjectOutputStream **wrapperStream,
                                    nsIStorageStream** stream);

NS_IMPORT nsresult
NewBufferFromStorageStream(nsIStorageStream *storageStream, 
                           char** buffer, PRUint32* len);
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
  char* outbufPtr = NULL;
  nsAutoArrayPtr<char> outbuf;  
  PRUint32 len;
  
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
  char* outbuf = NULL;
  PRUint32 len;
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
  
  rv = storageStream->Init(256, (PRUint32) -1, nullptr);
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

  char* bufPtr = NULL;
  nsAutoArrayPtr<char> buf;
  PRUint32 len;
  NewBufferFromStorageStream(storageStream, &bufPtr, &len);
  buf = bufPtr;

  // Since this is a post-startup write, it should be written and
  // available.
  rv = sc->PutBuffer(id, buf, len);
  if (NS_FAILED(rv)) {
    fail("failed to insert input stream");
    return rv;
  }
    
  char* buf2Ptr = NULL;
  nsAutoArrayPtr<char> buf2;
  PRUint32 len2;
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
TestEarlyShutdown() {
  nsresult rv;
  nsCOMPtr<nsIStartupCache> sc 
    = do_GetService("@mozilla.org/startupcache/cache;1", &rv);
  sc->InvalidateCache();

  const char* buf = "Find your soul beardmate on BeardBook";
  const char* id = "id";
  PRUint32 len;
  char* outbuf = NULL;
  
  sc->ResetStartupWriteTimer();
  rv = sc->PutBuffer(buf, id, strlen(buf) + 1);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIObserver> obs;
  sc->GetObserver(getter_AddRefs(obs));
  obs->Observe(nullptr, "xpcom-shutdown", nullptr);
  rv = WaitForStartupTimer();
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = sc->GetBuffer(id, &outbuf, &len);
  delete[] outbuf;

  if (rv == NS_ERROR_NOT_AVAILABLE) {
    passed("buffer not available after early shutdown");
  } else if (NS_SUCCEEDED(rv)) {
    fail("GetBuffer succeeded unexpectedly after early shutdown");
    return NS_ERROR_UNEXPECTED;
  } else {
    fail("GetBuffer gave an unexpected failure, expected NOT_AVAILABLE");
    return rv;
  }
 
  return NS_OK;
}

bool
SetupJS(JSContext **cxp)
{
  JSRuntime *rt = JS_NewRuntime(32 * 1024 * 1024);
  if (!rt)
    return false;
  JSContext *cx = JS_NewContext(rt, 8192);
  if (!cx)
    return false;
  *cxp = cx;
  return true;
}

bool
GetHistogramCounts(const char *testmsg, JSContext *cx, jsval *counts)
{
  nsCOMPtr<nsITelemetry> telemetry = do_GetService("@mozilla.org/base/telemetry;1");
  NS_NAMED_LITERAL_CSTRING(histogram_id, "STARTUP_CACHE_AGE_HOURS");
  JS::AutoValueRooter h(cx);
  nsresult trv = telemetry->GetHistogramById(histogram_id, cx, h.addr());
  if (NS_FAILED(trv)) {
    fail("%s: couldn't get histogram", testmsg);
    return false;
  }
  passed(testmsg);

  JS::AutoValueRooter snapshot_val(cx);
  JSFunction *snapshot_fn = NULL;
  JS::AutoValueRooter ss(cx);
  return (JS_GetProperty(cx, JSVAL_TO_OBJECT(h.value()), "snapshot",
                         snapshot_val.addr())
          && (snapshot_fn = JS_ValueToFunction(cx, snapshot_val.value()))
          && JS::Call(cx, JSVAL_TO_OBJECT(h.value()),
                      snapshot_fn, 0, NULL, ss.addr())
          && JS_GetProperty(cx, JSVAL_TO_OBJECT(ss.value()),
                            "counts", counts));
}

nsresult
CompareCountArrays(JSContext *cx, JSObject *before, JSObject *after)
{
  uint32_t before_size, after_size;
  if (!(JS_GetArrayLength(cx, before, &before_size)
        && JS_GetArrayLength(cx, after, &after_size))) {
    return NS_ERROR_UNEXPECTED;
  }

  if (before_size != after_size) {
    return NS_ERROR_UNEXPECTED;
  }

  for (uint32_t i = 0; i < before_size; ++i) {
    jsval before_num, after_num;

    if (!(JS_GetElement(cx, before, i, &before_num)
          && JS_GetElement(cx, after, i, &after_num))) {
      return NS_ERROR_UNEXPECTED;
    }

    JSBool same = JS_TRUE;
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

  // All of the elements of the histograms's count arrays differed.
  // Not good, we should have recorded something.
  return NS_ERROR_FAILURE;
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
  JSContext *cx;
  bool use_js = true;
  if (!SetupJS(&cx))
    use_js = false;

  JSAutoRequest req(cx);
  static JSClass global_class = {
    "global", JSCLASS_NEW_RESOLVE | JSCLASS_GLOBAL_FLAGS | JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub,
    JS_PropertyStub,  JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub,
    JS_ConvertStub
  };
  JSObject *glob = nullptr;
  if (use_js)
    glob = JS_NewGlobalObject(cx, &global_class, NULL);
  if (!glob)
    use_js = false;
  JSCrossCompartmentCall *compartment = nullptr;
  if (use_js)
    compartment = JS_EnterCrossCompartmentCall(cx, glob);
  if (!compartment)
    use_js = false;
  if (use_js && !JS_InitStandardClasses(cx, glob))
    use_js = false;

  JS::AutoValueRooter before_counts(cx);
  if (use_js && !GetHistogramCounts("STARTUP_CACHE_AGE_HOURS histogram before test",
                                 cx, before_counts.addr()))
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
  if (NS_FAILED(TestEarlyShutdown()))
    rv = 1;

  JS::AutoValueRooter after_counts(cx);
  if (use_js && !GetHistogramCounts("STARTUP_CACHE_AGE_HOURS histogram after test",
                                    cx, after_counts.addr()))
    use_js = false;

  if (!use_js) {
    fail("couldn't check histogram recording");
    rv = 1;
  } else {
    nsresult compare = CompareCountArrays(cx,
                                          JSVAL_TO_OBJECT(before_counts.value()),
                                          JSVAL_TO_OBJECT(after_counts.value()));
    if (compare == NS_ERROR_UNEXPECTED) {
      fail("count comparison error");
      rv = 1;
    } else if (compare == NS_ERROR_FAILURE) {
      fail("histogram didn't record samples");
      rv = 1;
    } else {
      passed("histogram records samples");
    }
  }

  if (use_js)
    JS_LeaveCrossCompartmentCall(compartment);

  return rv;
}
