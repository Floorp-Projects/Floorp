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

#include "nsWelcomeDlg.h"
#include "nsXInstaller.h"
#include <sys/stat.h>


nsWelcomeDlg::nsWelcomeDlg() :
    mReadmeFile(NULL)
{
}

nsWelcomeDlg::~nsWelcomeDlg()
{
    if (mReadmeFile)
        free (mReadmeFile);
}

void 
nsWelcomeDlg::Back(GtkWidget *aWidget, gpointer aData)
{
    DUMP("Back");
    if (aData != gCtx->wdlg) return;
    if (gCtx->bMoving) 
    {
        gCtx->bMoving = FALSE;
        return;
    }
}

void
nsWelcomeDlg::Next(GtkWidget *aWidget, gpointer aData)
{
    DUMP("Next");
    if (aData != gCtx->wdlg) return;
    if (gCtx->bMoving) 
    {
        gCtx->bMoving = FALSE;
        return;
    }

    // hide this notebook page
    gCtx->wdlg->Hide(nsXInstallerDlg::FORWARD_MOVE);

    // disconnect this dlg's nav btn signal handlers
    gtk_signal_disconnect(GTK_OBJECT(gCtx->back), gCtx->backID);
    gtk_signal_disconnect(GTK_OBJECT(gCtx->next), gCtx->nextID);

    // show the next dlg
    gCtx->ldlg->Show(nsXInstallerDlg::FORWARD_MOVE);
    gCtx->bMoving = TRUE;
}

int
nsWelcomeDlg::Parse(nsINIParser *aParser)
{
    int err = OK;
    int bufsize = 0;
    char *showDlg = NULL;
    
    /* compulsory keys*/
    XI_ERR_BAIL(aParser->GetStringAlloc(DLG_WELCOME, README, &mReadmeFile,
                                        &bufsize));
    if (bufsize == 0 || !mReadmeFile)
        return E_INVALID_KEY;

    /* optional keys */
    bufsize = 5;
    XI_ERR_BAIL(aParser->GetStringAlloc(DLG_WELCOME, SHOW_DLG, &showDlg,
                                        &bufsize));
    if (bufsize != 0 && showDlg)
    {
        if (0 == strncmp(showDlg, "TRUE", 4))
            mShowDlg = nsXInstallerDlg::SHOW_DIALOG;
        else if (0 == strncmp(showDlg, "FALSE", 5))
            mShowDlg = nsXInstallerDlg::SKIP_DIALOG;
    }

    bufsize = 0;
    XI_ERR_BAIL(aParser->GetStringAlloc(DLG_WELCOME, TITLE, &mTitle,
                                        &bufsize));
    if (bufsize == 0)
            XI_IF_FREE(mTitle); 

    return err;

BAIL:
    return err;
}

int
nsWelcomeDlg::Show(int aDirection)
{
    int err = 0;
    char *readmeContents = NULL;

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

        // read the readme file contents into memory
        readmeContents = GetReadmeContents();
        if (!readmeContents)
        {
            err = ErrorHandler(E_EMPTY_README);
            goto BAIL;
        }

        // create a new scrollable textarea and add it to the table
        GtkWidget *text = gtk_text_new(NULL, NULL);
        GdkFont *font = gdk_font_load( README_FONT );
        gtk_text_set_editable(GTK_TEXT(text), FALSE);
        gtk_table_attach(GTK_TABLE(mTable), text, 1, 2, 0, 1,
            static_cast<GtkAttachOptions>(GTK_FILL | GTK_EXPAND),
            static_cast<GtkAttachOptions>(GTK_FILL | GTK_EXPAND),
			0, 0);
        gtk_text_freeze(GTK_TEXT(text));
        gtk_text_insert (GTK_TEXT(text), font, &text->style->black, NULL,
                          readmeContents, -1);
        gtk_text_thaw(GTK_TEXT(text));
        gtk_text_set_word_wrap(GTK_TEXT(text), TRUE);
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
                   GTK_SIGNAL_FUNC(nsWelcomeDlg::Back), gCtx->wdlg);
    gCtx->nextID = gtk_signal_connect(GTK_OBJECT(gCtx->next), "clicked",
                   GTK_SIGNAL_FUNC(nsWelcomeDlg::Next), gCtx->wdlg);

    if (gCtx->back)
        gtk_widget_hide(gCtx->back);

    GTK_WIDGET_SET_FLAGS(gCtx->next, GTK_CAN_DEFAULT);
    gtk_widget_grab_default(gCtx->next);

    if (aDirection == nsXInstallerDlg::BACKWARD_MOVE)
    {
        // change the button titles back to Back/Next
        gtk_container_remove(GTK_CONTAINER(gCtx->next), gCtx->acceptLabel);
        gtk_container_remove(GTK_CONTAINER(gCtx->back), gCtx->declineLabel);
        gCtx->nextLabel = gtk_label_new(gCtx->Res("NEXT"));
        gCtx->backLabel = gtk_label_new(gCtx->Res("BACK"));
        gtk_container_add(GTK_CONTAINER(gCtx->next), gCtx->nextLabel);
        gtk_container_add(GTK_CONTAINER(gCtx->back), gCtx->backLabel);
        gtk_widget_show(gCtx->nextLabel);
        gtk_widget_show(gCtx->backLabel);
        gtk_widget_show(gCtx->next);
        gtk_widget_show(gCtx->back);
    }

BAIL:
    XI_IF_FREE(readmeContents);

    return err;
}

int 
nsWelcomeDlg::Hide(int aDirection)
{
    // hide all this dlg's widgets
    gtk_widget_hide(mTable);

    return OK;
}

int
nsWelcomeDlg::SetReadmeFile(char *aReadmeFile)
{
    if (!aReadmeFile)
        return E_PARAM;

    mReadmeFile = aReadmeFile;

    return OK;
}

char *
nsWelcomeDlg::GetReadmeFile()
{
    if (mReadmeFile)
        return mReadmeFile;

    return NULL;
}

char *
nsWelcomeDlg::GetReadmeContents()
{
    char *buf = NULL;
    FILE *fd = NULL;
    struct stat attr;
    int buflen;

    DUMP(mReadmeFile);
    if (!mReadmeFile)
        return NULL;
   
    // open file
    fd = fopen(mReadmeFile, "r");
    if (!fd) return NULL;
    DUMP("readme fopen");

    // get file length
    if (0 != stat(mReadmeFile, &attr)) return NULL;
    if (attr.st_size == 0) return NULL;
    DUMP("readme fstat");

    // allocate buffer of file length
    buflen = sizeof(char) * (attr.st_size + 1);
    buf = (char *) malloc(buflen);
    if (!buf) return NULL;
    memset(buf, 0, buflen);
    DUMP("readme buf malloc");

    // read entire file into buffer
    if (attr.st_size != ((int) fread(buf, sizeof(char), attr.st_size, fd))) 
        XI_IF_FREE(buf);
    DUMP("readme fread");

    // close file
    fclose(fd);
    DUMP("readme close");

    return buf;
}
