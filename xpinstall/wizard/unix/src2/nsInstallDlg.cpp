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

#include "nsInstallDlg.h"
#include "nsXInstaller.h"
#include "nsXIEngine.h"
#include <signal.h>

static char         *sXPInstallEngine;
static GtkWidget    *sMsg0Label;
static GtkWidget    *sMajorLabel;
static GtkWidget    *sMinorLabel;
static GtkWidget    *sMajorProgBar;
static GtkWidget    *sMinorProgBar;
static int          bDownload = FALSE;

nsInstallDlg::nsInstallDlg() :
    mMsg0(NULL)
{
}

nsInstallDlg::~nsInstallDlg()
{
    XI_IF_FREE(mMsg0);  
}

void
nsInstallDlg::Back(GtkWidget *aWidget, gpointer aData)
{
    DUMP("Back");
    if (aData != gCtx->idlg) return;
    if (gCtx->bMoving)
    {
        gCtx->bMoving = FALSE;
        return;
    }

    // hide this notebook page
    gCtx->idlg->Hide(nsXInstallerDlg::BACKWARD_MOVE);

    // disconnect this dlg's nav btn signal handlers
    gtk_signal_disconnect(GTK_OBJECT(gCtx->back), gCtx->backID);
    gtk_signal_disconnect(GTK_OBJECT(gCtx->next), gCtx->nextID);

    // show the last dlg
    if (gCtx->opt->mSetupType == (gCtx->sdlg->GetNumSetupTypes() - 1))
        gCtx->cdlg->Show(nsXInstallerDlg::BACKWARD_MOVE);
    else
        gCtx->sdlg->Show(nsXInstallerDlg::BACKWARD_MOVE);
    gCtx->bMoving = TRUE;
}

void
nsInstallDlg::Next(GtkWidget *aWidget, gpointer aData)
{
    DUMP("Next");
    int bCus;
    nsComponentList *comps = NULL;

    if (aData != gCtx->idlg) return;
    if (gCtx->bMoving)
    {
        gCtx->bMoving = FALSE;
        return;
    }

    bCus = (gCtx->opt->mSetupType == (gCtx->sdlg->GetNumSetupTypes() - 1));
    comps = gCtx->sdlg->GetSelectedSetupType()->GetComponents();

    // hide the cancel button
    if (gCtx->cancel)
        gtk_widget_hide(gCtx->cancel);

    // initialize progress bar cleanly
    int totalComps = 0;
    if (nsXIEngine::ExistAllXPIs(bCus, comps, &totalComps))
        bDownload = FALSE;
    else
        bDownload = TRUE;

    gtk_progress_set_activity_mode(GTK_PROGRESS(sMajorProgBar), FALSE);
    gtk_progress_bar_update(GTK_PROGRESS_BAR(sMajorProgBar), (gfloat) 0);
    gtk_label_set_text(GTK_LABEL(sMajorLabel), "");
    gtk_widget_show(sMajorLabel);
    gtk_widget_show(sMajorProgBar);

    gtk_widget_hide(gCtx->back);
    gtk_widget_hide(gCtx->next);
    gtk_widget_hide(sMsg0Label);
    XI_GTK_UPDATE_UI();

    WorkDammitWork((void*) NULL);

    gCtx->bMoving = TRUE;
    return;
}

int
nsInstallDlg::Parse(nsINIParser *aParser)
{
    int err = OK;
    int bufsize = 0;
    char *showDlg = NULL;
    
    /* compulsory keys*/
    XI_ERR_BAIL(aParser->GetStringAlloc(DLG_START_INSTALL, 
                XPINSTALL_ENGINE, &sXPInstallEngine, &bufsize));
    if (bufsize == 0 || !sXPInstallEngine)
        return E_INVALID_KEY;

    /* optional keys */
    bufsize = 0;
    err = aParser->GetStringAlloc(DLG_START_INSTALL, MSG0, &mMsg0, &bufsize);
    if (err != OK && err != nsINIParser::E_NO_KEY) goto BAIL; else err = OK;

    bufsize = 5;
    XI_ERR_BAIL(aParser->GetStringAlloc(DLG_START_INSTALL, SHOW_DLG, 
                &showDlg, &bufsize));
    if (bufsize != 0 && showDlg)
    {
        if (0 == strncmp(showDlg, "TRUE", 4))
            mShowDlg = nsXInstallerDlg::SHOW_DIALOG;
        else if (0 == strncmp(showDlg, "FALSE", 5))
            mShowDlg = nsXInstallerDlg::SKIP_DIALOG;
    }

    bufsize = 0;
    XI_ERR_BAIL(aParser->GetStringAlloc(DLG_START_INSTALL, TITLE, 
                &mTitle, &bufsize));
    if (bufsize == 0)
            XI_IF_FREE(mTitle); 

    return err;

BAIL:
    return err;
}

