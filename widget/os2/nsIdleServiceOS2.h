/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIdleServiceOS2_h__
#define nsIdleServiceOS2_h__

#include "nsIdleService.h"
#define INCL_DOSMODULEMGR
#define INCL_DOSERRORS
#include <os2.h>

class nsIdleServiceOS2 : public nsIdleService
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  // ask the DSSaver DLL (sscore.dll) for the time of the last activity
  bool PollIdleTime(PRUint32 *aIdleTime);

  static already_AddRefed<nsIdleServiceOS2> GetInstance()
  {
    nsIdleServiceOS2* idleService =
      static_cast<nsIdleServiceOS2*>(nsIdleService::GetInstance().get());
    if (!idleService) {
      idleService = new nsIdleServiceOS2();
      NS_ADDREF(idleService);
    }
    
    return idleService;
  }
  
private:
  HMODULE mHMod; // module handle for screensaver DLL
  bool mInitialized; // fully initialized (function found in screensaver DLL?)

protected:
  nsIdleServiceOS2();
  ~nsIdleServiceOS2();
  bool UsePollMode();
};

#endif // nsIdleServiceOS2_h__
