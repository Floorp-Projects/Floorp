/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CRASHREPORTER_GTK_COMMON_H__
#define CRASHREPORTER_GTK_COMMON_H__

#include <glib.h>
#include <gtk/gtk.h>

#include <string>
#include <vector>

const char kIniFile[] = "crashreporter.ini";

extern GtkWidget* gWindow;
extern GtkWidget* gSubmitReportCheck;
extern GtkWidget* gIncludeURLCheck;
extern GtkWidget* gThrobber;
extern GtkWidget* gProgressLabel;
extern GtkWidget* gCloseButton;
extern GtkWidget* gRestartButton;

extern std::vector<std::string> gRestartArgs;
extern GThread* gSendThreadID;

extern bool gInitialized;
extern bool gDidTrySend;
extern StringTable gFiles;
extern StringTable gQueryParameters;
extern std::string gHttpProxy;
extern std::string gAuth;
extern std::string gCACertificateFile;
extern std::string gSendURL;
extern std::string gURLParameter;

void LoadProxyinfo();
gpointer SendThread(gpointer args);
gboolean WindowDeleted(GtkWidget* window,
                       GdkEvent* event,
                       gpointer userData);
gboolean check_escape(GtkWidget* window, GdkEventKey* event, gpointer data);
void SubmitReportChecked(GtkButton* sender, gpointer userData);
void IncludeURLClicked(GtkButton* sender, gpointer userData);
void CloseClicked(GtkButton* button,
                  gpointer userData);
void RestartClicked(GtkButton* button,
                    gpointer userData);

#endif // CRASHREPORTER_GTK_COMMON_H__
