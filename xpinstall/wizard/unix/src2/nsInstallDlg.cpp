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

#include "nsSocket.h"
#include "nsInstallDlg.h"
#include "nsXInstaller.h"
#include "nsXIEngine.h"
#include <signal.h>

#define NUM_PS_ENTRIES 4

static char             *sXPInstallEngine;
static nsRunApp         *sRunAppList = NULL;

static GtkWidget        *sDLTable = NULL;
static GtkWidget        *sMsg0Label;
static GtkWidget        *sMajorLabel;
static GtkWidget        *sMinorLabel;
static GtkWidget        *sRateLabel;
static GtkWidget        *sMajorProgBar;
static GtkWidget        *sMinorProgBar;
static GtkWidget        *sPSTextEntry[NUM_PS_ENTRIES];

static int              bDownload = FALSE;
static struct timeval   sDLStartTime;

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
    if (bDownload && sDLTable)
        gtk_widget_hide(sDLTable);
    XI_GTK_UPDATE_UI();

    WorkDammitWork((void*) NULL);
    
    // run all specified applications after installation
    if (sRunAppList)
    {
        RunApps();
        FreeRunAppList();
    }

    gCtx->bMoving = TRUE;
    return;
}

int
nsInstallDlg::Parse(nsINIParser *aParser)
{
    int err = OK;
    int bufsize = 0;
    char *showDlg = NULL;
    char secName[64];
    int i;
    char *app = NULL, *args = NULL;
    nsRunApp *newRunApp = NULL;
    
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

    for (i = 0; err == OK; i++)
    {
        /* construct RunAppX section name */
        sprintf(secName, RUNAPPd, i);
        err = aParser->GetStringAlloc(secName, TARGET, &app, &bufsize);
        if (err == OK && bufsize > 0)
        {
            /* "args" is optional: this may return E_NO_KEY which we ignore */
            err = aParser->GetStringAlloc(secName, ARGS, &args, &bufsize);
            newRunApp = new nsRunApp(app, args);
            if (!newRunApp)
                return E_MEM;
            err = AppendRunApp(newRunApp);
        }
    }
    err = OK; /* reset error since RunAppX sections are optional
                 and we could have gotten a parse error (E_NO_SEC) */

    return err;

BAIL:
    return err;
}

int 
nsInstallDlg::AppendRunApp(nsRunApp *aNewRunApp)
{
    int err = OK;
    nsRunApp *currRunApp = NULL;

    /* param check */
    if (!aNewRunApp)
        return E_PARAM;

    /* special case: list is empty */
    if (!sRunAppList)
    {
        sRunAppList = aNewRunApp;
        return OK;
    }

    /* list has at least one element */
    currRunApp = sRunAppList;
    while (currRunApp)
    {
        if (!currRunApp->GetNext())
        {
            currRunApp->SetNext(aNewRunApp);
            break;
        }
    }
    return err;
}

void
nsInstallDlg::FreeRunAppList()
{
    nsRunApp *currRunApp = NULL, *nextRunApp = NULL;

    currRunApp = sRunAppList;
    while (currRunApp)
    {
        nextRunApp = currRunApp->GetNext();
        delete currRunApp;
        currRunApp = nextRunApp;
    }
}

void
nsInstallDlg::RunApps()
{
    nsRunApp *currRunApp = sRunAppList;
    char *argv[3], *dest;
    char apppath[MAXPATHLEN];
    extern char **environ; /* globally available to all processes */
    int pid;

    while (currRunApp)
    {
        /* run application with supplied args */
        if ((pid = fork()) == 0)
        {
            /* child */

            dest = gCtx->opt->mDestination;
            if (*(dest + strlen(dest)) == '/') /* trailing slash */
                sprintf(apppath, "%s%s", dest, currRunApp->GetApp());
            else                               /* no trailing slash */
                sprintf(apppath, "%s/%s", dest, currRunApp->GetApp());

            argv[0] = apppath;
            argv[1] = currRunApp->GetArgs();
            argv[2] = NULL; /* null-terminate arg vector */
            execve(apppath, argv, environ);

            /* shouldn't reach this but in case execve fails we will */
            exit(0);
        }
        /* parent continues running to finish installation */

        currRunApp = currRunApp->GetNext();
    }
}

