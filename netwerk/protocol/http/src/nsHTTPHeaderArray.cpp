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
 * Contributor(s): 
 */


#include "nsHTTPHeaderArray.h"
#include "nsHTTPAtoms.h"


nsHeaderEntry::nsHeaderEntry(nsIAtom* aHeaderAtom, const char* aValue)
  : mAtom(aHeaderAtom)
{
  NS_INIT_REFCNT();

  mValue = aValue;
}


nsHeaderEntry::~nsHeaderEntry()
{
  // mAtom  = null_nsCOMPtr(); -- the |nsCOMPtr| automatically |Release()|s.  No need to force it
}


NS_IMPL_THREADSAFE_ISUPPORTS(nsHeaderEntry, NS_GET_IID(nsIHTTPHeader))


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
nsHeaderEntry::GetFieldName(char** aResult)
{
    if (!aResult)
        return NS_ERROR_NULL_POINTER;

    const char* standardName;
    nsHTTPHeaderArray::GetStandardHeaderName(mAtom, &standardName);
    if (standardName)
    {
        *aResult = nsCRT::strdup(standardName);
    }
    else
    {
        nsAutoString name;
        mAtom->ToString(name);
        *aResult = name.ToNewCString();
    }
    return NS_OK;

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
  (void)NS_NewISupportsArray(getter_AddRefs(mHTTPHeaders));
}

void
nsHTTPHeaderArray::Clear ()
{
    if (mHTTPHeaders)
        mHTTPHeaders -> Clear ();
}


nsHTTPHeaderArray::~nsHTTPHeaderArray()
{
    if (mHTTPHeaders)
        mHTTPHeaders -> Clear ();

    // mHTTPHeaders = null_nsCOMPtr (); -- the |nsCOMPtr| automatically |Release()|s.  No need to force it
}


