/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Samir Gehani <sgehani@netscape.com>
 *     Brian Ryner <bryner@brianryner.com>
 */

#include "nsWelcomeDlg.h"
#include "nsXInstaller.h"
#include <sys/stat.h>

const int nsWelcomeDlg::kMsgCount;

nsWelcomeDlg::nsWelcomeDlg()
  : mTitle(NULL), mWelcomeMsg(NULL), mPixmap(NULL), mBox(NULL)
{
  memset(mMsgs, 0, sizeof(mMsgs));
}

nsWelcomeDlg::~nsWelcomeDlg()
{
  XI_IF_FREE(mPixmap);
  for (int i = 0; i < kMsgCount; ++i)
    XI_IF_FREE(mMsgs[i]);

  XI_IF_FREE(mWelcomeMsg);
  XI_IF_FREE(mTitle);
}

void 
nsWelcomeDlg::Back(GtkWidget *aWidget, gpointer aData)
{
  DUMP("Back");
  if (aData != gCtx->wdlg) return;
}

void
nsWelcomeDlg::Next(GtkWidget *aWidget, gpointer aData)
{
  DUMP("Next");
  if (aData != gCtx->wdlg) return;

  // hide this notebook page
  gCtx->wdlg->Hide(nsXInstallerDlg::FORWARD_MOVE);

  // disconnect this dlg's nav btn signal handlers
  gtk_signal_disconnect(GTK_OBJECT(gCtx->back), gCtx->backID);
  gtk_signal_disconnect(GTK_OBJECT(gCtx->next), gCtx->nextID);

  // show the next dlg
  gCtx->ldlg->Show(nsXInstallerDlg::FORWARD_MOVE);
}

int
nsWelcomeDlg::Parse(nsINIParser *aParser)
{
  int bufsize = 0;
  char *showDlg = NULL;
  char msgName[] = "Message0";
  char *tmp;

  /* optional keys */
  bufsize = 5;
  aParser->GetStringAlloc(DLG_WELCOME, SHOW_DLG, &showDlg, &bufsize);
  if (bufsize != 0 && showDlg) {
    if (strncmp(showDlg, "TRUE", 4) == 0)
      mShowDlg = nsXInstallerDlg::SHOW_DIALOG;
    else if (strncmp(showDlg, "FALSE", 5) == 0)
      mShowDlg = nsXInstallerDlg::SKIP_DIALOG;
  }

  bufsize = 0;
  aParser->GetStringAlloc(DLG_WELCOME, TITLE, &mTitle, &bufsize);
  if (bufsize == 0)
    XI_IF_FREE(mTitle); 

  bufsize = 0;
  aParser->GetStringAlloc(DLG_WELCOME, WATERMARK, &mPixmap, &bufsize);
  if (bufsize == 0)
    XI_IF_FREE(mPixmap);

  bufsize = 0;
  aParser->GetStringAlloc(DLG_WELCOME, MSGWELCOME, &tmp, &bufsize);
  if (bufsize == 0) {
    XI_IF_FREE(tmp);
  } else {
    mWelcomeMsg = g_strdup_printf(tmp, gCtx->opt->mProductName);
  }

  for (int i = 0; i < kMsgCount; ++i) {
    bufsize = 0;
    msgName[7] = (char)(i + 48);  // ASCII 48 == 0
    aParser->GetStringAlloc(DLG_WELCOME, msgName, &tmp, &bufsize);
    if (bufsize == 0) {
      XI_IF_FREE(tmp);
    } else {
      mMsgs[i] = g_strdup_printf(tmp, gCtx->opt->mProductName);
    }
  }

  return OK;
}

