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

#if (MOZ_PLATFORM_MAEMO == 5)
struct DBusMessage;  /* libosso.h references internals of dbus */

#ifndef MOZ_ENABLE_DBUS
#include <dbus/dbus.h>
#endif

#include <dbus/dbus-protocol.h>
#include <libosso.h>

// These come from <mce/dbus-names.h> (maemo sdk 5+)
#define MCE_SERVICE "com.nokia.mce"
#define MCE_REQUEST_IF "com.nokia.mce.request"
#define MCE_REQUEST_PATH "/com/nokia/mce/request"
#define MCE_SIGNAL_IF "com.nokia.mce.signal"
#define MCE_DEVICE_ORIENTATION_SIG "sig_device_orientation_ind"
#define MCE_MATCH_RULE "type='signal',interface='" MCE_SIGNAL_IF "',member='" MCE_DEVICE_ORIENTATION_SIG "'"
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
#if (MOZ_PLATFORM_MAEMO == 5)
  osso_context_t *m_osso_context;    
  /* A note about why we need to have m_hw_state:
     the osso hardware callback does not tell us what changed, just
     that something has changed.  We need to keep track of state so
     that we can determine what has changed.
  */  
  osso_hw_state_t m_hw_state;
#endif
};

#if (MOZ_PLATFORM_MAEMO == 5)
static nsresult
GetMostRecentWindow(const PRUnichar* aType, nsIDOMWindow** aWindow)
{
  nsCOMPtr<nsIWindowMediator> wm = do_GetService("@mozilla.org/appshell/window-mediator;1");
  if (wm)
    return wm->GetMostRecentWindow(aType, aWindow);
  return NS_ERROR_FAILURE;
}

static GtkWidget*
WidgetForDOMWindow(nsISupports *aWindow)
{
  nsCOMPtr<nsPIDOMWindow> domWindow(do_QueryInterface(aWindow));
  if (!domWindow)
    return NULL;

  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(domWindow->GetDocShell());
  if (!baseWindow)
    return NULL;

  nsCOMPtr<nsIWidget> widget;
  baseWindow->GetMainWidget(getter_AddRefs(widget));
  if (!widget)
    return NULL;

  return (GtkWidget*)(widget->GetNativeData(NS_NATIVE_SHELLWIDGET));
}

static void
OssoSetWindowOrientation(bool aPortrait)
{
  // If we locked the screen, ignore any orientation changes
  bool lockScreen = false;
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs)
    prefs->GetBoolPref("toolkit.screen.lock", &lockScreen);

  if (lockScreen)
    return;

  // Tell Hildon desktop to force our window to be either portrait or landscape,
  // depending on the current rotation
  // NOTE: We only update the most recent top-level window so this is only
  //       suitable for apps with only one window.
  nsCOMPtr<nsIDOMWindow> window;
  GetMostRecentWindow(EmptyString().get(), getter_AddRefs(window));
  GtkWidget* widget = WidgetForDOMWindow(window);
  if (widget && widget->window) {
    GdkWindow *gdk = widget->window;
    GdkAtom request = gdk_atom_intern("_HILDON_PORTRAIT_MODE_REQUEST", FALSE);

    if (aPortrait) {
      gulong portrait_set = 1;
      gdk_property_change(gdk, request, gdk_x11_xatom_to_atom(XA_CARDINAL),
                          32, GDK_PROP_MODE_REPLACE, (const guchar *) &portrait_set, 1);
    }
    else {
      gdk_property_delete(gdk, request);
    }
  }

  // Update the system info property
  nsCOMPtr<nsIWritablePropertyBag2> info = do_GetService("@mozilla.org/system-info;1");
  if (info) {
    info->SetPropertyAsAString(NS_LITERAL_STRING("screen-orientation"),
                               aPortrait ? NS_LITERAL_STRING("portrait") : NS_LITERAL_STRING("landscape"));
  }
}