nsresult nsHTTPHeaderArray::SetHeader(nsIAtom* aHeader, 
        const char* aValue){
  nsHeaderEntry *entry = nsnull;
  PRInt32 i;

  NS_ASSERTION(mHTTPHeaders, "header array doesn't exist.");
  if (!mHTTPHeaders) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  i = GetEntry(aHeader, &entry);
  //
  // If a NULL value is passed in, then delete the header entry...
  //
  if (!aValue) {
    if (entry) {
      mHTTPHeaders->RemoveElementAt(i);
      NS_RELEASE(entry);
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
    NS_ADDREF(entry);
    // XXX this method incorrectly returns a bool
    nsresult rv = mHTTPHeaders->AppendElement(entry) ? 
        NS_OK : NS_ERROR_FAILURE;
    NS_ASSERTION(NS_SUCCEEDED(rv), "AppendElement failed");
  } 
  // 
  // Append the new value to the existing string
  //
  else if (IsHeaderMultiple(aHeader)) {
    if (nsHTTPAtoms::Set_Cookie == aHeader ||
            nsHTTPAtoms::WWW_Authenticate == aHeader) {
        // special case these headers and use a newline
        // delimiter to delimit the cookies from one another
        // we can't use the standard comma because there
        // set-cookie headers that include commas in the cookie value
        // contrary to the specs not allowing it.
        entry->mValue.Append(LF);
        entry->mValue.Append(aValue);
    } else {
        // delimit each value from the others using a comma
        // (HTTP spec delimiter)
        entry->mValue.Append(", ");
        entry->mValue.Append(aValue);
    }
  }
  //
  // Replace the existing string with the new value
  //
  else {
      entry->mValue = aValue;
  }

  NS_RELEASE(entry);

  return NS_OK;
}


nsresult nsHTTPHeaderArray::GetHeader(nsIAtom* aHeader, char* *aResult)
{
  nsHeaderEntry *entry = nsnull;

  *aResult = nsnull;

  NS_ASSERTION(mHTTPHeaders, "header array doesn't exist.");
  if (!mHTTPHeaders) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  GetEntry(aHeader, &entry);

  if (entry) {
    *aResult = entry->mValue.ToNewCString();
    NS_RELEASE(entry);
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
  (void)mHTTPHeaders->Count(&count);
  
  for (i = 0; i < count; i++) {
    nsISupports *entry = nsnull;
    nsHeaderEntry* element;
    
    entry   = mHTTPHeaders->ElementAt(i);
    element = NS_STATIC_CAST(nsHeaderEntry*, entry);
    
    if (aHeader == element->mAtom.get()) { 
      // Does not need to be AddRefed again because the ElementAt() already
      // did it once...
      *aResult = element;
      return i;
    }
    NS_RELEASE(entry);
  }
  return -1;
}


nsresult nsHTTPHeaderArray::GetEnumerator(nsISimpleEnumerator** aResult)
{
  nsresult rv = NS_OK;
  nsISimpleEnumerator* enumerator;

  NS_ASSERTION(mHTTPHeaders, "header array doesn't exist.");
  if (!mHTTPHeaders) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  enumerator = new nsHTTPHeaderEnumerator(mHTTPHeaders);
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
  //
  // The following Request headers *must* have a single value.
  //
  return ((nsHTTPAtoms::Accept_Charset      == aHeader) ||
      (nsHTTPAtoms::Authorization       == aHeader) ||
      (nsHTTPAtoms::From                == aHeader) || 
      (nsHTTPAtoms::Host                == aHeader) ||
      (nsHTTPAtoms::If_Modified_Since   == aHeader) ||
      (nsHTTPAtoms::If_Unmodified_Since == aHeader) ||
      (nsHTTPAtoms::Location            == aHeader) ||
      (nsHTTPAtoms::Max_Forwards        == aHeader) ||
      (nsHTTPAtoms::Referer             == aHeader) ||
      (nsHTTPAtoms::User_Agent          == aHeader)
      ) ? 
    PR_FALSE : PR_TRUE;
}


nsHTTPHeaderEnumerator::nsHTTPHeaderEnumerator(nsISupportsArray* aHeaderArray)
  : mHeaderArray(aHeaderArray), mIndex(0)
{
  NS_INIT_REFCNT();
}


nsHTTPHeaderEnumerator::~nsHTTPHeaderEnumerator()
{
  // mHeaderArray = null_nsCOMPtr(); -- the |nsCOMPtr| automatically |Release()|s.  No need to force it
}


//
// Implement nsISupports methods
//
NS_IMPL_ISUPPORTS(nsHTTPHeaderEnumerator, 
                  NS_GET_IID(nsISimpleEnumerator))


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

void 
nsHTTPHeaderArray::GetStandardHeaderName(nsIAtom* aAtom, const char** aResult)
{
    if (!aAtom || !aResult)
        return; // NS_ERROR_NULL_POINTER;

    if (nsHTTPAtoms::Accept == aAtom)
        *aResult = "Accept";
    else if (nsHTTPAtoms::Accept_Charset == aAtom)
        *aResult = "Accept-Charset";
    else if (nsHTTPAtoms::Accept_Encoding == aAtom)
        *aResult = "Accept-Encoding";
    else if (nsHTTPAtoms::Accept_Language == aAtom)
        *aResult = "Accept-Language";
    else if (nsHTTPAtoms::Accept_Ranges == aAtom)
        *aResult = "Accept-Ranges";
    else if (nsHTTPAtoms::Age == aAtom)
        *aResult = "Age";
    else if (nsHTTPAtoms::Allow == aAtom)
        *aResult = "Allow";
    else if (nsHTTPAtoms::Authentication == aAtom)
        *aResult = "Authentication";
    else if (nsHTTPAtoms::Authorization == aAtom)
        *aResult = "Authorization";
    else if (nsHTTPAtoms::Cache_Control == aAtom)
        *aResult = "Cache-Control";
    else if (nsHTTPAtoms::Connection == aAtom)
        *aResult = "Connection";
    else if (nsHTTPAtoms::Content_Base == aAtom)
        *aResult = "Content-Base";
    else if (nsHTTPAtoms::Content_Encoding == aAtom)
        *aResult = "Content-Encoding";
    else if (nsHTTPAtoms::Content_Language == aAtom)
        *aResult = "Content-Language";
    else if (nsHTTPAtoms::Content_Length == aAtom)
        *aResult = "Content-Length";
    else if (nsHTTPAtoms::Content_Location == aAtom)
        *aResult = "Content-Location";
    else if (nsHTTPAtoms::Content_MD5 == aAtom)
        *aResult = "Content-MD5";
    else if (nsHTTPAtoms::Content_Range == aAtom)
        *aResult = "Content-Range";
    else if (nsHTTPAtoms::Content_Transfer_Encoding == aAtom)
        *aResult = "Content-Transfer-Encoding";
    else if (nsHTTPAtoms::Content_Type == aAtom)
        *aResult = "Content-Type";
    else if (nsHTTPAtoms::Date == aAtom)
        *aResult = "Date";
    else if (nsHTTPAtoms::Derived_From == aAtom)
        *aResult = "Derived-From";
    else if (nsHTTPAtoms::ETag == aAtom)
        *aResult = "ETag";
    else if (nsHTTPAtoms::Expect == aAtom)
        *aResult = "Expect";
    else if (nsHTTPAtoms::Expires == aAtom)
        *aResult = "Expires";
    else if (nsHTTPAtoms::From == aAtom)
        *aResult = "From";
    else if (nsHTTPAtoms::Forwarded == aAtom)
        *aResult = "Forwarded";
    else if (nsHTTPAtoms::Host == aAtom)
        *aResult = "Host";
    else if (nsHTTPAtoms::If_Match == aAtom)
        *aResult = "If-Match";
    else if (nsHTTPAtoms::If_Match_Any == aAtom)
        *aResult = "If-Match-Any";
    else if (nsHTTPAtoms::If_Modified_Since == aAtom)
        *aResult = "If-Modified-Since";
    else if (nsHTTPAtoms::If_None_Match == aAtom)
        *aResult = "If-None-Match";
    else if (nsHTTPAtoms::If_None_Match_Any == aAtom)
        *aResult = "If-None-Match-Any";
    else if (nsHTTPAtoms::If_Range == aAtom)
        *aResult = "If-Range";
    else if (nsHTTPAtoms::If_Unmodified_Since == aAtom)
        *aResult = "If-Unmodified-Since";
    else if (nsHTTPAtoms::Keep_Alive == aAtom)
        *aResult = "Keep-Alive";
    else if (nsHTTPAtoms::Last_Modified == aAtom)
        *aResult = "Last-Modified";
    else if (nsHTTPAtoms::Link == aAtom)
        *aResult = "Link";
    else if (nsHTTPAtoms::Location == aAtom)
        *aResult = "Location";
    else if (nsHTTPAtoms::Max_Forwards == aAtom)
        *aResult = "Max-Forwards";
    else if (nsHTTPAtoms::Message_Id == aAtom)
        *aResult = "Message-ID";
    else if (nsHTTPAtoms::Mime == aAtom)
        *aResult = "Mime";
    else if (nsHTTPAtoms::Pragma == aAtom)
        *aResult = "Pragma";
    else if (nsHTTPAtoms::Proxy_Authenticate == aAtom)
        *aResult = "Proxy-Authenticate";
    else if (nsHTTPAtoms::Proxy_Authorization == aAtom)
        *aResult = "Proxy-Authorization";
    else if (nsHTTPAtoms::Range == aAtom)
        *aResult = "Range";
    else if (nsHTTPAtoms::Referer == aAtom)
        *aResult = "Referer";
    else if (nsHTTPAtoms::Retry_After == aAtom)
        *aResult = "Retry-After";
    else if (nsHTTPAtoms::Server == aAtom)
        *aResult = "Server";
    else if (nsHTTPAtoms::Set_Cookie == aAtom)
        *aResult = "Set-Cookie";
    else if (nsHTTPAtoms::TE == aAtom)
        *aResult = "TE";
    else if (nsHTTPAtoms::Title == aAtom)
        *aResult = "Title";
    else if (nsHTTPAtoms::Trailer == aAtom)
        *aResult = "Trailer";
    else if (nsHTTPAtoms::Transfer_Encoding == aAtom)
        *aResult = "Transfer-Encoding";
    else if (nsHTTPAtoms::Upgrade == aAtom)
        *aResult = "Upgrade";
    else if (nsHTTPAtoms::URI == aAtom)
        *aResult = "URI";
    else if (nsHTTPAtoms::User_Agent == aAtom)
        *aResult = "User-Agent";
    else if (nsHTTPAtoms::Version == aAtom)
        *aResult = "Version";
    else if (nsHTTPAtoms::Warning == aAtom)
        *aResult = "Warning";
    else if (nsHTTPAtoms::WWW_Authenticate == aAtom)
        *aResult = "WWW-Authenticate";
    else
        *aResult = nsnull;
}

