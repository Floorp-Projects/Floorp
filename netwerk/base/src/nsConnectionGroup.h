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

#ifndef nsConnectionGroup_h__
#define nsConnectionGroup_h__

#include "nsIConnectionGroup.h"
#include "nsISupportsArray.h"

class nsIProtocolConnection;

class nsConnectionGroup : public nsIConnectionGroup
{
public:
    NS_DECL_ISUPPORTS

    // nsICancelable methods:
    NS_IMETHOD Cancel(void);
    NS_IMETHOD Suspend(void);
    NS_IMETHOD Resume(void);

    // nsICollection methods:
    NS_IMETHOD_(PRUint32) Count(void) const;
    NS_IMETHOD AppendElement(nsISupports *aItem);
    NS_IMETHOD RemoveElement(nsISupports *aItem);
    NS_IMETHOD Enumerate(nsIEnumerator* *result);
    NS_IMETHOD Clear(void);

    // nsIConnectionGroup methods:
    NS_IMETHOD AddChildGroup(nsIConnectionGroup* aGroup);
    NS_IMETHOD RemoveChildGroup(nsIConnectionGroup* aGroup);

    // nsConnectionGroup methods:
    nsConnectionGroup();
    virtual ~nsConnectionGroup();

    nsresult Init();

protected:
    nsISupportsArray* mElements;

};

#endif // nsConnectionGroup_h__