int
nsInstallDlg::Show(int aDirection)
{
    int err = OK;
    GtkWidget *hbox = NULL;
    GtkWidget *vbox = NULL;

    XI_VERIFY(gCtx);
    XI_VERIFY(gCtx->notebook);

    if (mWidgetsInit == FALSE)
    {
        // create a new table and add it as a page of the notebook
        mTable = gtk_table_new(4, 1, FALSE);
        gtk_notebook_append_page(GTK_NOTEBOOK(gCtx->notebook), mTable, NULL);
        mPageNum = gtk_notebook_get_current_page(GTK_NOTEBOOK(gCtx->notebook));
        gtk_widget_show(mTable);

        // insert a static text widget in the first row
        sMsg0Label = gtk_label_new(mMsg0);
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), sMsg0Label, FALSE, FALSE, 0);
        gtk_widget_show(hbox);
        gtk_table_attach(GTK_TABLE(mTable), hbox, 0, 1, 1, 2,
            static_cast<GtkAttachOptions>(GTK_FILL | GTK_EXPAND),
			GTK_FILL, 20, 20);
        gtk_widget_show(sMsg0Label);

        // vbox with two widgets packed in: label0 / progmeter0 (major)
        vbox = gtk_vbox_new(FALSE, 0);
        hbox = gtk_hbox_new(FALSE, 0);
        sMajorLabel = gtk_label_new("");
        gtk_box_pack_start(GTK_BOX(hbox), sMajorLabel, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
        gtk_widget_show(hbox);
        gtk_widget_show(sMajorLabel);

        sMajorProgBar = gtk_progress_bar_new();
        gtk_box_pack_start(GTK_BOX(vbox), sMajorProgBar, FALSE, FALSE, 0);
        // gtk_widget_show(sMajorProgBar);

        gtk_table_attach(GTK_TABLE(mTable), vbox, 0, 1, 2, 3, 
            static_cast<GtkAttachOptions>(GTK_FILL | GTK_EXPAND),
			GTK_FILL, 20, 20);
        gtk_widget_show(vbox); 

        // vbox with two widgets packed in: label1 / progmeter1 (minor)
        vbox = gtk_vbox_new(FALSE, 0);
        hbox = gtk_hbox_new(FALSE, 0);
        sMinorLabel = gtk_label_new("");
        gtk_box_pack_start(GTK_BOX(hbox), sMinorLabel, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
        gtk_widget_show(hbox);
        gtk_widget_show(sMinorLabel);

        sMinorProgBar = gtk_progress_bar_new();
        gtk_box_pack_start(GTK_BOX(vbox), sMinorProgBar, FALSE, FALSE, 0);
        // gtk_widget_show(sMinorProgBar); 

        gtk_table_attach(GTK_TABLE(mTable), vbox, 0, 1, 3, 4, 
            static_cast<GtkAttachOptions>(GTK_FILL | GTK_EXPAND),
			GTK_FILL, 20, 20);
        gtk_widget_show(vbox); 
        
        mWidgetsInit = TRUE;
    }
    else
    {
        gtk_notebook_set_page(GTK_NOTEBOOK(gCtx->notebook), mPageNum);
        gtk_widget_show(mTable);
    }

    // signal connect the buttons
    gCtx->backID = gtk_signal_connect(GTK_OBJECT(gCtx->back), "clicked",
                   GTK_SIGNAL_FUNC(nsInstallDlg::Back), gCtx->idlg);
    gCtx->nextID = gtk_signal_connect(GTK_OBJECT(gCtx->next), "released",
                   GTK_SIGNAL_FUNC(nsInstallDlg::Next), gCtx->idlg);

    if (gCtx->opt->mSetupType != (gCtx->sdlg->GetNumSetupTypes() - 1))
    {
        // show back btn again after setup type dlg where we couldn't go back
        gtk_widget_show(gCtx->back);
    }

    // always change title of next button to "Install"
    gtk_container_remove(GTK_CONTAINER(gCtx->next), gCtx->nextLabel); 
    gCtx->installLabel = gtk_label_new(INSTALL);
    gtk_container_add(GTK_CONTAINER(gCtx->next), gCtx->installLabel);
    gtk_widget_show(gCtx->installLabel);
    gtk_widget_show(gCtx->next);

    return err;
}

int
nsInstallDlg::Hide(int aDirection)
{
    gtk_widget_hide(mTable);

    return OK;
}

int
nsInstallDlg::SetMsg0(char *aMsg)
{
    if (!aMsg)
        return E_PARAM;

    mMsg0 = aMsg;

    return OK;
}

char *
nsInstallDlg::GetMsg0()
{
    if (mMsg0)
        return mMsg0;

    return NULL;
}

