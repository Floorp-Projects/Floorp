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

#ifndef nsIXULDocumentInfo_h__
#define nsIXULDocumentInfo_h__

#include "nsISupports.h"

// {798E24A1-D7A5-11d2-BF86-00105A1B0627}
#define NS_IXULDOCUMENTINFO_IID \
{ 0x798e24a1, 0xd7a5, 0x11d2, { 0xbf, 0x86, 0x0, 0x10, 0x5a, 0x1b, 0x6, 0x27 } }

class nsIDocument;
class nsIXULContentSink;

class nsIXULDocumentInfo : public nsISupports {
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IXULDOCUMENTINFO_IID; return iid; }

    NS_IMETHOD Init(nsIDocument* aParentDoc, nsIXULContentSink* aParentSink) = 0;
    NS_IMETHOD GetDocument(nsIDocument** aResult) = 0;
    NS_IMETHOD GetContentSink(nsIXULContentSink** aResult) = 0;
};


extern nsresult
NS_NewXULDocumentInfo(nsIXULDocumentInfo** result);

#endif // nsIXULDocumentInfo_h__
