/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/scache/StartupCache.h"
#include "mozilla/scache/StartupCacheUtils.h"

#include "nsDirectoryServiceDefs.h"
#include "nsIClassInfo.h"
#include "nsIOutputStream.h"
#include "nsIObserver.h"
#include "nsISerializable.h"
#include "nsISupports.h"
#include "nsIStringStream.h"
#include "nsIStorageStream.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIURI.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIXPConnect.h"
#include "nsThreadUtils.h"
#include "prenv.h"
#include "prio.h"
#include "prprf.h"
#include "mozilla/Maybe.h"
#include "mozilla/Printf.h"
#include "mozilla/UniquePtr.h"
#include "nsNetCID.h"
#include "nsIURIMutator.h"

using namespace JS;

using namespace mozilla::scache;
using mozilla::UniquePtr;

void
WaitForStartupTimer()
{
  StartupCache* sc = StartupCache::GetSingleton();
  PR_Sleep(10 * PR_TicksPerSecond());

  while (true) {
    NS_ProcessPendingEvents(nullptr);
    if (sc->StartupWriteComplete()) {
      return;
    }
    PR_Sleep(1 * PR_TicksPerSecond());
  }
}

class TestStartupCache : public ::testing::Test
{
protected:
  TestStartupCache();
  ~TestStartupCache();

  nsCOMPtr<nsIFile> mSCFile;
};

TestStartupCache::TestStartupCache()
{
  NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(mSCFile));
  mSCFile->AppendNative(NS_LITERAL_CSTRING("test-startupcache.tmp"));
  nsAutoCString path;
  mSCFile->GetNativePath(path);
  char* env = mozilla::Smprintf("MOZ_STARTUP_CACHE=%s", path.get()).release();
  PR_SetEnv(env);
  // We intentionally leak `env` here because it is required by PR_SetEnv
  MOZ_LSAN_INTENTIONALLY_LEAK_OBJECT(env);
  StartupCache::GetSingleton()->InvalidateCache();
}
TestStartupCache::~TestStartupCache()
{
  PR_SetEnv("MOZ_STARTUP_CACHE=");
  StartupCache::GetSingleton()->InvalidateCache();
}


TEST_F(TestStartupCache, StartupWriteRead)
{
  nsresult rv;
  StartupCache* sc = StartupCache::GetSingleton();

  const char* buf = "Market opportunities for BeardBook";
  const char* id = "id";
  UniquePtr<char[]> outbuf;
  uint32_t len;

  rv = sc->PutBuffer(id, buf, strlen(buf) + 1);
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  rv = sc->GetBuffer(id, &outbuf, &len);
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  EXPECT_STREQ(buf, outbuf.get());

  rv = sc->ResetStartupWriteTimer();
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  WaitForStartupTimer();

  rv = sc->GetBuffer(id, &outbuf, &len);
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  EXPECT_STREQ(buf, outbuf.get());
}

TEST_F(TestStartupCache, WriteInvalidateRead)
{
  nsresult rv;
  const char* buf = "BeardBook competitive analysis";
  const char* id = "id";
  UniquePtr<char[]> outbuf;
  uint32_t len;
  StartupCache* sc = StartupCache::GetSingleton();
  ASSERT_TRUE(sc);

  rv = sc->PutBuffer(id, buf, strlen(buf) + 1);
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  sc->InvalidateCache();

  rv = sc->GetBuffer(id, &outbuf, &len);
  EXPECT_EQ(rv, NS_ERROR_NOT_AVAILABLE);
}

TEST_F(TestStartupCache, WriteObject)
{
  nsresult rv;

  nsCOMPtr<nsIURI> obj;

  NS_NAMED_LITERAL_CSTRING(spec, "http://www.mozilla.org");
  rv = NS_MutateURI(NS_SIMPLEURIMUTATOR_CONTRACTID)
         .SetSpec(spec)
         .Finalize(obj);
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  StartupCache* sc = StartupCache::GetSingleton();

  // Create an object stream. Usually this is done with
  // NewObjectOutputWrappedStorageStream, but that uses
  // StartupCache::GetSingleton in debug builds, and we
  // don't have access to that here. Obviously.
  const char* id = "id";
  nsCOMPtr<nsIStorageStream> storageStream
    = do_CreateInstance("@mozilla.org/storagestream;1");
  ASSERT_TRUE(storageStream);

  rv = storageStream->Init(256, (uint32_t) -1);
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIObjectOutputStream> objectOutput
    = do_CreateInstance("@mozilla.org/binaryoutputstream;1");
  ASSERT_TRUE(objectOutput);

  nsCOMPtr<nsIOutputStream> outputStream
    = do_QueryInterface(storageStream);

  rv = objectOutput->SetOutputStream(outputStream);
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsISupports> objQI(do_QueryInterface(obj));
  rv = objectOutput->WriteObject(objQI, true);
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  UniquePtr<char[]> buf;
  uint32_t len;
  NewBufferFromStorageStream(storageStream, &buf, &len);

  // Since this is a post-startup write, it should be written and
  // available.
  rv = sc->PutBuffer(id, buf.get(), len);
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  UniquePtr<char[]> buf2;
  uint32_t len2;
  nsCOMPtr<nsIObjectInputStream> objectInput;
  rv = sc->GetBuffer(id, &buf2, &len2);
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  rv = NewObjectInputStreamFromBuffer(Move(buf2), len2,
                                      getter_AddRefs(objectInput));
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsISupports> deserialized;
  rv = objectInput->ReadObject(true, getter_AddRefs(deserialized));
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIURI> uri(do_QueryInterface(deserialized));
  ASSERT_TRUE(uri);

  nsCString outSpec;
  rv = uri->GetSpec(outSpec);
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_TRUE(outSpec.Equals(spec));
}
