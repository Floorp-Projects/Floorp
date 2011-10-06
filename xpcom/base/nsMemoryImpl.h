/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsMemoryImpl_h__
#define nsMemoryImpl_h__

#include "nsIMemory.h"
#include "nsIRunnable.h"
#include "prtime.h"

// nsMemoryImpl is a static object. We can do this because it doesn't have
// a constructor/destructor or any instance members. Please don't add
// instance member variables, only static member variables.

class nsMemoryImpl : public nsIMemory
{
public:
    // We don't use the generic macros because we are a special static object
    NS_IMETHOD QueryInterface(REFNSIID aIID, void** aResult);
    NS_IMETHOD_(nsrefcnt) AddRef(void) { return 1; }
    NS_IMETHOD_(nsrefcnt) Release(void) { return 1; }

    NS_DECL_NSIMEMORY

    static nsresult Create(nsISupports* outer,
                           const nsIID& aIID, void **aResult);

    NS_HIDDEN_(nsresult) FlushMemory(const PRUnichar* aReason, bool aImmediate);
    NS_HIDDEN_(nsresult) RunFlushers(const PRUnichar* aReason);

protected:
    struct FlushEvent : public nsIRunnable {
        NS_DECL_ISUPPORTS_INHERITED
        NS_DECL_NSIRUNNABLE
        const PRUnichar* mReason;
    };

    static PRInt32    sIsFlushing;
    static FlushEvent sFlushEvent;
    static PRIntervalTime sLastFlushTime;
};

#endif // nsMemoryImpl_h__
