/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUserIdleServiceGTK_h__
#define nsUserIdleServiceGTK_h__

#include "nsUserIdleService.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <gdk/gdkx.h>

typedef struct {
  Window window;               // Screen saver window
  int state;                   // ScreenSaver(Off,On,Disabled)
  int kind;                    // ScreenSaver(Blanked,Internal,External)
  unsigned long til_or_since;  // milliseconds since/til screensaver kicks in
  unsigned long idle;          // milliseconds idle
  unsigned long event_mask;    // event stuff
} XScreenSaverInfo;

class nsUserIdleServiceGTK : public nsUserIdleService {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(nsUserIdleServiceGTK, nsUserIdleService)

  virtual bool PollIdleTime(uint32_t* aIdleTime) override;

  static already_AddRefed<nsUserIdleServiceGTK> GetInstance() {
    RefPtr<nsUserIdleServiceGTK> idleService =
        nsUserIdleService::GetInstance().downcast<nsUserIdleServiceGTK>();
    if (!idleService) {
      idleService = new nsUserIdleServiceGTK();
    }

    return idleService.forget();
  }

 private:
  ~nsUserIdleServiceGTK();
  XScreenSaverInfo* mXssInfo;

 protected:
  nsUserIdleServiceGTK();
  virtual bool UsePollMode() override;
};

#endif  // nsUserIdleServiceGTK_h__
