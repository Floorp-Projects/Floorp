/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "crashreporter.h"

#include <unistd.h>
#include <dlfcn.h>
#include <errno.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <gdk/gdkkeysyms.h>

#include <algorithm>
#include <string>
#include <vector>

#include "common/linux/http_upload.h"
#include "crashreporter.h"
#include "crashreporter_gtk_common.h"

#ifndef GDK_KEY_Escape
#  define GDK_KEY_Escape GDK_Escape
#endif

using std::string;
using std::vector;

using namespace CrashReporter;

GtkWidget* gWindow = 0;
GtkWidget* gSubmitReportCheck = 0;
GtkWidget* gIncludeURLCheck = 0;
GtkWidget* gThrobber = 0;
GtkWidget* gProgressLabel = 0;
GtkWidget* gCloseButton = 0;
GtkWidget* gRestartButton = 0;

bool gInitialized = false;
bool gDidTrySend = false;
StringTable gFiles;
StringTable gQueryParameters;
string gHttpProxy;
string gAuth;
string gCACertificateFile;
string gSendURL;
string gURLParameter;
vector<string> gRestartArgs;
GThread* gSendThreadID;

// From crashreporter_linux.cpp
void SaveSettings();
void SendReport();
void DisableGUIAndSendReport();
void TryInitGnome();
void UpdateSubmit();

static bool RestartApplication() {
  char** argv = reinterpret_cast<char**>(
      malloc(sizeof(char*) * (gRestartArgs.size() + 1)));

  if (!argv) return false;

  unsigned int i;
  for (i = 0; i < gRestartArgs.size(); i++) {
    argv[i] = (char*)gRestartArgs[i].c_str();
  }
  argv[i] = 0;

  pid_t pid = fork();
  if (pid == -1) {
    free(argv);
    return false;
  } else if (pid == 0) {
    (void)execv(argv[0], argv);
    _exit(1);
  }

  free(argv);

  return true;
}

// Quit the app, used as a timeout callback
static gboolean CloseApp(gpointer data) {
  if (!gAutoSubmit) {
    gtk_main_quit();
  }
  g_thread_join(gSendThreadID);
  return FALSE;
}

static gboolean ReportCompleted(gpointer success) {
  gtk_widget_hide(gThrobber);
  string str =
      success ? gStrings[ST_REPORTSUBMITSUCCESS] : gStrings[ST_SUBMITFAILED];
  gtk_label_set_text(GTK_LABEL(gProgressLabel), str.c_str());
  g_timeout_add(5000, CloseApp, 0);
  return FALSE;
}

#define HTTP_PROXY_DIR "/system/http_proxy"

void LoadProxyinfo() {
  class GConfClient;
  typedef GConfClient* (*_gconf_default_fn)();
  typedef gboolean (*_gconf_bool_fn)(GConfClient*, const gchar*, GError**);
  typedef gint (*_gconf_int_fn)(GConfClient*, const gchar*, GError**);
  typedef gchar* (*_gconf_string_fn)(GConfClient*, const gchar*, GError**);

  if (getenv("http_proxy"))
    return;  // libcurl can use the value from the environment

  static void* gconfLib = dlopen("libgconf-2.so.4", RTLD_LAZY);
  if (!gconfLib) return;

  _gconf_default_fn gconf_client_get_default =
      (_gconf_default_fn)dlsym(gconfLib, "gconf_client_get_default");
  _gconf_bool_fn gconf_client_get_bool =
      (_gconf_bool_fn)dlsym(gconfLib, "gconf_client_get_bool");
  _gconf_int_fn gconf_client_get_int =
      (_gconf_int_fn)dlsym(gconfLib, "gconf_client_get_int");
  _gconf_string_fn gconf_client_get_string =
      (_gconf_string_fn)dlsym(gconfLib, "gconf_client_get_string");

  if (!(gconf_client_get_default && gconf_client_get_bool &&
        gconf_client_get_int && gconf_client_get_string)) {
    dlclose(gconfLib);
    return;
  }

  GConfClient* conf = gconf_client_get_default();

  if (gconf_client_get_bool(conf, HTTP_PROXY_DIR "/use_http_proxy", nullptr)) {
    gint port;
    gchar *host = nullptr, *httpproxy = nullptr;

    host = gconf_client_get_string(conf, HTTP_PROXY_DIR "/host", nullptr);
    port = gconf_client_get_int(conf, HTTP_PROXY_DIR "/port", nullptr);

    if (port && host && *host != '\0') {
      httpproxy = g_strdup_printf("http://%s:%d/", host, port);
      gHttpProxy = httpproxy;
    }

    g_free(host);
    g_free(httpproxy);

    if (gconf_client_get_bool(conf, HTTP_PROXY_DIR "/use_authentication",
                              nullptr)) {
      gchar *user, *password, *auth = nullptr;

      user = gconf_client_get_string(
          conf, HTTP_PROXY_DIR "/authentication_user", nullptr);
      password = gconf_client_get_string(
          conf, HTTP_PROXY_DIR "/authentication_password", nullptr);

      if (user && password) {
        auth = g_strdup_printf("%s:%s", user, password);
        gAuth = auth;
      }

      g_free(user);
      g_free(password);
      g_free(auth);
    }
  }

  g_object_unref(conf);

  // Don't dlclose gconfLib as libORBit-2 uses atexit().
}

