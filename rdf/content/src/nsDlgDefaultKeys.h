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

#ifndef nsIDlgDefaultKeys_h__
#define nsIDlgDefaultKeys_h__

// {daedcb42-1dd1-11b2-b1d2-caf06cb40387}
#define NS_IDLGDEFAULTKEYS_IID \
{ 0xdaedcb42, 0x1dd1, 0x11b2, { 0xb1, 0xd2, 0xca, 0xf0, 0x6c, 0xb4, 0x3, 0x87 } }

class nsIDOMElement;

class nsIDlgDefaultKeys: public nsISupports {
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IDLGDEFAULTKEYS_IID; return iid; }

    NS_IMETHOD Init(nsIDOMElement* anElement, nsIDOMDocument* aDocument) = 0;
};

extern nsresult
NS_NewDlgDefaultKeys(nsIDlgDefaultKeys** result);

#endif // nsIDlgDefaultKeys_h__
