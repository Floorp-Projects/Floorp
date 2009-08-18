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
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <dlfcn.h>

#include <algorithm>
#include <cctype>

#include <signal.h>

#ifdef MOZ_ENABLE_GCONF
#include <gconf/gconf-client.h>
#endif

#include <gtk/gtk.h>
#include <glib.h>
#include <string.h>

#include "common/linux/http_upload.h"

using std::string;
using std::vector;

using namespace CrashReporter;

static GtkWidget* gWindow = 0;
static GtkWidget* gSubmitReportCheck = 0;
static GtkWidget* gViewReportButton = 0;
static GtkWidget* gCommentText = 0;
static GtkWidget* gIncludeURLCheck = 0;
static GtkWidget* gEmailMeCheck = 0;
static GtkWidget* gEmailEntry = 0;
static GtkWidget* gThrobber = 0;
static GtkWidget* gProgressLabel = 0;
static GtkWidget* gCloseButton = 0;
static GtkWidget* gRestartButton = 0;

static bool gInitialized = false;
static bool gDidTrySend = false;
static string gDumpFile;
static StringTable gQueryParameters;
static string gSendURL;
static string gHttpProxy;
static string gAuth;
static vector<string> gRestartArgs;
static string gURLParameter;

static bool gEmailFieldHint = true;
static bool gCommentFieldHint = true;

static GThread* gSendThreadID;

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
      gEmailFieldHint = false;
    }
    if (settings.find("EmailMe") != settings.end()) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gEmailMeCheck),
                                   settings["EmailMe"][0] != '0');
    }
    if (settings.find("IncludeURL") != settings.end() &&
        gIncludeURLCheck != 0) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gIncludeURLCheck),
                                   settings["IncludeURL"][0] != '0');
    }
    bool enabled;
    if (settings.find("SubmitReport") != settings.end())
      enabled = settings["SubmitReport"][0] != '0';
    else
      enabled = ShouldEnableSending();
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gSubmitReportCheck),
                                 enabled);
  }
}

static void SaveSettings()
{
  StringTable settings;

  ReadStringsFromFile(gSettingsPath + "/" + kIniFile, settings, true);
  if (!gEmailFieldHint)
    settings["Email"] = gtk_entry_get_text(GTK_ENTRY(gEmailEntry));
  else
    settings.erase("Email");

  settings["EmailMe"] =
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gEmailMeCheck)) ? "1" : "0";
  if (gIncludeURLCheck != 0)
    settings["IncludeURL"] =
      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gIncludeURLCheck))
      ? "1" : "0";
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

// Quit the app, used as a timeout callback
static gboolean CloseApp(gpointer data)
{
  gtk_main_quit();
  g_thread_join(gSendThreadID);
  return FALSE;
}

static gboolean ReportCompleted(gpointer success)
{
  gtk_widget_hide_all(gThrobber);
  string str = success ? gStrings[ST_REPORTSUBMITSUCCESS]
                       : gStrings[ST_SUBMITFAILED];
  gtk_label_set_text(GTK_LABEL(gProgressLabel), str.c_str());
  g_timeout_add(5000, CloseApp, 0);
  return FALSE;
}

#ifdef MOZ_ENABLE_GCONF
#define HTTP_PROXY_DIR "/system/http_proxy"

static void LoadProxyinfo()
{
  GConfClient *conf = gconf_client_get_default();

  if (!getenv ("http_proxy") &&
      gconf_client_get_bool(conf, HTTP_PROXY_DIR "/use_http_proxy", NULL)) {
    gint port;
    gchar *host = NULL, *httpproxy = NULL;

    host = gconf_client_get_string(conf, HTTP_PROXY_DIR "/host", NULL);
    port = gconf_client_get_int(conf, HTTP_PROXY_DIR "/port", NULL);

    if (port && host && host != '\0') {
      httpproxy = g_strdup_printf("http://%s:%d/", host, port);
      gHttpProxy = httpproxy;
    }

    g_free(host);
    g_free(httpproxy);

    if(gconf_client_get_bool(conf, HTTP_PROXY_DIR "/use_authentication", NULL)) {
      gchar *user, *password, *auth = NULL;

      user = gconf_client_get_string(conf,
                                     HTTP_PROXY_DIR "/authentication_user",
                                     NULL);
      password = gconf_client_get_string(conf,
                                         HTTP_PROXY_DIR
                                         "/authentication_password",
                                         NULL);

      if (user != "\0") {
        auth = g_strdup_printf("%s:%s", user, password);
        gAuth = auth;
      }

      g_free(user);
      g_free(password);
      g_free(auth);
    }
  }

  g_object_unref(conf);
}
#endif

