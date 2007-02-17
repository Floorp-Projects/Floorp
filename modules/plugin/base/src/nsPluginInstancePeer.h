/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsPluginInstancePeer_h___
#define nsPluginInstancePeer_h___

#include "nsIPluginInstancePeer2.h"
#include "nsIWindowlessPlugInstPeer.h"
#include "nsIPluginTagInfo2.h"
#include "nsIPluginInstanceOwner.h"
#ifdef OJI
#include "nsIJVMPluginTagInfo.h"
#endif
#include "nsPIPluginInstancePeer.h"

class nsPluginInstancePeerImpl : public nsIPluginInstancePeer2,
                                 public nsIWindowlessPluginInstancePeer,
                                 public nsIPluginTagInfo2,
#ifdef OJI
                                 public nsIJVMPluginTagInfo,
#endif
                                 public nsPIPluginInstancePeer
								
{
public:
  nsPluginInstancePeerImpl();
  virtual ~nsPluginInstancePeerImpl();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPLUGININSTANCEPEER
  NS_DECL_NSIWINDOWLESSPLUGININSTANCEPEER
  NS_DECL_NSIPLUGININSTANCEPEER2
  NS_DECL_NSIPLUGINTAGINFO
  NS_DECL_NSIPLUGINTAGINFO2

  //XXX Why isn't this ifdef'd like the class declaration?
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

  NS_DECL_NSPIPLUGININSTANCEPEER

  //locals

  nsresult Initialize(nsIPluginInstanceOwner *aOwner,
                      const nsMIMEType aMimeType);

  nsresult SetOwner(nsIPluginInstanceOwner *aOwner);

private:
  nsIPluginInstance       *mInstance; //we don't add a ref to this
  nsIPluginInstanceOwner  *mOwner;    //we don't add a ref to this
  nsMIMEType              mMIMEType;
  PRUint32                mThreadID;
  PRBool                  mStopped;
};

#endif
