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
 *   Michael Ventnor <m.ventnor@gmail.com>
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
#include "nsXREDirProvider.h"
#include "nsReadableUtils.h"

#include <stdlib.h>
#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#ifdef NS_OSSO
struct DBusMessage;  /* libosso.h references internals of dbus */

#include <dbus/dbus.h>
#include <dbus/dbus-protocol.h>
#include <libosso.h>

#endif

#define MIN_GTK_MAJOR_VERSION 2
#define MIN_GTK_MINOR_VERSION 10
#define UNSUPPORTED_GTK_MSG "We're sorry, this application requires a version of the GTK+ library that is not installed on your computer.\n\n\
You have GTK+ %d.%d.\nThis application requires GTK+ %d.%d or newer.\n\n\
Please upgrade your GTK+ library if you wish to use this application."

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
typedef GnomeProgram * (*_gnome_program_get_fn)(void);
typedef const GnomeModuleInfo * (*_libgnomeui_module_info_get_fn)();
typedef GnomeClient * (*_gnome_master_client_fn)(void);
typedef void (*GnomeInteractFunction)(GnomeClient *, gint, GnomeDialogType,
                                      gpointer);
typedef void (*_gnome_client_request_interaction_fn)(GnomeClient *,
                                                     GnomeDialogType,
                                                     GnomeInteractFunction,
                                                     gpointer);
typedef void (*_gnome_interaction_key_return_fn)(gint, gboolean);
typedef void (*_gnome_client_set_restart_command_fn)(GnomeClient*, gint, gchar*[]);

static _gnome_client_request_interaction_fn gnome_client_request_interaction;
static _gnome_interaction_key_return_fn gnome_interaction_key_return;
static _gnome_client_set_restart_command_fn gnome_client_set_restart_command;

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
  if (!shutdown)
    return TRUE;

  nsCOMPtr<nsIObserverService> obsServ =
    do_GetService("@mozilla.org/observer-service;1");

  nsCOMPtr<nsISupportsPRBool> didSaveSession =
    do_CreateInstance(NS_SUPPORTS_PRBOOL_CONTRACTID);

  if (!obsServ || !didSaveSession)
    return TRUE; // OOM

  didSaveSession->SetData(PR_FALSE);
  obsServ->NotifyObservers(didSaveSession, "session-save", nsnull);

  PRBool status;
  didSaveSession->GetData(&status);

  // Didn't save, or no way of saving. So signal for quit-application.
  if (!status) {
    if (interact == GNOME_INTERACT_ANY)
      gnome_client_request_interaction(client, GNOME_DIALOG_NORMAL,
                                       interact_cb, nsnull);
    return TRUE;
  }
  
  // Is there a request to suppress default binary launcher? 
  char* argv1 = getenv("MOZ_APP_LAUNCHER");

  if(!argv1) {
    // Tell GNOME the command for restarting us so that we can be part of XSMP session restore
    NS_ASSERTION(gDirServiceProvider, "gDirServiceProvider is NULL! This shouldn't happen!");
    nsCOMPtr<nsIFile> executablePath;
    nsresult rv;

    PRBool dummy;
    rv = gDirServiceProvider->GetFile(XRE_EXECUTABLE_FILE, &dummy, getter_AddRefs(executablePath));

    if (NS_SUCCEEDED(rv)) {
      nsCAutoString path;

      // Strip off the -bin suffix to get the shell script we should run; this is what Breakpad does
      nsCAutoString leafName;
      rv = executablePath->GetNativeLeafName(leafName);
      if (NS_SUCCEEDED(rv) && StringEndsWith(leafName, NS_LITERAL_CSTRING("-bin"))) {
        leafName.SetLength(leafName.Length() - strlen("-bin"));
        executablePath->SetNativeLeafName(leafName);
      }
  
      executablePath->GetNativePath(path);
      argv1 = (char*)(path.get());
    }
  }

  if(argv1) {
    gnome_client_set_restart_command(client, 1, &argv1);
  }

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
  NS_IMETHOD Stop( PRBool *aResult);

private:
#ifdef NS_OSSO
  osso_context_t *m_osso_context;    
  /* A note about why we need to have m_hw_state:
     the osso hardware callback does not tell us what changed, just
     that something has changed.  We need to keep track of state so
     that we can determine what has changed.
  */  
  osso_hw_state_t m_hw_state;
#endif
};

#ifdef NS_OSSO

static void OssoDisplayCallback(osso_display_state_t state, gpointer data)
{
  nsCOMPtr<nsIObserverService> os = do_GetService("@mozilla.org/observer-service;1");
  if (!os)
      return;
 
  if (state == OSSO_DISPLAY_ON)
      os->NotifyObservers(nsnull, "system-display-on", nsnull);
  else
      os->NotifyObservers(nsnull, "system-display-dimmed-or-off", nsnull);
}

static void OssoHardwareCallback(osso_hw_state_t *state, gpointer data)
{
  NS_ASSERTION(state, "osso_hw_state_t must not be null.");
  NS_ASSERTION(data, "data must not be null.");

  osso_hw_state_t* ourState = (osso_hw_state_t*) data;

  if (state->shutdown_ind) {
    nsCOMPtr<nsIAppStartup> appService =  do_GetService("@mozilla.org/toolkit/app-startup;1");
    if (appService)
      appService->Quit(nsIAppStartup::eForceQuit);
    return;
  }

  if (state->memory_low_ind) {
      if (! ourState->memory_low_ind) {
      nsCOMPtr<nsIObserverService> os = do_GetService("@mozilla.org/observer-service;1");
      if (os)
        os->NotifyObservers(nsnull, "memory-pressure", NS_LITERAL_STRING("low-memory").get());
    }
  }
  
  if (state->system_inactivity_ind != ourState->system_inactivity_ind) {
      nsCOMPtr<nsIObserverService> os = do_GetService("@mozilla.org/observer-service;1");
      if (!os)
        return;
 
      if (state->system_inactivity_ind)
          os->NotifyObservers(nsnull, "system-idle", nsnull);
      else
          os->NotifyObservers(nsnull, "system-active", nsnull);
  }

  memcpy(ourState, state, sizeof(osso_hw_state_t));
}