static gpointer SendThread(gpointer args)
{
  string response, error;

#ifdef MOZ_ENABLE_GCONF
  LoadProxyinfo();
#endif

  bool success = google_breakpad::HTTPUpload::SendRequest
    (gSendURL,
     gQueryParameters,
     gDumpFile,
     "upload_file_minidump",
     gHttpProxy, gAuth,
     &response,
     &error);
  if (success) {
    LogMessage("Crash report submitted successfully");
  }
  else {
    LogMessage("Crash report submission failed: " + error);
  }

  SendCompleted(success, response);
  // Apparently glib is threadsafe, and will schedule this
  // on the main thread, see:
  // http://library.gnome.org/devel/gtk-faq/stable/x500.html
  g_idle_add(ReportCompleted, (gpointer)success);

  return NULL;
}

static void SendReport()
{
  // disable all our gui controls, show the throbber + change the progress text
  gtk_widget_set_sensitive(gSubmitReportCheck, FALSE);
  gtk_widget_set_sensitive(gViewReportButton, FALSE);
  gtk_widget_set_sensitive(gCommentText, FALSE);
  gtk_widget_set_sensitive(gIncludeURLCheck, FALSE);
  gtk_widget_set_sensitive(gEmailMeCheck, FALSE);
  gtk_widget_set_sensitive(gEmailEntry, FALSE);
  gtk_widget_set_sensitive(gCloseButton, FALSE);
  gtk_widget_set_sensitive(gRestartButton, FALSE);
  gtk_widget_show_all(gThrobber);
  gtk_label_set_text(GTK_LABEL(gProgressLabel),
                     gStrings[ST_REPORTDURINGSUBMIT].c_str());

  // and spawn a thread to do the sending
  GError* err;
  gSendThreadID = g_thread_create(SendThread, NULL, TRUE, &err);
}

static void ShowReportInfo(GtkTextView* viewReportTextView)
{
  GtkTextBuffer* buffer =
    gtk_text_view_get_buffer(viewReportTextView);

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

static void MaybeSubmitReport()
{
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gSubmitReportCheck))) {
    gDidTrySend = true;
    SendReport();
  } else {
    gtk_main_quit();
  }
}

static void CloseClicked(GtkButton* button,
                         gpointer userData)
{
  SaveSettings();
  MaybeSubmitReport();
}

static void RestartClicked(GtkButton* button,
                           gpointer userData)
{
  SaveSettings();
  RestartApplication();
  MaybeSubmitReport();
}

static void UpdateSubmit()
{
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gSubmitReportCheck))) {
    gtk_widget_set_sensitive(gViewReportButton, TRUE);
    gtk_widget_set_sensitive(gCommentText, TRUE);
    gtk_widget_set_sensitive(gIncludeURLCheck, TRUE);
    gtk_widget_set_sensitive(gEmailMeCheck, TRUE);
    gtk_widget_set_sensitive(gEmailEntry,
                             gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gEmailMeCheck)));
    gtk_label_set_text(GTK_LABEL(gProgressLabel),
                       gStrings[ST_REPORTPRESUBMIT].c_str());
  } else {
    gtk_widget_set_sensitive(gViewReportButton, FALSE);
    gtk_widget_set_sensitive(gCommentText, FALSE);
    gtk_widget_set_sensitive(gIncludeURLCheck, FALSE);
    gtk_widget_set_sensitive(gEmailMeCheck, FALSE);
    gtk_widget_set_sensitive(gEmailEntry, FALSE);
    gtk_label_set_text(GTK_LABEL(gProgressLabel), "");
  }
}

static void SubmitReportChecked(GtkButton* sender, gpointer userData)
{
  UpdateSubmit();
}

