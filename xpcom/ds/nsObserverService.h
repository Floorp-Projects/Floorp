/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsObserverService_h___
#define nsObserverService_h___

#include "nsIObserverService.h"
#include "nsObserverList.h"
#include "nsTHashtable.h"
#include "mozilla/Attributes.h"

// {D07F5195-E3D1-11d2-8ACD-00105A1B8860}
#define NS_OBSERVERSERVICE_CID \
    { 0xd07f5195, 0xe3d1, 0x11d2, { 0x8a, 0xcd, 0x0, 0x10, 0x5a, 0x1b, 0x88, 0x60 } }

class nsObserverService MOZ_FINAL : public nsIObserverService {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_OBSERVERSERVICE_CID)

  nsObserverService();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVERSERVICE
  
  void Shutdown();

  static nsresult
  Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

  // Unmark any strongly held observers implemented in JS so the cycle
  // collector will not traverse them.
  NS_IMETHOD UnmarkGrayStrongObservers();

private:
  ~nsObserverService(void);

  bool mShuttingDown;
  nsTHashtable<nsObserverList> mObserverTopicTable;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsObserverService, NS_OBSERVERSERVICE_CID)

#endif /* nsObserverService_h___ */
