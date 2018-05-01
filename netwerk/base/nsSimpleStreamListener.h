/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSimpleStreamListener_h__
#define nsSimpleStreamListener_h__

#include "nsISimpleStreamListener.h"
#include "nsIOutputStream.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace net {

class nsSimpleStreamListener : public nsISimpleStreamListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSISIMPLESTREAMLISTENER

    nsSimpleStreamListener() = default;

protected:
    virtual ~nsSimpleStreamListener() = default;

    nsCOMPtr<nsIOutputStream>    mSink;
    nsCOMPtr<nsIRequestObserver> mObserver;
};

} // namespace net
} // namespace mozilla

#endif
