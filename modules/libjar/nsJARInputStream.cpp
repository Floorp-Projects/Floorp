/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* nsJARInputStream.cpp
 * 
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Netscape Communicator source code. The Initial Developer of the Original Code is Netscape Communications Corporation.  Portions created by Netscape are Copyright (C) 1999 Netscape Communications Corporation.  All Rights Reserved.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mitch Stoltz <mstoltz@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsJARInputStream.h"
#include "zipfile.h"           // defines ZIP error codes
#include "nsZipArchive.h"


/*---------------------------------------------
 *  nsISupports implementation
 *--------------------------------------------*/

NS_IMPL_THREADSAFE_ISUPPORTS1(nsJARInputStream, nsIInputStream)

/*----------------------------------------------------------
 * nsJARInputStream implementation
 *--------------------------------------------------------*/

NS_IMETHODIMP 
nsJARInputStream::Available(PRUint32 *_retval)
{
    *_retval = mReadInfo.Available();
    
    return NS_OK;
}

NS_IMETHODIMP
nsJARInputStream::Read(char* buf, PRUint32 count, PRUint32 *bytesRead)
{
    if (!mJAR)
        return NS_BASE_STREAM_CLOSED;
    
    PRInt32 err = mReadInfo.Read(buf, count, bytesRead);
    return err == ZIP_OK ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJARInputStream::ReadSegments(nsWriteSegmentFun writer, void * closure, PRUint32 count, PRUint32 *_retval)
{
    // don't have a buffer to read from, so this better not be called!
    NS_NOTREACHED("Consumers should be using Read()!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJARInputStream::IsNonBlocking(PRBool *aNonBlocking)
{
    *aNonBlocking = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsJARInputStream::Close()
{
    NS_IF_RELEASE(mJAR);
    return NS_OK;
}

nsresult 
nsJARInputStream::Init(nsJAR* aJAR, const char* aFilename)
{
  if (!aFilename)
    return NS_ERROR_NULL_POINTER;
  mJAR = aJAR;
  NS_ADDREF(mJAR);

  PRInt32 result;
  PRFileDesc* fd = aJAR->OpenFile();
  NS_ASSERTION(fd, "Couldn't open JAR!");

  if (!fd)
      return NS_ERROR_UNEXPECTED;
  
  result = aJAR->mZip.ReadInit(aFilename, &mReadInfo, fd);
  if (result != ZIP_OK)
    return NS_ERROR_FAILURE;
  return NS_OK;
}

//----------------------------------------------
// nsJARInputStream constructor and destructor
//----------------------------------------------

nsJARInputStream::nsJARInputStream()
    : mJAR(nsnull)
{
}

nsJARInputStream::~nsJARInputStream()
{
  Close();
}

