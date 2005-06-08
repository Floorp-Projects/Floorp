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
#include <string.h>
#include <stdio.h>

#define TIMER_INTERVAL 100
#define MAX_TEXT_LEN 200

static float    sProgressVal;  // between 0 and 100
static gboolean sQuit = FALSE;
static guint    sTimerID;

static GtkWidget *sWin;
static GtkWidget *sLabel;
static GtkWidget *sProgressBar;

static const char *sProgramPath;

// stack based FILE wrapper to ensure that fclose is called.
class AutoFILE {
public:
  AutoFILE(FILE *fp) : fp_(fp) {}
  ~AutoFILE() { if (fp_) fclose(fp_); }
  operator FILE *() { return fp_; }
private:
  FILE *fp_;
};

// very basic parser for updater.ini
static gboolean
ReadStrings(char title[MAX_TEXT_LEN], char info[MAX_TEXT_LEN])
{
  char path[PATH_MAX];
  snprintf(path, sizeof(path), "%s.ini", sProgramPath);

  AutoFILE fp = fopen(path, "r");
  if (!fp)
    return FALSE;

  if (!fgets(title, MAX_TEXT_LEN, fp))
    return FALSE;
  if (!fgets(title, MAX_TEXT_LEN, fp))
    return FALSE;
  if (!fgets(info, MAX_TEXT_LEN, fp))
    return FALSE;

  // trim trailing newline character and leading 'key='

  char *strings[] = {
    title, info, NULL
  };
  for (char **p = strings; *p; ++p) {
    int len = strlen(*p);
    if (len)
      (*p)[len - 1] = '\0';

    char *eq = strchr(*p, '=');
    if (!eq)
      return FALSE;
    memmove(*p, eq + 1, len - (eq - *p + 1));
  }

  return TRUE;
}

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

static void
DoNothing()
{
}

int
InitProgressUI(int *pargc, char ***pargv)
{
  sProgramPath = (*pargv)[0];

  gtk_init(pargc, pargv);
  return 0;
}

int
ShowProgressUI()
{
  // Only show the Progress UI if the process is taking significant time.
  // Here we measure significant time as taking more than one second.

  usleep(500000);

  if (sQuit || sProgressVal > 50.0f)
    return 0;

  char titleText[MAX_TEXT_LEN], infoText[MAX_TEXT_LEN]; 
  if (!ReadStrings(titleText, infoText))
    return -1;
  
  sWin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  if (!sWin)
    return -1;
  gtk_window_set_title(GTK_WINDOW(sWin), titleText);
  gtk_window_set_type_hint(GTK_WINDOW(sWin), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position(GTK_WINDOW(sWin), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_resizable(GTK_WINDOW(sWin), FALSE);
  gtk_window_set_decorated(GTK_WINDOW(sWin), FALSE);
  gtk_signal_connect(GTK_OBJECT(sWin), "delete", DoNothing, NULL);

  GtkWidget *vbox = gtk_vbox_new(TRUE, 6);
  sLabel = gtk_label_new(infoText);
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
