/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDataFlavor.h"
#include "nsReadableUtils.h"

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

  char * str = ToNewCString(mMimeType);

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
