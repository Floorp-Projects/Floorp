/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef ___nsNetModRegEntry_h___
#define ___nsNetModRegEntry_h___

#include "nsINetModRegEntry.h"
#include "nsIEventQueue.h"

class nsNetModRegEntry : public nsINetModRegEntry {
public:
    // nsISupports
    NS_DECL_ISUPPORTS

    // nsINetModRegEntry
    NS_IMETHOD GetMNotify(nsINetNotify **aNotify);
    NS_IMETHOD GetMEventQ(nsIEventQueue **aEventQ);
    NS_IMETHOD GetMTopic(char **aTopic);
    NS_IMETHOD GetMCID(nsCID **aCID);

    NS_IMETHOD Equals(nsINetModRegEntry* aEntry, PRBool *_retVal);

    // nsNetModRegEntry
    nsNetModRegEntry(const char *aTopic, nsIEventQueue *aEventQ, nsINetNotify *aNotify, nsCID aCID);
    virtual ~nsNetModRegEntry();

    char                *mTopic;
    nsIEventQueue       *mEventQ;
    nsINetNotify        *mNotify;
    nsCID               mCID;
};

#endif //___nsNetModRegEntry_h___
