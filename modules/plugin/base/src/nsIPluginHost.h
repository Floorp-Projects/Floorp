/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#ifndef nsIPluginHost_h___
#define nsIPluginHost_h___

#include "xp_core.h"
#include "nsplugindefs.h"
#include "nsIFactory.h"
#include "nsString.h"
#include "nsIPluginInstanceOwner.h"
#include "nsIStreamListener.h"
#include "nsNetUtil.h"
#include "nsIStringStream.h"
#include "prlink.h"  // for PRLibrary

class nsIPlugin;
class nsIURI;
class nsIDOMPlugin;

#define NS_IPLUGINHOST_IID \
{ 0x264c0640, 0x1c31, 0x11d2, \
{ 0xa8, 0x2e, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

struct nsIPluginHost : public nsIFactory
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPLUGINHOST_IID)

  NS_IMETHOD
  Init(void) = 0;

  NS_IMETHOD
  Destroy(void) = 0;

  NS_IMETHOD
  LoadPlugins(void) = 0;
  
  NS_IMETHOD
  GetPluginFactory(const char *aMimeType, nsIPlugin** aPlugin) = 0;

  NS_IMETHOD
  InstantiateEmbededPlugin(const char *aMimeType, nsIURI* aURL, nsIPluginInstanceOwner *aOwner) = 0;

  NS_IMETHOD
  InstantiateFullPagePlugin(const char *aMimeType, nsString& aURLSpec, nsIStreamListener *&aStreamListener, nsIPluginInstanceOwner *aOwner) = 0;

  NS_IMETHOD
  SetUpPluginInstance(const char *aMimeType, nsIURI *aURL, nsIPluginInstanceOwner *aOwner) = 0;

  NS_IMETHOD
  IsPluginEnabledForType(const char* aMimeType) = 0;

  NS_IMETHOD
  IsPluginEnabledForExtension(const char* aExtension, const char* &aMimeType) = 0;

  NS_IMETHOD
  GetPluginCount(PRUint32* aPluginCount) = 0;
  
  NS_IMETHOD
  GetPlugins(PRUint32 aPluginCount, nsIDOMPlugin* aPluginArray[]) = 0;

  NS_IMETHOD
  StopPluginInstance(nsIPluginInstance* aInstance) = 0;

  NS_IMETHOD
  HandleBadPlugin(PRLibrary* aLibrary) = 0;
};



/**
 * Used for creating the correct input stream for plugins
 * We can either have raw data (with or without \r\n\r\n) or a path to a file (but it must be native!)
 * When making an nsIInputStream stream for the plugins POST data, be sure to take into 
 * account that it could be binary and full of nulls, see bug 105417. Also, we need 
 * to make a copy of the buffer because the plugin may have allocated it on the stack.
 * For an example of this, see Shockwave registration or bug 108966
 */
inline nsresult
NS_NewPluginPostDataStream(nsIInputStream **result,
                           const char *data,
                           PRUint32 contentLength,
                           PRBool isFile = PR_FALSE,
                           PRBool headers = PR_FALSE)

{
  if (!data)
    return NS_ERROR_UNEXPECTED;

  nsresult rv = NS_ERROR_FAILURE;

  if (!isFile) { // do raw data case first
    if (contentLength < 1)
      return NS_ERROR_UNEXPECTED;

    char * buf = nsnull;

    /** 
     * makes the buffer correct according to the assumption of nsHTTPRequest.cpp 
     * that postData include "\r\n\r\n". 
     * This will search for "\r\n\n", which indicates the end of 
     * the last header. It will then search for the first non-whitespace 
     * character after the last header. It will then create a new buffer 
     * with the existing headers, a correct "\r\n\r\n", then the post data.  
     * If no "\r\n" is found, the data does not contain headers, and a simple 
     * "\r\n\r\n" is prepended to the buffer. See bug 60228 for more info.
     * ...but not for headers!
     */

    if (!headers && !PL_strnstr(data, "\r\n\r\n", contentLength)) {
      const char *crlf = nsnull;
      const char *crlfcrlf = "\r\n\r\n";
      const char *t;
      char *newBuf;
      PRInt32 headersLen = 0, dataLen = 0;

      if (!(newBuf = (char*)nsMemory::Alloc(contentLength + 4))) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      nsCRT::memset(newBuf, 0, contentLength + 4);

      if (!(crlf = PL_strnstr(data, "\r\n\n", contentLength))) {
        nsMemory::Free(newBuf);
        return NS_ERROR_NULL_POINTER;
      }
      headersLen = crlf - data;

      // find the next non-whitespace char
      t = crlf + 3;
      while (*t == '\r' || *t == '\n' || *t == '\t' || *t == ' ' && *t) {
        t++;
      }
      if (*t) {
        // copy the headers
        memcpy(newBuf, data, headersLen);
        // copy the correct crlfcrlf
        memcpy(newBuf + headersLen, crlfcrlf, 4);
        // copy the rest of the postData
        dataLen = contentLength - (t - data);
        memcpy(newBuf + headersLen + 4, t, dataLen);
        contentLength = headersLen + 4 + dataLen;
        buf = newBuf;
      } else {
        nsMemory::Free(newBuf);
        return NS_ERROR_NULL_POINTER;
      }
    } else {

      // We got correctly formated data passed in!

      if (!(buf = (char*)nsMemory::Alloc(contentLength)))
        return NS_ERROR_OUT_OF_MEMORY;

      memcpy(buf, data, contentLength);
    }
    nsCOMPtr<nsIStringInputStream> sis = do_CreateInstance("@mozilla.org/io/string-input-stream;1",&rv);
    if (NS_SUCCEEDED(rv)) {
      sis->AdoptData(buf, contentLength);  // let the string stream manage our data

      rv = CallQueryInterface(sis, result);
    }
  } else
    rv = NS_NewPostDataStream(result, isFile, data, 0);  // used only for disk data

  return rv;
}

#endif // nsIPluginHost_h___
