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
  if (mZip == 0)
    *_retval = 0;
  else
    *_retval = mZip->Available(mReadInfo);

  return NS_OK;
}

NS_IMETHODIMP
nsJARInputStream::Read(char* buf, PRUint32 count, PRUint32 *_retval)
{
  if (mZip == nsnull)
  {
    *_retval = 0;
    return NS_OK;
  }

  if( (mZip->Read(mReadInfo, buf, count, _retval)) != ZIP_OK )
    return NS_ERROR_FAILURE;
  
  // Pass the buffer on to the JAR parser
  if (mJAR && mJAR->SupportsRSAVerification())
  {
    nsStringKey key(mEntryName);
    nsJARManifestItem* manItem = 
      (nsJARManifestItem*)mJAR->mManifestData.Get(&key);
    if (manItem && !manItem->entryDigestsCalculated)
    {
      nsresult rv = NS_OK;
      if (digestContexts[0] == 0)
	for (PRInt32 b=0; b<JAR_DIGEST_COUNT && NS_SUCCEEDED(rv); b++)
	  rv = mJAR->DigestBegin(&digestContexts[b], b);
      for (PRInt32 c=0; c<JAR_DIGEST_COUNT && NS_SUCCEEDED(rv); c++)
	rv = mJAR->DigestData(digestContexts[c], buf, *_retval);
      if (NS_SUCCEEDED(rv))
      {
	PRUint32 available;
	rv = Available(&available);
	if (available == 0)
        {
	  for (PRInt32 d=0; d<JAR_DIGEST_COUNT && NS_SUCCEEDED(rv); d++)
	    rv = mJAR->CalculateDigest(digestContexts[d], 
				       &(manItem->calculatedEntryDigests[d]));
	  if (NS_SUCCEEDED(rv))
	    manItem->entryDigestsCalculated = PR_TRUE;
	}
      }
      return rv;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsJARInputStream::Close()
{
  delete mReadInfo;
  return NS_OK;
}

nsresult 
nsJARInputStream::Init(nsJAR* aJAR, nsZipArchive* aZip, const char* aFilename)
{
  if (!(aZip && aFilename))
    return NS_ERROR_NULL_POINTER;
  mZip = aZip;
  mJAR = aJAR;
  mEntryName = (char*)aFilename;
  for (PRInt32 a=0; a<JAR_DIGEST_COUNT; a++)
    digestContexts[a] = 0;

  PRInt32 result; 
  result = mZip->ReadInit(mEntryName, &mReadInfo);
  if (result != ZIP_OK)
    return NS_ERROR_FAILURE;
  else
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
{
  NS_INIT_REFCNT();
  mJAR = nsnull;
}

nsJARInputStream::~nsJARInputStream()
{
  Close();
}


