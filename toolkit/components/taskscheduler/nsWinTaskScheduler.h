/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WINTASKSCHEDULER_H_
#define WINTASKSCHEDULER_H_

#include "nsIWinTaskSchedulerService.h"

class nsWinTaskSchedulerService : public nsIWinTaskSchedulerService {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWINTASKSCHEDULERSERVICE

  nsWinTaskSchedulerService() = default;

 protected:
  virtual ~nsWinTaskSchedulerService() = default;
};

#endif /* WINTASKSCHEDULER_H_ */