static bool OssoIsScreenOn(osso_context_t* ctx)
{
  osso_return_t rv;
  osso_rpc_t ret;
  bool result = false;

  rv = osso_rpc_run_system(ctx, MCE_SERVICE, MCE_REQUEST_PATH, MCE_REQUEST_IF,
                           "get_display_status", &ret, DBUS_TYPE_INVALID);
  if (rv == OSSO_OK) {
      if (strcmp(ret.value.s, "on") == 0)
          result = true;

      osso_rpc_free_val(&ret);
  }
  return result;
}

static void OssoRequestAccelerometer(osso_context_t *ctx, bool aEnabled)
{
  osso_return_t rv;
  osso_rpc_t ret;

  rv = osso_rpc_run_system(ctx, 
                           MCE_SERVICE,
                           MCE_REQUEST_PATH, MCE_REQUEST_IF,
                           aEnabled ? "req_accelerometer_enable" : "req_accelerometer_disable",
                           aEnabled ? &ret : NULL,
                           DBUS_TYPE_INVALID);

  // Orientation might changed while the accelerometer was off, so let's update
  // the window's orientation
  if (rv == OSSO_OK && aEnabled) {    
      OssoSetWindowOrientation(strcmp(ret.value.s, "portrait") == 0);
      osso_rpc_free_val(&ret);
  }
}

static void OssoDisplayCallback(osso_display_state_t state, gpointer data)
{
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (!os)
      return;

  osso_context_t* context = (osso_context_t*) data;

  if (state == OSSO_DISPLAY_ON) {
      os->NotifyObservers(nullptr, "system-display-on", nullptr);
      OssoRequestAccelerometer(context, true);
  } else {
      os->NotifyObservers(nullptr, "system-display-dimmed-or-off", nullptr);
      OssoRequestAccelerometer(context, false);
  }
}

static void OssoHardwareCallback(osso_hw_state_t *state, gpointer data)
{
  NS_ASSERTION(state, "osso_hw_state_t must not be null.");
  NS_ASSERTION(data, "data must not be null.");

  osso_hw_state_t* ourState = (osso_hw_state_t*) data;

  if (state->shutdown_ind) {
    nsCOMPtr<nsIAppStartup> appService = do_GetService("@mozilla.org/toolkit/app-startup;1");
    if (appService)
      appService->Quit(nsIAppStartup::eForceQuit);
    return;
  }

  if (state->memory_low_ind && !ourState->memory_low_ind) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os)
      os->NotifyObservers(nullptr, "memory-pressure", NS_LITERAL_STRING("low-memory").get());
  }
  
  if (state->system_inactivity_ind != ourState->system_inactivity_ind) {
      nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
      if (!os)
        return;
 
      if (state->system_inactivity_ind)
          os->NotifyObservers(nullptr, "system-idle", nullptr);
      else
          os->NotifyObservers(nullptr, "system-active", nullptr);
  }

  memcpy(ourState, state, sizeof(osso_hw_state_t));
}

static gint
OssoDbusCallback(const gchar *interface, const gchar *method,
                 GArray *arguments, gpointer data, osso_rpc_t *retval)
{
  retval->type = DBUS_TYPE_INVALID;

  // The "top_application" method just wants us to focus the top-most window.
  if (!strcmp("top_application", method)) {
    nsCOMPtr<nsIDOMWindow> window;
    GetMostRecentWindow(NS_LITERAL_STRING("").get(), getter_AddRefs(window));
    if (window)
      window->Focus();

    return OSSO_OK;
  }

  if (!strcmp("quit", method)) {
    nsCOMPtr<nsIAppStartup> appService = do_GetService("@mozilla.org/toolkit/app-startup;1");
    if (appService)
      appService->Quit(nsIAppStartup::eForceQuit);

    return OSSO_OK;
  }

  // Other methods can have arguments, which we convert and send to commandline
  // handlers.
  nsCOMPtr<nsICommandLineRunner> cmdLine
    (do_CreateInstance("@mozilla.org/toolkit/command-line;1"));

  nsCOMPtr<nsIFile> workingDir;
  NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR,
                         getter_AddRefs(workingDir));

  char** argv = 0;
  int argc = 0;

  // Not all DBus methods pass arguments
  if (arguments && arguments->len > 0) {
    // Create argument list with a dummy argv[0]
    argc = arguments->len + 1;
    argv = (char**)calloc(1, argc * sizeof(*argv));

    // Start at 1 to skip the dummy argv[0]
    for (int i = 1; i < argc; i++) {
      osso_rpc_t* entry = (osso_rpc_t*)&g_array_index(arguments, osso_rpc_t, i - 1);
      if (entry->type != DBUS_TYPE_STRING)
        continue;

      argv[i] = strdup(entry->value.s);
    }
  }

  cmdLine->Init(argc, argv, workingDir, nsICommandLine::STATE_REMOTE_AUTO);

  // Cleanup argument list
  while (argc) {
    free(argv[--argc]);
  }
  free(argv);

  cmdLine->Run();

  return OSSO_OK;
}

