/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nativefilewatcher_h__
#define mozilla_nativefilewatcher_h__

#include "nsINativeFileWatcher.h"

namespace mozilla {

class NativeFileWatcherService final : public nsINativeFileWatcherService {
 public:
  NS_DECL_ISUPPORTS

  NativeFileWatcherService(){};

  nsresult Init() { return NS_OK; };

  NS_IMETHOD AddPath(const nsAString& aPathToWatch,
                     nsINativeFileWatcherCallback* aOnChange,
                     nsINativeFileWatcherErrorCallback* aOnError,
                     nsINativeFileWatcherSuccessCallback* aOnSuccess) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  };

  NS_IMETHOD RemovePath(
      const nsAString& aPathToRemove, nsINativeFileWatcherCallback* aOnChange,
      nsINativeFileWatcherErrorCallback* aOnError,
      nsINativeFileWatcherSuccessCallback* aOnSuccess) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  };

 private:
  ~NativeFileWatcherService(){};
  NativeFileWatcherService(const NativeFileWatcherService& other) = delete;
  void operator=(const NativeFileWatcherService& other) = delete;
};

}  // namespace mozilla

#endif  // mozilla_nativefilewatcher_h__
