/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsScreenManagerCocoa_h_
#define nsScreenManagerCocoa_h_

#import <Cocoa/Cocoa.h>

#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "nsIScreenManager.h"
#include "nsScreenCocoa.h"

class nsScreenManagerCocoa : public nsIScreenManager
{
public:
    nsScreenManagerCocoa();

    NS_DECL_ISUPPORTS
    NS_DECL_NSISCREENMANAGER

protected:
    virtual ~nsScreenManagerCocoa();

private:

    nsScreenCocoa *ScreenForCocoaScreen(NSScreen *screen);
    nsTArray< nsRefPtr<nsScreenCocoa> > mScreenList;
};

#endif // nsScreenManagerCocoa_h_
