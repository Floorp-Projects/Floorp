/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPKIParamBlock.h"
#include "nsIServiceManager.h"
#include "nsIDialogParamBlock.h"
#include "nsIMutableArray.h"

NS_IMPL_THREADSAFE_ISUPPORTS2(nsPKIParamBlock, nsIPKIParamBlock,
                                               nsIDialogParamBlock)

nsPKIParamBlock::nsPKIParamBlock()
{
}

nsresult
nsPKIParamBlock::Init()
{
  mDialogParamBlock = do_CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID);
  return (mDialogParamBlock == nullptr) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

nsPKIParamBlock::~nsPKIParamBlock()
{
}


NS_IMETHODIMP 
nsPKIParamBlock::SetNumberStrings( PRInt32 inNumStrings )
{
  return mDialogParamBlock->SetNumberStrings(inNumStrings);
}

NS_IMETHODIMP 
nsPKIParamBlock::SetInt(PRInt32 inIndex, PRInt32 inInt)
{
  return mDialogParamBlock->SetInt(inIndex, inInt);
}

NS_IMETHODIMP 
nsPKIParamBlock::GetInt(PRInt32 inIndex, PRInt32 *outInt)
{
  return mDialogParamBlock->GetInt(inIndex, outInt);
}


NS_IMETHODIMP 
nsPKIParamBlock::GetString(PRInt32 inIndex, PRUnichar **_retval)
{
  return mDialogParamBlock->GetString(inIndex, _retval);
}

NS_IMETHODIMP 
nsPKIParamBlock::SetString(PRInt32 inIndex, const PRUnichar *inString)
{
  return mDialogParamBlock->SetString(inIndex, inString);
}

NS_IMETHODIMP
nsPKIParamBlock::GetObjects(nsIMutableArray * *aObjects)
{
  return mDialogParamBlock->GetObjects(aObjects);
}

NS_IMETHODIMP
nsPKIParamBlock::SetObjects(nsIMutableArray * aObjects)
{
  return mDialogParamBlock->SetObjects(aObjects);
}



/* void setISupportAtIndex (in PRInt32 index, in nsISupports object); */
NS_IMETHODIMP 
nsPKIParamBlock::SetISupportAtIndex(PRInt32 index, nsISupports *object)
{
  if (!mSupports) {
    mSupports = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID);
    if (mSupports == nullptr) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  // Ignore any InsertElementAt error, because this function always did that
  mSupports->InsertElementAt(object, index-1);
  return NS_OK;
}

/* nsISupports getISupportAtIndex (in PRInt32 index); */
NS_IMETHODIMP 
nsPKIParamBlock::GetISupportAtIndex(PRInt32 index, nsISupports **_retval)
{
  NS_ENSURE_ARG(_retval);

  *_retval = mSupports->ElementAt(index-1);
  return NS_OK;
}


