/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/*

  The sort service interface. This is a singleton object, and should be
  obtained from the <tt>nsServiceManager</tt>.
 */

#ifndef nsIXULSortService_h__
#define nsIXULSortService_h__

#include "nscore.h"
#include "nsISupports.h"
#include "nsIDOMNode.h"
#include "nsString.h"


// {BFD05261-834C-11d2-8EAC-00805F29F371}
#define NS_IXULSORTSERVICE_IID \
{ 0xbfd05261, 0x834c, 0x11d2, { 0x8e, 0xac, 0x0, 0x80, 0x5f, 0x29, 0xf3, 0x71 } }

class nsIRDFCompositeDataSource;
class nsIContent;
class nsIRDFResource;
class nsIDOMNode;

class nsIXULSortService : public nsISupports {
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IXULSORTSERVICE_IID; return iid; }

    NS_IMETHOD DoSort(nsIDOMNode* node, const nsString& sortResource, const nsString& sortDirection) = 0;
    NS_IMETHOD OpenContainer(nsIRDFCompositeDataSource *db, nsIContent *container,
			nsIRDFResource **flatArray, PRInt32 numElements, PRInt32 elementSize) = 0;
    NS_IMETHOD InsertContainerNode(nsIContent *container, nsIContent *child) = 0;
};


extern nsresult
NS_NewXULSortService(nsIXULSortService** result);

#endif // nsIXULSortService_h__
