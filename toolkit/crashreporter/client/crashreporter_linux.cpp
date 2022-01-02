/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <dlfcn.h>
#include <fcntl.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>

#include <cctype>

#include "crashreporter.h"
#include "crashreporter_gtk_common.h"

#define LABEL_MAX_CHAR_WIDTH 48

using std::ios;
using std::string;
using std::vector;

using namespace CrashReporter;

static GtkWidget* gViewReportButton = 0;
static GtkWidget* gCommentTextLabel = 0;
static GtkWidget* gCommentText = 0;

static bool gCommentFieldHint = true;

// handle from dlopen'ing libgnome
static void* gnomeLib = nullptr;
// handle from dlopen'ing libgnomeui
static void* gnomeuiLib = nullptr;

static void LoadSettings() {
  /*
   * NOTE! This code needs to stay in sync with the preference checking
   *       code in in nsExceptionHandler.cpp.
   */

  StringTable settings;
  if (ReadStringsFromFile(gSettingsPath + "/" + kIniFile, settings, true)) {
    if (settings.find("IncludeURL") != settings.end() &&
        gIncludeURLCheck != 0) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gIncludeURLCheck),
                                   settings["IncludeURL"][0] != '0');
    }
    bool enabled = true;
    if (settings.find("SubmitReport") != settings.end())
      enabled = settings["SubmitReport"][0] != '0';
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gSubmitReportCheck),
                                 enabled);
  }
}

static string Escape(const string& str) {
  string ret;
  for (auto c : str) {
    if (c == '\\') {
      ret += "\\\\";
    } else if (c == '\n') {
      ret += "\\n";
    } else if (c == '\t') {
      ret += "\\t";
    } else {
      ret.push_back(c);
    }
  }

  return ret;
}

static bool WriteStrings(ostream& out, const string& header,
                         StringTable& strings, bool escape) {
  out << "[" << header << "]" << std::endl;
  for (const auto& iter : strings) {
    out << iter.first << "=";
    if (escape) {
      out << Escape(iter.second);
    } else {
      out << iter.second;
    }

    out << std::endl;
  }

  return true;
}

static bool WriteStringsToFile(const string& path, const string& header,
                               StringTable& strings, bool escape) {
  ofstream* f = UIOpenWrite(path, ios::trunc);
  bool success = false;
  if (f->is_open()) {
    success = WriteStrings(*f, header, strings, escape);
    f->close();
  }

  delete f;
  return success;
}

void SaveSettings() {
  /*
   * NOTE! This code needs to stay in sync with the preference setting
   *       code in in nsExceptionHandler.cpp.
   */

  StringTable settings;

  ReadStringsFromFile(gSettingsPath + "/" + kIniFile, settings, true);
  if (gIncludeURLCheck != 0)
    settings["IncludeURL"] =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gIncludeURLCheck)) ? "1"
                                                                          : "0";
  settings["SubmitReport"] =
      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gSubmitReportCheck)) ? "1"
                                                                          : "0";

  WriteStringsToFile(gSettingsPath + "/" + kIniFile, "Crash Reporter", settings,
                     true);
}

void SendReport() {
  LoadProxyinfo();

  // spawn a thread to do the sending
  gSendThreadID = g_thread_create(SendThread, nullptr, TRUE, nullptr);
}

void DisableGUIAndSendReport() {
  // disable all our gui controls, show the throbber + change the progress text
  gtk_widget_set_sensitive(gSubmitReportCheck, FALSE);
  gtk_widget_set_sensitive(gViewReportButton, FALSE);
  gtk_widget_set_sensitive(gCommentText, FALSE);
  if (gIncludeURLCheck) gtk_widget_set_sensitive(gIncludeURLCheck, FALSE);
  gtk_widget_set_sensitive(gCloseButton, FALSE);
  if (gRestartButton) gtk_widget_set_sensitive(gRestartButton, FALSE);
  gtk_widget_show_all(gThrobber);
  gtk_label_set_text(GTK_LABEL(gProgressLabel),
                     gStrings[ST_REPORTDURINGSUBMIT].c_str());

  SendReport();
}

static void ShowReportInfo(GtkTextView* viewReportTextView) {
  GtkTextBuffer* buffer = gtk_text_view_get_buffer(viewReportTextView);

  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer, &start);
  gtk_text_buffer_get_end_iter(buffer, &end);

  gtk_text_buffer_delete(buffer, &start, &end);

  for (Json::ValueConstIterator iter = gQueryParameters.begin();
       iter != gQueryParameters.end(); ++iter) {
    gtk_text_buffer_insert(buffer, &end, iter.name().c_str(),
                           iter.name().length());
    gtk_text_buffer_insert(buffer, &end, ": ", -1);
    string value;
    if (iter->isString()) {
      value = iter->asString();
    } else {
      Json::StreamWriterBuilder builder;
      builder["indentation"] = "";
      value = writeString(builder, *iter);
    }
    gtk_text_buffer_insert(buffer, &end, value.c_str(), value.length());
    gtk_text_buffer_insert(buffer, &end, "\n", -1);
  }

  gtk_text_buffer_insert(buffer, &end, "\n", -1);
  gtk_text_buffer_insert(buffer, &end, gStrings[ST_EXTRAREPORTINFO].c_str(),
                         -1);
}

