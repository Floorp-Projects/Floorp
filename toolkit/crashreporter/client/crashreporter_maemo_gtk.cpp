/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <fcntl.h>
#include <hildon-1/hildon/hildon.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>

#include <cctype>

#include "crashreporter.h"
#include "crashreporter_gtk_common.h"

using std::string;
using std::vector;

using namespace CrashReporter;

void LoadSettings()
{
  /*
   * NOTE! This code needs to stay in sync with the preference checking
   *       code in in nsExceptionHandler.cpp.
   */

  //XXX: share with desktop linux code?
  StringTable settings;
  if (ReadStringsFromFile(gSettingsPath + "/" + kIniFile, settings, true)) {
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

void SaveSettings()
{
  /*
   * NOTE! This code needs to stay in sync with the preference setting
   *       code in in nsExceptionHandler.cpp.
   */

  StringTable settings;

  ReadStringsFromFile(gSettingsPath + "/" + kIniFile, settings, true);

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

/*
 * Check if a crashreporter.crt file exists next
 * to the crashreporter binary, and if so set gCACertificateFile
 * to its path. The CA cert will then be used by libcurl to authenticate
 * the server's SSL certificate.
 */
static void FindCACertificateFile()
{
  string path = gArgv[0];
  path += ".crt";
  if (UIFileExists(path)) {
    gCACertificateFile = path;
  }
}

void SendReport()
{
  // disable all our gui controls, show the throbber + change the progress text
  gtk_widget_set_sensitive(gSubmitReportCheck, FALSE);
  if (gIncludeURLCheck)
    gtk_widget_set_sensitive(gIncludeURLCheck, FALSE);
  gtk_widget_set_sensitive(gCloseButton, FALSE);
  if (gRestartButton)
    gtk_widget_set_sensitive(gRestartButton, FALSE);
  gtk_widget_show_all(gThrobber);
  gtk_label_set_text(GTK_LABEL(gProgressLabel),
                     gStrings[ST_REPORTDURINGSUBMIT].c_str());

#ifdef MOZ_ENABLE_GCONF
  LoadProxyinfo();
#endif

  FindCACertificateFile();

  // and spawn a thread to do the sending
  GError* err;
  gSendThreadID = g_thread_create(SendThread, NULL, TRUE, &err);
}

void UpdateSubmit()
{
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gSubmitReportCheck))) {
    if (gIncludeURLCheck)
      gtk_widget_set_sensitive(gIncludeURLCheck, TRUE);
    gtk_label_set_text(GTK_LABEL(gProgressLabel),
                       gStrings[ST_REPORTPRESUBMIT].c_str());
  } else {
    if (gIncludeURLCheck)
      gtk_widget_set_sensitive(gIncludeURLCheck, FALSE);
    gtk_label_set_text(GTK_LABEL(gProgressLabel), "");
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

void UIShutdown()
{
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
  gtk_widget_set_size_request(descriptionLabel, -1, -1);
  gtk_label_set_line_wrap(GTK_LABEL(descriptionLabel), TRUE);
  gtk_label_set_selectable(GTK_LABEL(descriptionLabel), TRUE);
  gtk_misc_set_alignment(GTK_MISC(descriptionLabel), 0, 0.5);

  gSubmitReportCheck =
    gtk_check_button_new_with_label(gStrings[ST_CHECKSUBMIT].c_str());

  gtk_box_pack_start(GTK_BOX(vbox), gSubmitReportCheck, FALSE, FALSE, 0);
  g_signal_connect(gSubmitReportCheck, "clicked",
                   G_CALLBACK(SubmitReportChecked), 0);

  if (gQueryParameters.find("URL") != gQueryParameters.end()) {
    gIncludeURLCheck =
      gtk_check_button_new_with_label(gStrings[ST_CHECKURL].c_str());
    gtk_box_pack_start(GTK_BOX(vbox), gIncludeURLCheck, FALSE, FALSE, 0);
    g_signal_connect(gIncludeURLCheck, "clicked", G_CALLBACK(IncludeURLClicked), 0);
    // on by default
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gIncludeURLCheck), TRUE);
  }

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
  gtk_widget_set_size_request(gProgressLabel, 500, -1);
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

  UpdateSubmit();

  gtk_widget_show_all(gWindow);
  // stick this here to avoid the show_all above...
  gtk_widget_hide_all(gThrobber);

  gtk_main();

  return gDidTrySend;
}