int
nsWelcomeDlg::Show(int aDirection)
{
  int err = 0;
  int msgLength = 0;
  char *msgText, *textPos;

  XI_VERIFY(gCtx);
  XI_VERIFY(gCtx->notebook);

  if (mWidgetsInit == FALSE) {
    // static widget init

    // Note our page number in the wizard.
    mPageNum = gtk_notebook_get_current_page(GTK_NOTEBOOK(gCtx->notebook));

    // Set up the window title
    gtk_window_set_title(GTK_WINDOW(gCtx->window), mTitle);

    mBox = gtk_hbox_new(FALSE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(gCtx->notebook), mBox, NULL);

    GtkWidget *wmbox = gtk_event_box_new();
    if (mPixmap) {
      GdkPixbuf *pb = gdk_pixbuf_new_from_file(mPixmap, NULL);
      if (pb) {
        GdkPixmap *pm = NULL;
        gdk_pixbuf_render_pixmap_and_mask(pb, &pm, NULL, 0);
        if (pm) {
          GtkStyle *newStyle = gtk_style_copy(gtk_widget_get_style(wmbox));
          newStyle->bg_pixmap[GTK_STATE_NORMAL] = pm;
          gtk_widget_set_style(wmbox, newStyle);

          // newStyle now owns the pixmap, so we don't unref it.
          g_object_unref(newStyle);

          // Make the watermark box the width of the pixmap.
          gint width, height;
          gdk_drawable_get_size(pm, &width, &height);
          gtk_widget_set_size_request(wmbox, width, -1);
        }

        // We're done with rendering the pixbuf, so we can destroy it.
        gdk_pixbuf_unref(pb);
      }
    }

    gtk_box_pack_start(GTK_BOX(mBox), wmbox, FALSE, FALSE, 0);

    wmbox = gtk_event_box_new();
    gtk_box_pack_start_defaults(GTK_BOX(mBox), wmbox);

    GtkWidget *inner_hbox = gtk_hbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(wmbox), inner_hbox);

    GtkWidget *spacer = gtk_alignment_new(0, 0, 0, 0);
    gtk_widget_set_size_request(spacer, 5, -1);
    gtk_box_pack_start(GTK_BOX(inner_hbox), spacer, FALSE, FALSE, 0);

    GtkWidget *vbox = gtk_vbox_new(FALSE, 8);
    gtk_box_pack_start(GTK_BOX(inner_hbox), vbox, FALSE, FALSE, 0);

    for (int i = 0; i < kMsgCount - 1; ++i)
      msgLength += strlen(mMsgs[i]);

#define kSpanBegin "<span weight=\"bold\" size=\"larger\">"
#define kSpanEnd   "</span>\n\n"

    msgLength += strlen(kSpanBegin) + strlen(mWelcomeMsg) + strlen(kSpanEnd);
    msgText = (char*) malloc(msgLength + (kMsgCount - 1) * 2 + 1);

    textPos = msgText;

    textPos += sprintf(textPos, kSpanBegin "%s" kSpanEnd, mWelcomeMsg);

    for (int i = 0; i < kMsgCount - 1; ++i)
      textPos += sprintf(textPos, "%s\n\n", mMsgs[i]);

    GtkWidget *msg = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(msg), msgText);
    gtk_label_set_line_wrap(GTK_LABEL(msg), TRUE);
    gtk_misc_set_alignment(GTK_MISC(msg), 0.0, 0.5);

    GdkColor black_col = { 0, 0, 0, 0 };
    GdkColor white_col = { 0, 65535, 65535, 65535 };

    gtk_widget_modify_text(msg, GTK_STATE_NORMAL, &black_col);
    gtk_widget_modify_bg(wmbox, GTK_STATE_NORMAL, &white_col);

    gtk_box_pack_start(GTK_BOX(vbox), msg, FALSE, FALSE, 0);
    gtk_container_child_set(GTK_CONTAINER(vbox), msg, "padding", 5, NULL);

    free(msgText);

    // Now add 100px of padding, and then the last mesage.
    spacer = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
    gtk_widget_set_size_request(spacer, -1, 110);
    gtk_box_pack_start(GTK_BOX(vbox), spacer, FALSE, FALSE, 0);

    msgText = (char*) malloc(strlen(mMsgs[kMsgCount - 1]) + 2);
    sprintf(msgText, "%s\n", mMsgs[kMsgCount - 1]);

    msg = gtk_label_new(msgText);
    gtk_widget_modify_text(msg, GTK_STATE_NORMAL, &black_col);
    gtk_misc_set_alignment(GTK_MISC(msg), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), msg, FALSE, FALSE, 0);
    gtk_container_child_set(GTK_CONTAINER(vbox), msg, "padding", 5, NULL);

    free(msgText);

    mWidgetsInit = TRUE;
  } else {
    gtk_notebook_set_page(GTK_NOTEBOOK(gCtx->notebook), mPageNum);
  }

  // No header image for the welcome dialog
  gtk_widget_set_usize(gCtx->header, 0, 0);

  gtk_widget_show_all(mBox);

  // signal connect the buttons
  gCtx->backID = gtk_signal_connect(GTK_OBJECT(gCtx->back), "clicked",
                                    GTK_SIGNAL_FUNC(nsWelcomeDlg::Back),
                                    gCtx->wdlg);
  gCtx->nextID = gtk_signal_connect(GTK_OBJECT(gCtx->next), "clicked",
                                    GTK_SIGNAL_FUNC(nsWelcomeDlg::Next),
                                    gCtx->wdlg);

  gtk_widget_set_sensitive(gCtx->back, FALSE);

  GTK_WIDGET_SET_FLAGS(gCtx->next, GTK_CAN_DEFAULT);
  gtk_widget_grab_default(gCtx->next);
  gtk_widget_grab_focus(gCtx->next);

  return err;
}

int 
nsWelcomeDlg::Hide(int aDirection)
{
  // hide all this dlg's widgets
  gtk_widget_hide(mBox);

  return OK;
}