void UpdateSubmit() {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gSubmitReportCheck))) {
    gtk_widget_set_sensitive(gViewReportButton, TRUE);
    gtk_widget_set_sensitive(gCommentText, TRUE);
    if (gIncludeURLCheck) gtk_widget_set_sensitive(gIncludeURLCheck, TRUE);
    gtk_label_set_text(GTK_LABEL(gProgressLabel),
                       gStrings[ST_REPORTPRESUBMIT].c_str());
  } else {
    gtk_widget_set_sensitive(gViewReportButton, FALSE);
    gtk_widget_set_sensitive(gCommentText, FALSE);
    if (gIncludeURLCheck) gtk_widget_set_sensitive(gIncludeURLCheck, FALSE);
    gtk_label_set_text(GTK_LABEL(gProgressLabel), "");
  }
}

static void ViewReportClicked(GtkButton* button, gpointer userData) {
  GtkDialog* dialog = GTK_DIALOG(gtk_dialog_new_with_buttons(
      gStrings[ST_VIEWREPORTTITLE].c_str(), GTK_WINDOW(gWindow),
      GTK_DIALOG_MODAL, GTK_STOCK_OK, GTK_RESPONSE_OK, nullptr));

  GtkWidget* scrolled = gtk_scrolled_window_new(0, 0);
  gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(dialog)),
                    scrolled);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled),
                                      GTK_SHADOW_IN);
  gtk_widget_set_vexpand(scrolled, TRUE);

  GtkWidget* viewReportTextView = gtk_text_view_new();
  gtk_container_add(GTK_CONTAINER(scrolled), viewReportTextView);
  gtk_text_view_set_editable(GTK_TEXT_VIEW(viewReportTextView), FALSE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(viewReportTextView), GTK_WRAP_WORD);
  gtk_widget_set_size_request(GTK_WIDGET(viewReportTextView), -1, 100);

  ShowReportInfo(GTK_TEXT_VIEW(viewReportTextView));

  gtk_dialog_set_default_response(dialog, GTK_RESPONSE_OK);
  gtk_widget_set_size_request(GTK_WIDGET(dialog), 400, 200);
  gtk_widget_show_all(GTK_WIDGET(dialog));
  gtk_dialog_run(dialog);
  gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void CommentChanged(GtkTextBuffer* buffer, gpointer userData) {
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer, &start);
  gtk_text_buffer_get_end_iter(buffer, &end);
  const char* comment = gtk_text_buffer_get_text(buffer, &start, &end, TRUE);
  if (comment[0] == '\0' || gCommentFieldHint) {
    gQueryParameters.removeMember("Comments");
  } else {
    gQueryParameters["Comments"] = comment;
  }
}

static void CommentInsert(GtkTextBuffer* buffer, GtkTextIter* location,
                          gchar* text, gint len, gpointer userData) {
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
                           bool* hintShowing, const char* hintText) {
  GtkTextBuffer* buffer = nullptr;
  if (GTK_IS_TEXT_VIEW(widget))
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget));

  if (gainedFocus) {
    if (*hintShowing) {
      if (buffer == nullptr) {  // sort of cheating
        gtk_entry_set_text(GTK_ENTRY(widget), "");
      } else {  // GtkTextView
        gtk_text_buffer_set_text(buffer, "", 0);
      }
      gtk_widget_modify_text(widget, GTK_STATE_NORMAL, nullptr);
      *hintShowing = false;
    }
  } else {
    // lost focus
    const char* text = nullptr;
    if (buffer == nullptr) {
      text = gtk_entry_get_text(GTK_ENTRY(widget));
    } else {
      GtkTextIter start, end;
      gtk_text_buffer_get_start_iter(buffer, &start);
      gtk_text_buffer_get_end_iter(buffer, &end);
      text = gtk_text_buffer_get_text(buffer, &start, &end, TRUE);
    }

    if (text == nullptr || text[0] == '\0') {
      *hintShowing = true;

      if (buffer == nullptr) {
        gtk_entry_set_text(GTK_ENTRY(widget), hintText);
      } else {
        gtk_text_buffer_set_text(buffer, hintText, -1);
      }

      gtk_widget_modify_text(
          widget, GTK_STATE_NORMAL,
          &gtk_widget_get_style(widget)->text[GTK_STATE_INSENSITIVE]);
    }
  }
}

