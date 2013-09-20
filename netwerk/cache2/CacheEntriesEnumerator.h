/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheEntriesEnumerator__h__
#define CacheEntriesEnumerator__h__

#include "nsCOMPtr.h"

class nsIFile;
class nsIDirectoryEnumerator;
class nsIThread;

namespace mozilla {
namespace net {

class CacheFileIOManager;
class CacheFileListener;
class CacheFile;

class CacheEntriesEnumeratorCallback : public nsISupports
{
public:
  virtual void OnFile(CacheFile* aFile) = 0;
};

class CacheEntriesEnumerator
{
public:
  ~CacheEntriesEnumerator();

  bool HasMore();
  nsresult GetNextCacheFile(CacheEntriesEnumeratorCallback* aCallback);
  nsresult GetNextFile(nsIFile** aFile);
  nsresult GetCacheFileFromFile(nsIFile* aFile, CacheEntriesEnumeratorCallback* aCallback);

protected:
  friend class CacheFileIOManager;
  CacheEntriesEnumerator(nsIFile* aEntriesDirectory);
  nsresult Init();

private:
  nsCOMPtr<nsIDirectoryEnumerator> mEnumerator;
  nsCOMPtr<nsIFile> mEntriesDirectory;
  nsCOMPtr<nsIFile> mCurrentFile;

#ifdef DEBUG
  nsCOMPtr<nsIThread> mThreadCheck;
#endif
};

} // net
} // mozilla

#endif
