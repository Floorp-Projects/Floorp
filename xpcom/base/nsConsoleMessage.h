/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsconsolemessage_h__
#define __nsconsolemessage_h__

#include "mozilla/Attributes.h"

#include "nsIConsoleMessage.h"
#include "nsString.h"

class nsConsoleMessage MOZ_FINAL : public nsIConsoleMessage {
public:
    nsConsoleMessage();
    nsConsoleMessage(const PRUnichar *message);

    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSICONSOLEMESSAGE

private:
    ~nsConsoleMessage() {}

    int64_t mTimeStamp;
    nsString mMessage;
};

#endif /* __nsconsolemessage_h__ */
