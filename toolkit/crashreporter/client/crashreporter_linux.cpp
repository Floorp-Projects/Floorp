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
 * The Original Code is Mozilla Toolkit Crash Reporter
 *
 * The Initial Developer of the Original Code is
 * Ted Mielczarek <ted.mielczarek@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "crashreporter.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <dlfcn.h>

#include <algorithm>
#include <cctype>

#include <signal.h>

#include <gtk/gtk.h>

#include "common/linux/http_upload.h"

using std::string;
using std::vector;

using namespace CrashReporter;

static GtkWidget* gWindow = 0;
static GtkWidget* gViewReportTextView = 0;
static GtkWidget* gSubmitReportCheck = 0;
static GtkWidget* gEmailMeCheck = 0;
static GtkWidget* gEmailEntry = 0;

static bool gInitialized = false;
static string gDumpFile;
static StringTable gQueryParameters;
static string gSendURL;
static vector<string> gRestartArgs;

// handle from dlopen'ing libgnome
static void* gnomeLib = NULL;
// handle from dlopen'ing libgnomeui
static void* gnomeuiLib = NULL;

static const char kIniFile[] = "crashreporter.ini";

static void LoadSettings()
{
  StringTable settings;
  if (ReadStringsFromFile(gSettingsPath + "/" + kIniFile, settings, true)) {
    if (settings.find("Email") != settings.end()) {
      gtk_entry_set_text(GTK_ENTRY(gEmailEntry), settings["Email"].c_str());
    }
    if (settings.find("EmailMe") != settings.end()) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gEmailMeCheck),
                                   settings["EmailMe"][0] != '0');
    }
    if (settings.find("SubmitReport") != settings.end()) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gSubmitReportCheck),
                                   settings["SubmitReport"][0] != '0');
    }
  }
}

static void SaveSettings()
{
  StringTable settings;

  ReadStringsFromFile(gSettingsPath + "/" + kIniFile, settings, true);
  settings["Email"] = gtk_entry_get_text(GTK_ENTRY(gEmailEntry));
  settings["EmailMe"] =
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gEmailMeCheck)) ? "1" : "0";

  settings["SubmitReport"] =
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gSubmitReportCheck))
    ? "1" : "0";

  WriteStringsToFile(gSettingsPath + "/" + kIniFile,
                     "Crash Reporter", settings, true);
}

static bool RestartApplication()
{
  char** argv = reinterpret_cast<char**>(
    malloc(sizeof(char*) * (gRestartArgs.size() + 1)));

  if (!argv) return false;

  unsigned int i;
  for (i = 0; i < gRestartArgs.size(); i++) {
    argv[i] = (char*)gRestartArgs[i].c_str();
  }
  argv[i] = 0;

  pid_t pid = fork();
  if (pid == -1)
    return false;
  else if (pid == 0) {
    (void)execv(argv[0], argv);
    _exit(1);
  }

  free(argv);

  return true;
}

static gboolean SendReportIdle(gpointer userData)
{
  string response;
  bool success = google_breakpad::HTTPUpload::SendRequest
    (gSendURL,
     gQueryParameters,
     gDumpFile,
     "upload_file_minidump",
     "", "",
     &response);
  if (success) {
    LogMessage("Crash report submitted successfully");
  }
  else {
    //XXX: HTTPUpload::SendRequest eats the curl error code, filed
    // http://code.google.com/p/google-breakpad/issues/detail?id=221
    LogMessage("Crash report submission failed");
  }
  SendCompleted(success, response);

  if (!success) {
    GtkWidget* errorDialog =
      gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
                             GTK_MESSAGE_ERROR,
                             GTK_BUTTONS_CLOSE,
                             "%s", gStrings[ST_SUBMITFAILED].c_str());

    gtk_window_set_title(GTK_WINDOW(errorDialog),
                         gStrings[ST_CRASHREPORTERTITLE].c_str());
    gtk_dialog_run(GTK_DIALOG(errorDialog));
  }
  gtk_main_quit();

  return FALSE;
}

static void SendReport()
{
  // On the other platforms we spawn off a thread here.  Because the window
  // should be hidden before the report is sent, don't bother to spawn
  // a thread here.  Just kick it out to an idle handler (to give
  // gtk_widget_hide() a chance to take)
  g_idle_add(SendReportIdle, 0);
}

