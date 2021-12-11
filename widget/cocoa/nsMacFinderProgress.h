/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _MACFINDERPROGRESS_H_
#define _MACFINDERPROGRESS_H_

#include "nsIMacFinderProgress.h"
#include "nsCOMPtr.h"

class nsMacFinderProgress : public nsIMacFinderProgress {
 public:
  nsMacFinderProgress();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMACFINDERPROGRESS

 protected:
  virtual ~nsMacFinderProgress();

  NSProgress* mProgress;
};

#endif
