/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Samir Gehani <sgehani@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsLicenseDlg.h"
#include "nsXInstaller.h"
#include <sys/stat.h>

nsLicenseDlg::nsLicenseDlg() :
    mLicenseFile(NULL)
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
    if (gCtx->bMoving) 
    {
        gCtx->bMoving = FALSE;
        return;
    }
    
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
    gCtx->bMoving = TRUE;
#endif
}

void
nsLicenseDlg::Next(GtkWidget *aWidget, gpointer aData)
{
    DUMP("Next");
    if (aData != gCtx->ldlg) return;
    if (gCtx->bMoving) 
    {
        gCtx->bMoving = FALSE;
        return;
    }

    // hide this notebook page
    gCtx->ldlg->Hide(nsXInstallerDlg::FORWARD_MOVE);

    // disconnect this dlg's nav btn signal handlers
    gtk_signal_disconnect(GTK_OBJECT(gCtx->back), gCtx->backID);
    gtk_signal_disconnect(GTK_OBJECT(gCtx->next), gCtx->nextID);

    // show the next dlg
    gCtx->sdlg->Show(nsXInstallerDlg::FORWARD_MOVE);
    gCtx->bMoving = TRUE;
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

    /* optional keys */
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

    return err;

BAIL:
    return err;
}

int
nsLicenseDlg::Show(int aDirection)
{
    int err = OK;
    char *licenseContents = NULL;

    XI_VERIFY(gCtx);
    XI_VERIFY(gCtx->notebook);

    if (mWidgetsInit == FALSE) // static widget init
    {
        // create a new table and add it as a page of the notebook
        mTable = gtk_table_new(1, 3, FALSE);
        gtk_notebook_append_page(GTK_NOTEBOOK(gCtx->notebook), mTable, NULL);
        mPageNum = gtk_notebook_get_current_page(GTK_NOTEBOOK(gCtx->notebook));
        // gtk_table_set_row_spacing(GTK_TABLE(mTable), 0, 0);
        gtk_table_set_col_spacing(GTK_TABLE(mTable), 1, 0);
        gtk_widget_show(mTable);

        // read the license file contents into memory
        licenseContents = GetLicenseContents();
        if (!licenseContents)
        {
            err = ErrorHandler(E_EMPTY_LICENSE);
            goto BAIL;
        }

        // create a new scrollable textarea and add it to the table
        GtkWidget *text = gtk_text_new(NULL, NULL);
        GdkFont *font = gdk_font_load( LICENSE_FONT );
        gtk_text_set_editable(GTK_TEXT(text), FALSE);
        gtk_text_set_word_wrap(GTK_TEXT(text), TRUE);
        gtk_text_set_line_wrap(GTK_TEXT(text), TRUE);
        gtk_table_attach(GTK_TABLE(mTable), text, 1, 2, 0, 1,
            static_cast<GtkAttachOptions>(GTK_FILL | GTK_EXPAND),
            static_cast<GtkAttachOptions>(GTK_FILL | GTK_EXPAND),
			0, 0);
        gtk_text_freeze(GTK_TEXT(text));
        gtk_text_insert (GTK_TEXT(text), font, &text->style->black, NULL,
                          licenseContents, -1);
        gtk_text_thaw(GTK_TEXT(text));
        gtk_widget_show(text);

        // Add a vertical scrollbar to the GtkText widget 
        GtkWidget *vscrollbar = gtk_vscrollbar_new (GTK_TEXT (text)->vadj);
        gtk_table_attach(GTK_TABLE(mTable), vscrollbar, 2, 3, 0, 1,
            GTK_FILL,
			static_cast<GtkAttachOptions>(GTK_EXPAND | GTK_SHRINK | GTK_FILL),
			0, 0);
        gtk_widget_show(vscrollbar);

        mWidgetsInit = TRUE;
    }
    else
    {
        gtk_notebook_set_page(GTK_NOTEBOOK(gCtx->notebook), mPageNum);
        gtk_widget_show(mTable);
    }

    // signal connect the buttons
    gCtx->backID = gtk_signal_connect(GTK_OBJECT(gCtx->back), "clicked",
                   GTK_SIGNAL_FUNC(nsLicenseDlg::Back), gCtx->ldlg);
    gCtx->nextID = gtk_signal_connect(GTK_OBJECT(gCtx->next), "clicked",
                   GTK_SIGNAL_FUNC(nsLicenseDlg::Next), gCtx->ldlg);

    // show back button if we came from the welcome dlg
    if (aDirection == nsXInstallerDlg::FORWARD_MOVE)
        if (gCtx->back)
            gtk_widget_show(gCtx->back); 

    // always change the button titles to Accept/Decline
    gtk_container_remove(GTK_CONTAINER(gCtx->next), gCtx->nextLabel);
    gtk_container_remove(GTK_CONTAINER(gCtx->back), gCtx->backLabel);
    gCtx->acceptLabel = gtk_label_new(gCtx->Res("ACCEPT"));
    gCtx->declineLabel = gtk_label_new(gCtx->Res("DECLINE"));
    gtk_container_add(GTK_CONTAINER(gCtx->next), gCtx->acceptLabel);
    gtk_container_add(GTK_CONTAINER(gCtx->back), gCtx->declineLabel);
    gtk_widget_show(gCtx->acceptLabel);
    gtk_widget_show(gCtx->declineLabel);
    gtk_widget_show(gCtx->next);
    gtk_widget_show(gCtx->back);

BAIL:
    XI_IF_FREE(licenseContents);
    return err;
}

int
nsLicenseDlg::Hide(int aDirection)
{
    // hide all this dlg's widgets
    gtk_widget_hide(mTable);

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