static void ViewReportClicked(GtkButton* button,
                              gpointer userData)
{
  GtkDialog* dialog =
    GTK_DIALOG(gtk_dialog_new_with_buttons(gStrings[ST_VIEWREPORTTITLE].c_str(),
                                           GTK_WINDOW(gWindow),
                                           GTK_DIALOG_MODAL,
                                           GTK_STOCK_OK,
                                           GTK_RESPONSE_OK,
                                           NULL));

  GtkWidget* scrolled = gtk_scrolled_window_new(0, 0);
  gtk_container_add(GTK_CONTAINER(dialog->vbox), scrolled);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled),
                                      GTK_SHADOW_IN);

  GtkWidget* viewReportTextView = gtk_text_view_new();
  gtk_container_add(GTK_CONTAINER(scrolled), viewReportTextView);
  gtk_text_view_set_editable(GTK_TEXT_VIEW(viewReportTextView), FALSE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(viewReportTextView),
                              GTK_WRAP_WORD);
  gtk_widget_set_size_request(GTK_WIDGET(viewReportTextView), -1, 100);

  ShowReportInfo(GTK_TEXT_VIEW(viewReportTextView));

  gtk_dialog_set_default_response(dialog, GTK_RESPONSE_OK);
  gtk_widget_set_size_request(GTK_WIDGET(dialog), 400, 200);
  gtk_widget_show_all(GTK_WIDGET(dialog));
  gtk_dialog_run(dialog);
  gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void CommentChanged(GtkTextBuffer* buffer, gpointer userData)
{
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer, &start);
  gtk_text_buffer_get_end_iter(buffer, &end);
  const char* comment = gtk_text_buffer_get_text(buffer, &start, &end, TRUE);
  if (comment[0] == '\0' || gCommentFieldHint)
    gQueryParameters.erase("Comments");
  else
    gQueryParameters["Comments"] = comment;
}

static void CommentInsert(GtkTextBuffer* buffer,
                          GtkTextIter* location,
                          gchar* text,
                          gint len,
                          gpointer userData)
{
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer, &start);
  gtk_text_buffer_get_end_iter(buffer, &end);
  const char* comment = gtk_text_buffer_get_text(buffer, &start, &end, TRUE);

  // limit to 500 bytes in utf-8
  if (strlen(comment) + len > MAX_COMMENT_LENGTH) {
    g_signal_stop_emission_by_name(buffer, "insert-text");
  }
}

static void UpdateHintText(GtkWidget* widget, gboolean gainedFocus,
                           bool* hintShowing, const char* hintText)
{
  GtkTextBuffer* buffer = NULL;
  if (GTK_IS_TEXT_VIEW(widget))
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget));

  if (gainedFocus) {
    if (*hintShowing) {
      if (buffer == NULL) { // sort of cheating
        gtk_entry_set_text(GTK_ENTRY(widget), "");
      }
      else { // GtkTextView
        gtk_text_buffer_set_text(buffer, "", 0);
      }
      gtk_widget_modify_text(widget, GTK_STATE_NORMAL, NULL);
      *hintShowing = false;
    }
  }
  else {
    // lost focus
    const char* text = NULL;
    if (buffer == NULL) {
      text = gtk_entry_get_text(GTK_ENTRY(widget));
    }
    else {
      GtkTextIter start, end;
      gtk_text_buffer_get_start_iter(buffer, &start);
      gtk_text_buffer_get_end_iter(buffer, &end);
      text = gtk_text_buffer_get_text(buffer, &start, &end, TRUE);
    }

    if (text == NULL || text[0] == '\0') {
      *hintShowing = true;

      if (buffer == NULL) {
        gtk_entry_set_text(GTK_ENTRY(widget), hintText);
      }
      else {
        gtk_text_buffer_set_text(buffer, hintText, -1);
      }

      gtk_widget_modify_text(widget, GTK_STATE_NORMAL,
                              &gtk_widget_get_style(widget)->text[GTK_STATE_INSENSITIVE]);
    }
  }
}

static gboolean CommentFocusChange(GtkWidget* widget, GdkEventFocus* event,
                                   gpointer userData)
{
  UpdateHintText(widget, event->in, &gCommentFieldHint,
                 gStrings[ST_COMMENTGRAYTEXT].c_str());

  return FALSE;
}

static void UpdateURL()
{
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gIncludeURLCheck))) {
    gQueryParameters["URL"] = gURLParameter;
  } else {
    gQueryParameters.erase("URL");
  }
}

static void UpdateEmail()
{
  const char* email = gtk_entry_get_text(GTK_ENTRY(gEmailEntry));
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gEmailMeCheck))) {
    gtk_widget_set_sensitive(gEmailEntry, TRUE);
  } else {
    email = "";
    gtk_widget_set_sensitive(gEmailEntry, FALSE);
  }
  if (email[0] == '\0' || gEmailFieldHint)
    gQueryParameters.erase("Email");
  else
    gQueryParameters["Email"] = email;
}

