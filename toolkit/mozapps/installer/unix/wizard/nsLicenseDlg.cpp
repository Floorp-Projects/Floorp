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
 */

#include "nsLicenseDlg.h"
#include "nsXInstaller.h"
#include <sys/stat.h>

nsLicenseDlg::nsLicenseDlg() :
  mLicenseFile(NULL),
  mMsg0(NULL)
{
}

nsLicenseDlg::~nsLicenseDlg()
{
    XI_IF_FREE(mLicenseFile);
}

void
nsLicenseDlg::Back(GtkWidget *aWidget, gpointer aData)
{
    DUMP("Back");
    if (aData != gCtx->ldlg) return;
    
// XXX call gCtx->me->Shutdown() ?
    gtk_main_quit();
    return;

#if 0
    // hide this notebook page
    gCtx->ldlg->Hide(nsXInstallerDlg::BACKWARD_MOVE);

    // disconnect this dlg's nav btn signal handlers
    gtk_signal_disconnect(GTK_OBJECT(gCtx->back), gCtx->backID);
    gtk_signal_disconnect(GTK_OBJECT(gCtx->next), gCtx->nextID);

    // show the prev dlg
    gCtx->wdlg->Show(nsXInstallerDlg::BACKWARD_MOVE);
#endif
}

void
nsLicenseDlg::Next(GtkWidget *aWidget, gpointer aData)
{
    DUMP("Next");
    if (aData != gCtx->ldlg) return;

    // hide this notebook page
    gCtx->ldlg->Hide(nsXInstallerDlg::FORWARD_MOVE);

    // disconnect this dlg's nav btn signal handlers
    gtk_signal_disconnect(GTK_OBJECT(gCtx->back), gCtx->backID);
    gtk_signal_disconnect(GTK_OBJECT(gCtx->next), gCtx->nextID);

    // show the next dlg
    gCtx->sdlg->Show(nsXInstallerDlg::FORWARD_MOVE);
}

int
nsLicenseDlg::Parse(nsINIParser *aParser)
{
    int err = OK;
    int bufsize = 0;
    char *showDlg = NULL;
    
    /* compulsory keys*/
    XI_ERR_BAIL(aParser->GetStringAlloc(DLG_LICENSE, LICENSE, 
                &mLicenseFile, &bufsize));
    if (bufsize == 0 || !mLicenseFile)
        return E_INVALID_KEY;

    bufsize = 5;
    XI_ERR_BAIL(aParser->GetStringAlloc(DLG_LICENSE, SHOW_DLG, &showDlg,
                                        &bufsize));
    if (bufsize != 0 && showDlg)
    {
        if (0 == strncmp(showDlg, "TRUE", 4))
            mShowDlg = nsXInstallerDlg::SHOW_DIALOG;
        else if (0 == strncmp(showDlg, "FALSE", 5))
            mShowDlg = nsXInstallerDlg::SKIP_DIALOG;
    }

    bufsize = 0;
    XI_ERR_BAIL(aParser->GetStringAlloc(DLG_LICENSE, TITLE, &mTitle,
                                        &bufsize));
    if (bufsize == 0)
            XI_IF_FREE(mTitle); 

    aParser->GetStringAlloc(DLG_LICENSE, SUBTITLE, &mSubTitle, &bufsize);
    if (bufsize == 0)
      XI_IF_FREE(mSubTitle);

    aParser->GetStringAlloc(DLG_LICENSE, MSG0, &mMsg0, &bufsize);
    if (bufsize == 0)
      XI_IF_FREE(mMsg0);

BAIL:
    return err;
}

