/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Andrei Volkov <av@netscape.com>
 *   Brian Stell <bstell@netscape.com>
 *   Peter Lubczynski <peterl@netscape.com>
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

#ifndef _nsPluginNativeWindow_h_
#define _nsPluginNativeWindow_h_

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsIPluginInstance.h"
#include "nsplugindefs.h"
#include "nsIWidget.h"

/**
 * base class for native plugin window implementations
 */
class nsPluginNativeWindow : public nsPluginWindow
{
public: 
  nsPluginNativeWindow() : nsPluginWindow() {}
  virtual ~nsPluginNativeWindow() {}

  /**
   *   !!! CAUTION !!!
   *
   * The base class |nsPluginWindow| is defined as a struct in nsplugindefs.h,
   * thus it does not have a destructor of its own.
   * One should never attempt to delete |nsPluginNativeWindow| object instance
   * (or derivatives) using a pointer of |nsPluginWindow *| type. Should such
   * necessity occur it must be properly casted first.
   */

public:
  nsresult GetPluginInstance(nsCOMPtr<nsIPluginInstance> &aPluginInstance) { 
    aPluginInstance = mPluginInstance;
    return NS_OK;
  }
  nsresult SetPluginInstance(nsIPluginInstance *aPluginInstance) { 
    if (mPluginInstance != aPluginInstance)
      mPluginInstance = aPluginInstance;
    return NS_OK;
  }

  nsresult GetPluginWidget(nsIWidget **aWidget) {
    NS_IF_ADDREF(*aWidget = mWidget);
    return NS_OK;
  }
  nsresult SetPluginWidget(nsIWidget *aWidget) { 
    mWidget = aWidget;
    return NS_OK;
  }

public:
  virtual nsresult CallSetWindow(nsCOMPtr<nsIPluginInstance> &aPluginInstance) {
    // null aPluginInstance means that we want to call SetWindow(null)
    if (aPluginInstance)
      aPluginInstance->SetWindow(this);
    else if (mPluginInstance)
      mPluginInstance->SetWindow(nsnull);

    SetPluginInstance(aPluginInstance);
    return NS_OK;
  }

protected:
  nsCOMPtr<nsIPluginInstance> mPluginInstance;
  nsCOMPtr<nsIWidget>         mWidget;
};

nsresult PLUG_NewPluginNativeWindow(nsPluginNativeWindow ** aPluginNativeWindow);
nsresult PLUG_DeletePluginNativeWindow(nsPluginNativeWindow * aPluginNativeWindow);

#endif //_nsPluginNativeWindow_h_
