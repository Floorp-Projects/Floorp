/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsPluginHostImpl_h__
#define nsPluginHostImpl_h__

#include "xp_core.h"
#include "nsIPluginManager.h"
#include "nsIPluginHost.h"
#include "nsCRT.h"
#include "prlink.h"

class ns4xPlugin;

class nsPluginTag
{
public:
  nsPluginTag();
  ~nsPluginTag();

  nsPluginTag   *mNext;
  char          *mName;
  char          *mDescription;
  char          *mMimeType;
  char          *mMimeDescription;
  char          *mExtensions;
  PRInt32       mVariants;
  char          **mMimeTypeArray;
  char          **mMimeDescriptionArray;
  char          **mExtensionsArray;
  PRLibrary     *mLibrary;
  void          *mEntryPoint;
  ns4xPlugin    *mAdapter;
  PRUint32      mFlags;
};

#define NS_PLUGIN_FLAG_ENABLED    0x0001    //is this plugin enabled?
#define NS_PLUGIN_FLAG_OLDSCHOOL  0x0002    //is this a pre-xpcom plugin?

class nsPluginHostImpl : public nsIPluginManager, public nsIPluginHost
{
public:
  nsPluginHostImpl();
  ~nsPluginHostImpl();

  void* operator new(size_t sz) {
    void* rv = new char[sz];
    nsCRT::zero(rv, sz);
    return rv;
  }

  NS_DECL_ISUPPORTS

  //nsIPluginManager interface

  NS_IMETHOD
  ReloadPlugins(PRBool reloadPages);

  NS_IMETHOD
  UserAgent(const char* *resultingAgentString);

  NS_IMETHOD
  GetValue(nsPluginManagerVariable variable, void *value);

  NS_IMETHOD
  SetValue(nsPluginManagerVariable variable, void *value);

//  NS_IMETHOD
//  FetchURL(nsISupports* peer, nsURLInfo* urlInfo);

  //nsIPluginHost interface

  NS_IMETHOD
  Init(void);

  NS_IMETHOD
  LoadPlugins(void);

  NS_IMETHOD
  InstantiatePlugin(char *aMimeType, nsISupports ** aPluginInst);

private:
  char        *mPluginPath;
  nsPluginTag *mPlugins;
};

#endif
