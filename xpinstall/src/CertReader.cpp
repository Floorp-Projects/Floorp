/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is XPInstall Signing.
 *
 * The Initial Developer of the Original Code is Doug Turner.
 * Portions created by the Initial Developer are Copyright (C) 2002
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "zlib.h"
#include "zipstruct.h"

#include "CertReader.h"

#include "nsCRT.h"
#include "nsIServiceManager.h"
#include "nsISignatureVerifier.h"
#include "nsIInputStream.h"

#include "nsNetUtil.h"

// just a guess at the max size of the cert.
#define MAX_SIGNATURE_SIZE (32*1024) 


/*
 *  x t o i n t
 *
 *  Converts a two byte ugly endianed integer
 *  to our platform's integer.
 *
 */

static unsigned int xtoint (unsigned char *ii)
{
  return (int) (ii [0]) | ((int) ii [1] << 8);
}

/*
 *  x t o l o n g
 *
 *  Converts a four byte ugly endianed integer
 *  to our platform's integer.
 *
 */

static unsigned long xtolong (unsigned char *ll)
{
  unsigned long ret;
  
  ret =  ((((unsigned long) ll [0]) <<  0) |
          (((unsigned long) ll [1]) <<  8) |
          (((unsigned long) ll [2]) << 16) |
          (((unsigned long) ll [3]) << 24) );
  
  return ret;
}

static int my_inflate(unsigned char* compr, PRUint32 comprLen, unsigned char* uncompr, PRUint32 uncomprLen)
{
  int err;
  z_stream d_stream; /* decompression stream */
  memset (&d_stream, 0, sizeof (d_stream));
  
  // buffer is way to small to even deal with.
  if (uncomprLen < 10)
    return -1;

  *uncompr = '\0'; 

  if (inflateInit2 (&d_stream, -MAX_WBITS) != Z_OK)
    return -1;
  
  d_stream.next_in  = compr;
  d_stream.avail_in = (uInt)comprLen;
  
  d_stream.next_out = uncompr;
  d_stream.avail_out = (uInt)uncomprLen;
  
  err = inflate(&d_stream, Z_NO_FLUSH);
  
  if (err != Z_OK && err != Z_STREAM_END) { 
    inflateEnd(&d_stream);
    return -1;
  }
  
  err = inflateEnd(&d_stream);
  if (err != Z_OK) { 
    return -1;
  }
  return 0;
}

CertReader::CertReader(nsIURI* aURI, nsISupports* aContext, nsPICertNotification* aObs):
  mContext(aContext),
  mURI(aURI),
  mObserver(do_QueryInterface(aObs))
{
}

CertReader::~CertReader()
{
}

NS_IMPL_ISUPPORTS2(CertReader, nsIStreamListener, nsIRequestObserver)
  
NS_IMETHODIMP
CertReader::OnStartRequest(nsIRequest *request, nsISupports* context)
{
  mVerifier = do_GetService(SIGNATURE_VERIFIER_CONTRACTID);
  if (!mVerifier)
    return NS_BINDING_ABORTED;
  
  mLeftoverBuffer.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
CertReader::OnDataAvailable(nsIRequest *request, 
                            nsISupports* context,
                            nsIInputStream *aIStream, 
                            PRUint32 aSourceOffset,
                            PRUint32 aLength)
{
  if (!mVerifier)
    return NS_BINDING_ABORTED;
  
  char buf[4096];
  PRUint32 amt, size;
  nsresult rv;
  
  while (aLength) 
  {
    size = PR_MIN(aLength, sizeof(buf));
    
    rv = aIStream->Read(buf, size, &amt);
    
    if (NS_FAILED(rv)) 
      return rv;

    aLength -= amt;
    
    mLeftoverBuffer.Append(buf, amt);

    if (mLeftoverBuffer.Length() < ZIPLOCAL_SIZE)
      continue;

    const char* caret = mLeftoverBuffer.get();
    const char* end = caret + mLeftoverBuffer.Length();

    ZipLocal_* ziplocal = (ZipLocal_*) caret;

    if (xtolong(ziplocal->signature) != LOCALSIG)
      return NS_BINDING_ABORTED; 

    // did we read the entire file entry into memory?
    PRUint32 fileEntryLen = (ZIPLOCAL_SIZE + 
                             xtoint(ziplocal->filename_len) +
                             xtoint(ziplocal->extrafield_len) +
                             xtolong(ziplocal->size));


    // prevent downloading a huge file on an unsigned cert
    if (fileEntryLen > MAX_SIGNATURE_SIZE)
      return NS_BINDING_ABORTED;

    if (mLeftoverBuffer.Length() < fileEntryLen)
    {
      // we are just going to buffer and continue.
      continue;
    }

    // the assumption here is that we have the fileEntry in mLeftoverBuffer

    const char* data = (caret + 
                        ZIPLOCAL_SIZE + 
                        xtoint(ziplocal->filename_len) +
                        xtoint(ziplocal->extrafield_len));

    PRUint32 orgSize = xtolong ((unsigned char *) ziplocal->orglen);
    PRUint32 cSize   = xtolong ((unsigned char *) ziplocal->size);

    if (orgSize == 0)
      return NS_BINDING_ABORTED;

    unsigned char* orgData;
    int err = 0;

    orgData = (unsigned char*) malloc(orgSize);
    
    if (!orgData)
	  return NS_BINDING_ABORTED;

    if (xtoint(ziplocal->method) == DEFLATED) {

      err = my_inflate((unsigned char*)data, 
                       cSize, 
                       orgData,
                       orgSize);
    }
    else {
      memcpy(orgData, data, orgSize);
    }


    if (err == 0) 
    {
      PRInt32 verifyError;
      rv = mVerifier->VerifySignature((char*)orgData, orgSize, nsnull, 0, 
                                 &verifyError, getter_AddRefs(mPrincipal));
    }
    if (orgData)  
        free(orgData);
    
    return NS_BINDING_ABORTED;
  }

  return NS_BINDING_ABORTED;
}

NS_IMETHODIMP
CertReader::OnStopRequest(nsIRequest *request, nsISupports* context,
                          nsresult aStatus)
{
    mObserver->OnCertAvailable(mURI, 
                               mContext, 
                               aStatus,
                               mPrincipal);
    
    return NS_OK;
}