int
nsLicenseDlg::Show(int aDirection)
{
    int err = OK;
    char *licenseContents = NULL;
    char *titleBuf;

    XI_VERIFY(gCtx);
    XI_VERIFY(gCtx->notebook);

    if (!mWidgetsInit) {
      // static widget init

      // Create a vbox with the message and scrolled window, and append it
      // as a page of the notebook.

      mBox = gtk_vbox_new(FALSE, 6);
      gtk_container_set_border_width(GTK_CONTAINER(mBox), 12);

      gtk_notebook_append_page(GTK_NOTEBOOK(gCtx->notebook), mBox, NULL);
      mPageNum = gtk_notebook_get_current_page(GTK_NOTEBOOK(gCtx->notebook));

      // Change "\n" in the label to a space and a literal newline character.
      char *newline = strstr(mMsg0, "\\n");
      if (newline) {
        newline[0] = ' ';
        newline[1] = '\n';
      }

      GtkWidget *msg0 = gtk_label_new(NULL);
      gtk_label_set_markup(GTK_LABEL(msg0), mMsg0);
      gtk_label_set_line_wrap(GTK_LABEL(msg0), TRUE);
      gtk_misc_set_alignment(GTK_MISC(msg0), 0.0, 0.5);
      gtk_box_pack_start(GTK_BOX(mBox), msg0, FALSE, FALSE, 0);

      GtkWidget *frame = gtk_frame_new(NULL);
      //      gtk_container_set_border_width(GTK_CONTAINER(frame), 10);
      gtk_box_pack_start(GTK_BOX(mBox), frame, TRUE, TRUE, 0);

      GtkWidget *scrollWindow = gtk_scrolled_window_new(NULL, NULL);
      gtk_container_add(GTK_CONTAINER(frame), scrollWindow);

      // read the license file contents into memory
      licenseContents = GetLicenseContents();
      if (!licenseContents) {
        err = ErrorHandler(E_EMPTY_LICENSE);
        goto BAIL;
      }

      GtkTextBuffer *buffer = gtk_text_buffer_new(NULL);

      GtkTextIter start_iter;
      gtk_text_buffer_get_start_iter(buffer, &start_iter);
      gtk_text_buffer_insert_with_tags(buffer, &start_iter, licenseContents,
                                       -1, /* monoTag, */ NULL);

      GtkWidget *text = gtk_text_view_new_with_buffer(buffer);
      gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);
      gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
      gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text), 2);
      gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text), 2);
      //      gtk_text_view_set_top_margin(GTK_TEXT_VIEW(text), 2);
      //      gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(text), 2);

      gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollWindow),
                                     GTK_POLICY_AUTOMATIC,
                                     GTK_POLICY_AUTOMATIC);

      gtk_container_add(GTK_CONTAINER(scrollWindow), text);

      mWidgetsInit = TRUE;
    } else {
      gtk_notebook_set_page(GTK_NOTEBOOK(gCtx->notebook), mPageNum);
    }

    gtk_widget_show_all(mBox);

    // <b>title</b>\0
    titleBuf = new char[strlen(mTitle) + 9];
    sprintf(titleBuf, "<b>%s</b>", mTitle);

    gtk_label_set_markup(GTK_LABEL(gCtx->header_title), titleBuf);
    gtk_label_set_text(GTK_LABEL(gCtx->header_subtitle), mSubTitle);

    delete[] titleBuf;

    // Set up the header if we're moving forward.
    if (aDirection == nsXInstallerDlg::FORWARD_MOVE) {
      GtkStyle *style = gtk_widget_get_style(gCtx->header);
      if (style->bg_pixmap[GTK_STATE_NORMAL]) {
        gint width, height;
        gdk_drawable_get_size(style->bg_pixmap[GTK_STATE_NORMAL],
                              &width, &height);
        gtk_widget_set_size_request(gCtx->header, -1, height);
      }
    }

    // signal connect the buttons
    gCtx->backID = gtk_signal_connect(GTK_OBJECT(gCtx->back), "clicked",
                   GTK_SIGNAL_FUNC(nsLicenseDlg::Back), gCtx->ldlg);
    gCtx->nextID = gtk_signal_connect(GTK_OBJECT(gCtx->next), "clicked",
                   GTK_SIGNAL_FUNC(nsLicenseDlg::Next), gCtx->ldlg);

    // enable back button if we came from the welcome dlg
    if (aDirection == nsXInstallerDlg::FORWARD_MOVE)
        if (gCtx->back)
            gtk_widget_set_sensitive(gCtx->back, TRUE); 

    // always change the button titles to Accept/Decline
    gtk_button_set_label(GTK_BUTTON(gCtx->next), gCtx->Res("ACCEPT"));
    gtk_button_set_label(GTK_BUTTON(gCtx->back), gCtx->Res("DECLINE"));

BAIL:
    XI_IF_FREE(licenseContents);
    return err;
}

int
nsLicenseDlg::Hide(int aDirection)
{
    // hide all this dlg's widgets
  gtk_widget_hide(mBox);

    return OK;
}

int
nsLicenseDlg::SetLicenseFile(char *aLicenseFile)
{
    if (!aLicenseFile)
        return E_PARAM;

    mLicenseFile = aLicenseFile;

    return OK;
}

char *
nsLicenseDlg::GetLicenseFile()
{
    if (mLicenseFile)
        return mLicenseFile;

    return NULL;
}

char *
nsLicenseDlg::GetLicenseContents()
{
    char *buf = NULL;
    FILE *fd = NULL;
    struct stat attr;
    int buflen;

    DUMP(mLicenseFile);
    if (!mLicenseFile)
        return NULL;
   
    // open file
    fd = fopen(mLicenseFile, "r");
    if (!fd) return NULL;
    DUMP("license fopen");

    // get file length
    if (0 != stat(mLicenseFile, &attr)) return NULL;
    if (attr.st_size == 0) return NULL;
    DUMP("license fstat");

    // allocate buffer of file length
    buflen = sizeof(char) * (attr.st_size + 1);
    buf = (char *) malloc(buflen);
    if (!buf) return NULL;
    memset(buf, 0, buflen);
    DUMP("license buf malloc");

    // read entire file into buffer
    if (attr.st_size != ((int) fread(buf, sizeof(char), attr.st_size, fd))) 
        XI_IF_FREE(buf);
    DUMP("license fread");

    // close file
    fclose(fd);
    DUMP("license close");

    return buf;
}
