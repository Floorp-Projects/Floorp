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

#ifndef ilINetContext_h___
#define ilINetContext_h___

#include <stdio.h>
#include "nsISupports.h"

// IID for the ilINetContext interface
#define IL_INETCONTEXT_IID    \
{ 0x425da760, 0xb412, 0x11d1,  \
{ 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } }

class ilIURL;
class ilINetReader;

class ilINetContext : public nsISupports {
public:

  NS_DEFINE_STATIC_IID_ACCESSOR(IL_INETCONTEXT_IID)

  virtual ilINetContext* Clone()=0;

  virtual ImgCachePolicy GetReloadPolicy()=0;

  virtual void AddReferer(ilIURL *aUrl)=0;

  virtual void Interrupt()=0;

  virtual ilIURL* CreateURL(const char *aUrl, 
			    ImgCachePolicy aReloadMethod)=0;

  virtual PRBool IsLocalFileURL(char *aAddress)=0;

#ifdef NU_CACHE
  virtual PRBool IsURLInCache(ilIURL* iUrl)=0;
#else
  virtual PRBool IsURLInMemCache(ilIURL *aUrl)=0;

  virtual PRBool IsURLInDiskCache(ilIURL *aUrl)=0;
#endif /* NU_CACHE */

  virtual int GetURL (ilIURL * aUrl, ImgCachePolicy aLoadMethod,
		      ilINetReader *aReader, PRBool IsAnimationLoop)=0;

  virtual int GetContentLength (ilIURL * aUrl)=0;
};

#endif