static void ShowReportInfo()
{
  GtkTextBuffer* buffer =
    gtk_text_view_get_buffer(GTK_TEXT_VIEW(gViewReportTextView));

  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer, &start);
  gtk_text_buffer_get_end_iter(buffer, &end);

  gtk_text_buffer_delete(buffer, &start, &end);

  for (StringTable::iterator iter = gQueryParameters.begin();
       iter != gQueryParameters.end();
       iter++) {
    gtk_text_buffer_insert(buffer, &end, iter->first.c_str(), -1);
    gtk_text_buffer_insert(buffer, &end, ": ", -1);
    gtk_text_buffer_insert(buffer, &end, iter->second.c_str(), -1);
    gtk_text_buffer_insert(buffer, &end, "\n", -1);
  }

  gtk_text_buffer_insert(buffer, &end, "\n", -1);
  gtk_text_buffer_insert(buffer, &end,
                         gStrings[ST_EXTRAREPORTINFO].c_str(), -1);
}

static gboolean WindowDeleted(GtkWidget* window,
                              GdkEvent* event,
                              gpointer userData)
{
  SaveSettings();
  gtk_main_quit();
  return TRUE;
}

static void CloseClicked(GtkButton* button,
                         gpointer userData)
{
  SaveSettings();
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gSubmitReportCheck))) {
    gtk_widget_hide(gWindow);
    SendReport();
  } else {
    gtk_main_quit();
  }
}

static void RestartClicked(GtkButton* button,
                           gpointer userData)
{
  SaveSettings();
  RestartApplication();

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gSubmitReportCheck))) {
    gtk_widget_hide(gWindow);
    SendReport();
  } else {
    gtk_main_quit();
  }
}

static void UpdateEmail()
{
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gEmailMeCheck))) {
    gQueryParameters["Email"] = gtk_entry_get_text(GTK_ENTRY(gEmailEntry));
  } else {
    gQueryParameters.erase("Email");
  }
}

static void EmailMeClicked(GtkButton* sender, gpointer userData)
{
  UpdateEmail();
  ShowReportInfo();
}

static void EmailChanged(GtkEditable* editable, gpointer userData)
{
  // Email text changed, assume they want the "Email me" checkbox
  // updated appropriately
  const char* email = gtk_entry_get_text(GTK_ENTRY(editable));
  if (email[0] == '\0')
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gEmailMeCheck), FALSE);
  else
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gEmailMeCheck), TRUE);

  UpdateEmail();
  ShowReportInfo();
}

typedef struct _GnomeProgram GnomeProgram;
typedef struct _GnomeModuleInfo GnomeModuleInfo;
typedef GnomeProgram * (*_gnome_program_init_fn)(const char *, const char *,
                                                 const GnomeModuleInfo *, int,
                                                 char **, const char *, ...);
typedef const GnomeModuleInfo * (*_libgnomeui_module_info_get_fn)();

static void TryInitGnome()
{
  gnomeLib = dlopen("libgnome-2.so.0", RTLD_LAZY);
  if (!gnomeLib)
    return;

  gnomeuiLib = dlopen("libgnomeui-2.so.0", RTLD_LAZY);
  if (!gnomeuiLib)
    return;

  _gnome_program_init_fn gnome_program_init =
    (_gnome_program_init_fn)(dlsym(gnomeLib, "gnome_program_init"));
  _libgnomeui_module_info_get_fn libgnomeui_module_info_get =
    (_libgnomeui_module_info_get_fn)(dlsym(gnomeuiLib, "libgnomeui_module_info_get"));

  if (gnome_program_init && libgnomeui_module_info_get) {
    gnome_program_init("crashreporter", "1.0", libgnomeui_module_info_get(),
                       gArgc, gArgv, NULL);
  }

}

/* === Crashreporter UI Functions === */

bool UIInit()
{
  // breakpad probably left us with blocked signals, unblock them here
  sigset_t signals, old;
  sigfillset(&signals);
  sigprocmask(SIG_UNBLOCK, &signals, &old);

  if (gtk_init_check(&gArgc, &gArgv)) {
    gInitialized = true;
    TryInitGnome();
    return true;
  }

  return false;
}

void UIShutdown()
{
  if (gnomeLib)
    dlclose(gnomeLib);
  if (gnomeuiLib)
    dlclose(gnomeuiLib);
}

