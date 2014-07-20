/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsBidiKeyboard
#define __nsBidiKeyboard
#include "nsIBidiKeyboard.h"

class nsBidiKeyboard : public nsIBidiKeyboard
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIBIDIKEYBOARD

    nsBidiKeyboard();

protected:
    virtual ~nsBidiKeyboard();
};


#endif // __nsBidiKeyboard
