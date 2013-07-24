/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsEnvironment_h__
#define nsEnvironment_h__

#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"
#include "nsIEnvironment.h"

#define NS_ENVIRONMENT_CID \
  { 0X3D68F92UL, 0X9513, 0X4E25, \
  { 0X9B, 0XE9, 0X7C, 0XB2, 0X39, 0X87, 0X41, 0X72 } }
#define NS_ENVIRONMENT_CONTRACTID "@mozilla.org/process/environment;1"

class nsEnvironment MOZ_FINAL : public nsIEnvironment
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIENVIRONMENT

    static nsresult Create(nsISupports *aOuter, REFNSIID aIID,
                           void **aResult);

private:
    nsEnvironment() : mLock("nsEnvironment.mLock") { }
    ~nsEnvironment();

    mozilla::Mutex mLock;
};

#endif /* !nsEnvironment_h__ */