int
nsInstallDlg::Show(int aDirection)
{
    int err = OK;
    int totalComps = 0;
    GtkWidget *hbox = NULL;
    GtkWidget *vbox = NULL;
    GtkWidget *dlFrame, *dlCheckbox, *dlProxyBtn;

    XI_VERIFY(gCtx);
    XI_VERIFY(gCtx->notebook);

    if (mWidgetsInit == FALSE)
    {
        int bCus = 
            (gCtx->opt->mSetupType == (gCtx->sdlg->GetNumSetupTypes() - 1));
        nsComponentList *comps = 
            gCtx->sdlg->GetSelectedSetupType()->GetComponents(); 

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
        gtk_table_attach(GTK_TABLE(mTable), hbox, 0, 1, 0, 1,
            static_cast<GtkAttachOptions>(GTK_FILL | GTK_EXPAND),
			GTK_FILL, 20, 20);
        gtk_widget_show(sMsg0Label);

        if (!nsXIEngine::ExistAllXPIs(bCus, comps, &totalComps))
        {
            // insert a [ x ] heterogenous table
            sDLTable = gtk_table_new(2, 2, FALSE);
            gtk_widget_show(sDLTable);

            gtk_table_attach(GTK_TABLE(mTable), sDLTable, 0, 1, 1, 4, 
                static_cast<GtkAttachOptions>(GTK_FILL | GTK_EXPAND),
    			GTK_FILL, 20, 20);
            
            // download settings groupbox
            dlFrame = gtk_frame_new(gCtx->Res("DL_SETTINGS"));
            gtk_table_attach_defaults(GTK_TABLE(sDLTable), dlFrame, 0, 2, 0, 2);
            gtk_widget_show(dlFrame);
    
            // save installer modules checkbox and label
            dlCheckbox = gtk_check_button_new_with_label(
                            gCtx->Res("SAVE_MODULES"));
            gtk_widget_show(dlCheckbox);
            gtk_table_attach(GTK_TABLE(sDLTable), dlCheckbox, 0, 2, 0, 1,
                GTK_FILL, GTK_FILL, 10, 20);
            gtk_signal_connect(GTK_OBJECT(dlCheckbox), "toggled",
                GTK_SIGNAL_FUNC(SaveModulesToggled), NULL);

            // proxy settings button
            dlProxyBtn = gtk_button_new_with_label(gCtx->Res("PROXY_SETTINGS"));
            gtk_widget_show(dlProxyBtn);
            gtk_table_attach(GTK_TABLE(sDLTable), dlProxyBtn, 0, 1, 1, 2,
                GTK_FILL, GTK_FILL, 10, 10);
            gtk_signal_connect(GTK_OBJECT(dlProxyBtn), "clicked",
                   GTK_SIGNAL_FUNC(ShowProxySettings), NULL);
        }

        // vbox with two widgets packed in: label0 / progmeter0 (major)
        vbox = gtk_vbox_new(FALSE, 0);
        hbox = gtk_hbox_new(FALSE, 0);
        sMajorLabel = gtk_label_new("");
        sRateLabel = gtk_label_new("");
        gtk_box_pack_start(GTK_BOX(hbox), sMajorLabel, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), sRateLabel, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
        gtk_widget_show(hbox);
        gtk_widget_show(sMajorLabel);
        gtk_widget_show(sRateLabel);

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
    gCtx->nextID = gtk_signal_connect(GTK_OBJECT(gCtx->next), "clicked",
                   GTK_SIGNAL_FUNC(nsInstallDlg::Next), gCtx->idlg);

    if (gCtx->opt->mSetupType != (gCtx->sdlg->GetNumSetupTypes() - 1))
    {
        // show back btn again after setup type dlg where we couldn't go back
        gtk_widget_show(gCtx->back);
    }

    // always change title of next button to "Install"
    gtk_container_remove(GTK_CONTAINER(gCtx->next), gCtx->nextLabel); 
    gCtx->installLabel = gtk_label_new(gCtx->Res("INSTALL"));
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

    // save xpis if user requested so
    if (bDownload && gCtx->opt->mSaveModules)
    {
        engine->SaveXPIs();
    }

    ShowCompleteDlg();

BAIL:
    // destroy installer engine thread object
    XI_IF_DELETE(engine);

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
#if 0
    printf("progress percent: %f\taVal: %d\taMax: %d\n", percent, aVal, aMax);
#endif
        gtk_progress_set_activity_mode(GTK_PROGRESS(sMinorProgBar), FALSE);
        gtk_progress_bar_update(GTK_PROGRESS_BAR(sMinorProgBar), percent);
        gtk_widget_show(sMinorProgBar);

        sprintf(msg, gCtx->Res("PROCESSING_FILE"), aVal, aMax);
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
            if (!bDownload)
                sprintf(msg, gCtx->Res("PREPARING"), aName);
            break;

        case ACT_EXTRACT:
            sprintf(msg, gCtx->Res("EXTRACTING"), aName);
            break;

        case ACT_INSTALL:
            sprintf(msg, gCtx->Res("INSTALLING_XPI"), aName);
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

void
nsInstallDlg::SetDownloadComp(char *aName, int aNum, int aTotal)
{
    char label[64];

    // major label format e.g., "Downloading Navigator [4/12] at 635 K/sec..."
    sprintf(label, gCtx->Res("DOWNLOADING"), aName, aNum, aTotal);
    gtk_label_set_text(GTK_LABEL(sMajorLabel), label);

    gettimeofday(&sDLStartTime, NULL);
}

#define SHOW_EVERY_N_KB 16
int
nsInstallDlg::DownloadCB(int aBytesRd, int aTotal)
{
    struct timeval now;
    char label[64];
    int rate;
    gfloat percent = 0;
    static int timesCalled = 0;
    static int activityCount = 0;

    if (++timesCalled < SHOW_EVERY_N_KB)
        return 0; 
    else
        timesCalled = 0;

    gettimeofday(&now, NULL);
    rate = (int) nsSocket::CalcRate(&sDLStartTime, &now, aBytesRd);

    // only update rate in major label line
    sprintf(label, gCtx->Res("DLRATE"), rate);
    gtk_label_set_text(GTK_LABEL(sRateLabel), label);

    if (aTotal <= 0) 
    {
        // show some activity
        if (activityCount >= 5) activityCount = 0;
        percent = (gfloat)( (gfloat)activityCount++/ (gfloat)5 ); 
        gtk_progress_set_activity_mode(GTK_PROGRESS(sMajorProgBar), TRUE); 
        gtk_progress_bar_update(GTK_PROGRESS_BAR(sMajorProgBar), percent);
    }
    else
    {
        percent = (gfloat)aBytesRd/(gfloat)aTotal;
        gtk_progress_set_activity_mode(GTK_PROGRESS(sMajorProgBar), FALSE); 
        gtk_progress_bar_update(GTK_PROGRESS_BAR(sMajorProgBar), percent);
    }

    XI_GTK_UPDATE_UI();
    return 0;
}

void
nsInstallDlg::ClearRateLabel()
{
    gtk_label_set_text(GTK_LABEL(sRateLabel), "");
    gtk_progress_set_activity_mode(GTK_PROGRESS(sMajorProgBar), FALSE); 
    XI_GTK_UPDATE_UI();
}

void
nsInstallDlg::ShowCompleteDlg()
{
    DUMP("ShowCompleteDlg");

    GtkWidget *completeDlg, *label, *okButton, *packer;

    // throw up completion notification
    completeDlg = gtk_dialog_new();
    label = gtk_label_new(gCtx->Res("COMPLETED"));
    okButton = gtk_button_new_with_label(gCtx->Res("OK_LABEL"));
    packer = gtk_packer_new();

    gtk_packer_set_default_border_width(GTK_PACKER(packer), 20);
    gtk_packer_add_defaults(GTK_PACKER(packer), label, GTK_SIDE_BOTTOM,
                            GTK_ANCHOR_CENTER, GTK_FILL_X);
    gtk_window_set_modal(GTK_WINDOW(completeDlg), TRUE);
    gtk_window_set_title(GTK_WINDOW(completeDlg), gCtx->opt->mTitle);
    gtk_window_set_position(GTK_WINDOW(completeDlg), GTK_WIN_POS_CENTER);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(completeDlg)->vbox), packer);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(completeDlg)->action_area),
                      okButton);
    gtk_signal_connect(GTK_OBJECT(okButton), "clicked",
                       GTK_SIGNAL_FUNC(CompleteOK), completeDlg);
    gtk_widget_show_all(completeDlg);
    XI_GTK_UPDATE_UI();
}

