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

nsresult CacheEntriesEnumerator::GetNextFile(nsIFile** aFile)
{
#ifdef DEBUG
  MOZ_ASSERT(mThreadCheck == NS_GetCurrentThread());
#endif

  NS_ENSURE_TRUE(mCurrentFile, NS_ERROR_UNEXPECTED);

  mCurrentFile.forget(aFile);
  return NS_OK;
}

} // net
} // mozilla