static gboolean CommentFocusChange(GtkWidget* widget, GdkEventFocus* event,
                                   gpointer userData) {
  UpdateHintText(widget, event->in, &gCommentFieldHint,
                 gStrings[ST_COMMENTGRAYTEXT].c_str());

  return FALSE;
}

typedef struct _GnomeProgram GnomeProgram;
typedef struct _GnomeModuleInfo GnomeModuleInfo;
typedef GnomeProgram* (*_gnome_program_init_fn)(const char*, const char*,
                                                const GnomeModuleInfo*, int,
                                                char**, const char*, ...);
typedef const GnomeModuleInfo* (*_libgnomeui_module_info_get_fn)();

void TryInitGnome() {
  gnomeLib = dlopen("libgnome-2.so.0", RTLD_LAZY);
  if (!gnomeLib) return;

  gnomeuiLib = dlopen("libgnomeui-2.so.0", RTLD_LAZY);
  if (!gnomeuiLib) return;

  _gnome_program_init_fn gnome_program_init =
      (_gnome_program_init_fn)(dlsym(gnomeLib, "gnome_program_init"));
  _libgnomeui_module_info_get_fn libgnomeui_module_info_get =
      (_libgnomeui_module_info_get_fn)(dlsym(gnomeuiLib,
                                             "libgnomeui_module_info_get"));

  if (gnome_program_init && libgnomeui_module_info_get) {
    gnome_program_init("crashreporter", "1.0", libgnomeui_module_info_get(),
                       gArgc, gArgv, nullptr);
  }
}

/* === Crashreporter UI Functions === */

/*
 * Anything not listed here is in crashreporter_gtk_common.cpp:
 *  UIInit
 *  UIShowDefaultUI
 *  UIError_impl
 *  UIGetIniPath
 *  UIGetSettingsPath
 *  UIEnsurePathExists
 *  UIFileExists
 *  UIMoveFile
 *  UIDeleteFile
 *  UIOpenRead
 *  UIOpenWrite
 */

void UIShutdown() {
  if (gnomeuiLib) dlclose(gnomeuiLib);
  // Don't dlclose gnomeLib as libgnomevfs and libORBit-2 use atexit().
}

