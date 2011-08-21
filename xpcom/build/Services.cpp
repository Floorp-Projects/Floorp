/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Taras Glek <tglek@mozilla.com>
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

#include "mozilla/Services.h"
#include "nsComponentManager.h"
#include "nsIIOService.h"
#include "nsIDirectoryService.h"
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif
#include "nsIChromeRegistry.h"
#include "nsIObserverService.h"
#include "nsNetCID.h"
#include "nsObserverService.h"
#include "nsXPCOMPrivate.h"
#include "nsIStringBundle.h"
#include "nsIToolkitChromeRegistry.h"
#include "nsIXULOverlayProvider.h"
#include "IHistory.h"
#include "nsIXPConnect.h"

using namespace mozilla;
using namespace mozilla::services;

/*
 * Define a global variable and a getter for every service in ServiceList.
 * eg. gIOService and GetIOService()
 */
#define MOZ_SERVICE(NAME, TYPE, CONTRACT_ID)                            \
  static TYPE* g##NAME = nsnull;                                        \
                                                                        \
  already_AddRefed<TYPE>                                         \
  mozilla::services::Get##NAME()                                        \
  {                                                                     \
    if (!g##NAME) {                                                     \
      nsCOMPtr<TYPE> os = do_GetService(CONTRACT_ID);                   \
      g##NAME = os.forget().get();                                      \
    }                                                                   \
    NS_IF_ADDREF(g##NAME);                                              \
    return g##NAME;                                                     \
  }

#include "ServiceList.h"
#undef MOZ_SERVICE

/**
 * Clears service cache, sets gXPCOMShuttingDown
 */
void 
mozilla::services::Shutdown()
{
  gXPCOMShuttingDown = PR_TRUE;
#define MOZ_SERVICE(NAME, TYPE, CONTRACT_ID) NS_IF_RELEASE(g##NAME);
#include "ServiceList.h"
#undef MOZ_SERVICE
}
