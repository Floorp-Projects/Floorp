/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNativeAppSupportBase_h__
#define nsNativeAppSupportBase_h__

#include "nsAppRunner.h"
#include "nsINativeAppSupport.h"

// nsNativeAppSupportBase
//
// This is a default implementation of the nsINativeAppSupport interface
// declared in mozilla/xpfe/appshell/public/nsINativeAppSupport.h.

class nsNativeAppSupportBase : public nsINativeAppSupport {
public:
    nsNativeAppSupportBase();
    virtual ~nsNativeAppSupportBase();

    NS_DECL_ISUPPORTS
    NS_DECL_NSINATIVEAPPSUPPORT
};

#endif
