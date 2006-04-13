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
 * The Original Code is Minimo.
 *
 * The Initial Developer of the Original Code is
 * Doug Turner <dougt@meer.net>.
 * Portions created by the Initial Developer are Copyright (C) 2006
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

#include "nsNativeSDR.h"
#include "plbase64.h"

#include <Wincrypt.h>

NS_IMPL_ISUPPORTS1(nsNativeSDR, nsISecretDecoderRing)

nsNativeSDR::nsNativeSDR()
{
}

nsNativeSDR::~nsNativeSDR()
{
}

static nsresult encode(const unsigned char *data, PRInt32 dataLen, char **_retval)
{
  nsresult rv = NS_OK;
  
  *_retval = PL_Base64Encode((const char *)data, dataLen, NULL);

  if (!*_retval)
    rv = NS_ERROR_OUT_OF_MEMORY;

  return rv;
}

static nsresult decode(const char *data, unsigned char **result, PRInt32 * _retval)
{
  nsresult rv = NS_OK;
  PRUint32 len = strlen(data);
  int adjust = 0;
  
  /* Compute length adjustment */
  if (data[len-1] == '=') {
    adjust++;
    if (data[len-2] == '=') adjust++;
  }
  
  *result = (unsigned char *)PL_Base64Decode(data, len, NULL);
  if (!*result) { rv = NS_ERROR_ILLEGAL_VALUE; goto loser; }
  
  *_retval = (len*3)/4 - adjust;
  
 loser:
  return rv;
}

NS_IMETHODIMP nsNativeSDR::Encrypt(unsigned char * data, 
                                   PRInt32 dataLen, 
                                   unsigned char * *result, 
                                   PRInt32 *_retval)
{
  DATA_BLOB DataIn;
  DATA_BLOB DataOut;
  
  DataIn.pbData = data;    
  DataIn.cbData = dataLen;
  
  DataOut.pbData = nsnull;
  DataOut.cbData = 0;
  
  BOOL b = CryptProtectData( &DataIn,
                             NULL,
                             NULL,
                             NULL,
                             NULL,
                             0,
                             &DataOut);
  
  
  if (!b)
    return NS_ERROR_FAILURE;
  
  unsigned char* out = (unsigned char*) malloc(DataOut.cbData);
  if (!out)
    return NS_ERROR_OUT_OF_MEMORY;
  
  memcpy(out, DataOut.pbData, DataOut.cbData);
  LocalFree(DataOut.pbData);
  
  *result = out;
  *_retval = DataOut.cbData;
  
  return NS_OK;
  
}

NS_IMETHODIMP nsNativeSDR::Decrypt(unsigned char * data,
                                   PRInt32 dataLen, 
                                   unsigned char * *result, 
                                   PRInt32 *_retval)
{
  DATA_BLOB DataIn;
  DATA_BLOB DataOut;
  
  DataIn.pbData = data;    
  DataIn.cbData = dataLen;
  
  DataOut.pbData = nsnull;
  DataOut.cbData = 0;
  
  BOOL b = CryptUnprotectData(&DataIn,
                              NULL,
                              NULL,
                              NULL,
                              NULL,
                              0,
                              &DataOut);                           
  
  if (!b)
    return NS_ERROR_FAILURE;
  
  unsigned char* out = (unsigned char*) malloc(DataOut.cbData);
  if (!out)
    return NS_ERROR_OUT_OF_MEMORY;
  
  memcpy(out, DataOut.pbData, DataOut.cbData);
  LocalFree(DataOut.pbData);
  
  *result = out;
  *_retval = DataOut.cbData;
  
  return NS_OK;
}

NS_IMETHODIMP nsNativeSDR::EncryptString(const char *text, char **_retval)
{
  unsigned char *encrypted = 0;
  PRInt32 eLen;
  
  nsresult rv = Encrypt((unsigned char *)text, strlen(text), &encrypted, &eLen);
  if (NS_FAILED(rv))
    return rv;
  
  rv = encode(encrypted, eLen, _retval);
  
  if (encrypted) 
    free(encrypted);
  
  return NS_OK;
}

NS_IMETHODIMP nsNativeSDR::DecryptString(const char *crypt, char **_retval)
{
  unsigned char *decoded = 0;
  PRInt32 decodedLen;
  unsigned char *decrypted = 0;
  PRInt32 decryptedLen;
  
  nsresult rv = decode(crypt, &decoded, &decodedLen);
  if (NS_FAILED(rv))
    return rv;
  
  rv = Decrypt(decoded, decodedLen, &decrypted, &decryptedLen);
  if (NS_FAILED(rv))
  {
    free(decoded);
    return rv;
  }
  
  char *r = (char *)malloc(decryptedLen+1);
  if (!r) 
  {
    free(decoded);
    free(decrypted);       
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  memcpy(r, decrypted, decryptedLen);
  r[decryptedLen] = 0;
  
  *_retval = r;
  return NS_OK;
}

NS_IMETHODIMP nsNativeSDR::ChangePassword()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsNativeSDR::Logout()
{
  return NS_OK;
}

NS_IMETHODIMP nsNativeSDR::LogoutAndTeardown()
{
  return NS_OK;
}


