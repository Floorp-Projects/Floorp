/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Doug Turner <dougt@netscape.com>
 */

#ifndef _NSIDIRENUMERATORIMPLWIN_H_
#define _NSIDIRENUMERATORIMPLWIN_H_

#include "nscore.h"
#include "prio.h"
#include "nsIDirectoryEnumerator.h"
#include "nsIDirEnumeratorImpl.h"

class NS_COM  
nsIDirEnumeratorImpl : public nsIDirectoryEnumerator
{
public:
    NS_DEFINE_STATIC_CID_ACCESSOR(NS_IDIRECTORYENUMERATOR_CID)
    
    nsIDirEnumeratorImpl();
    virtual ~nsIDirEnumeratorImpl();

    static NS_METHOD Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

    // nsISupports interface
    NS_DECL_ISUPPORTS
    
    NS_DECL_NSISIMPLEENUMERATOR

    // nsIDirectoryIterator interface
    NS_DECL_NSIDIRECTORYENUMERATOR
    
protected:
    PRDir* mDir;

    nsIFile *mParent;
    nsIFile *mNext;
    PRBool mResolveSymlinks;
    const nsIFile *mStarting;
};

#endif
