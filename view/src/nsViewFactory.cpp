/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nscore.h"
#include "nsIGenericFactory.h"
#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIGenericFactory.h"

#include "nsViewsCID.h"
#include "nsView.h"
#include "nsScrollingView.h"
#include "nsScrollPortView.h"

#include "nsIModule.h"
#include "nsIPref.h"

#include "nsViewManager.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsViewManager)

static NS_IMETHODIMP ViewManagerConstructor(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  return nsViewManagerConstructor(aOuter, aIID, aResult);
}

/* man, I'm going to hell for this, but they're not refcounted */
#undef NS_ADDREF
#define NS_ADDREF(x)  (void)0;
#undef NS_RELEASE
#define NS_RELEASE(x) (void)0;
NS_GENERIC_FACTORY_CONSTRUCTOR(nsView)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScrollingView)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScrollPortView)

static nsModuleComponentInfo components[] = {
  { "View Manager", NS_VIEW_MANAGER_CID, "@mozilla.org/view-manager;1",
    ViewManagerConstructor },
  { "View", NS_VIEW_CID, "@mozilla.org/view;1", nsViewConstructor },
  { "Scrolling View", NS_SCROLLING_VIEW_CID, "@mozilla.org/scrolling-view;1",
    nsScrollingViewConstructor },
  { "Scroll Port View", NS_SCROLL_PORT_VIEW_CID,
    "@mozilla.org/scroll-port-view;1", nsScrollPortViewConstructor }
};

NS_IMPL_NSGETMODULE(nsViewModule, components)
