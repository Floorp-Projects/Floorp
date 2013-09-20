/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheEntriesEnumerator.h"

#include "CacheFileIOManager.h"
#include "CacheFile.h"

#include "nsIDirectoryEnumerator.h"
#include "nsIFile.h"
#include "nsIThread.h"
#include "nsISimpleEnumerator.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace net {

CacheEntriesEnumerator::CacheEntriesEnumerator(nsIFile* aEntriesDirectory)
: mEntriesDirectory(aEntriesDirectory)
{
  MOZ_COUNT_CTOR(CacheEntriesEnumerator);
}

CacheEntriesEnumerator::~CacheEntriesEnumerator()
{
  MOZ_COUNT_DTOR(CacheEntriesEnumerator);

  if (mEnumerator) {
    mEnumerator->Close();
    ProxyReleaseMainThread(mEnumerator);
  }
}

nsresult CacheEntriesEnumerator::Init()
{
  nsresult rv;

  nsCOMPtr<nsISimpleEnumerator> e;
  rv = mEntriesDirectory->GetDirectoryEntries(getter_AddRefs(e));

  if (NS_ERROR_FILE_NOT_FOUND == rv || NS_ERROR_FILE_TARGET_DOES_NOT_EXIST == rv) {
    return NS_OK;
  }

  NS_ENSURE_SUCCESS(rv, rv);

  mEnumerator = do_QueryInterface(e, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

bool CacheEntriesEnumerator::HasMore()
{
#ifdef DEBUG
  if (!mThreadCheck)
    mThreadCheck = NS_GetCurrentThread();
  else
    MOZ_ASSERT(mThreadCheck == NS_GetCurrentThread());
#endif

  if (!mEnumerator) {
    return false;
  }

  if (mCurrentFile)
    return true;

  nsresult rv;

  rv = mEnumerator->GetNextFile(getter_AddRefs(mCurrentFile));

  if (NS_FAILED(rv)) {
    mEnumerator->Close();
    mEnumerator = nullptr;
    return false;
  }

  return !!mCurrentFile;
}

namespace { // anon

class FileConsumer : public CacheFileListener
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  nsresult Init(nsCSubstring const & aKey,
                CacheEntriesEnumeratorCallback* aCallback);

  virtual ~FileConsumer() {}

private:
  NS_IMETHOD OnFileReady(nsresult aResult, bool aIsNew);
  NS_IMETHOD OnFileDoomed(nsresult aResult) { return NS_OK; }

  nsRefPtr<CacheFile> mFile;
  nsRefPtr<CacheEntriesEnumeratorCallback> mCallback;
};

nsresult FileConsumer::Init(const nsCSubstring &aKey,
                            CacheEntriesEnumeratorCallback *aCallback)
{
  mCallback = aCallback;

  mFile = new CacheFile();
  nsresult rv = mFile->Init(aKey, false, false, false, true, this);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP FileConsumer::OnFileReady(nsresult aResult, bool aIsNew)
{
  //MOZ_ASSERT(!aIsNew);

  if (NS_FAILED(aResult)) {
    mCallback->OnFile(nullptr);
  }
  else {
    mCallback->OnFile(mFile);
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS1(FileConsumer, CacheFileListener);

} // anon

nsresult CacheEntriesEnumerator::GetNextCacheFile(CacheEntriesEnumeratorCallback* aCallback)
{
#ifdef DEBUG
  MOZ_ASSERT(mThreadCheck == NS_GetCurrentThread());
#endif

  nsresult rv;

  NS_ENSURE_TRUE(mCurrentFile, NS_ERROR_UNEXPECTED);

  nsAutoCString key;
  rv = mCurrentFile->GetNativeLeafName(key);

  mCurrentFile = nullptr;

  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<FileConsumer> consumer = new FileConsumer();
  rv = consumer->Init(key, aCallback);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult CacheEntriesEnumerator::GetNextFile(nsIFile** aFile)
{
#ifdef DEBUG
  MOZ_ASSERT(mThreadCheck == NS_GetCurrentThread());
#endif

  NS_ENSURE_TRUE(mCurrentFile, NS_ERROR_UNEXPECTED);

  mCurrentFile.forget(aFile);
  return NS_OK;
}

nsresult CacheEntriesEnumerator::GetCacheFileFromFile(nsIFile* aFile,
                                                      CacheEntriesEnumeratorCallback* aCallback)
{
  nsresult rv;

  nsAutoCString key;
  rv = aFile->GetNativeLeafName(key);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<FileConsumer> consumer = new FileConsumer();
  rv = consumer->Init(key, aCallback);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

} // net
} // mozilla
