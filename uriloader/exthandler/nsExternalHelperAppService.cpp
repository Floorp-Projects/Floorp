/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Scott MacGregor <mscott@netscape.com>
 */

#include "nsExternalHelperAppService.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIFile.h"
#include "nsIChannel.h"
#include "nsXPIDLString.h"
#include "nsMemory.h"
#include "nsIStreamListener.h"

NS_IMPL_THREADSAFE_ADDREF(nsExternalHelperAppService)
NS_IMPL_THREADSAFE_RELEASE(nsExternalHelperAppService)

NS_INTERFACE_MAP_BEGIN(nsExternalHelperAppService)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIExternalHelperAppService)
   NS_INTERFACE_MAP_ENTRY(nsIExternalHelperAppService)
NS_INTERFACE_MAP_END_THREADSAFE

nsExternalHelperAppService::nsExternalHelperAppService()
{
  NS_INIT_ISUPPORTS();
}

nsExternalHelperAppService::~nsExternalHelperAppService()
{
}

/* boolean canHandleContent (in string aMimeContentType); */
NS_IMETHODIMP nsExternalHelperAppService::CanHandleContent(const char *aMimeContentType, nsIURI * aURI, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIStreamListener doContent (in string aMimeContentType, in nsIURI aURI, in nsISupports aWindowContext, out boolean aAbortProcess); */
NS_IMETHODIMP nsExternalHelperAppService::DoContent(const char *aMimeContentType, nsIURI *aURI, nsISupports *aWindowContext, PRBool *aAbortProcess, nsIStreamListener **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsExternalAppHandler * nsExternalHelperAppService::CreateNewExternalHandler()
{
  nsExternalAppHandler* handler = nsnull;
  NS_NEWXPCOM(handler, nsExternalAppHandler);
  // add any XP intialization code for an external handler that we may need here...
  // right now we don't have any but i bet we will before we are done.
  return handler;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// begin external app handler implementation 
//////////////////////////////////////////////////////////////////////////////////////////////////////

NS_IMPL_THREADSAFE_ADDREF(nsExternalAppHandler)
NS_IMPL_THREADSAFE_RELEASE(nsExternalAppHandler)

NS_INTERFACE_MAP_BEGIN(nsExternalAppHandler)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStreamListener)
   NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
   NS_INTERFACE_MAP_ENTRY(nsIStreamObserver)
NS_INTERFACE_MAP_END_THREADSAFE

nsExternalAppHandler::nsExternalAppHandler()
{
  NS_INIT_ISUPPORTS();

  mDataBuffer = (char *) nsMemory::Alloc((sizeof(char) * DATA_BUFFER_SIZE));
}

nsExternalAppHandler::~nsExternalAppHandler()
{
  if (mDataBuffer)
    nsMemory::Free(mDataBuffer);
}

NS_IMETHODIMP nsExternalAppHandler::OnStartRequest(nsIChannel * aChannel, nsISupports * aCtxt)
{
  NS_ENSURE_ARG(aChannel);

  // create a temp file for the data...and open it for writing.
  NS_GetSpecialDirectory("system.OS_TemporaryDirectory", getter_AddRefs(mTempFile));

  nsCOMPtr<nsIURI> uri;
  aChannel->GetURI(getter_AddRefs(uri));
  nsCOMPtr<nsIURL> url = do_QueryInterface(uri);

  if (url)
  {
    // try to extract the file name from the url and use that as a first pass as the
    // leaf name of our temp file...
    nsXPIDLCString leafName;
    url->GetFileName(getter_Copies(leafName));
    if (leafName)
    {
      mTempFile->Append(leafName); // WARNING --> neeed a make Unique routine on nsIFile!!
    }
    else
      mTempFile->Append("test.tmp"); // WARNING THIS IS TEMPORARY CODE!!!
  }
  else
    mTempFile->Append("test.tmp"); // WARNING THIS IS TEMPORARY CODE!!!

  nsCOMPtr<nsIFileChannel> mFileChannel = do_CreateInstance(NS_LOCALFILECHANNEL_PROGID);
  if (mFileChannel)
  {
    mFileChannel->Init(mTempFile, -1, 0);
    mFileChannel->OpenOutputStream(getter_AddRefs(mOutStream));
  }

  return NS_OK;
}

NS_IMETHODIMP nsExternalAppHandler::OnDataAvailable(nsIChannel * aChannel, nsISupports * aCtxt,
                                                  nsIInputStream * inStr, PRUint32 sourceOffset, PRUint32 count)
{
  // read the data out of the stream and write it to the temp file.
  PRUint32 numBytesRead = 0;
  if (mOutStream && mDataBuffer && count > 0)
  {
    PRUint32 numBytesRead = 0; 
    PRUint32 numBytesWritten = 0;
    while (count > 0) // while we still have bytes to copy...
    {
      inStr->Read(mDataBuffer, PR_MIN(count, DATA_BUFFER_SIZE - 1), &numBytesRead);
      if (count >= numBytesRead)
        count -= numBytesRead; // subtract off the number of bytes we just read
      else
        count = 0;
      mOutStream->Write(mDataBuffer, numBytesRead, &numBytesWritten);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsExternalAppHandler::OnStopRequest(nsIChannel * aChannel, nsISupports *aCtxt, 
                                                nsresult aStatus, const PRUnichar * errorMsg)
{
  // go ahead and execute the application passing in our temp file as an argument
  // this may involve us calling back into the OS external app service to make the call
  // for actually launching the helper app. It'd be great if nsIFile::spawn could be made to work
  // on the mac...right now the mac implementation ignores all arguments passed in.

  // close the stream...
  if (mOutStream)
    mOutStream->Close();

  return NS_OK;
}

nsresult nsExternalAppHandler::Init(nsIFile * aExternalApplication)
{
  mExternalApplication = aExternalApplication;
  return NS_OK;
}
