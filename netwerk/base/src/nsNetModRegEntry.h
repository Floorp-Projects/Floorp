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
#include "nsCOMPtr.h"

class nsNetModRegEntry : public nsINetModRegEntry {
public:
    // nsISupports
    NS_DECL_ISUPPORTS

    // nsINetModRegEntry
    NS_IMETHOD GetSyncProxy(nsINetNotify * *aSyncProxy);
    NS_IMETHOD GetAsyncProxy(nsINetNotify * *aAsyncProxy);
    NS_IMETHOD GetTopic(char * *aTopic);
    NS_IMETHOD Equals(nsINetModRegEntry* aEntry, PRBool *_retVal);

    // nsNetModRegEntry
    nsNetModRegEntry(const char *aTopic, nsINetNotify *aNotify, nsresult *result);
    virtual ~nsNetModRegEntry();

protected:
    char                    *mTopic;
    nsCOMPtr<nsINetNotify>  mSyncProxy;
    nsCOMPtr<nsINetNotify>  mAsyncProxy;
    nsCOMPtr<nsIEventQueue> mEventQ;
};

#endif //___nsNetModRegEntry_h___
