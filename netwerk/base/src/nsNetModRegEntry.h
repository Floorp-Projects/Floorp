/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef ___nsNetModRegEntry_h___
#define ___nsNetModRegEntry_h___

#include "nsINetModRegEntry.h"
#include "prlock.h"
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
    nsCOMPtr<nsINetNotify>  mRealNotifier;
    nsCOMPtr<nsINetNotify>  mSyncProxy;
    nsCOMPtr<nsINetNotify>  mAsyncProxy;
    nsCOMPtr<nsIEventQueue> mEventQ;

    nsresult BuildProxy(PRBool sync);

    PRMonitor*          mMonitor;
};

#endif //___nsNetModRegEntry_h___
