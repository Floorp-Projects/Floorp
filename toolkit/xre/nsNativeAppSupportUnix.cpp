/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Unix Native App Support.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Wu <flamingice@sourmilk.net>    (original author)
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

#include "nsNativeAppSupportBase.h"
#include "nsCOMPtr.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIObserverService.h"
#include "nsIAppStartup.h"
#include "nsServiceManagerUtils.h"
#include "prlink.h"

#include <stdlib.h>
#include <glib.h>
#include <glib-object.h>

typedef struct _GnomeProgram GnomeProgram;
typedef struct _GnomeModuleInfo GnomeModuleInfo;
typedef struct _GnomeClient GnomeClient;

typedef enum {
  GNOME_SAVE_GLOBAL,
  GNOME_SAVE_LOCAL,
  GNOME_SAVE_BOTH
} GnomeSaveStyle;

typedef enum {
  GNOME_INTERACT_NONE,
  GNOME_INTERACT_ERRORS,
  GNOME_INTERACT_ANY
} GnomeInteractStyle;

typedef enum {
  GNOME_DIALOG_ERROR,
  GNOME_DIALOG_NORMAL
} GnomeDialogType;

typedef GnomeProgram * (*_gnome_program_init_fn)(const char *, const char *,
                                                 const GnomeModuleInfo *, int,
                                                 char **, const char *, ...);
typedef const GnomeModuleInfo * (*_libgnomeui_module_info_get_fn)();
typedef GnomeClient * (*_gnome_master_client_fn)(void);
typedef void (*GnomeInteractFunction)(GnomeClient *, gint, GnomeDialogType,
                                      gpointer);
typedef void (*_gnome_client_request_interaction_fn)(GnomeClient *,
                                                     GnomeDialogType,
                                                     GnomeInteractFunction,
                                                     gpointer);
typedef void (*_gnome_interaction_key_return_fn)(gint, gboolean);

static _gnome_client_request_interaction_fn gnome_client_request_interaction;
static _gnome_interaction_key_return_fn gnome_interaction_key_return;

void interact_cb(GnomeClient *client, gint key,
                 GnomeDialogType type, gpointer data)
{
  nsCOMPtr<nsIObserverService> obsServ =
    do_GetService("@mozilla.org/observer-service;1");
  nsCOMPtr<nsISupportsPRBool> cancelQuit =
    do_CreateInstance(NS_SUPPORTS_PRBOOL_CONTRACTID);

  cancelQuit->SetData(PR_FALSE);

  obsServ->NotifyObservers(cancelQuit, "quit-application-requested", nsnull);

  PRBool abortQuit;
  cancelQuit->GetData(&abortQuit);

  gnome_interaction_key_return(key, abortQuit);
}

gboolean save_yourself_cb(GnomeClient *client, gint phase,
                          GnomeSaveStyle style, gboolean shutdown,
                          GnomeInteractStyle interact, gboolean fast,
                          gpointer user_data)
{
  if (interact == GNOME_INTERACT_ANY)
    gnome_client_request_interaction(client, GNOME_DIALOG_NORMAL,
                                     interact_cb, nsnull);
  return TRUE;
}

void die_cb(GnomeClient *client, gpointer user_data)
{
  nsCOMPtr<nsIAppStartup> appService =
    do_GetService("@mozilla.org/toolkit/app-startup;1");

  if (appService)
    appService->Quit(nsIAppStartup::eForceQuit);
}

class nsNativeAppSupportUnix : public nsNativeAppSupportBase
{
public:
  NS_IMETHOD Start(PRBool* aRetVal);
};

NS_IMETHODIMP
nsNativeAppSupportUnix::Start(PRBool *aRetVal)
{
  *aRetVal = PR_TRUE;

  PRLibrary *gnomeuiLib = PR_LoadLibrary("libgnomeui-2.so.0");
  if (!gnomeuiLib)
    return NS_OK;

  PRLibrary *gnomeLib = PR_LoadLibrary("libgnome-2.so.0");
  if (!gnomeLib) {
    PR_UnloadLibrary(gnomeuiLib);
    return NS_OK;
  }

  _gnome_program_init_fn gnome_program_init =
    (_gnome_program_init_fn)PR_FindFunctionSymbol(gnomeLib, "gnome_program_init");
  _libgnomeui_module_info_get_fn libgnomeui_module_info_get = (_libgnomeui_module_info_get_fn)PR_FindFunctionSymbol(gnomeuiLib, "libgnomeui_module_info_get");
  if (!gnome_program_init || !libgnomeui_module_info_get) {
    PR_UnloadLibrary(gnomeuiLib);
    PR_UnloadLibrary(gnomeLib);
    return NS_OK;
  }

#ifdef ACCESSIBILITY
  // We will load gail, atk-bridge by ourself later
  // We can't run atk-bridge init here, because gail get the control
  // Set GNOME_ACCESSIBILITY to 0 can avoid this
  static const char *accEnv = "GNOME_ACCESSIBILITY";
  const char *accOldValue = getenv(accEnv);
  setenv(accEnv, "0", 1);
#endif

  char *argv[2] = { "gecko", "--disable-crash-dialog" };
  gnome_program_init("Gecko", "1.0", libgnomeui_module_info_get(), 2, argv, NULL);

#ifdef ACCESSIBILITY
  if (accOldValue) { 
    setenv(accEnv, accOldValue, 1);
  } else {
    unsetenv(accEnv);
  }
#endif

  // Careful! These libraries cannot be unloaded after this point because
  // gnome_program_init causes atexit handlers to be registered. Strange
  // crashes will occur if these libraries are unloaded.

  gnome_client_request_interaction = (_gnome_client_request_interaction_fn)
    PR_FindFunctionSymbol(gnomeuiLib, "gnome_client_request_interaction");
  gnome_interaction_key_return = (_gnome_interaction_key_return_fn)
    PR_FindFunctionSymbol(gnomeuiLib, "gnome_interaction_key_return");

  _gnome_master_client_fn gnome_master_client = (_gnome_master_client_fn)
    PR_FindFunctionSymbol(gnomeuiLib, "gnome_master_client");

  GnomeClient *client = gnome_master_client();
  g_signal_connect(client, "save-yourself", G_CALLBACK(save_yourself_cb), NULL);
  g_signal_connect(client, "die", G_CALLBACK(die_cb), NULL);

  return NS_OK;
}

nsresult
NS_CreateNativeAppSupport(nsINativeAppSupport **aResult)
{
  nsNativeAppSupportBase* native = new nsNativeAppSupportUnix();
  if (!native)
    return NS_ERROR_OUT_OF_MEMORY;

  *aResult = native;
  NS_ADDREF(*aResult);

  return NS_OK;
}
