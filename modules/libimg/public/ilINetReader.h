/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef ilINetReader_h___
#define ilINetReader_h___

#include <stdio.h>
#include "nsISupports.h"
#include "ntypes.h"

// IID for the ilINetReader interface
#define IL_INETREADER_IID    \
{ 0xbe324220, 0xb416, 0x11d1,  \
{ 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } }

class ilIURL;


class ilINetReader : public nsISupports {
public:

  virtual unsigned int WriteReady()=0;
  
  virtual int FirstWrite(const unsigned char *str, int32 len)=0;

  virtual int Write(const unsigned char *str, int32 len)=0;

  virtual void StreamAbort(int status)=0;

  virtual void StreamComplete(PRBool is_multipart)=0;

  virtual void NetRequestDone(ilIURL *urls, int status)=0;
  
  virtual PRBool StreamCreated(ilIURL *urls, int type)=0;
  
  virtual PRBool IsMulti()=0;
};

#endif