void
nsInstallDlg::CompleteOK(GtkWidget *aWidget, gpointer aData)
{
    GtkWidget *dlg = (GtkWidget *) aData;

    if (dlg)
        gtk_widget_destroy(dlg);

    gtk_main_quit();
}

void
nsInstallDlg::SaveModulesToggled(GtkWidget *aWidget, gpointer aData)
{
    if (GTK_TOGGLE_BUTTON(aWidget)->active)
    {
        DUMP("Save modules toggled on");
        gCtx->opt->mSaveModules = TRUE;
    }
    else
    {
        DUMP("Save modules toggled off");
        gCtx->opt->mSaveModules = FALSE;
    }
}

void
nsInstallDlg::ShowProxySettings(GtkWidget *aWidget, gpointer aData)
{
    GtkWidget *psDlg, *psTable, *packer;
    GtkWidget *okButton, *cancelButton;
    GtkWidget *psLabel[NUM_PS_ENTRIES];
    int i;
    char resName[16], *text;

    psDlg = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(psDlg), gCtx->opt->mTitle);
    gtk_window_set_position(GTK_WINDOW(psDlg), GTK_WIN_POS_CENTER);

    psTable = gtk_table_new(5, 2, FALSE);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(psDlg)->vbox), psTable);
    gtk_widget_show(psTable);

    // create labels
    for (i = 0; i < NUM_PS_ENTRIES; ++i) 
    {
        sprintf(resName, "PS_LABEL%d", i);
        psLabel[i] = gtk_label_new(gCtx->Res(resName));
        gtk_widget_show(psLabel[i]);

        packer = gtk_packer_new();
        gtk_packer_add_defaults(GTK_PACKER(packer), psLabel[i], GTK_SIDE_RIGHT,
            GTK_ANCHOR_CENTER, GTK_FILL_X);
        gtk_widget_show(packer);

        gtk_table_attach(GTK_TABLE(psTable), packer, 0, 1, i, i + 1,
            static_cast<GtkAttachOptions>(GTK_FILL | GTK_EXPAND),
            static_cast<GtkAttachOptions>(GTK_FILL | GTK_EXPAND), 5, 5);
    }
    
    // create text entry fields
    for (i = 0; i < NUM_PS_ENTRIES; ++i)
    {
        sPSTextEntry[i] = gtk_entry_new();
        gtk_entry_set_editable(GTK_ENTRY(sPSTextEntry[i]), TRUE);

        // reset text if we already opened this dlg before
        if (i == 0) text = gCtx->opt->mProxyHost;
        if (i == 1) text = gCtx->opt->mProxyPort;
        if (i == 2) text = gCtx->opt->mProxyUser;
        if (i == 3) text = gCtx->opt->mProxyPswd;

        if (text)
            gtk_entry_set_text(GTK_ENTRY(sPSTextEntry[i]), text);

        // password field
        if (i + 1 == NUM_PS_ENTRIES)
            gtk_entry_set_visibility(GTK_ENTRY(sPSTextEntry[i]), FALSE); 

        gtk_widget_show(sPSTextEntry[i]);

        gtk_table_attach(GTK_TABLE(psTable), sPSTextEntry[i], 
            1, 2, i, i + 1,
            static_cast<GtkAttachOptions>(GTK_FILL | GTK_EXPAND),
            static_cast<GtkAttachOptions>(GTK_FILL | GTK_EXPAND), 5, 5);
    }

    // pre-populate text entry fields if data already stored
    okButton = gtk_button_new_with_label(gCtx->Res("OK_LABEL"));
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(psDlg)->action_area),
        okButton);
    gtk_signal_connect(GTK_OBJECT(okButton), "clicked",
                   GTK_SIGNAL_FUNC(PSDlgOK), psDlg);
    gtk_widget_show(okButton);

    cancelButton = gtk_button_new_with_label(gCtx->Res("CANCEL_LABEL"));
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(psDlg)->action_area),
        cancelButton);
    gtk_signal_connect(GTK_OBJECT(cancelButton), "clicked",
                   GTK_SIGNAL_FUNC(PSDlgCancel), psDlg);
    gtk_widget_show(cancelButton);

    gtk_widget_show(psDlg);
}

