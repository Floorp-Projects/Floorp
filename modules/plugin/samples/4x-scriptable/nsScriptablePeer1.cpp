/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

// ==============================
// ! Scriptability related code !
// ==============================
//

/////////////////////////////////////////////////////
//
// This file implements the nsScriptablePeer object
// The native methods of this class are supposed to
// be callable from JavaScript
//
#include "plugin.h"

nsScriptablePeer::nsScriptablePeer(CPlugin* aPlugin)
{

  NS_INIT_ISUPPORTS();
  mPlugin = aPlugin;
}

nsScriptablePeer::~nsScriptablePeer()
{
}

// Notice that we expose our claim to implement nsIClassInfo.
NS_IMPL_ISUPPORTS2(nsScriptablePeer, nsI4xScriptablePlugin, nsIClassInfo)

//
// the following two methods will be callable from JavaScript
//
NS_IMETHODIMP nsScriptablePeer::ShowVersion()
{
  if (mPlugin)
    mPlugin->showVersion();

  return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::Clear()
{
  if (mPlugin)
    mPlugin->clear();

  return NS_OK;
}
