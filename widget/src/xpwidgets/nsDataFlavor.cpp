/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsDataFlavor.h"

static NS_DEFINE_IID(kIDataFlavor, NS_IDATAFLAVOR_IID);

NS_IMPL_ADDREF(nsDataFlavor)
NS_IMPL_RELEASE(nsDataFlavor)

//-------------------------------------------------------------------------
//
// DataFlavor constructor
//
//-------------------------------------------------------------------------
nsDataFlavor::nsDataFlavor()
{
  NS_INIT_REFCNT();
}

//-------------------------------------------------------------------------
//
// DataFlavor destructor
//
//-------------------------------------------------------------------------
nsDataFlavor::~nsDataFlavor()
{
}

/**
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 * 
*/ 
nsresult nsDataFlavor::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{

  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = NS_NOINTERFACE;

  if (aIID.Equals(kIDataFlavor)) {
    *aInstancePtr = (void*) ((nsIDataFlavor*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return rv;
}

/**
  * 
  *
  */
NS_METHOD nsDataFlavor::Init(const nsString & aMimeType, const nsString & aHumanPresentableName)
{
  mMimeType = aMimeType;
  mHumanPresentableName = aHumanPresentableName;

  char * str = mMimeType.ToNewCString();

  delete[] str;

  return NS_OK;
}

/**
  * 
  *
  */
NS_METHOD nsDataFlavor::GetMimeType(nsString & aMimeStr) const
{
  aMimeStr = mMimeType;
  return NS_OK;
}

/**
  * 
  *
  */
NS_METHOD nsDataFlavor::GetHumanPresentableName(nsString & aHumanPresentableName) const
{
  aHumanPresentableName = mHumanPresentableName;
  return NS_OK;
}

/**
  * 
  *
  */
NS_METHOD nsDataFlavor::Equals(const nsIDataFlavor * aDataFlavor)
{
  nsString mimeInQues;
  aDataFlavor->GetMimeType(mimeInQues);

  return (mMimeType.Equals(mimeInQues)?NS_OK:NS_ERROR_FAILURE);
}


/**
  *  Cache of nsDataFlavor instances.
  *
  *  Does this addRef, and do I have to release the last flavor 
  *  before I call GetPredefined again?
  */
NS_METHOD nsDataFlavor::GetPredefinedDataFlavor(nsString & aStr, 
                                                nsIDataFlavor ** aDataFlavor)
{
  return NS_OK;
}
