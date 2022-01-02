/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <gtk/gtk.h>
#include <unistd.h>
#include "mozilla/Sprintf.h"
#include "mozilla/Atomics.h"
#include "progressui.h"
#include "readstrings.h"
#include "updatererrors.h"

#define TIMER_INTERVAL 100

static float sProgressVal;  // between 0 and 100
static mozilla::Atomic<gboolean> sQuit(FALSE);
static gboolean sEnableUI;
static guint sTimerID;

static GtkWidget* sWin;
static GtkWidget* sLabel;
static GtkWidget* sProgressBar;
static GdkPixbuf* sPixbuf;

StringTable sStrings;

static gboolean UpdateDialog(gpointer data) {
  if (sQuit) {
    gtk_widget_hide(sWin);
    gtk_main_quit();
  }

  float progress = sProgressVal;

  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(sProgressBar),
                                progress / 100.0);

  return TRUE;
}

static gboolean OnDeleteEvent(GtkWidget* widget, GdkEvent* event,
                              gpointer user_data) {
  return TRUE;
}

int InitProgressUI(int* pargc, char*** pargv) {
  sEnableUI = gtk_init_check(pargc, pargv);
  if (sEnableUI) {
    // Prepare to show the UI here in case the files are modified by the update.
    char ini_path[PATH_MAX];
    SprintfLiteral(ini_path, "%s.ini", (*pargv)[0]);
    if (ReadStrings(ini_path, &sStrings) != OK) {
      sEnableUI = false;
      return -1;
    }

    char icon_path[PATH_MAX];
    SprintfLiteral(icon_path, "%s/icons/updater.png", (*pargv)[2]);
    sPixbuf = gdk_pixbuf_new_from_file(icon_path, nullptr);
  }
  return 0;
}

int ShowProgressUI() {
  if (!sEnableUI) {
    return -1;
  }

  // Only show the Progress UI if the process is taking a significant amount of
  // time where a significant amount of time is defined as .5 seconds after
  // ShowProgressUI is called sProgress is less than 70.
  usleep(500000);

  if (sQuit || sProgressVal > 70.0f) {
    return 0;
  }

  sWin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  if (!sWin) {
    return -1;
  }

  g_signal_connect(G_OBJECT(sWin), "delete_event", G_CALLBACK(OnDeleteEvent),
                   nullptr);

  gtk_window_set_title(GTK_WINDOW(sWin), sStrings.title.get());
  gtk_window_set_type_hint(GTK_WINDOW(sWin), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position(GTK_WINDOW(sWin), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_resizable(GTK_WINDOW(sWin), FALSE);
  gtk_window_set_decorated(GTK_WINDOW(sWin), TRUE);
  gtk_window_set_deletable(GTK_WINDOW(sWin), FALSE);
  gtk_window_set_icon(GTK_WINDOW(sWin), sPixbuf);
  g_object_unref(sPixbuf);

  GtkWidget* vbox = gtk_vbox_new(TRUE, 6);
  sLabel = gtk_label_new(sStrings.info.get());
  gtk_misc_set_alignment(GTK_MISC(sLabel), 0.0f, 0.0f);
  sProgressBar = gtk_progress_bar_new();

  gtk_box_pack_start(GTK_BOX(vbox), sLabel, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), sProgressBar, TRUE, TRUE, 0);

  sTimerID = g_timeout_add(TIMER_INTERVAL, UpdateDialog, nullptr);

  gtk_container_set_border_width(GTK_CONTAINER(sWin), 10);
  gtk_container_add(GTK_CONTAINER(sWin), vbox);
  gtk_widget_show_all(sWin);

  gtk_main();
  return 0;
}

// Called on a background thread
void QuitProgressUI() { sQuit = TRUE; }

// Called on a background thread
void UpdateProgressUI(float progress) {
  sProgressVal = progress;  // 32-bit writes are atomic
}
