/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of the Original Code is Alexander. Portions
 * created by Alexander Larsson are Copyright (C) 1999
 * Alexander Larsson. All Rights Reserved. 
 */
#include "nscore.h"
#include "GtkMozillaInputStream.h"

#include "stdio.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIInputStreamIID, NS_IINPUTSTREAM_IID);
static NS_DEFINE_IID(kIBaseStreamIID, NS_IBASESTREAM_IID);

GtkMozillaInputStream::GtkMozillaInputStream(void)
{
  mBuffer = nsnull;
}

GtkMozillaInputStream::~GtkMozillaInputStream(void)
{
}

NS_IMETHODIMP
GtkMozillaInputStream::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  nsISupports *ifp = nsnull;

  if (aIID.Equals(kIInputStreamIID)) {
    ifp = (nsIInputStream*)this;
  } else if(aIID.Equals(kIBaseStreamIID)) {
    ifp = (nsIBaseStream*)this;;
  } else if(aIID.Equals(kISupportsIID)) {
    ifp = this;
  } else {
    *aInstancePtr = 0;
   
    return NS_NOINTERFACE;
  }

  *aInstancePtr = (void *)ifp;

  NS_ADDREF(ifp);
  
  return NS_OK;
}

NS_IMPL_RELEASE(GtkMozillaInputStream)
NS_IMPL_ADDREF(GtkMozillaInputStream)

  // nsIInputStream interface
NS_IMETHODIMP
GtkMozillaInputStream::Available(PRUint32 *aLength)
{
  *aLength = mCount;
  return NS_OK;
}
  
NS_IMETHODIMP
GtkMozillaInputStream::Read(char* aBuf, PRUint32 aCount, PRUint32 *aReadCount)
{
  if (mCount<aCount)
    mReadCount = mCount;
  else
    mReadCount = aCount;

  memcpy(aBuf, mBuffer, mReadCount);
  
  *aReadCount = mReadCount;
  
  mBuffer = nsnull;
  mCount = 0;
  
  return NS_OK;
}
  
// nsIBaseStream interface
NS_IMETHODIMP
GtkMozillaInputStream::Close(void)
{
  printf("GtkMozillaInputStream::Close() called.\n");
  return NS_OK;
}

// Specific interface
NS_IMETHODIMP
GtkMozillaInputStream::Fill(const char *aBuffer, PRUint32 aCount)
{
  mBuffer = aBuffer;
  mCount = aCount;
  return NS_OK;
}

NS_IMETHODIMP
GtkMozillaInputStream::FillResult(PRUint32 *aReadCount)
{
  *aReadCount = mReadCount;
  return NS_OK;
}
