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
#include "nsIXPConnect.h"
#include "prio.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"

using namespace JS;

namespace mozilla {
namespace scache {

NS_IMPORT nsresult
NewObjectInputStreamFromBuffer(UniquePtr<char[]> buffer, uint32_t len, 
                               nsIObjectInputStream** stream);

// We can't retrieve the wrapped stream from the objectOutputStream later,
// so we return it here.
NS_IMPORT nsresult
NewObjectOutputWrappedStorageStream(nsIObjectOutputStream **wrapperStream,
                                    nsIStorageStream** stream);

NS_IMPORT nsresult
NewBufferFromStorageStream(nsIStorageStream *storageStream, 
                           UniquePtr<char[]>* buffer, uint32_t* len);
} // namespace scache
} // namespace mozilla

using namespace mozilla::scache;
using mozilla::UniquePtr;

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
  UniquePtr<char[]> outbuf;  
  uint32_t len;
  
  rv = sc->PutBuffer(id, buf, strlen(buf) + 1);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = sc->GetBuffer(id, &outbuf, &len);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_STR_MATCH(buf, outbuf.get(), "pre-write read");

  rv = sc->ResetStartupWriteTimer();
  rv = WaitForStartupTimer();
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = sc->GetBuffer(id, &outbuf, &len);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_STR_MATCH(buf, outbuf.get(), "simple write/read");

  return NS_OK;
}

nsresult
TestWriteInvalidateRead() {
  nsresult rv;
  const char* buf = "BeardBook competitive analysis";
  const char* id = "id";
  UniquePtr<char[]> outbuf;
  uint32_t len;
  nsCOMPtr<nsIStartupCache> sc
    = do_GetService("@mozilla.org/startupcache/cache;1", &rv);
  sc->InvalidateCache();

  rv = sc->PutBuffer(id, buf, strlen(buf) + 1);
  NS_ENSURE_SUCCESS(rv, rv);

  sc->InvalidateCache();

  rv = sc->GetBuffer(id, &outbuf, &len);
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
  rv = obj->SetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);
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
  
  rv = storageStream->Init(256, (uint32_t) -1);
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

  UniquePtr<char[]> buf;
  uint32_t len;
  NewBufferFromStorageStream(storageStream, &buf, &len);

  // Since this is a post-startup write, it should be written and
  // available.
  rv = sc->PutBuffer(id, buf.get(), len);
  if (NS_FAILED(rv)) {
    fail("failed to insert input stream");
    return rv;
  }
    
  UniquePtr<char[]> buf2;
  uint32_t len2;
  nsCOMPtr<nsIObjectInputStream> objectInput;
  rv = sc->GetBuffer(id, &buf2, &len2);
  if (NS_FAILED(rv)) {
    fail("failed to retrieve buffer");
    return rv;
  }

  rv = NewObjectInputStreamFromBuffer(Move(buf2), len2,
                                      getter_AddRefs(objectInput));
  if (NS_FAILED(rv)) {
    fail("failed to created input stream");
    return rv;
  }  

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
    rv = uri->GetSpec(outSpec);
    if (NS_FAILED(rv)) {
      fail("failed to get spec");
      return rv;
    }
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
  UniquePtr<char[]> outbuf;
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
  UniquePtr<char[]> outbuf;
  
  sc->ResetStartupWriteTimer();
  rv = sc->PutBuffer(id, buf, strlen(buf) + 1);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIObserver> obs;
  sc->GetObserver(getter_AddRefs(obs));
  obs->Observe(nullptr, "xpcom-shutdown", nullptr);
  rv = WaitForStartupTimer();
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = sc->GetBuffer(id, &outbuf, &len);

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

int main(int argc, char** argv)
{
  ScopedXPCOM xpcom("Startup Cache");
  if (xpcom.failed())
    return 1;

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (!prefs) {
    fail("prefs");
    return 1;
  }
  prefs->SetIntPref("hangmonitor.timeout", 0);

  int rv = 0;
  nsresult scrv;

  // Register TestStartupCacheTelemetry
  nsCOMPtr<nsIFile> manifest;
  scrv = NS_GetSpecialDirectory(NS_GRE_DIR,
                                getter_AddRefs(manifest));
  if (NS_FAILED(scrv)) {
    fail("NS_XPCOM_CURRENT_PROCESS_DIR");
    return 1;
  }

#ifdef XP_MACOSX
  nsCOMPtr<nsIFile> tempManifest;
  manifest->Clone(getter_AddRefs(tempManifest));
  manifest->AppendNative(
    NS_LITERAL_CSTRING("TestStartupCacheTelemetry.manifest"));
  bool exists;
  manifest->Exists(&exists);
  if (!exists) {
    // Workaround for bug 1080338 in mozharness.
    manifest = tempManifest.forget();
    manifest->SetNativeLeafName(NS_LITERAL_CSTRING("MacOS"));
    manifest->AppendNative(
      NS_LITERAL_CSTRING("TestStartupCacheTelemetry.manifest"));
  }
#else
  manifest->AppendNative(
    NS_LITERAL_CSTRING("TestStartupCacheTelemetry.manifest"));
#endif

  XRE_AddManifestLocation(NS_APP_LOCATION, manifest);

  nsCOMPtr<nsIObserver> telemetryThing =
    do_GetService("@mozilla.org/testing/startup-cache-telemetry.js");
  if (!telemetryThing) {
    fail("telemetryThing");
    return 1;
  }
  scrv = telemetryThing->Observe(nullptr, "save-initial", nullptr);
  if (NS_FAILED(scrv)) {
    fail("save-initial");
    rv = 1;
  }

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

  scrv = telemetryThing->Observe(nullptr, "save-initial", nullptr);
  if (NS_FAILED(scrv)) {
    fail("check-final");
    rv = 1;
  }

  return rv;
}