void UIShowDefaultUI()
{
  GtkWidget* errorDialog =
    gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
                           GTK_MESSAGE_ERROR,
                           GTK_BUTTONS_CLOSE,
                           "%s", gStrings[ST_CRASHREPORTERDEFAULT].c_str());

  gtk_window_set_title(GTK_WINDOW(errorDialog),
                       gStrings[ST_CRASHREPORTERTITLE].c_str());
  gtk_dialog_run(GTK_DIALOG(errorDialog));
}

void UIShowCrashUI(const string& dumpfile,
                   const StringTable& queryParameters,
                   const string& sendURL,
                   const vector<string>& restartArgs)
{
  gDumpFile = dumpfile;
  gQueryParameters = queryParameters;
  gSendURL = sendURL;
  gRestartArgs = restartArgs;

  gWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(gWindow),
                       gStrings[ST_CRASHREPORTERTITLE].c_str());
  gtk_window_set_resizable(GTK_WINDOW(gWindow), FALSE);
  gtk_container_set_border_width(GTK_CONTAINER(gWindow), 12);
  g_signal_connect(gWindow, "delete-event", G_CALLBACK(WindowDeleted), 0);

  GtkWidget* vbox = gtk_vbox_new(FALSE, 6);
  gtk_container_add(GTK_CONTAINER(gWindow), vbox);

  GtkWidget* titleLabel = gtk_label_new("");
  gtk_box_pack_start(GTK_BOX(vbox), titleLabel, FALSE, FALSE, 0);
  gtk_misc_set_alignment(GTK_MISC(titleLabel), 0, 0.5);
  char* markup = g_strdup_printf("<b>%s</b>",
                                 gStrings[ST_CRASHREPORTERHEADER].c_str());
  gtk_label_set_markup(GTK_LABEL(titleLabel), markup);
  g_free(markup);

  GtkWidget* descriptionLabel =
    gtk_label_new(gStrings[ST_CRASHREPORTERDESCRIPTION].c_str());
  gtk_box_pack_start(GTK_BOX(vbox), descriptionLabel, TRUE, TRUE, 0);
  // force the label to line wrap
  gtk_widget_set_size_request(descriptionLabel, 400, -1);
  gtk_label_set_line_wrap(GTK_LABEL(descriptionLabel), TRUE);
  gtk_label_set_selectable(GTK_LABEL(descriptionLabel), TRUE);
  gtk_misc_set_alignment(GTK_MISC(descriptionLabel), 0, 0.5);

  // this is honestly how they suggest you indent a section
  GtkWidget* indentBox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), indentBox, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(indentBox), gtk_label_new(""), FALSE, FALSE, 6);

  GtkWidget* innerVBox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(indentBox), innerVBox, TRUE, TRUE, 0);

  GtkWidget* expander = gtk_expander_new(gStrings[ST_VIEWREPORT].c_str());
  gtk_box_pack_start(GTK_BOX(innerVBox), expander, FALSE, FALSE, 6);

  GtkWidget* scrolled = gtk_scrolled_window_new(0, 0);
  gtk_container_add(GTK_CONTAINER(expander), scrolled);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled),
                                      GTK_SHADOW_IN);

  gViewReportTextView = gtk_text_view_new();
  gtk_container_add(GTK_CONTAINER(scrolled), gViewReportTextView);
  gtk_text_view_set_editable(GTK_TEXT_VIEW(gViewReportTextView), FALSE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(gViewReportTextView),
                              GTK_WRAP_WORD);
  gtk_widget_set_size_request(GTK_WIDGET(gViewReportTextView), -1, 100);

  gSubmitReportCheck =
    gtk_check_button_new_with_label(gStrings[ST_CHECKSUBMIT].c_str());
  gtk_box_pack_start(GTK_BOX(innerVBox), gSubmitReportCheck, FALSE, FALSE, 0);

  gEmailMeCheck =
    gtk_check_button_new_with_label(gStrings[ST_CHECKEMAIL].c_str());
  gtk_box_pack_start(GTK_BOX(innerVBox), gEmailMeCheck, FALSE, FALSE, 0);
  g_signal_connect(gEmailMeCheck, "clicked", G_CALLBACK(EmailMeClicked), 0);

  GtkWidget* emailIndentBox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(innerVBox), emailIndentBox, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(emailIndentBox), gtk_label_new(""),
                     FALSE, FALSE, 9);

  gEmailEntry = gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(emailIndentBox), gEmailEntry, TRUE, TRUE, 0);
  g_signal_connect(gEmailEntry, "changed", G_CALLBACK(EmailChanged), 0);

  GtkWidget* buttonBox = gtk_hbutton_box_new();
  gtk_box_pack_end(GTK_BOX(vbox), buttonBox, FALSE, FALSE, 0);
  gtk_box_set_spacing(GTK_BOX(buttonBox), 6);
  gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonBox), GTK_BUTTONBOX_END);

  GtkWidget* closeButton =
    gtk_button_new_with_label(gStrings[ST_CLOSE].c_str());
  gtk_box_pack_start(GTK_BOX(buttonBox), closeButton, FALSE, FALSE, 0);
  GTK_WIDGET_SET_FLAGS(closeButton, GTK_CAN_DEFAULT);
  g_signal_connect(closeButton, "clicked", G_CALLBACK(CloseClicked), 0);

  GtkWidget* restartButton = 0;
  if (restartArgs.size() > 0) {
    restartButton = gtk_button_new_with_label(gStrings[ST_RESTART].c_str());
    gtk_box_pack_start(GTK_BOX(buttonBox), restartButton, FALSE, FALSE, 0);
    GTK_WIDGET_SET_FLAGS(restartButton, GTK_CAN_DEFAULT);
    g_signal_connect(restartButton, "clicked", G_CALLBACK(RestartClicked), 0);
  }

  gtk_widget_grab_focus(gEmailEntry);

  gtk_widget_grab_default(restartButton ? restartButton : closeButton);

  LoadSettings();
  ShowReportInfo();

  gtk_widget_show_all(gWindow);

  gtk_main();
}

