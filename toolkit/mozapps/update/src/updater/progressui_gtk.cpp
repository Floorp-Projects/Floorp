/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
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

#include <stdio.h>
#include <gtk/gtk.h>
#include <unistd.h>
#include "progressui.h"
#include "readstrings.h"
#include "errors.h"

#define TIMER_INTERVAL 100

static float    sProgressVal;  // between 0 and 100
static gboolean sQuit = FALSE;
static gboolean sEnableUI;
static guint    sTimerID;

static GtkWidget *sWin;
static GtkWidget *sLabel;
static GtkWidget *sProgressBar;

static const char *sProgramPath;

static gboolean
UpdateDialog(gpointer data)
{
  if (sQuit)
  {
    gtk_widget_hide(sWin);
    gtk_main_quit();
  }

  float progress = sProgressVal;

  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(sProgressBar),
                                progress / 100.0);

  return TRUE;
}

static gboolean
OnDeleteEvent(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  return TRUE;
}

int
InitProgressUI(int *pargc, char ***pargv)
{
  sProgramPath = (*pargv)[0];

  sEnableUI = gtk_init_check(pargc, pargv);
  return 0;
}

int
ShowProgressUI()
{
  if (!sEnableUI)
    return -1;

  // Only show the Progress UI if the process is taking significant time.
  // Here we measure significant time as taking more than one second.

  usleep(500000);

  if (sQuit || sProgressVal > 50.0f)
    return 0;

  char path[PATH_MAX];
  snprintf(path, sizeof(path), "%s.ini", sProgramPath);

  StringTable strings;
  if (ReadStrings(path, &strings) != OK)
    return -1;
  
  sWin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  if (!sWin)
    return -1;

  // GTK 2.2 seems unable to prevent our dialog from being closed when
  // the user hits the close button on the dialog.  This problem only
  // occurs when either one of the following methods are called:
  //   gtk_window_set_position
  //   gtk_window_set_type_hint
  // For this reason, we disable window decorations.
 
  g_signal_connect(G_OBJECT(sWin), "delete_event",
                   G_CALLBACK(OnDeleteEvent), NULL);

  gtk_window_set_title(GTK_WINDOW(sWin), strings.title);
  gtk_window_set_type_hint(GTK_WINDOW(sWin), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position(GTK_WINDOW(sWin), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_resizable(GTK_WINDOW(sWin), FALSE);
  gtk_window_set_decorated(GTK_WINDOW(sWin), FALSE);

  GtkWidget *vbox = gtk_vbox_new(TRUE, 6);
  sLabel = gtk_label_new(strings.info);
  gtk_misc_set_alignment(GTK_MISC(sLabel), 0.0f, 0.0f);
  sProgressBar = gtk_progress_bar_new();

  gtk_box_pack_start(GTK_BOX(vbox), sLabel, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), sProgressBar, TRUE, TRUE, 0);

  sTimerID = g_timeout_add(TIMER_INTERVAL, UpdateDialog, NULL);

  gtk_container_set_border_width(GTK_CONTAINER(sWin), 10);
  gtk_container_add(GTK_CONTAINER(sWin), vbox);
  gtk_widget_show_all(sWin);

  gtk_main();
  return 0;
}

// Called on a background thread
void
QuitProgressUI()
{
  sQuit = TRUE;
}

// Called on a background thread
void
UpdateProgressUI(float progress)
{
  sProgressVal = progress;  // 32-bit writes are atomic
}
