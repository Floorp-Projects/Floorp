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

#ifndef nsPluginInstancePeer_h___
#define nsPluginInstancePeer_h___

#include "nsIPluginInstancePeer.h"
#include "nsIWindowlessPlugInstPeer.h"
#include "nsIPluginTagInfo.h"
#include "nsIPluginInstanceOwner.h"
#include "nsIJVMPluginTagInfo.h"


class nsPluginInstancePeerImpl : public nsIPluginInstancePeer,
								 public nsIWindowlessPluginInstancePeer,
                                 public nsIPluginTagInfo2,
                                 public nsIJVMPluginTagInfo
								
{
public:
  nsPluginInstancePeerImpl();
  virtual ~nsPluginInstancePeerImpl();

  NS_DECL_ISUPPORTS

  //nsIPluginInstancePeer interface

  NS_IMETHOD
  GetValue(nsPluginInstancePeerVariable variable, void *value);

  NS_IMETHOD
  GetMIMEType(nsMIMEType *result);

  NS_IMETHOD
  GetMode(nsPluginMode *result);

  NS_IMETHOD
  NewStream(nsMIMEType type, const char* target, nsIOutputStream* *result);

  NS_IMETHOD
  ShowStatus(const char* message);

  NS_IMETHOD
  SetWindowSize(PRUint32 width, PRUint32 height);

  NS_IMETHOD
  GetJSWindow(JSObject* *outJSWindow);

  // nsIWindowlessPluginInstancePeer

  // (Corresponds to NPN_InvalidateRect.)
  NS_IMETHOD
  InvalidateRect(nsPluginRect *invalidRect);

  // (Corresponds to NPN_InvalidateRegion.)
  NS_IMETHOD
  InvalidateRegion(nsPluginRegion invalidRegion);

  // (Corresponds to NPN_ForceRedraw.)
  NS_IMETHOD
  ForceRedraw(void);

  /* The tag info interfaces all pass through calls to the 
     nsPluginInstanceOwner (see nsObjectFrame.cpp) */

  //nsIPluginTagInfo interface

  NS_IMETHOD
  GetAttributes(PRUint16& n, const char*const*& names, const char*const*& values);

  NS_IMETHOD
  GetAttribute(const char* name, const char* *result);

  //nsIPluginTagInfo2 interface

  NS_IMETHOD
  GetTagType(nsPluginTagType *result);

  NS_IMETHOD
  GetTagText(const char* *result);

  NS_IMETHOD
  GetParameters(PRUint16& n, const char*const*& names, const char*const*& values);

  NS_IMETHOD
  GetParameter(const char* name, const char* *result);
  
  NS_IMETHOD
  GetDocumentBase(const char* *result);
  
  NS_IMETHOD
  GetDocumentEncoding(const char* *result);
  
  NS_IMETHOD
  GetAlignment(const char* *result);
  
  NS_IMETHOD
  GetWidth(PRUint32 *result);
  
  NS_IMETHOD
  GetHeight(PRUint32 *result);
  
  NS_IMETHOD
  GetBorderVertSpace(PRUint32 *result);
  
  NS_IMETHOD
  GetBorderHorizSpace(PRUint32 *result);

  NS_IMETHOD
  GetUniqueID(PRUint32 *result);

  //nsIJVMPluginTagInfo interface

  NS_IMETHOD
  GetCode(const char* *result);

  NS_IMETHOD
  GetCodeBase(const char* *result);

  NS_IMETHOD
  GetArchive(const char* *result);

  NS_IMETHOD
  GetName(const char* *result);

  NS_IMETHOD
  GetMayScript(PRBool *result);

  //locals

  nsresult Initialize(nsIPluginInstanceOwner *aInstance,
                      const nsMIMEType aMimeType);

  nsresult GetOwner(nsIPluginInstanceOwner *&aOwner);

private:
  nsIPluginInstance       *mInstance; //we don't add a ref to this
  nsIPluginInstanceOwner  *mOwner;    //we don't add a ref to this
  nsMIMEType              mMIMEType;
};

#endif
