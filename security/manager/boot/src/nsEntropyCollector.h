/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsEntropyCollector_h___
#define nsEntropyCollector_h___

#include "nsIEntropyCollector.h"
#include "nsIBufEntropyCollector.h"
#include "nsCOMPtr.h"

#define NS_ENTROPYCOLLECTOR_CID \
 { /* 34587f4a-be18-43c0-9112-b782b08c0add */       \
  0x34587f4a, 0xbe18, 0x43c0,                       \
 {0x91, 0x12, 0xb7, 0x82, 0xb0, 0x8c, 0x0a, 0xdd} }

class nsEntropyCollector : public nsIBufEntropyCollector
{
  public:
    nsEntropyCollector();
    virtual ~nsEntropyCollector();

    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIENTROPYCOLLECTOR
    NS_DECL_NSIBUFENTROPYCOLLECTOR

    enum { entropy_buffer_size = 1024 };

  protected:
    unsigned char mEntropyCache[entropy_buffer_size];
    int32_t mBytesCollected;
    unsigned char *mWritePointer;
    nsCOMPtr<nsIEntropyCollector> mForwardTarget;
};

#endif /* !defined nsEntropyCollector_h__ */
