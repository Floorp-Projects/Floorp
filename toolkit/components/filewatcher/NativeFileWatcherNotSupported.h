/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nativefilewatcher_h__
#define mozilla_nativefilewatcher_h__

#include "nsINativeFileWatcher.h"

namespace mozilla {

class NativeFileWatcherService MOZ_FINAL : public nsINativeFileWatcherService
{
public:
  NS_DECL_ISUPPORTS

  NativeFileWatcherService()
  {
  };

  nsresult Init()
  {
    return NS_OK;
  };

  NS_IMETHODIMP AddPath(const nsAString& aPathToWatch,
                        nsINativeFileWatcherCallback* aOnChange,
                        nsINativeFileWatcherErrorCallback* aOnError,
                        nsINativeFileWatcherSuccessCallback* aOnSuccess)
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  };

  NS_IMETHODIMP RemovePath(const nsAString& aPathToRemove,
                           nsINativeFileWatcherCallback* aOnChange,
                           nsINativeFileWatcherErrorCallback* aOnError,
                           nsINativeFileWatcherSuccessCallback* aOnSuccess)
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  };

private:
  ~NativeFileWatcherService() { };
  NativeFileWatcherService(const NativeFileWatcherService& other) MOZ_DELETE;
  void operator=(const NativeFileWatcherService& other) MOZ_DELETE;
};

NS_IMPL_ISUPPORTS(NativeFileWatcherService, nsINativeFileWatcherService);

} // namespace mozilla

#endif // mozilla_nativefilewatcher_h__