bool UIShowCrashUI(const StringTable& files, const Json::Value& queryParameters,
                   const string& sendURL, const vector<string>& restartArgs) {
  gFiles = files;
  gQueryParameters = queryParameters;
  gSendURL = sendURL;
  gRestartArgs = restartArgs;
  if (gQueryParameters.isMember("URL")) {
    gURLParameter = gQueryParameters["URL"].asString();
  }

  if (gAutoSubmit) {
    SendReport();
    CloseApp(nullptr);
    return true;
  }

  gWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(gWindow),
                       gStrings[ST_CRASHREPORTERTITLE].c_str());
  gtk_window_set_resizable(GTK_WINDOW(gWindow), FALSE);
  gtk_window_set_position(GTK_WINDOW(gWindow), GTK_WIN_POS_CENTER);
  gtk_container_set_border_width(GTK_CONTAINER(gWindow), 12);
  g_signal_connect(gWindow, "delete-event", G_CALLBACK(WindowDeleted), 0);
  g_signal_connect(gWindow, "key_press_event", G_CALLBACK(check_escape),
                   nullptr);

  GtkWidget* vbox = gtk_vbox_new(FALSE, 6);
  gtk_container_add(GTK_CONTAINER(gWindow), vbox);

  GtkWidget* titleLabel = gtk_label_new("");
  gtk_box_pack_start(GTK_BOX(vbox), titleLabel, FALSE, FALSE, 0);
  gtk_misc_set_alignment(GTK_MISC(titleLabel), 0, 0.5);
  char* markup =
      g_strdup_printf("<b>%s</b>", gStrings[ST_CRASHREPORTERHEADER].c_str());
  gtk_label_set_markup(GTK_LABEL(titleLabel), markup);
  g_free(markup);

  GtkWidget* descriptionLabel =
      gtk_label_new(gStrings[ST_CRASHREPORTERDESCRIPTION].c_str());
  gtk_box_pack_start(GTK_BOX(vbox), descriptionLabel, TRUE, TRUE, 0);
  // force the label to line wrap
  gtk_label_set_max_width_chars(GTK_LABEL(descriptionLabel),
                                LABEL_MAX_CHAR_WIDTH);
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
  gtk_button_box_set_layout(GTK_BUTTON_BOX(viewReportButtonBox),
                            GTK_BUTTONBOX_START);

  gViewReportButton =
      gtk_button_new_with_label(gStrings[ST_VIEWREPORT].c_str());
  gtk_box_pack_start(GTK_BOX(viewReportButtonBox), gViewReportButton, FALSE,
                     FALSE, 0);
  g_signal_connect(gViewReportButton, "clicked", G_CALLBACK(ViewReportClicked),
                   0);

  GtkWidget* scrolled = gtk_scrolled_window_new(0, 0);
  gtk_container_add(GTK_CONTAINER(innerVBox), scrolled);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled),
                                      GTK_SHADOW_IN);
  gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled),
                                             100);

  gCommentTextLabel = gtk_label_new(gStrings[ST_COMMENTGRAYTEXT].c_str());
  gCommentText = gtk_text_view_new();
  gtk_label_set_mnemonic_widget(GTK_LABEL(gCommentTextLabel), gCommentText);
  gtk_text_view_set_accepts_tab(GTK_TEXT_VIEW(gCommentText), FALSE);
  g_signal_connect(gCommentText, "focus-in-event",
                   G_CALLBACK(CommentFocusChange), 0);
  g_signal_connect(gCommentText, "focus-out-event",
                   G_CALLBACK(CommentFocusChange), 0);

  GtkTextBuffer* commentBuffer =
      gtk_text_view_get_buffer(GTK_TEXT_VIEW(gCommentText));
  g_signal_connect(commentBuffer, "changed", G_CALLBACK(CommentChanged), 0);
  g_signal_connect(commentBuffer, "insert-text", G_CALLBACK(CommentInsert), 0);

  gtk_container_add(GTK_CONTAINER(scrolled), gCommentText);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(gCommentText), GTK_WRAP_WORD_CHAR);
  gtk_widget_set_size_request(GTK_WIDGET(gCommentText), -1, 100);

  if (gQueryParameters.isMember("URL")) {
    gIncludeURLCheck =
        gtk_check_button_new_with_label(gStrings[ST_CHECKURL].c_str());
    gtk_box_pack_start(GTK_BOX(innerVBox), gIncludeURLCheck, FALSE, FALSE, 0);
    g_signal_connect(gIncludeURLCheck, "clicked", G_CALLBACK(IncludeURLClicked),
                     0);
    // on by default
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gIncludeURLCheck), TRUE);
  }

  GtkWidget* progressBox = gtk_hbox_new(FALSE, 6);
  gtk_box_pack_start(GTK_BOX(vbox), progressBox, TRUE, TRUE, 0);

  // Get the throbber image from alongside the executable
  char* dir = g_path_get_dirname(gArgv[0]);
  char* path = g_build_filename(dir, "Throbber-small.gif", nullptr);
  g_free(dir);
  gThrobber = gtk_image_new_from_file(path);
  gtk_box_pack_start(GTK_BOX(progressBox), gThrobber, FALSE, FALSE, 0);

  gProgressLabel = gtk_label_new(gStrings[ST_REPORTPRESUBMIT].c_str());
  gtk_box_pack_start(GTK_BOX(progressBox), gProgressLabel, TRUE, TRUE, 0);
  // force the label to line wrap
  gtk_label_set_max_width_chars(GTK_LABEL(gProgressLabel),
                                LABEL_MAX_CHAR_WIDTH);
  gtk_label_set_line_wrap(GTK_LABEL(gProgressLabel), TRUE);

  GtkWidget* buttonBox = gtk_hbutton_box_new();
  gtk_box_pack_end(GTK_BOX(vbox), buttonBox, FALSE, FALSE, 0);
  gtk_box_set_spacing(GTK_BOX(buttonBox), 6);
  gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonBox), GTK_BUTTONBOX_END);

  gCloseButton = gtk_button_new_with_label(gStrings[ST_QUIT].c_str());
  gtk_box_pack_start(GTK_BOX(buttonBox), gCloseButton, FALSE, FALSE, 0);
  gtk_widget_set_can_default(gCloseButton, TRUE);
  g_signal_connect(gCloseButton, "clicked", G_CALLBACK(CloseClicked), 0);

  gRestartButton = 0;
  if (!restartArgs.empty()) {
    gRestartButton = gtk_button_new_with_label(gStrings[ST_RESTART].c_str());
    gtk_box_pack_start(GTK_BOX(buttonBox), gRestartButton, FALSE, FALSE, 0);
    gtk_widget_set_can_default(gRestartButton, TRUE);
    g_signal_connect(gRestartButton, "clicked", G_CALLBACK(RestartClicked), 0);
  }

  gtk_widget_grab_focus(gSubmitReportCheck);

  gtk_widget_grab_default(gRestartButton ? gRestartButton : gCloseButton);

  LoadSettings();

  UpdateSubmit();

  UpdateHintText(gCommentText, FALSE, &gCommentFieldHint,
                 gStrings[ST_COMMENTGRAYTEXT].c_str());

  gtk_widget_show_all(gWindow);
  // stick this here to avoid the show_all above...
  gtk_widget_hide(gThrobber);

  gtk_main();

  return gDidTrySend;
}
