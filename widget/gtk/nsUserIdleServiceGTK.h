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

class nsUserIdleServiceGTK;

class UserIdleServiceImpl {
 public:
  explicit UserIdleServiceImpl(nsUserIdleServiceGTK* aUserIdleService)
      : mUserIdleServiceGTK(aUserIdleService){};

  virtual bool PollIdleTime(uint32_t* aIdleTime) = 0;
  virtual bool ProbeImplementation() = 0;

  virtual ~UserIdleServiceImpl() = default;

 protected:
  nsUserIdleServiceGTK* mUserIdleServiceGTK;
};

#define IDLE_SERVICE_MUTTER 0
#define IDLE_SERVICE_XSCREENSAVER 1
#define IDLE_SERVICE_NONE 2

class nsUserIdleServiceGTK : public nsUserIdleService {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(nsUserIdleServiceGTK, nsUserIdleService)

  // The idle time in ms
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
      idleService->ProbeService();
    }

    return idleService.forget();
  }

  void ProbeService();
  void AcceptServiceCallback();
  void RejectAndTryNextServiceCallback();

 protected:
  nsUserIdleServiceGTK() = default;

 private:
  ~nsUserIdleServiceGTK() = default;

  mozilla::UniquePtr<UserIdleServiceImpl> mIdleService;
#ifdef MOZ_ENABLE_DBUS
  int mIdleServiceType = IDLE_SERVICE_MUTTER;
#else
  int mIdleServiceType = IDLE_SERVICE_XSCREENSAVER;
#endif
  // We have a working idle service.
  bool mIdleServiceInitialized = false;
};

#endif  // nsUserIdleServiceGTK_h__
