/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNSSDialogs_h
#define nsNSSDialogs_h

#include "nsCOMPtr.h"
#include "nsICertificateDialogs.h"
#include "nsIClientAuthDialogs.h"
#include "nsIStringBundle.h"
#include "nsITokenDialogs.h"
#include "nsITokenPasswordDialogs.h"

#define NS_NSSDIALOGS_CID                            \
  {                                                  \
    0x518e071f, 0x1dd2, 0x11b2, {                    \
      0x93, 0x7e, 0xc4, 0x5f, 0x14, 0xde, 0xf7, 0x78 \
    }                                                \
  }

class nsNSSDialogs : public nsICertificateDialogs,
                     public nsIClientAuthDialogs,
                     public nsITokenDialogs,
                     public nsITokenPasswordDialogs {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITOKENPASSWORDDIALOGS
  NS_DECL_NSICERTIFICATEDIALOGS
  NS_DECL_NSICLIENTAUTHDIALOGS
  NS_DECL_NSITOKENDIALOGS
  nsNSSDialogs();

  nsresult Init();

 protected:
  virtual ~nsNSSDialogs();
  nsCOMPtr<nsIStringBundle> mPIPStringBundle;
};

#endif  // nsNSSDialogs_h