void
nsInstallDlg::PSDlgOK(GtkWidget *aWidget, gpointer aData)
{
    GtkWidget *dlg = (GtkWidget *) aData;
    char *text;

    // grab proxy host field
    if (sPSTextEntry[0])
    {
        text = gtk_editable_get_chars(GTK_EDITABLE(sPSTextEntry[0]), 0, -1);
        if (text)
        {
            XI_IF_FREE(gCtx->opt->mProxyHost);
            gCtx->opt->mProxyHost = text;
        }
    }

    // grab proxy port field
    if (sPSTextEntry[1])
    {
        text = gtk_editable_get_chars(GTK_EDITABLE(sPSTextEntry[1]), 0, -1);
        if (text)
        {
            XI_IF_FREE(gCtx->opt->mProxyPort);
            gCtx->opt->mProxyPort = text;
        }
    }

    // grab proxy user field
    if (sPSTextEntry[2])
    {
        text = gtk_editable_get_chars(GTK_EDITABLE(sPSTextEntry[2]), 0, -1);
        if (text)
        {
            XI_IF_FREE(gCtx->opt->mProxyUser);
            gCtx->opt->mProxyUser = text;
        }
    }

    // grab proxy pswd field
    if (sPSTextEntry[3])
    {
        text = gtk_editable_get_chars(GTK_EDITABLE(sPSTextEntry[3]), 0, -1);
        if (text)
        {
            XI_IF_FREE(gCtx->opt->mProxyPswd);
            gCtx->opt->mProxyPswd = text;
        }
    }

    if (dlg)
        gtk_widget_destroy(dlg);
}

void
nsInstallDlg::PSDlgCancel(GtkWidget *aWidget, gpointer aData)
{
    GtkWidget *dlg = (GtkWidget *) aData;

    if (dlg)
        gtk_widget_destroy(dlg);
}
