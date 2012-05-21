/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef __nsLookAndFeel
#define __nsLookAndFeel

#include "nsXPLookAndFeel.h"

class nsLookAndFeel : public nsXPLookAndFeel
{
public:
    nsLookAndFeel();
    virtual ~nsLookAndFeel();

    virtual bool GetFontImpl(FontID aID, nsString& aName, gfxFontStyle& aStyle);
    virtual nsresult GetIntImpl(IntID aID, PRInt32 &aResult);

protected:
    virtual nsresult NativeGetColor(ColorID aID, nscolor &aColor);
};

#endif