void *
nsInstallDlg::WorkDammitWork(void *arg)
{
    DUMP("WorkDammitWork");

    int err = OK;

    // perform the installation
    nsXIEngine *engine = new nsXIEngine();
    if (!engine)
    {
        ErrorHandler(E_MEM);
        return NULL;
    }

    // get the component list for the current setup type
    nsComponentList *comps = NULL;
    nsComponent *xpiengine = NULL;
    int bCus = (gCtx->opt->mSetupType == (gCtx->sdlg->GetNumSetupTypes() - 1));
    comps = gCtx->sdlg->GetSelectedSetupType()->GetComponents(); 
    if (!comps)
    {
        ErrorHandler(E_NO_COMPONENTS);
        return NULL;
    }
    
    if (!sXPInstallEngine) return NULL;
    xpiengine = comps->GetCompByArchive(sXPInstallEngine);

    // 1> download
    XI_ERR_BAIL(engine->Download(bCus, comps));

    // 2> extract engine
    XI_ERR_BAIL(engine->Extract(xpiengine));
    
    // 3> install .xpis
    XI_ERR_BAIL(engine->Install(bCus, comps, gCtx->opt->mDestination));

BAIL:
    // destroy installer engine thread object
    XI_IF_DELETE(engine);
    gtk_main_quit();

    return NULL;
}

void
nsInstallDlg::XPIProgressCB(const char *aMsg, int aVal, int aMax)
{
    // DUMP("XPIProgressCB");

    if (!aMsg)
        return;

    static int updates = 0;
    char msg[64];
    const char *colon = NULL, *lastSlash = NULL;

    if (aMax > 0)
    {
        // reset for next component
        if (updates)
            updates = 0;

        gfloat percent = (gfloat)((gfloat)aVal/(gfloat)aMax);
#ifdef DEBUG
    printf("progress percent: %f\taVal: %d\taMax: %d\n", percent, aVal, aMax);
#endif
        gtk_progress_set_activity_mode(GTK_PROGRESS(sMinorProgBar), FALSE);
        gtk_progress_bar_update(GTK_PROGRESS_BAR(sMinorProgBar), percent);
        gtk_widget_show(sMinorProgBar);

        sprintf(msg, PROCESSING_FILE, aVal, aMax);
    }
    else
    {
        updates++;
        if (updates > 5)
            updates = 0;
        gfloat percent = (gfloat)((gfloat)updates/(gfloat)5);

        gtk_progress_set_activity_mode(GTK_PROGRESS(sMinorProgBar), TRUE);
        gtk_progress_bar_update(GTK_PROGRESS_BAR(sMinorProgBar), percent);
        gtk_widget_show(sMinorProgBar);

        /* tack on XPInstall action */
        memset(msg, 0, 64);
        colon = strchr(aMsg, ':');
        if (colon)
            strncpy(msg, aMsg, colon - aMsg);

        strncat(msg, " ", 1);

        /* tack on leaf name */
        lastSlash = strrchr(aMsg, '/');
        if (lastSlash)
            strncat(msg, lastSlash + 1, strlen(lastSlash) - 1);

        strncat(msg, "\0", 1);
    }

    gtk_label_set_text(GTK_LABEL(sMinorLabel), msg);
    gtk_widget_draw(sMinorLabel, NULL);

    XI_GTK_UPDATE_UI();
}

void
nsInstallDlg::MajorProgressCB(char *aName, int aNum, int aTotal, int aActivity)
{
    // DUMP("MajorProgressCB");

    char msg[256];

    if (!aName)
        return;

#ifdef DEBUG
    printf("%s %d: Name = %s\tNum = %d\tTotal = %d\tAct = %d\n", 
    __FILE__, __LINE__, aName, aNum, aTotal, aActivity);
#endif

    switch (aActivity)
    {
        case ACT_DOWNLOAD:
            if (bDownload)
                sprintf(msg, DOWNLOADING, aName);
            else
                sprintf(msg, PREPARING, aName);
            break;

        case ACT_EXTRACT:
            sprintf(msg, EXTRACTING, aName);
            break;

        case ACT_INSTALL:
            sprintf(msg, INSTALLING_XPI, aName);
            break;

        default:
            break;
    }

    gtk_label_set_text(GTK_LABEL(sMajorLabel), msg);
    gtk_widget_show(sMajorLabel);

    if (aTotal <= 0)
    {
        XI_ASSERT(0, "aTotal was <= 0");
        XI_GTK_UPDATE_UI();
        return;
    }

    gfloat percent = (gfloat)((gfloat)aNum/(gfloat)aTotal);
    gtk_progress_bar_update(GTK_PROGRESS_BAR(sMajorProgBar), percent);
    gtk_widget_show(sMajorProgBar);

    // reset minor progress ui
    if (aActivity == ACT_INSTALL)
    {
        gtk_label_set_text(GTK_LABEL(sMinorLabel), "");
        gtk_progress_bar_update(GTK_PROGRESS_BAR(sMinorProgBar), (gfloat)0);
        gtk_widget_show(sMinorLabel);
        gtk_widget_show(sMinorProgBar);
    }

    XI_GTK_UPDATE_UI();
}
