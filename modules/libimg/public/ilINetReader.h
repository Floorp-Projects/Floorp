/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef ilINetReader_h___
#define ilINetReader_h___

#include <stdio.h>
#include "nsISupports.h"


// IID for the ilINetReader interface
#define IL_INETREADER_IID    \
{ 0xbe324220, 0xb416, 0x11d1,  \
{ 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } }

class ilIURL;


class ilINetReader : public nsISupports {
public:

  NS_DEFINE_STATIC_IID_ACCESSOR(IL_INETREADER_IID);
  
  NS_IMETHOD WriteReady(PRUint32 *max_read)=0;
  
  NS_IMETHOD FirstWrite(const unsigned char *str, int32 len, char* url)=0;

  NS_IMETHOD Write(const unsigned char *str, int32 len)=0;

  NS_IMETHOD StreamAbort(int status)=0;

  NS_IMETHOD StreamComplete(PRBool is_multipart)=0;

  NS_IMETHOD NetRequestDone(ilIURL *urls, int status)=0;
  
  virtual PRBool StreamCreated(ilIURL *urls, char* type)=0;
  
  virtual PRBool IsMulti()=0;

  NS_IMETHOD FlushImgBuffer()=0;

};

#endif
