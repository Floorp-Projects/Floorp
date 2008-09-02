/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=8:
 */
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
 * Christopher Blizzard.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 *   Benjamin Smedberg <benjamin@smedbergs.us>
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

#include "nsQtRemoteService.h"

#include <X11/Xatom.h> // for XA_STRING
#include <stdlib.h>

#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsPIDOMWindow.h"
#include "nsIGenericFactory.h"
#include "nsILocalFile.h"
#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "nsIWeakReference.h"
#include "nsIWidget.h"
#include "nsIAppShellService.h"
#include "nsAppShellCID.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "prprf.h"
#include "prenv.h"
#include "nsCRT.h"

#ifdef MOZ_WIDGET_GTK2
//#include "nsGTKToolkit.h"
#endif

#include "nsICommandLineRunner.h"
#include "nsXULAppAPI.h"

#define MOZILLA_VERSION_PROP   "_MOZILLA_VERSION"
#define MOZILLA_LOCK_PROP      "_MOZILLA_LOCK"
#define MOZILLA_COMMAND_PROP   "_MOZILLA_COMMAND"
#define MOZILLA_RESPONSE_PROP  "_MOZILLA_RESPONSE"
#define MOZILLA_USER_PROP      "_MOZILLA_USER"
#define MOZILLA_PROFILE_PROP   "_MOZILLA_PROFILE"
#define MOZILLA_PROGRAM_PROP   "_MOZILLA_PROGRAM"
#define MOZILLA_COMMANDLINE_PROP "_MOZILLA_COMMANDLINE"

#ifdef IS_BIG_ENDIAN
#define TO_LITTLE_ENDIAN32(x) \
    ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >> 8) | \
    (((x) & 0x0000ff00) << 8) | (((x) & 0x000000ff) << 24))
#else
#define TO_LITTLE_ENDIAN32(x) (x)
#endif

const unsigned char kRemoteVersion[] = "5.1";

NS_IMPL_ISUPPORTS2(nsQtRemoteService,
                   nsIRemoteService,
                   nsIObserver)

NS_IMETHODIMP
nsQtRemoteService::Startup(const char* aAppName, const char* aProfileName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// #ifdef MOZ_WIDGET_GTK2
// static nsGTKToolkit* GetGTKToolkit()
// {
//   nsCOMPtr<nsIAppShellService> svc = do_GetService(NS_APPSHELLSERVICE_CONTRACTID);
//   if (!svc)
//     return nsnull;
//   nsCOMPtr<nsIDOMWindowInternal> window;
//   svc->GetHiddenDOMWindow(getter_AddRefs(window));
//   if (!window)
//     return nsnull;
//   nsIWidget* widget = GetMainWidget(window);
//   if (!widget)
//     return nsnull;
//   nsIToolkit* toolkit = widget->GetToolkit();
//   if (!toolkit)
//     return nsnull;
//   return static_cast<nsGTKToolkit*>(toolkit);
// }
// #endif

NS_IMETHODIMP
nsQtRemoteService::RegisterWindow(nsIDOMWindow* aWindow)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsQtRemoteService::Shutdown()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsQtRemoteService::Observe(nsISupports* aSubject,
                            const char *aTopic,
                            const PRUnichar *aData)
{
  return NS_OK;
}

// {C0773E90-5799-4eff-AD03-3EBCD85624AC}
#define NS_REMOTESERVICE_CID \
  { 0xc0773e90, 0x5799, 0x4eff, { 0xad, 0x3, 0x3e, 0xbc, 0xd8, 0x56, 0x24, 0xac } }

NS_GENERIC_FACTORY_CONSTRUCTOR(nsQtRemoteService)

static const nsModuleComponentInfo components[] =
{
  { "Remote Service",
    NS_REMOTESERVICE_CID,
    "@mozilla.org/toolkit/remote-service;1",
    nsQtRemoteServiceConstructor
  }
};

NS_IMPL_NSGETMODULE(RemoteServiceModule, components)
