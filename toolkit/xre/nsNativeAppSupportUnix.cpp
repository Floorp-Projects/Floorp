/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

#include "nsIFile.h"
#include "nsDirectoryServiceDefs.h"
#include "nsICommandLineRunner.h"
#include "nsIWindowMediator.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIBaseWindow.h"
#include "nsIWidget.h"
#include "nsIWritablePropertyBag2.h"
#include "nsIPrefService.h"
#include "mozilla/Services.h"

#include <stdlib.h>
#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#ifdef MOZ_X11
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#endif

#ifdef MOZ_ENABLE_DBUS
#include <dbus/dbus.h>
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
typedef void (*_gnome_client_set_restart_command_fn)(GnomeClient*, gint, gchar*[]);

static _gnome_client_set_restart_command_fn gnome_client_set_restart_command;

gboolean save_yourself_cb(GnomeClient *client, gint phase,
                          GnomeSaveStyle style, gboolean shutdown,
                          GnomeInteractStyle interact, gboolean fast,
                          gpointer user_data)
{
  nsCOMPtr<nsIObserverService> obsServ =
    mozilla::services::GetObserverService();

  nsCOMPtr<nsISupportsPRBool> didSaveSession =
    do_CreateInstance(NS_SUPPORTS_PRBOOL_CONTRACTID);

  if (!obsServ || !didSaveSession)
    return TRUE; // OOM

  // Notify observers to save the session state
  didSaveSession->SetData(false);
  obsServ->NotifyObservers(didSaveSession, "session-save", nullptr);

  bool status;
  didSaveSession->GetData(&status);

  // If there was no session saved and the save_yourself request is
  // caused by upcoming shutdown we like to prepare for it
  if (!status && shutdown) {
    nsCOMPtr<nsISupportsPRBool> cancelQuit =
      do_CreateInstance(NS_SUPPORTS_PRBOOL_CONTRACTID);

    cancelQuit->SetData(false);
    obsServ->NotifyObservers(cancelQuit, "quit-application-requested", nullptr);

    bool abortQuit;
    cancelQuit->GetData(&abortQuit);
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
  NS_IMETHOD Start(bool* aRetVal);
  NS_IMETHOD Stop(bool *aResult);
  NS_IMETHOD Enable();

private:
};

NS_IMETHODIMP
nsNativeAppSupportUnix::Start(bool *aRetVal)
{
  NS_ASSERTION(gAppData, "gAppData must not be null.");

// The dbus library is used by both nsWifiScannerDBus and BluetoothDBusService,
// from diffrent threads. This could lead to race conditions if the dbus is not
// initialized before making any other library calls.
#ifdef MOZ_ENABLE_DBUS
  dbus_threads_init_default();
#endif

#if (MOZ_WIDGET_GTK == 2)
  if (gtk_major_version < MIN_GTK_MAJOR_VERSION ||
      (gtk_major_version == MIN_GTK_MAJOR_VERSION && gtk_minor_version < MIN_GTK_MINOR_VERSION)) {
    GtkWidget* versionErrDialog = gtk_message_dialog_new(nullptr,
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
#endif

  *aRetVal = true;

#if defined(MOZ_X11) && (MOZ_WIDGET_GTK == 2)

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

#endif /* MOZ_X11 && (MOZ_WIDGET_GTK == 2) */

#ifdef ACCESSIBILITY
  // We will load gail, atk-bridge by ourself later
  // We can't run atk-bridge init here, because gail get the control
  // Set GNOME_ACCESSIBILITY to 0 can avoid this
  static const char *accEnv = "GNOME_ACCESSIBILITY";
  const char *accOldValue = getenv(accEnv);
  setenv(accEnv, "0", 1);
#endif

#if defined(MOZ_X11) && (MOZ_WIDGET_GTK == 2)
  if (!gnome_program_get()) {
    gnome_program_init("Gecko", "1.0", libgnomeui_module_info_get(),
                       gArgc, gArgv, nullptr);
  }
#endif /* MOZ_X11 && (MOZ_WIDGET_GTK == 2) */

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

  // TODO GTK3 - see Bug 694570 - Stop using libgnome and libgnomeui on Linux
#if defined(MOZ_X11) && (MOZ_WIDGET_GTK == 2)
  gnome_client_set_restart_command = (_gnome_client_set_restart_command_fn)
    PR_FindFunctionSymbol(gnomeuiLib, "gnome_client_set_restart_command");

  _gnome_master_client_fn gnome_master_client = (_gnome_master_client_fn)
    PR_FindFunctionSymbol(gnomeuiLib, "gnome_master_client");

  GnomeClient *client = gnome_master_client();
  g_signal_connect(client, "save-yourself", G_CALLBACK(save_yourself_cb), nullptr);
  g_signal_connect(client, "die", G_CALLBACK(die_cb), nullptr);

  // Set the correct/requested restart command in any case.

  // Is there a request to suppress default binary launcher?
  nsAutoCString path;
  char* argv1 = getenv("MOZ_APP_LAUNCHER");

  if(!argv1) {
    // Tell the desktop the command for restarting us so that we can be part of XSMP session restore
    NS_ASSERTION(gDirServiceProvider, "gDirServiceProvider is NULL! This shouldn't happen!");
    nsCOMPtr<nsIFile> executablePath;
    nsresult rv;

    bool dummy;
    rv = gDirServiceProvider->GetFile(XRE_EXECUTABLE_FILE, &dummy, getter_AddRefs(executablePath));

    if (NS_SUCCEEDED(rv)) {
      // Strip off the -bin suffix to get the shell script we should run; this is what Breakpad does
      nsAutoCString leafName;
      rv = executablePath->GetNativeLeafName(leafName);
      if (NS_SUCCEEDED(rv) && StringEndsWith(leafName, NS_LITERAL_CSTRING("-bin"))) {
        leafName.SetLength(leafName.Length() - strlen("-bin"));
        executablePath->SetNativeLeafName(leafName);
      }

      executablePath->GetNativePath(path);
      argv1 = (char*)(path.get());
    }
  }

  if (argv1) {
    gnome_client_set_restart_command(client, 1, &argv1);
  }
#endif /* MOZ_X11 && (MOZ_WIDGET_GTK == 2) */

  return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportUnix::Stop(bool *aResult)
{
  NS_ENSURE_ARG(aResult);
  *aResult = true;
  return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportUnix::Enable()
{
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