static DBusHandlerResult
OssoModeControlCallback(DBusConnection *con, DBusMessage *msg, gpointer data)
{
  if (dbus_message_is_signal(msg, MCE_SIGNAL_IF, MCE_DEVICE_ORIENTATION_SIG)) {
    DBusMessageIter iter;
    if (dbus_message_iter_init(msg, &iter)) {
      const gchar *mode = NULL;
      dbus_message_iter_get_basic(&iter, &mode);

      OssoSetWindowOrientation(strcmp(mode, "portrait") == 0);
    }
  }
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

#endif

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
#endif

#if (MOZ_PLATFORM_MAEMO == 5)
  /* zero state out. */
  memset(&m_hw_state, 0, sizeof(osso_hw_state_t));

  /* Initialize maemo application
     
     The initalization name will be of the form "Vendor.Name".
     If a Vendor isn't given, then we will just use "Name".
     
     Note that this value must match your X-Osso-Service name
     defined in your desktop file.  If it doesn't, the OSSO
     system will happily kill your process.
  */
  nsAutoCString applicationName;
  if (gAppData->vendor) {
      applicationName.Append(gAppData->vendor);
      applicationName.Append(".");
  }
  applicationName.Append(gAppData->name);
  ToLowerCase(applicationName);

  m_osso_context = osso_initialize(applicationName.get(), 
                                   gAppData->version ? gAppData->version : "1.0",
                                   true,
                                   nullptr);

  /* Check that initilialization was ok */
  if (m_osso_context == nullptr) {
      return NS_ERROR_FAILURE;
  }

  osso_hw_set_event_cb(m_osso_context, nullptr, OssoHardwareCallback, &m_hw_state);
  osso_hw_set_display_event_cb(m_osso_context, OssoDisplayCallback, m_osso_context);
  osso_rpc_set_default_cb_f(m_osso_context, OssoDbusCallback, nullptr);

  // Setup an MCE callback to monitor orientation
  DBusConnection *connnection = (DBusConnection*)osso_get_sys_dbus_connection(m_osso_context);
  dbus_bus_add_match(connnection, MCE_MATCH_RULE, nullptr);
  dbus_connection_add_filter(connnection, OssoModeControlCallback, nullptr, nullptr);
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
    gnome_program_init("Gecko", "1.0", libgnomeui_module_info_get(), gArgc, gArgv, NULL);
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
  g_signal_connect(client, "save-yourself", G_CALLBACK(save_yourself_cb), NULL);
  g_signal_connect(client, "die", G_CALLBACK(die_cb), NULL);

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

#if (MOZ_PLATFORM_MAEMO == 5)
  if (m_osso_context) {
    // Disable the accelerometer when closing
    OssoRequestAccelerometer(m_osso_context, false);

    // Remove the MCE callback filter
    DBusConnection *connnection = (DBusConnection*)osso_get_sys_dbus_connection(m_osso_context);
    dbus_connection_remove_filter(connnection, OssoModeControlCallback, nullptr);

    osso_hw_unset_event_cb(m_osso_context, nullptr);
    osso_rpc_unset_default_cb_f(m_osso_context, OssoDbusCallback, nullptr);
    osso_deinitialize(m_osso_context);
    m_osso_context = nullptr;
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportUnix::Enable()
{
#if (MOZ_PLATFORM_MAEMO == 5)
  // Enable the accelerometer for orientation support
  if (OssoIsScreenOn(m_osso_context))
      OssoRequestAccelerometer(m_osso_context, true);
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
