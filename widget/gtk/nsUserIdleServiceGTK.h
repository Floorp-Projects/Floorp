/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUserIdleServiceGTK_h__
#define nsUserIdleServiceGTK_h__

#include "nsUserIdleService.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/UniquePtr.h"

class UserIdleServiceImpl {
 public:
  virtual bool PollIdleTime(uint32_t* aIdleTime) = 0;

  bool IsAvailable() const { return mInitialized; }
  virtual ~UserIdleServiceImpl() = default;

 protected:
  bool mInitialized = false;
};

class nsUserIdleServiceGTK : public nsUserIdleService {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(nsUserIdleServiceGTK, nsUserIdleService)

  virtual bool PollIdleTime(uint32_t* aIdleTime) override;

  static already_AddRefed<nsUserIdleServiceGTK> GetInstance() {
    RefPtr<nsUserIdleServiceGTK> idleService =
        nsUserIdleService::GetInstance().downcast<nsUserIdleServiceGTK>();
    if (!idleService) {
      // Avoid late instantiation or resurrection during shutdown.
      if (mozilla::AppShutdown::IsInOrBeyond(
              mozilla::ShutdownPhase::AppShutdownConfirmed)) {
        return nullptr;
      }
      idleService = new nsUserIdleServiceGTK();
    }

    return idleService.forget();
  }

 protected:
  nsUserIdleServiceGTK();

 private:
  ~nsUserIdleServiceGTK(){};

  mozilla::UniquePtr<UserIdleServiceImpl> mIdleService;
};

#endif  // nsUserIdleServiceGTK_h__
