/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Boris Zbarsky <bzbarsky@mit.edu> (Original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * URI class to be used for cases when a simple URI actually resolves to some
 * other sort of URI, with the latter being what's loaded when the load
 * happens.  All objects of this class should always be immutable, so that the
 * inner URI and this URI don't get out of sync.  The Clone() implementation
 * will guarantee this for the clone, but it's up to the protocol handlers
 * creating these URIs to ensure that in the first place.  The innerURI passed
 * to this URI will be set immutable if possible.
 */

#ifndef nsSimpleNestedURI_h__
#define nsSimpleNestedURI_h__

#include "nsCOMPtr.h"
#include "nsSimpleURI.h"
#include "nsINestedURI.h"

class nsSimpleNestedURI : public nsSimpleURI,
                          public nsINestedURI
{
public:
    // To be used by deserialization only.  Leaves this object in an
    // uninitialized state that will throw on most accesses.
    nsSimpleNestedURI()
    {
    }

    // Constructor that should generally be used when constructing an object of
    // this class with |operator new|.
    nsSimpleNestedURI(nsIURI* innerURI);

    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSINESTEDURI
    NS_DECL_NSIIPCSERIALIZABLE

    // Overrides for various methods nsSimpleURI implements follow.
  
    // nsIURI overrides
    NS_IMETHOD Equals(nsIURI* other, PRBool *result);
    virtual nsSimpleURI* StartClone();

    // nsISerializable overrides
    NS_IMETHOD Read(nsIObjectInputStream* aStream);
    NS_IMETHOD Write(nsIObjectOutputStream* aStream);

    // Override the nsIClassInfo method GetClassIDNoAlloc to make sure our
    // nsISerializable impl works right.
    NS_IMETHOD GetClassIDNoAlloc(nsCID *aClassIDNoAlloc);  

protected:
    nsCOMPtr<nsIURI> mInnerURI;
};

#endif /* nsSimpleNestedURI_h__ */
