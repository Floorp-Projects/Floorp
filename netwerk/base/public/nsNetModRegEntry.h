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

class nsNetModRegEntry : nsINetModRegEntry {
public:
    // nsISupports
    NS_DECL_ISUPPORTS

    // nsINetModRegEntry
    NS_IMETHOD GetmNotify(nsINetNotify **aNotify);
    NS_IMETHOD GetmEventQ(nsIEventQueue **aEventQ);
    NS_IMETHOD GetmTopic(char **aTopic);

    // nsNetModRegEntry
    nsNetModRegEntry(char *aTopic, nsIEventQueue *aEventQ, nsINetNotify *aNotify);
    ~nsNetModRegEntry();

private:
    char                *mTopic;
    nsIEventQueue       *mEventQ;
    nsINetNotify        *mNotify;
};

#endif //___nsNetModRegEntry_h___