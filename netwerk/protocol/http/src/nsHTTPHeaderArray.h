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
  NS_IMETHOD GetField(nsIAtom** aResult);
  NS_IMETHOD GetValue(char ** aResult);

  nsHeaderEntry(nsIAtom* aHeaderAtom, const char* aValue);
  virtual ~nsHeaderEntry();

  nsCOMPtr<nsIAtom> mAtom;
  nsString          mValue;
};


class nsHTTPHeaderArray
{
public:
  nsHTTPHeaderArray();
  ~nsHTTPHeaderArray();

  nsresult SetHeader(nsIAtom* aHeader, const char* i_Value);
  nsresult GetHeader(nsIAtom* aHeader, char* *o_Value);
  nsresult GetEnumerator(nsISimpleEnumerator** aResult);

protected:
  PRInt32 GetEntry(nsIAtom* aHeader, nsHeaderEntry** aResult);
  PRBool  IsHeaderMultiple(nsIAtom* aHeader);

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
