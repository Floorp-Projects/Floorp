/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#ifndef nsISupportsArray_h___
#define nsISupportsArray_h___

#include "nsCom.h"
#include "nsISupports.h"

// {791eafa0-b9e6-11d1-8031-006008159b5a}
#define NS_ISUPPORTSARRAY_IID \
{0x791eafa0, 0xb9e6, 0x11d1,  \
    {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

// Enumerator callback function. Return PR_FALSE to stop
typedef PRBool (*nsISupportsArrayEnumFunc)(nsISupports* aElement, void *aData);

class nsISupportsArray : public nsISupports {
public:
  NS_IMETHOD_(nsISupportsArray&) operator=(const nsISupportsArray& other) = 0;
  NS_IMETHOD_(PRBool) operator==(const nsISupportsArray& other) const = 0;
  NS_IMETHOD_(PRBool) Equals(const nsISupportsArray* other) const = 0;
 
  NS_IMETHOD_(PRInt32) Count(void) const = 0;

  NS_IMETHOD_(nsISupports*)  ElementAt(PRInt32 aIndex) const = 0;
  NS_IMETHOD_(nsISupports*)  operator[](PRInt32 aIndex) const = 0;

  NS_IMETHOD_(PRInt32)  IndexOf(const nsISupports* aPossibleElement, PRInt32 aStartIndex = 0) const = 0;
  NS_IMETHOD_(PRInt32)  LastIndexOf(const nsISupports* aPossibleElement) const = 0;

  NS_IMETHOD_(PRBool) InsertElementAt(nsISupports* aElement, PRInt32 aIndex) = 0;

  NS_IMETHOD_(PRBool) ReplaceElementAt(nsISupports* aElement, PRInt32 aIndex) = 0;

  NS_IMETHOD_(PRBool) AppendElement(nsISupports* aElement) = 0;

  NS_IMETHOD_(PRBool) RemoveElementAt(PRInt32 aIndex) = 0;
  NS_IMETHOD_(PRBool) RemoveElement(const nsISupports* aElement, PRInt32 aStartIndex = 0) = 0;
  NS_IMETHOD_(PRBool) RemoveLastElement(const nsISupports* aElement) = 0;
  NS_IMETHOD_(void)   Clear(void) = 0;

  NS_IMETHOD_(void)   Compact(void) = 0;

  NS_IMETHOD_(PRBool) EnumerateForwards(nsISupportsArrayEnumFunc aFunc, void* aData) = 0;
  NS_IMETHOD_(PRBool) EnumerateBackwards(nsISupportsArrayEnumFunc aFunc, void* aData) = 0;

private:
  // Copy constructors are not allowed
// XXX test wether this has to be here  nsISupportsArray(const nsISupportsArray& other);
};

extern NS_COM nsresult
  NS_NewISupportsArray(nsISupportsArray** aInstancePtrResult);


#endif // nsISupportsArray_h___
