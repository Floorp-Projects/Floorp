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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsEnumeratorUtils_h__
#define nsEnumeratorUtils_h__

#include "nsIEnumerator.h"
#include "nsISupportsArray.h"

class NS_COM nsArrayEnumerator : public nsISimpleEnumerator
{
public:
    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsISimpleEnumerator interface
    NS_IMETHOD HasMoreElements(PRBool* aResult);
    NS_IMETHOD GetNext(nsISupports** aResult);

    // nsArrayEnumerator methods
    nsArrayEnumerator(nsISupportsArray* aValueArray);

private:
    ~nsArrayEnumerator(void);

protected:
    nsISupportsArray* mValueArray;
    PRInt32 mIndex;
};

extern NS_COM nsresult
NS_NewArrayEnumerator(nsISimpleEnumerator* *result,
                      nsISupportsArray* array);

////////////////////////////////////////////////////////////////////////////////

class NS_COM nsSingletonEnumerator : public nsISimpleEnumerator
{
public:
    NS_DECL_ISUPPORTS

    // nsISimpleEnumerator methods
    NS_IMETHOD HasMoreElements(PRBool* aResult);
    NS_IMETHOD GetNext(nsISupports** aResult);

    nsSingletonEnumerator(nsISupports* aValue);

private:
    ~nsSingletonEnumerator();

protected:
    nsISupports* mValue;
    PRBool mConsumed;
};

extern "C" NS_COM nsresult
NS_NewSingletonEnumerator(nsISimpleEnumerator* *result,
                          nsISupports* singleton);

////////////////////////////////////////////////////////////////////////

class NS_COM nsUnionEnumerator : public nsISimpleEnumerator
{
public:
    NS_DECL_ISUPPORTS

    // nsISimpleEnumerator methods
    NS_IMETHOD HasMoreElements(PRBool* aResult);
    NS_IMETHOD GetNext(nsISupports** aResult);

    nsUnionEnumerator(nsISimpleEnumerator* firstEnumerator,
                      nsISimpleEnumerator* secondEnumerator);

private:
    ~nsUnionEnumerator();

protected:
    nsCOMPtr<nsISimpleEnumerator> mFirstEnumerator, mSecondEnumerator;
    PRBool mConsumed;
    PRBool mAtSecond;
};

extern "C" NS_COM nsresult
NS_NewUnionEnumerator(nsISimpleEnumerator* *result,
                      nsISimpleEnumerator* firstEnumerator,
                      nsISimpleEnumerator* secondEnumerator);

////////////////////////////////////////////////////////////////////////

#endif /* nsEnumeratorUtils_h__ */