void UIError_impl(const string& message)
{
  if (!gInitialized) {
    // Didn't initialize, this is the best we can do
    printf("Error: %s\n", message.c_str());
    return;
  }

  GtkWidget* errorDialog =
    gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
                           GTK_MESSAGE_ERROR,
                           GTK_BUTTONS_CLOSE,
                           "%s", message.c_str());

  gtk_window_set_title(GTK_WINDOW(errorDialog),
                       gStrings[ST_CRASHREPORTERTITLE].c_str());
  gtk_dialog_run(GTK_DIALOG(errorDialog));
}

bool UIGetIniPath(string& path)
{
  path = gArgv[0];
  path.append(".ini");

  return true;
}

/*
 * Settings are stored in ~/.vendor/product, or
 * ~/.product if vendor is empty.
 */
bool UIGetSettingsPath(const string& vendor,
                       const string& product,
                       string& settingsPath)
{
  char* home = getenv("HOME");

  if (!home)
    return false;

  settingsPath = home;
  settingsPath += "/.";
  if (!vendor.empty()) {
    string lc_vendor;
    std::transform(vendor.begin(), vendor.end(), back_inserter(lc_vendor),
                   (int(*)(int)) std::tolower);
    settingsPath += lc_vendor + "/";
  }
  string lc_product;
  std::transform(product.begin(), product.end(), back_inserter(lc_product),
                 (int(*)(int)) std::tolower);
  settingsPath += lc_product + "/Crash Reports";
  return true;
}

bool UIEnsurePathExists(const string& path)
{
  int ret = mkdir(path.c_str(), S_IRWXU);
  int e = errno;
  if (ret == -1 && e != EEXIST)
    return false;

  return true;
}

bool UIFileExists(const string& path)
{
  struct stat sb;
  int ret = stat(path.c_str(), &sb);
  if (ret == -1 || !(sb.st_mode & S_IFREG))
    return false;

  return true;
}

bool UIMoveFile(const string& file, const string& newfile)
{
  return (rename(file.c_str(), newfile.c_str()) != -1);
}

bool UIDeleteFile(const string& file)
{
  return (unlink(file.c_str()) != -1);
}

std::ifstream* UIOpenRead(const string& filename)
{
  return new std::ifstream(filename.c_str(), std::ios::in);
}

std::ofstream* UIOpenWrite(const string& filename, bool append) // append=false
{
  return new std::ofstream(filename.c_str(),
                           append ? std::ios::out | std::ios::app
                                  : std::ios::out);
}