static void IncludeURLClicked(GtkButton* sender, gpointer userData)
{
  UpdateURL();
}

static void EmailMeClicked(GtkButton* sender, gpointer userData)
{
  UpdateEmail();
}

static void EmailChanged(GtkEditable* editable, gpointer userData)
{
  UpdateEmail();
}

static gboolean EmailFocusChange(GtkWidget* widget, GdkEventFocus* event,
                         gpointer userData)
{
  UpdateHintText(widget, event->in, &gEmailFieldHint,
                 gStrings[ST_EMAILGRAYTEXT].c_str());

  return FALSE;
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

  // tell glib we're going to use threads
  g_thread_init(NULL);

  if (gtk_init_check(&gArgc, &gArgv)) {
    gInitialized = true;

    if (gStrings.find("isRTL") != gStrings.end() &&
        gStrings["isRTL"] == "yes")
      gtk_widget_set_default_direction(GTK_TEXT_DIR_RTL);

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

bool UIShowCrashUI(const string& dumpfile,
                   const StringTable& queryParameters,
                   const string& sendURL,
                   const vector<string>& restartArgs)
{
  gDumpFile = dumpfile;
  gQueryParameters = queryParameters;
  gSendURL = sendURL;
  gRestartArgs = restartArgs;
  if (gQueryParameters.find("URL") != gQueryParameters.end())
    gURLParameter = gQueryParameters["URL"];

  gWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(gWindow),
                       gStrings[ST_CRASHREPORTERTITLE].c_str());
  gtk_window_set_resizable(GTK_WINDOW(gWindow), FALSE);
  gtk_window_set_position(GTK_WINDOW(gWindow), GTK_WIN_POS_CENTER);
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

  GtkWidget* innerVBox1 = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(indentBox), innerVBox1, TRUE, TRUE, 0);

  gSubmitReportCheck =
    gtk_check_button_new_with_label(gStrings[ST_CHECKSUBMIT].c_str());
  gtk_box_pack_start(GTK_BOX(innerVBox1), gSubmitReportCheck, FALSE, FALSE, 0);
  g_signal_connect(gSubmitReportCheck, "clicked",
                   G_CALLBACK(SubmitReportChecked), 0);

  // indent again, below the "submit report" checkbox
  GtkWidget* indentBox2 = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(innerVBox1), indentBox2, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(indentBox2), gtk_label_new(""), FALSE, FALSE, 6);

  GtkWidget* innerVBox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(indentBox2), innerVBox, TRUE, TRUE, 0);
  gtk_box_set_spacing(GTK_BOX(innerVBox), 6);

  GtkWidget* viewReportButtonBox = gtk_hbutton_box_new();
  gtk_box_pack_start(GTK_BOX(innerVBox), viewReportButtonBox, FALSE, FALSE, 0);
  gtk_box_set_spacing(GTK_BOX(viewReportButtonBox), 6);
  gtk_button_box_set_layout(GTK_BUTTON_BOX(viewReportButtonBox), GTK_BUTTONBOX_START);

  gViewReportButton =
    gtk_button_new_with_label(gStrings[ST_VIEWREPORT].c_str());
  gtk_box_pack_start(GTK_BOX(viewReportButtonBox), gViewReportButton, FALSE, FALSE, 0);
  g_signal_connect(gViewReportButton, "clicked", G_CALLBACK(ViewReportClicked), 0);

  GtkWidget* scrolled = gtk_scrolled_window_new(0, 0);
  gtk_container_add(GTK_CONTAINER(innerVBox), scrolled);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled),
                                      GTK_SHADOW_IN);

  gCommentText = gtk_text_view_new();
  gtk_text_view_set_accepts_tab(GTK_TEXT_VIEW(gCommentText), FALSE);
  g_signal_connect(gCommentText, "focus-in-event", G_CALLBACK(CommentFocusChange), 0);
  g_signal_connect(gCommentText, "focus-out-event", G_CALLBACK(CommentFocusChange), 0);

  GtkTextBuffer* commentBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gCommentText));
  g_signal_connect(commentBuffer, "changed", G_CALLBACK(CommentChanged), 0);
  g_signal_connect(commentBuffer, "insert-text", G_CALLBACK(CommentInsert), 0);

  gtk_container_add(GTK_CONTAINER(scrolled), gCommentText);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(gCommentText),
                              GTK_WRAP_WORD);
  gtk_widget_set_size_request(GTK_WIDGET(gCommentText), -1, 100);

  if (gQueryParameters.find("URL") != gQueryParameters.end()) {
    gIncludeURLCheck =
      gtk_check_button_new_with_label(gStrings[ST_CHECKURL].c_str());
    gtk_box_pack_start(GTK_BOX(innerVBox), gIncludeURLCheck, FALSE, FALSE, 0);
    g_signal_connect(gIncludeURLCheck, "clicked", G_CALLBACK(IncludeURLClicked), 0);
    // on by default
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gIncludeURLCheck), TRUE);
  }

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
  g_signal_connect(gEmailEntry, "focus-in-event", G_CALLBACK(EmailFocusChange), 0);
  g_signal_connect(gEmailEntry, "focus-out-event", G_CALLBACK(EmailFocusChange), 0);

  GtkWidget* progressBox = gtk_hbox_new(FALSE, 6);
  gtk_box_pack_start(GTK_BOX(vbox), progressBox, TRUE, TRUE, 0);

  // Get the throbber image from alongside the executable
  char* dir = g_path_get_dirname(gArgv[0]);
  char* path = g_build_filename(dir, "Throbber-small.gif", NULL);
  g_free(dir);
  gThrobber = gtk_image_new_from_file(path);
  gtk_box_pack_start(GTK_BOX(progressBox), gThrobber, FALSE, FALSE, 0);

  gProgressLabel =
    gtk_label_new(gStrings[ST_REPORTPRESUBMIT].c_str());
  gtk_box_pack_start(GTK_BOX(progressBox), gProgressLabel, TRUE, TRUE, 0);
  // force the label to line wrap
  gtk_widget_set_size_request(gProgressLabel, 400, -1);
  gtk_label_set_line_wrap(GTK_LABEL(gProgressLabel), TRUE);

  GtkWidget* buttonBox = gtk_hbutton_box_new();
  gtk_box_pack_end(GTK_BOX(vbox), buttonBox, FALSE, FALSE, 0);
  gtk_box_set_spacing(GTK_BOX(buttonBox), 6);
  gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonBox), GTK_BUTTONBOX_END);

  gCloseButton =
    gtk_button_new_with_label(gStrings[ST_QUIT].c_str());
  gtk_box_pack_start(GTK_BOX(buttonBox), gCloseButton, FALSE, FALSE, 0);
  GTK_WIDGET_SET_FLAGS(gCloseButton, GTK_CAN_DEFAULT);
  g_signal_connect(gCloseButton, "clicked", G_CALLBACK(CloseClicked), 0);

  gRestartButton = 0;
  if (restartArgs.size() > 0) {
    gRestartButton = gtk_button_new_with_label(gStrings[ST_RESTART].c_str());
    gtk_box_pack_start(GTK_BOX(buttonBox), gRestartButton, FALSE, FALSE, 0);
    GTK_WIDGET_SET_FLAGS(gRestartButton, GTK_CAN_DEFAULT);
    g_signal_connect(gRestartButton, "clicked", G_CALLBACK(RestartClicked), 0);
  }

  gtk_widget_grab_focus(gSubmitReportCheck);

  gtk_widget_grab_default(gRestartButton ? gRestartButton : gCloseButton);

  LoadSettings();

  UpdateEmail();
  UpdateSubmit();

  UpdateHintText(gCommentText, FALSE, &gCommentFieldHint,
                 gStrings[ST_COMMENTGRAYTEXT].c_str());
  UpdateHintText(gEmailEntry, FALSE, &gEmailFieldHint,
                 gStrings[ST_EMAILGRAYTEXT].c_str());

  gtk_widget_show_all(gWindow);
  // stick this here to avoid the show_all above...
  gtk_widget_hide_all(gThrobber);

  gtk_main();

  return gDidTrySend;
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
  if (!rename(file.c_str(), newfile.c_str()))
    return true;
  if (errno != EXDEV)
    return false;

  // use system /bin/mv instead, time to fork
  pid_t pID = vfork();
  if (pID < 0) {
    // Failed to fork
    return false;
  }
  if (pID == 0) {
    char* const args[4] = {
      "mv",
      strdup(file.c_str()),
      strdup(newfile.c_str()),
      0
    };
    if (args[1] && args[2])
      execve("/bin/mv", args, 0);
    if (args[1])
      free(args[1]);
    if (args[2])
      free(args[2]);
    exit(-1);
  }
  int status;
  waitpid(pID, &status, 0);
  return UIFileExists(newfile);
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
