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


#include "nsHTTPHeaderArray.h"
#include "nsHTTPAtoms.h"


nsHeaderEntry::nsHeaderEntry(nsIAtom* aHeaderAtom, const char* aValue)
{
  NS_INIT_REFCNT();

  mAtom  = aHeaderAtom;
  mValue = aValue;
}


nsHeaderEntry::~nsHeaderEntry()
{
  mAtom  = null_nsCOMPtr();
}


NS_IMPL_ISUPPORTS(nsHeaderEntry, nsCOMTypeInfo<nsIHTTPHeader>::GetIID())


NS_IMETHODIMP
nsHeaderEntry::GetField(nsIAtom** aResult)
{
  nsresult rv = NS_OK;

  if (aResult) {
    *aResult = mAtom;
    NS_IF_ADDREF(*aResult);
  } else {
    rv = NS_ERROR_NULL_POINTER;
  }
  return rv;
}


NS_IMETHODIMP
nsHeaderEntry::GetValue(char ** aResult)
{
  nsresult rv = NS_OK;

  if (aResult) {
    *aResult = mValue.ToNewCString();
  } else {
    rv = NS_ERROR_NULL_POINTER;
  }
  return rv;
}



nsHTTPHeaderArray::nsHTTPHeaderArray()
{
  (void)NS_NewISupportsArray(getter_AddRefs(m_pHTTPHeaders));
}


nsHTTPHeaderArray::~nsHTTPHeaderArray()
{
  if (m_pHTTPHeaders) {
    m_pHTTPHeaders->Clear();
  }
  m_pHTTPHeaders = null_nsCOMPtr();
}


nsresult nsHTTPHeaderArray::SetHeader(nsIAtom* aHeader, const char* aValue)
{
  nsCOMPtr<nsHeaderEntry> entry;
  PRInt32 i;

  NS_ASSERTION(m_pHTTPHeaders, "header array doesn't exist.");
  if (!m_pHTTPHeaders) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  i = GetEntry(aHeader, getter_AddRefs(entry));

  //
  // If a NULL value is passed in, then delete the header entry...
  //
  if (!aValue) {
    if (entry) {
      m_pHTTPHeaders->RemoveElementAt(i);
    }
    return NS_OK;
  }

  //
  // Create a new entry or append the value to an existing entry...
  //
  if (!entry) {
    entry = new nsHeaderEntry(aHeader, aValue);
    if (!entry) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    m_pHTTPHeaders->AppendElement(entry);
  } 
  // 
  // Append the new value to the existing string
  //
  else if (IsHeaderMultiple(aHeader)) {
    entry->mValue.Append(", ");
    entry->mValue.Append(aValue);
  }
  //
  // Replace the existing string with the new value
  //
  else {
    entry->mValue.SetString(aValue);
  }

  return NS_OK;
}


nsresult nsHTTPHeaderArray::GetHeader(nsIAtom* aHeader, char* *aResult)
{
  nsCOMPtr<nsHeaderEntry> entry;

  *aResult = nsnull;

  NS_ASSERTION(m_pHTTPHeaders, "header array doesn't exist.");
  if (!m_pHTTPHeaders) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  (void) GetEntry(aHeader, getter_AddRefs(entry));
  if (entry) {
    *aResult = entry->mValue.ToNewCString();
    if (!*aResult) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  return NS_OK;
}


PRInt32 nsHTTPHeaderArray::GetEntry(nsIAtom* aHeader, nsHeaderEntry** aResult)
{
  PRUint32 i, count;

  *aResult = nsnull;  

  count = 0;
  (void)m_pHTTPHeaders->Count(&count);
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISupports> entry;
    nsHeaderEntry* element;
    
    entry   = m_pHTTPHeaders->ElementAt(i);
    element = (nsHeaderEntry*)entry.get();
    if (aHeader == element->mAtom.get()) {
      *aResult = element;
      NS_ADDREF(*aResult);
      return i;
    }
  }
  return -1;
}


nsresult nsHTTPHeaderArray::GetEnumerator(nsISimpleEnumerator** aResult)
{
  nsresult rv = NS_OK;
  nsISimpleEnumerator* enumerator;

  NS_ASSERTION(m_pHTTPHeaders, "header array doesn't exist.");
  if (!m_pHTTPHeaders) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  enumerator = new nsHTTPHeaderEnumerator(m_pHTTPHeaders);
  if (enumerator) {
    NS_ADDREF(enumerator);
  } else {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  *aResult = enumerator;

  return rv;
}


PRBool nsHTTPHeaderArray::IsHeaderMultiple(nsIAtom* aHeader)
{
  PRBool bIsMultiple;

  //
  // The following Request headers *must* have a single value.
  //
  if ((nsHTTPAtoms::Accept_Charset      == aHeader) ||
      (nsHTTPAtoms::From                == aHeader) || 
      (nsHTTPAtoms::Host                == aHeader) ||
      (nsHTTPAtoms::If_Modified_Since   == aHeader) ||
      (nsHTTPAtoms::If_Unmodified_Since == aHeader) ||
      (nsHTTPAtoms::Max_Forwards        == aHeader) ||
      (nsHTTPAtoms::Referer             == aHeader) ||
      (nsHTTPAtoms::User_Agent          == aHeader)) {
    bIsMultiple = PR_FALSE;
  } else {
    bIsMultiple = PR_TRUE;
  }

  return bIsMultiple;
}




nsHTTPHeaderEnumerator::nsHTTPHeaderEnumerator(nsISupportsArray* aHeaderArray)
{
  NS_INIT_REFCNT();

  mIndex = 0;
  mHeaderArray = aHeaderArray;
}


nsHTTPHeaderEnumerator::~nsHTTPHeaderEnumerator()
{
  mHeaderArray = null_nsCOMPtr();
}


//
// Implement nsISupports methods
//
NS_IMPL_ISUPPORTS(nsHTTPHeaderEnumerator, 
                  nsCOMTypeInfo<nsISimpleEnumerator>::GetIID())


//
// nsISimpleEnumerator methods:
//
NS_IMETHODIMP
nsHTTPHeaderEnumerator::HasMoreElements(PRBool* aResult)
{
  PRUint32 count = 0;

  if (!aResult) return NS_ERROR_NULL_POINTER;
  *aResult = PR_FALSE;

  if (!mHeaderArray) return NS_ERROR_NULL_POINTER;

  (void)mHeaderArray->Count(&count);
  if (count > mIndex) {
    *aResult = PR_TRUE;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsHTTPHeaderEnumerator::GetNext(nsISupports** aResult)
{
  nsresult rv = NS_OK;
  PRUint32 count = 0;

  if (!aResult) return NS_ERROR_NULL_POINTER;
  *aResult = nsnull;

  if (mHeaderArray) {
    (void)mHeaderArray->Count(&count);
    if (count > mIndex) {
      *aResult = NS_STATIC_CAST(nsISupports*, mHeaderArray->ElementAt(mIndex));
      mIndex += 1;
    }
  } else {
    rv = NS_ERROR_NULL_POINTER;
  }

  return rv;
}