gpointer SendThread(gpointer args) {
  string response, error;
  long response_code;

  bool success = google_breakpad::HTTPUpload::SendRequest(
      gSendURL, gQueryParameters, gFiles, gHttpProxy, gAuth, gCACertificateFile,
      &response, &response_code, &error);
  if (success) {
    LogMessage("Crash report submitted successfully");
  } else {
    LogMessage("Crash report submission failed: " + error);
  }

  SendCompleted(success, response);

  if (!gAutoSubmit) {
    // Apparently glib is threadsafe, and will schedule this
    // on the main thread, see:
    // http://library.gnome.org/devel/gtk-faq/stable/x499.html
    g_idle_add(ReportCompleted, (gpointer)success);
  }

  return nullptr;
}

gboolean WindowDeleted(GtkWidget* window, GdkEvent* event, gpointer userData) {
  SaveSettings();
  gtk_main_quit();
  return TRUE;
}

gboolean check_escape(GtkWidget* window, GdkEventKey* event,
                      gpointer userData) {
  if (event->keyval == GDK_KEY_Escape) {
    gtk_main_quit();
    return TRUE;
  }
  return FALSE;
}

static void MaybeSubmitReport() {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gSubmitReportCheck))) {
    gDidTrySend = true;
    DisableGUIAndSendReport();
  } else {
    gtk_main_quit();
  }
}

void CloseClicked(GtkButton* button, gpointer userData) {
  SaveSettings();
  MaybeSubmitReport();
}

void RestartClicked(GtkButton* button, gpointer userData) {
  SaveSettings();
  RestartApplication();
  MaybeSubmitReport();
}

static void UpdateURL() {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gIncludeURLCheck))) {
    gQueryParameters["URL"] = gURLParameter;
  } else {
    gQueryParameters.erase("URL");
  }
}

void SubmitReportChecked(GtkButton* sender, gpointer userData) {
  UpdateSubmit();
}

void IncludeURLClicked(GtkButton* sender, gpointer userData) { UpdateURL(); }

/* === Crashreporter UI Functions === */

bool UIInit() {
  // breakpad probably left us with blocked signals, unblock them here
  sigset_t signals, old;
  sigfillset(&signals);
  sigprocmask(SIG_UNBLOCK, &signals, &old);

  // tell glib we're going to use threads
  g_thread_init(nullptr);

  if (gtk_init_check(&gArgc, &gArgv)) {
    gInitialized = true;

    if (gStrings.find("isRTL") != gStrings.end() && gStrings["isRTL"] == "yes")
      gtk_widget_set_default_direction(GTK_TEXT_DIR_RTL);

    return true;
  }

  return false;
}

void UIShowDefaultUI() {
  GtkWidget* errorDialog = gtk_message_dialog_new(
      nullptr, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s",
      gStrings[ST_CRASHREPORTERDEFAULT].c_str());

  gtk_window_set_title(GTK_WINDOW(errorDialog),
                       gStrings[ST_CRASHREPORTERTITLE].c_str());
  gtk_dialog_run(GTK_DIALOG(errorDialog));
}

void UIError_impl(const string& message) {
  if (!gInitialized) {
    // Didn't initialize, this is the best we can do
    printf("Error: %s\n", message.c_str());
    return;
  }

  GtkWidget* errorDialog =
      gtk_message_dialog_new(nullptr, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                             GTK_BUTTONS_CLOSE, "%s", message.c_str());

  gtk_window_set_title(GTK_WINDOW(errorDialog),
                       gStrings[ST_CRASHREPORTERTITLE].c_str());
  gtk_dialog_run(GTK_DIALOG(errorDialog));
}

bool UIGetIniPath(string& path) {
  path = gArgv[0];
  path.append(".ini");

  return true;
}

/*
 * Settings are stored in ~/.vendor/product, or
 * ~/.product if vendor is empty.
 */
bool UIGetSettingsPath(const string& vendor, const string& product,
                       string& settingsPath) {
  char* home = getenv("HOME");

  if (!home) return false;

  settingsPath = home;
  settingsPath += "/.";
  if (!vendor.empty()) {
    string lc_vendor;
    std::transform(vendor.begin(), vendor.end(), back_inserter(lc_vendor),
                   (int (*)(int))std::tolower);
    settingsPath += lc_vendor + "/";
  }
  string lc_product;
  std::transform(product.begin(), product.end(), back_inserter(lc_product),
                 (int (*)(int))std::tolower);
  settingsPath += lc_product + "/Crash Reports";
  return true;
}

bool UIMoveFile(const string& file, const string& newfile) {
  if (!rename(file.c_str(), newfile.c_str())) return true;
  if (errno != EXDEV) return false;

  // use system /bin/mv instead, time to fork
  pid_t pID = vfork();
  if (pID < 0) {
    // Failed to fork
    return false;
  }
  if (pID == 0) {
    char* const args[4] = {const_cast<char*>("mv"), strdup(file.c_str()),
                           strdup(newfile.c_str()), 0};
    if (args[1] && args[2]) execve("/bin/mv", args, 0);
    free(args[1]);
    free(args[2]);
    exit(-1);
  }
  int status;
  waitpid(pID, &status, 0);
  return UIFileExists(newfile);
}
