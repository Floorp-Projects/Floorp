/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */


/*

  A helper class used to implement attributes.

*/

#ifndef nsGenericAttribute_h__
#define nsGenericAttribute_h__

class nsGenericAttribute
{
public:
    nsGenericAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, const nsString& aValue)
        : mNameSpaceID(aNameSpaceID),
          mName(aName),
          mValue(aValue)
    {
        NS_IF_ADDREF(mName);
    }

    ~nsGenericAttribute(void)
    {
        NS_IF_RELEASE(mName);
    }

    PRInt32   mNameSpaceID;
    nsIAtom*  mName;
    nsString  mValue;
};


#endif // nsGenericAttribute_h__

