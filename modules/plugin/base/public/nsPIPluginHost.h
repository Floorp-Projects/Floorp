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

#ifndef nsPIPluginHost_h___
#define nsPIPluginHost_h___

#include "nsIPluginInstance.h"

#define NS_PIPLUGINHOST_IID                        \
{/* 8e3d71e6-2319-11d5-9cf8-0060b0fbd8ac */        \
  0x8e3d71e6, 0x2319, 0x11d5,                      \
  {0x9c, 0xf8, 0x00, 0x60, 0xb0, 0xfb, 0xd8, 0xac} \
}

class nsPIPluginHost : public nsISupports
{
public:
	NS_DEFINE_STATIC_IID_ACCESSOR(NS_PIPLUGINHOST_IID)

 /*
  * To notify the plugin manager that the plugin created a script object 
  */
  NS_IMETHOD
  SetIsScriptableInstance(nsCOMPtr<nsIPluginInstance> aPluginInstance, PRBool aScriptable) = 0;


 /**
  * This method parses post buffer to find out case insensitive "Content-length" string
  * and CR or LF some where after that, then it assumes there is http headers in
  * the input buffer and continue to search for end of headers (CRLFCRLF or LFLF).
  * It will *always malloc()* output buffer (caller is responsible to free it) 
  * if input buffer starts with LF, which comes from 4.x spec 
  * http://developer.netscape.com/docs/manuals/communicator/plugin/pgfn2.htm#1007754
  * "If no custom headers are required, simply add a blank
  * line ('\n') to the beginning of the file or buffer.",
  * it skips that '\n' and considers rest of the input buffer as data.
  * If "Content-length" string and end of headers is found 
  *   it substitutes single LF with CRLF in the headers, so the end of headers
  *   always will be CRLFCRLF (single CR in headers, if any, remain untouched)
  * else
  *   it puts "Content-length: "+size_of_data+CRLFCRLF at the beginning of the output buffer
  * and memcpy data to the output buffer 
  *
  * On failure outPostData and outPostDataLen will be set in 0.  
  * @param inPostData, the post data
  * @param the length of inPostData
  * @param outPostData the buffer.
  * @param outPostDataLen the length of outPostData
  **/
  NS_IMETHOD
  ParsePostBufferToFixHeaders(const char *inPostData, PRUint32 inPostDataLen, 
              char **outPostData, PRUint32 *outPostDataLen) = 0;
 /*
  * To create tmp file with Content len header in, it will use by http POST
  */
   NS_IMETHOD
  CreateTmpFileToPost(const char *postDataURL, char **pTmpFileName) = 0;
};

#endif /* nsPIPluginHost_h___ */
