/* nsJARInputStream.cpp
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Netscape Communicator source code. 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 * Mitch Stoltz <mstoltz@netscape.com>
 */
#include "nsJARInputStream.h"
#include "zipfile.h"           // defines ZIP error codes
#include "nsZipArchive.h"


/*---------------------------------------------
 *  nsISupports implementation
 *--------------------------------------------*/

NS_IMPL_ISUPPORTS1(nsJARInputStream, nsIInputStream);

/*----------------------------------------------------------
 * nsJARInputStream implementation
 *--------------------------------------------------------*/

NS_IMETHODIMP 
nsJARInputStream::Available(PRUint32 *_retval)
{
  if (Zip() == 0)
    *_retval = 0;
  else
    *_retval = Zip()->Available(mReadInfo);

  return NS_OK;
}

NS_IMETHODIMP
nsJARInputStream::Read(char* buf, PRUint32 count, PRUint32 *bytesRead)
{
  if (Zip() == nsnull)
  {
    *bytesRead = 0;
    return NS_OK;
  }

  PRInt32 err = Zip()->Read(mReadInfo, buf, count, bytesRead);
#ifdef DEBUG_warren
//  printf("read %d from %s\n", *bytesRead, mEntryName);
#endif
  return err == ZIP_OK ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJARInputStream::Close()
{
  NS_RELEASE(mJAR);
  delete mReadInfo;
  return NS_OK;
}

nsresult 
nsJARInputStream::Init(nsJAR* aJAR, const char* aFilename)
{
  if (!aFilename)
    return NS_ERROR_NULL_POINTER;
  mJAR = aJAR;
  NS_ADDREF(mJAR);
  mEntryName = nsCRT::strdup(aFilename);

  PRInt32 result; 
  result = Zip()->ReadInit(mEntryName, &mReadInfo);
  if (result != ZIP_OK)
    return NS_ERROR_FAILURE;
  
  // Pass the file (already in memory) on to the JAR parser
  if (aJAR)
    return aJAR->VerifyEntry(mEntryName, mReadInfo->mFileBuffer,
			     mReadInfo->mItem->realsize);
  return NS_OK;
}

NS_METHOD
nsJARInputStream::Create(nsISupports* ignored, const nsIID& aIID, void* *aResult)
{    
  nsJARInputStream* is = new nsJARInputStream();
  if (is == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(is);
  nsresult rv = is->QueryInterface(aIID, aResult);
  NS_RELEASE(is);
  return rv;
}

//----------------------------------------------
// nsJARInputStream constructor and destructor
//----------------------------------------------

nsJARInputStream::nsJARInputStream()
  : mJAR(nsnull), mEntryName(nsnull), mReadInfo(nsnull)
{
  NS_INIT_REFCNT();
}

nsJARInputStream::~nsJARInputStream()
{
  Close();
  if (mEntryName) nsCRT::free(mEntryName);
}


