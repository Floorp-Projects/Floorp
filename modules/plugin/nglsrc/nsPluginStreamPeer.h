/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsPluginStreamPeer_h___
#define nsPluginStreamPeer_h___

#include "nscore.h"
#include "nsIPluginStreamPeer.h"

class nsIURI;

class nsPluginStreamPeer : public nsIPluginStreamPeer
{
public:
  nsPluginStreamPeer();
  ~nsPluginStreamPeer();

  NS_DECL_ISUPPORTS

  NS_IMETHOD
  GetURL(const char* *result);

  NS_IMETHOD
  GetEnd(PRUint32 *result);

  NS_IMETHOD
  GetLastModified(PRUint32 *result);

  NS_IMETHOD
  GetNotifyData(void* *result);

  NS_IMETHOD
  GetReason(nsPluginReason *result);

  NS_IMETHOD
  GetMIMEType(nsMIMEType *result);

  //locals

  nsresult Initialize(nsIURI *aURL, PRUint32 aLength,
                      PRUint32 aLastMod, nsMIMEType aMIMEType,
                      void *aNotifyData);

  nsresult SetReason(nsPluginReason aReason);

private:
  nsIURI          *mURL;
  PRUint32        mLength;
  PRUint32        mLastMod;
  void            *mNotifyData;
  nsMIMEType      mMIMEType;
  char            *mURLSpec;
  nsPluginReason  mReason;
};

#endif
