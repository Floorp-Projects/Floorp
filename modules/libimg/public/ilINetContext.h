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

#ifndef ilINetContext_h___
#define ilINetContext_h___

#include <stdio.h>
#include "nsISupports.h"
#include "ntypes.h"

// IID for the ilINetContext interface
#define IL_INETCONTEXT_IID    \
{ 0x425da760, 0xb412, 0x11d1,  \
{ 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } }

class ilIURL;
class ilINetReader;

class ilINetContext : public nsISupports {
public:

  virtual ilINetContext* Clone()=0;

  virtual NET_ReloadMethod GetReloadPolicy()=0;

  virtual void AddReferer(ilIURL *aUrl)=0;

  virtual void Interrupt()=0;

  virtual ilIURL* CreateURL(const char *aUrl, 
			    NET_ReloadMethod aReloadMethod)=0;

  virtual PRBool IsLocalFileURL(char *aAddress)=0;

  virtual PRBool IsURLInMemCache(ilIURL *aUrl)=0;

  virtual PRBool IsURLInDiskCache(ilIURL *aUrl)=0;

  virtual int GetURL (ilIURL * aUrl, NET_ReloadMethod aLoadMethod,
		      ilINetReader *aReader)=0;
};

#endif
