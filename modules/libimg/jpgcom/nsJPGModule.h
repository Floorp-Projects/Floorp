/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsJPGModule_h___
#define nsJPGModule_h___

#include "nsIModule.h"

class nsJPGModule : public nsIModule
{
public:
    NS_DECL_ISUPPORTS

    // Construction, Init and desstruction
    nsJPGModule();
    virtual ~nsJPGModule();

    // nsIModule Interfaces
    NS_DECL_NSIMODULE

    // Facility for counting object instances
    int IncrementObjCount() { if (mObjCount == -1) mObjCount = 0; return ++mObjCount; }
    int DecrementObjCount() { if (mObjCount == -1) mObjCount = 0; return --mObjCount; }
    int GetObjCount() { return mObjCount; }

private:
    int mObjCount;
    nsISupports* mClassObject;
};

#endif /* nsJPGModule_h___ */
