/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Original Author: Gagan Saksena <gagan@netscape.com>
 *
 * Contributor(s): 
 */

#ifndef nsHTTPHeaderArray_h___
#define nsHTTPHeaderArray_h___

#include "nscore.h"
#include "nsIHTTPHeader.h"
#include "nsISupportsArray.h"
#include "nsIAtom.h"
#include "nsISimpleEnumerator.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsVoidArray.h"

class nsHeaderEntry : public nsIHTTPHeader
{
public:
  // nsISupports methods:
  NS_DECL_ISUPPORTS

  // nsIHTTPHeader methods:
  NS_DECL_NSIHTTPHEADER

  nsHeaderEntry(nsIAtom* aHeaderAtom, const char* aValue);
  virtual ~nsHeaderEntry();

  nsCOMPtr<nsIAtom> mAtom;
  nsCString         mValue;
};


class nsHTTPHeaderArray
{
public:
  nsHTTPHeaderArray();
  ~nsHTTPHeaderArray();

  nsresult      SetHeader(nsIAtom* aHeader, const char* i_Value);
  nsresult      GetHeader(nsIAtom* aHeader, char* *o_Value);
  nsresult      GetEnumerator(nsISimpleEnumerator** aResult);
  static void   GetStandardHeaderName(nsIAtom* aHeader, const char** aResult);

  void Clear ();

protected:
  PRInt32   GetEntry(nsIAtom* aHeader, nsHeaderEntry** aResult);
  PRBool    IsHeaderMultiple(nsIAtom* aHeader);

protected:
  nsCOMPtr<nsISupportsArray> mHTTPHeaders;
};


class nsHTTPHeaderEnumerator : public nsISimpleEnumerator
{
public:

  // nsISupports methods:
  NS_DECL_ISUPPORTS

  // nsISimpleEnumerator methods:
  NS_IMETHOD HasMoreElements(PRBool* aResult);
  NS_IMETHOD GetNext(nsISupports** aResult);

  // Class methods:
  nsHTTPHeaderEnumerator(nsISupportsArray* aHeaderArray);
  virtual ~nsHTTPHeaderEnumerator();

protected:
  nsCOMPtr<nsISupportsArray> mHeaderArray;
  PRUint32 mIndex;
};

#endif /* nsHTTPHeaderArray_h___ */