#endif

NS_IMETHODIMP
nsNativeAppSupportUnix::Start(PRBool *aRetVal)
{
  NS_ASSERTION(gAppData, "gAppData must not be null.");

  if (gtk_major_version < MIN_GTK_MAJOR_VERSION ||
      (gtk_major_version == MIN_GTK_MAJOR_VERSION && gtk_minor_version < MIN_GTK_MINOR_VERSION)) {
    GtkWidget* versionErrDialog = gtk_message_dialog_new(NULL,
                     GtkDialogFlags(GTK_DIALOG_MODAL |
                                    GTK_DIALOG_DESTROY_WITH_PARENT),
                     GTK_MESSAGE_ERROR,
                     GTK_BUTTONS_OK,
                     UNSUPPORTED_GTK_MSG,
                     gtk_major_version,
                     gtk_minor_version,
                     MIN_GTK_MAJOR_VERSION,
                     MIN_GTK_MINOR_VERSION);
    gtk_dialog_run(GTK_DIALOG(versionErrDialog));
    gtk_widget_destroy(versionErrDialog);
    exit(0);
  }

#ifdef NS_OSSO
  /* zero state out. */
  memset(&m_hw_state, 0, sizeof(osso_hw_state_t));

  /* Initialize maemo application
     
     The initalization name will be of the form "Vendor.Name".
     If a Vendor isn't given, then we will just use "Name".
     
     Note that this value must match your X-Osso-Service name
     defined in your desktop file.  If it doesn't, the OSSO
     system will happily kill your process.
  */
  nsCAutoString applicationName;
  if(gAppData->vendor) {
      applicationName.Append(gAppData->vendor);
      applicationName.Append(".");
  }
  applicationName.Append(gAppData->name);

  m_osso_context = osso_initialize(applicationName.get(), 
                                   gAppData->version ? gAppData->version : "1.0",
                                   PR_TRUE,
                                   nsnull);

  /* Check that initilialization was ok */
  if (m_osso_context == nsnull) {
      return NS_ERROR_FAILURE;
  }

  osso_hw_set_event_cb(m_osso_context,
                       nsnull,
                       OssoHardwareCallback,
                       &m_hw_state);


  osso_hw_set_display_event_cb(m_osso_context,
                               OssoDisplayCallback,
                               nsnull);




#endif

  *aRetVal = PR_TRUE;

#ifdef MOZ_X11

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
  _gnome_program_get_fn gnome_program_get =
    (_gnome_program_get_fn)PR_FindFunctionSymbol(gnomeLib, "gnome_program_get"); 
 _libgnomeui_module_info_get_fn libgnomeui_module_info_get = (_libgnomeui_module_info_get_fn)PR_FindFunctionSymbol(gnomeuiLib, "libgnomeui_module_info_get");
  if (!gnome_program_init || !gnome_program_get || !libgnomeui_module_info_get) {
    PR_UnloadLibrary(gnomeuiLib);
    PR_UnloadLibrary(gnomeLib);
    return NS_OK;
  }

#endif /* MOZ_X11 */

#ifdef ACCESSIBILITY
  // We will load gail, atk-bridge by ourself later
  // We can't run atk-bridge init here, because gail get the control
  // Set GNOME_ACCESSIBILITY to 0 can avoid this
  static const char *accEnv = "GNOME_ACCESSIBILITY";
  const char *accOldValue = getenv(accEnv);
  setenv(accEnv, "0", 1);
#endif

#ifdef MOZ_X11
  if (!gnome_program_get()) {
    gnome_program_init("Gecko", "1.0", libgnomeui_module_info_get(), gArgc, gArgv, NULL);
  }
#endif /* MOZ_X11 */

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

#ifdef MOZ_X11
  gnome_client_request_interaction = (_gnome_client_request_interaction_fn)
    PR_FindFunctionSymbol(gnomeuiLib, "gnome_client_request_interaction");
  gnome_interaction_key_return = (_gnome_interaction_key_return_fn)
    PR_FindFunctionSymbol(gnomeuiLib, "gnome_interaction_key_return");
  gnome_client_set_restart_command = (_gnome_client_set_restart_command_fn)
    PR_FindFunctionSymbol(gnomeuiLib, "gnome_client_set_restart_command");

  _gnome_master_client_fn gnome_master_client = (_gnome_master_client_fn)
    PR_FindFunctionSymbol(gnomeuiLib, "gnome_master_client");

  GnomeClient *client = gnome_master_client();
  g_signal_connect(client, "save-yourself", G_CALLBACK(save_yourself_cb), NULL);
  g_signal_connect(client, "die", G_CALLBACK(die_cb), NULL);
#endif /* MOZ_X11 */

  return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportUnix::Stop( PRBool *aResult )
{
  NS_ENSURE_ARG( aResult );
  *aResult = PR_TRUE;

#ifdef NS_OSSO
  if (m_osso_context)
  {
    osso_hw_unset_event_cb(m_osso_context, nsnull);
    osso_deinitialize(m_osso_context);
    m_osso_context = nsnull;
  }
#endif
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
