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

#include "nscore.h"
#include "nsSetupTypeDlg.h"
#include "nsXInstaller.h"

#include <sys/types.h>
#include <dirent.h>

// need these for statfs 

#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif

#ifdef HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif

#ifdef HAVE_STATVFS
#define STATFS statvfs
#else
#define STATFS statfs
#endif

#include <sys/wait.h>


static GtkWidget        *sBrowseBtn;
static gint             sBrowseBtnID;
static GtkWidget        *sFolder;
static GSList           *sGroup;
static GtkWidget        *sCreateDestDlg;
static GtkWidget        *sDelInstDlg;
static nsLegacyCheck    *sLegacyChecks = NULL;
static nsObjectIgnore   *sObjectsToIgnore = NULL;

nsSetupTypeDlg::nsSetupTypeDlg() :
    mMsg0(NULL),
    mSetupTypeList(NULL)
{
}

nsSetupTypeDlg::~nsSetupTypeDlg()
{
    FreeSetupTypeList();
    FreeLegacyChecks();

    XI_IF_FREE(mMsg0)
}

void 
nsSetupTypeDlg::Next(GtkWidget *aWidget, gpointer aData)
{
    DUMP("Next");
    if (aData && aData != gCtx->sdlg) return;
    if (gCtx->bMoving)
    {
        gCtx->bMoving = FALSE;
        return;
    }

    // verify selected destination directory exists
    if (OK != nsSetupTypeDlg::VerifyDestination())
        return;

    // old installation may exist: delete it
    if (OK != nsSetupTypeDlg::DeleteOldInst())
        return;

    // make sure destination directory is empty
    if (OK != nsSetupTypeDlg::CheckDestEmpty())
        return;

    // if not custom setup type verify disk space
    if (gCtx->opt->mSetupType != (gCtx->sdlg->GetNumSetupTypes() - 1))
    {
        if (OK != nsSetupTypeDlg::VerifyDiskSpace())
            return;
    }

    if (gCtx->opt->mMode == nsXIOptions::MODE_DEFAULT)
    {
        // hide this notebook page
        gCtx->sdlg->Hide();
    }

    // show the last dlg
    if (gCtx->opt->mSetupType == (gCtx->sdlg->GetNumSetupTypes() - 1))
    {
        gCtx->cdlg->Show();
    }
    else
    {
        if (gCtx->opt->mMode != nsXIOptions::MODE_SILENT)
        {
            gCtx->idlg->Show();
        }
        if (gCtx->opt->mMode != nsXIOptions::MODE_DEFAULT)
        {
            gCtx->idlg->Next((GtkWidget *)NULL, (gpointer) gCtx->idlg);
        }
    }

    // When Next() is not invoked from a signal handler, the caller passes
    // aData as NULL so we know not to do the bMoving hack. 
    if (aData)
    {
        gCtx->bMoving = TRUE;
    }
}

int
nsSetupTypeDlg::Parse(nsINIParser *aParser)
{
    int err = OK;
    int bufsize = 0;
    char *showDlg = NULL;
    int i, j;
    char *defSec = NULL; // default Setup Type
    char *currSec = (char *) malloc(strlen(SETUP_TYPEd) + 1); // e.g. SetupType12
    if (!currSec) return E_MEM;
    char *currKey = (char *) malloc(1 + 3); // e.g. C0, C1, C12
    if (!currKey) return E_MEM;
    char *currOIKey = (char *) malloc(strlen(OBJECT_IGNOREd) + 1);
    if (!currOIKey) return E_MEM;
    char *currLCSec = (char *) malloc(strlen(LEGACY_CHECKd) + 1);
    if (!currLCSec) return E_MEM;
    char *currVal = NULL;
    nsObjectIgnore *currOI = NULL, *nextOI = NULL;
    char *currFile = NULL, *currMsg = NULL;
    nsLegacyCheck *currLC = NULL, *nextLC = NULL;
    nsComponent *currComp = NULL;
    nsComponent *currCompDup = NULL;
    int currIndex;
    int currNumComps = 0;

    nsComponentList *compList = gCtx->cdlg->GetCompList();
    DUMP("Pre-verification of comp list")
    XI_VERIFY(compList);
    DUMP("Post-verification of comp list")

    nsSetupType *currST = NULL;
    char *currDescShort = NULL;
    char *currDescLong = NULL;

    XI_VERIFY(gCtx);

    /* optional keys */
    err = aParser->GetStringAlloc(GENERAL, DEFAULT_SETUP_TYPE, &defSec, &bufsize);
    if (err != OK && err != nsINIParser::E_NO_KEY) goto BAIL; else err = OK;

    bufsize = 0;
    err = aParser->GetStringAlloc(DLG_SETUP_TYPE, MSG0, &mMsg0, &bufsize);
    if (err != OK && err != nsINIParser::E_NO_KEY) goto BAIL; else err = OK;

    bufsize = 5;
    err = aParser->GetStringAlloc(DLG_SETUP_TYPE, SHOW_DLG, &showDlg, &bufsize);
    if (err != OK && err != nsINIParser::E_NO_KEY) goto BAIL; else err = OK;
    if (bufsize != 0 && showDlg)
    {
        if (0 == strncmp(showDlg, "TRUE", 4))
            mShowDlg = nsXInstallerDlg::SHOW_DIALOG;
        else if (0 == strncmp(showDlg, "FALSE", 5))
            mShowDlg = nsXInstallerDlg::SKIP_DIALOG;
    }

    bufsize = 0;
    err = aParser->GetStringAlloc(DLG_SETUP_TYPE, TITLE, &mTitle, &bufsize);
    if (err != OK && err != nsINIParser::E_NO_KEY) goto BAIL; else err = OK;
    if (bufsize == 0)
            XI_IF_FREE(mTitle); 

    /* Objects to Ignore */
    for (i = 0; i < MAX_LEGACY_CHECKS; i++)
    {
        // construct key name based on index
        memset(currOIKey, 0, (strlen(OBJECT_IGNOREd) + 1));
        sprintf(currOIKey, OBJECT_IGNOREd, i);

        // get ObjectToIgnore key
        bufsize = 0;
        err = aParser->GetStringAlloc(CLEAN_UPGRADE, currOIKey, &currFile, &bufsize);
        if (err != OK) 
        { 
            if (err != nsINIParser::E_NO_SEC &&
                err != nsINIParser::E_NO_KEY) goto BAIL; 
            else 
            {
                err = OK; 
                break; 
            } 
        }
        nextOI = new nsObjectIgnore(currFile);

        if (currOI)
        {
           currOI->SetNext(nextOI);
        }
        else if (!sObjectsToIgnore)
        {
           sObjectsToIgnore = nextOI;
        }

        currOI = nextOI;
    }

    /* legacy check */
    for (i = 0; i < MAX_LEGACY_CHECKS; i++)
    {
        // construct section name based on index
        memset(currLCSec, 0, (strlen(LEGACY_CHECKd) + 1));
        sprintf(currLCSec, LEGACY_CHECKd, i);

        // get "Filename" and "Message" keys
        bufsize = 0;
        err = aParser->GetStringAlloc(currLCSec, FILENAME, &currFile, &bufsize);
        if (err != OK) 
        { 
            if (err != nsINIParser::E_NO_SEC &&
                err != nsINIParser::E_NO_KEY) goto BAIL; 
            else 
            {
                err = OK; 
                break; 
            } 
        }

        bufsize = 0;
        err = aParser->GetStringAlloc(currLCSec, MSG, &currMsg, &bufsize);
        if (err != OK)
        {
            if (err != nsINIParser::E_NO_SEC &&
                err != nsINIParser::E_NO_KEY) goto BAIL; 
            else 
            {
                err = OK; 
                break; 
            } 
        }
        nextLC = new nsLegacyCheck(currFile, currMsg);

        if (currLC)
        {
           currLC->SetNext(nextLC);
        }
        else if (!sLegacyChecks)
        {
           sLegacyChecks = nextLC;
        }

        currLC = nextLC;
    }

    /* setup types */
    gCtx->opt->mSetupType = 0;
    for (i=0; i<MAX_SETUP_TYPES; i++)
    {
        sprintf(currSec, SETUP_TYPEd, i);

        bufsize = 0;
        err = aParser->GetStringAlloc(currSec, DESC_SHORT, &currDescShort,
                                      &bufsize);
        if (err != OK && err != nsINIParser::E_NO_SEC) goto fin_iter;
        if (bufsize == 0 || err == nsINIParser::E_NO_SEC) // no more setup types
        {
            err = OK;
            break;
        }

        if (defSec && strcasecmp(currDescShort, defSec) == 0)
            gCtx->opt->mSetupType = i;

        bufsize = 0;
        err = aParser->GetStringAlloc(currSec, DESC_LONG, &currDescLong,
                                      &bufsize);
        if (err != OK || bufsize == 0) goto fin_iter;

        currST = new nsSetupType();
        if (!currST) goto fin_iter;

        currST->SetDescShort(currDescShort);
        currST->SetDescLong(currDescLong);

        currNumComps = 0;
        for (j=0; j<MAX_COMPONENTS; j++)
        {
            sprintf(currKey, Cd, j);
            
            bufsize = 0;
            err = aParser->GetStringAlloc(currSec, currKey, &currVal, 
                                          &bufsize);
            if (err != OK && err != nsINIParser::E_NO_KEY) continue;
            if (bufsize == 0 || err == nsINIParser::E_NO_KEY) 
            {
                err = OK;
                break;
            }
        
            currComp = NULL;
            currIndex = atoi(currVal + strlen(COMPONENT));
            currComp = compList->GetCompByIndex(currIndex);
            if (currComp)
            {
                // preserve next ptr
                currCompDup = currComp->Duplicate(); 
                currST->SetComponent(currCompDup);
                currNumComps++;
            }
        }
        if (currNumComps > 0)
        {
            AddSetupType(currST);
            currST = NULL;
        }

fin_iter:
        XI_IF_DELETE(currST);
    }

    err = OK;

BAIL:
    XI_IF_FREE(currSec);
    XI_IF_FREE(currKey);
    XI_IF_FREE(currOIKey);
    XI_IF_FREE(currLCSec);

    return err;
}

int
nsSetupTypeDlg::Show()
{
    int err = OK;
    int numSetupTypes = 0;
    int i;
    GtkWidget *stTable = NULL;
    GtkWidget *radbtns[MAX_SETUP_TYPES];
    GtkWidget *desc[MAX_SETUP_TYPES];
    nsSetupType *currST = NULL;
    GtkWidget *destTable = NULL;
    GtkWidget *frame = NULL;
    GtkWidget *hbox = NULL;

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
        GtkWidget *msg0 = gtk_label_new(mMsg0);
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), msg0, FALSE, FALSE, 0);
        gtk_widget_show(hbox);
        gtk_table_attach(GTK_TABLE(mTable), hbox, 0, 1, 1, 2,
            static_cast<GtkAttachOptions>(GTK_FILL | GTK_EXPAND),
            GTK_FILL, 20, 20);
        gtk_widget_show(msg0);

        // insert a [n x 2] heterogeneous table in the second row
        // where n = numSetupTypes
        numSetupTypes = GetNumSetupTypes();
        stTable = gtk_table_new(numSetupTypes, 4, FALSE);
        gtk_widget_show(stTable);
        gtk_table_attach(GTK_TABLE(mTable), stTable, 0, 1, 2, 3,
            static_cast<GtkAttachOptions>(GTK_EXPAND | GTK_FILL),
            static_cast<GtkAttachOptions>(GTK_EXPAND | GTK_FILL),
            20, 0);

        currST = GetSetupTypeList();
        if (!currST) return E_NO_SETUPTYPES;

        sGroup=NULL;

        // radio buttons
        for (i = 0; i < numSetupTypes; i++)
        {
            radbtns[i] = gtk_radio_button_new_with_label(sGroup,
                            currST->GetDescShort());
            sGroup = gtk_radio_button_group(GTK_RADIO_BUTTON(radbtns[i]));
            gtk_table_attach(GTK_TABLE(stTable), radbtns[i], 0, 1, i, i+1,
                static_cast<GtkAttachOptions>(GTK_FILL | GTK_EXPAND),
                static_cast<GtkAttachOptions>(GTK_FILL | GTK_EXPAND), 0, 0);
            gtk_signal_connect(GTK_OBJECT(radbtns[i]), "toggled",
                               GTK_SIGNAL_FUNC(RadBtnToggled),
                               reinterpret_cast<void *>(i));
            gtk_widget_show(radbtns[i]);

            desc[i] = gtk_label_new(currST->GetDescLong());
            gtk_label_set_justify(GTK_LABEL(desc[i]), GTK_JUSTIFY_LEFT);
            gtk_label_set_line_wrap(GTK_LABEL(desc[i]), TRUE);
            hbox = gtk_hbox_new(FALSE, 0);
            gtk_box_pack_start(GTK_BOX(hbox), desc[i], FALSE, FALSE, 0);
            gtk_widget_show(hbox);
            gtk_table_attach_defaults(GTK_TABLE(stTable), hbox, 1, 2, i, i+1);
            gtk_widget_show(desc[i]);

            currST = currST->GetNext();
        }

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
                                        radbtns[gCtx->opt->mSetupType]), TRUE);

        // insert a [1 x 2] heterogeneous table in the third row
        destTable = gtk_table_new(1, 2, FALSE);
        gtk_widget_show(destTable); 

        gtk_table_attach(GTK_TABLE(mTable), destTable, 0, 1, 3, 4,
            static_cast<GtkAttachOptions>(GTK_EXPAND | GTK_FILL),
            static_cast<GtkAttachOptions>(GTK_EXPAND | GTK_FILL),
            20, 5);
        frame = gtk_frame_new(gCtx->Res("DEST_DIR"));
        gtk_table_attach_defaults(GTK_TABLE(destTable), frame, 0, 2, 0, 1);
        gtk_widget_show(frame);

        if (!gCtx->opt->mDestination)
        {
            gCtx->opt->mDestination = (char*)malloc(MAXPATHLEN * sizeof(char));
            getcwd(gCtx->opt->mDestination, MAXPATHLEN);
        }
        sFolder = gtk_label_new(gCtx->opt->mDestination);
        gtk_label_set_line_wrap(GTK_LABEL(sFolder), TRUE);
        gtk_widget_show(sFolder);
        gtk_table_attach_defaults(GTK_TABLE(destTable), sFolder, 0, 1, 0, 1);

        sBrowseBtn = gtk_button_new_with_label(gCtx->Res("BROWSE"));
        gtk_widget_show(sBrowseBtn);
        gtk_table_attach(GTK_TABLE(destTable), sBrowseBtn, 1, 2, 0, 1,
            static_cast<GtkAttachOptions>(GTK_EXPAND | GTK_FILL),
            GTK_SHRINK, 10, 10);

        mWidgetsInit = TRUE;
    }
    else
    {
        gtk_notebook_set_page(GTK_NOTEBOOK(gCtx->notebook), mPageNum);
        gtk_widget_show(mTable);
    }

    // signal connect the buttons
    // NOTE: back button disfunctional in this dlg since user accepted license
    gCtx->nextID = gtk_signal_connect(GTK_OBJECT(gCtx->next), "clicked",
                   GTK_SIGNAL_FUNC(nsSetupTypeDlg::Next), gCtx->sdlg);
    sBrowseBtnID = gtk_signal_connect(GTK_OBJECT(sBrowseBtn), "clicked",
                   GTK_SIGNAL_FUNC(nsSetupTypeDlg::SelectFolder), NULL);  

    // set up the next button.
    gCtx->nextLabel = gtk_label_new(gCtx->Res("NEXT"));
    gtk_widget_show(gCtx->nextLabel);
    gtk_container_add(GTK_CONTAINER(gCtx->next), gCtx->nextLabel);
    gtk_widget_show(gCtx->next);

    return err;
}

int
nsSetupTypeDlg::Hide()
{
    gtk_widget_hide(mTable);

    // disconnect and remove this dlg's nav btn
    gtk_signal_disconnect(GTK_OBJECT(sBrowseBtn), sBrowseBtnID);
    gtk_signal_disconnect(GTK_OBJECT(gCtx->next), gCtx->nextID);
    gtk_container_remove(GTK_CONTAINER(gCtx->next), gCtx->nextLabel); 
    gtk_widget_hide(gCtx->next);

    return OK;
}

int
nsSetupTypeDlg::SetMsg0(char *aMsg)
{
    if (!aMsg)
        return E_PARAM;

    mMsg0 = aMsg;

    return OK;
}

char *
nsSetupTypeDlg::GetMsg0()
{
    if (mMsg0)
        return mMsg0;

    return NULL;
}

int
nsSetupTypeDlg::AddSetupType(nsSetupType *aSetupType)
{
    if (!aSetupType)
        return E_PARAM;

    if (!mSetupTypeList)
    {
        mSetupTypeList = aSetupType;
        return OK;
    }

    nsSetupType *curr = mSetupTypeList;
    nsSetupType *next;
    while (curr)
    {
        next = NULL;
        next = curr->GetNext();
    
        if (!next)
        {
            return curr->SetNext(aSetupType);
        }

        curr = next;
    }

    return OK;
}

nsSetupType *
nsSetupTypeDlg::GetSetupTypeList()
{
    if (mSetupTypeList)
        return mSetupTypeList;

    return NULL;
}

int
nsSetupTypeDlg::GetNumSetupTypes()
{
    int num = 0;
    nsSetupType *curr = NULL;

    if (!mSetupTypeList)
        return 0;
    
    curr = mSetupTypeList;
    while(curr)
    {
        num++;
        curr = curr->GetNext();
    }

    return num;
}

nsSetupType *
nsSetupTypeDlg::GetSelectedSetupType()
{
    nsSetupType *curr = NULL;
    int numSetupTypes = GetNumSetupTypes();
    int setupTypeCount = 0;

    curr = GetSetupTypeList();
    while (curr && setupTypeCount < numSetupTypes)  // paranoia!
    {
        if (setupTypeCount == gCtx->opt->mSetupType)
            return curr;        

        setupTypeCount++;
        curr = curr->GetNext();
    }

    return NULL;
}

void
nsSetupTypeDlg::FreeSetupTypeList()
{
    nsSetupType *curr = mSetupTypeList;
    nsSetupType *prev;
    
    while (curr)
    {
        prev = curr;
        curr = curr->GetNext();

        XI_IF_DELETE(prev);
    }
}

void
nsSetupTypeDlg::FreeLegacyChecks()
{
    nsLegacyCheck *curr = NULL;
    nsLegacyCheck *last = NULL;

    if (sLegacyChecks)
    {
        curr = sLegacyChecks;

        while(curr)
        {
            last = curr;
            curr = last->GetNext();

            XI_IF_DELETE(last);
        }
    }
}

void
nsSetupTypeDlg::SelectFolder(GtkWidget *aWidget, gpointer aData)
{
    DUMP("SelectFolder");

    GtkWidget *fileSel = NULL;
    char *selDir = gCtx->opt->mDestination;

    fileSel = gtk_file_selection_new(gCtx->Res("SELECT_DIR"));
    gtk_window_set_modal(GTK_WINDOW(fileSel), TRUE);
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(fileSel), selDir);
    gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fileSel)->ok_button),
                       "clicked", (GtkSignalFunc) SelectFolderOK, fileSel);
    gtk_signal_connect_object(GTK_OBJECT(
                                GTK_FILE_SELECTION(fileSel)->cancel_button),
                                "clicked", (GtkSignalFunc) SelectFolderCancel,
                                GTK_OBJECT(fileSel));
    gtk_widget_show(fileSel); 
}

void
nsSetupTypeDlg::SelectFolderOK(GtkWidget *aWidget, GtkFileSelection *aFileSel)
{
    DUMP("SelectFolderOK");

    struct stat destStat;
    char *selDir = gtk_file_selection_get_filename(
                    GTK_FILE_SELECTION(aFileSel));

    // put the candidate file name in the global variable, then verify it

    strcpy(gCtx->opt->mDestination, selDir);

    if (0 == stat(selDir, &destStat))
        if (!S_ISDIR(destStat.st_mode) || VerifyDestination() != OK ) /* not a directory, or we don't have access permissions, so don't tear down */
            return;

    // update folder path displayed
    gtk_label_set_text(GTK_LABEL(sFolder), gCtx->opt->mDestination);
    gtk_widget_show(sFolder);

    // tear down file sel dlg
    gtk_object_destroy(GTK_OBJECT(aFileSel)); 
}

void
nsSetupTypeDlg::SelectFolderCancel(GtkWidget *aWidget, 
                                   GtkFileSelection *aFileSel)
{
    // tear down file sel dlg
    gtk_object_destroy(GTK_OBJECT(aWidget)); 
    gtk_object_destroy(GTK_OBJECT(aFileSel)); 
}

void
nsSetupTypeDlg::RadBtnToggled(GtkWidget *aWidget, gpointer aData)
{
    DUMP("RadBtnToggled");
    
    gCtx->opt->mSetupType = NS_PTR_TO_INT32(aData);
}

int
nsSetupTypeDlg::VerifyDestination()
{
    int stat_err = 0;
    struct stat stbuf; 
    GtkWidget *yesButton, *noButton, *label;
    GtkWidget *noPermsDlg, *okButton;
    char message[MAXPATHLEN];
  
    stat_err = stat(gCtx->opt->mDestination, &stbuf);
    if (stat_err == 0)
    {
      if (access(gCtx->opt->mDestination, R_OK | W_OK | X_OK ) != 0)
      {
        if (gCtx->opt->mMode != nsXIOptions::MODE_DEFAULT) {
          ErrorHandler(E_NO_PERMS);
          return E_NO_PERMS;
        }
        sprintf(message, gCtx->Res("NO_PERMS"), gCtx->opt->mDestination);

        noPermsDlg = gtk_dialog_new();
        gtk_window_set_modal(GTK_WINDOW(noPermsDlg), TRUE);
        label = gtk_label_new(message);
        okButton = gtk_button_new_with_label(gCtx->Res("OK_LABEL"));

        if (noPermsDlg && label && okButton)
        {
          gtk_window_set_title(GTK_WINDOW(noPermsDlg), gCtx->opt->mTitle);
          gtk_window_set_position(GTK_WINDOW(noPermsDlg), 
            GTK_WIN_POS_CENTER);
          gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
          gtk_box_pack_start(GTK_BOX(
            GTK_DIALOG(noPermsDlg)->action_area), okButton, FALSE, FALSE, 10);
          gtk_signal_connect(GTK_OBJECT(okButton), "clicked", 
            GTK_SIGNAL_FUNC(NoPermsOK), noPermsDlg);
          gtk_box_pack_start(GTK_BOX(
            GTK_DIALOG(noPermsDlg)->vbox), label, FALSE, FALSE, 10);

          GTK_WIDGET_SET_FLAGS(okButton, GTK_CAN_DEFAULT);
          gtk_widget_grab_default(okButton);

          gtk_widget_show_all(noPermsDlg);
        }

        return E_NO_PERMS;
      }
      else
      {
        // perms OK, we can proceed
        return OK;
      }
    }

    if (gCtx->opt->mMode != nsXIOptions::MODE_DEFAULT)
    {
      CreateDestYes((GtkWidget *)NULL, (gpointer) gCtx->sdlg);
      return E_NO_DEST;
    }
    // destination doesn't exist so ask user if we should create it
    sprintf(message, gCtx->Res("DOESNT_EXIST"), gCtx->opt->mDestination);

    sCreateDestDlg = gtk_dialog_new();
    gtk_window_set_modal(GTK_WINDOW(sCreateDestDlg), TRUE);
    label = gtk_label_new(message);
    yesButton = gtk_button_new_with_label(gCtx->Res("YES_LABEL"));
    noButton = gtk_button_new_with_label(gCtx->Res("NO_LABEL"));

    gtk_window_set_title(GTK_WINDOW(sCreateDestDlg), gCtx->opt->mTitle);
    gtk_window_set_position(GTK_WINDOW(sCreateDestDlg), GTK_WIN_POS_CENTER);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(sCreateDestDlg)->action_area),
                      yesButton);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(sCreateDestDlg)->action_area),
                      noButton);
    gtk_signal_connect(GTK_OBJECT(yesButton), "clicked",
                       GTK_SIGNAL_FUNC(CreateDestYes), sCreateDestDlg);
    gtk_signal_connect(GTK_OBJECT(noButton), "clicked",
                       GTK_SIGNAL_FUNC(CreateDestNo), sCreateDestDlg);

    GTK_WIDGET_SET_FLAGS(yesButton, GTK_CAN_DEFAULT);
    gtk_widget_grab_default(yesButton);

    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(sCreateDestDlg)->vbox), label);
    
    gtk_widget_show_all(sCreateDestDlg);

    return E_NO_DEST;
}

void
nsSetupTypeDlg::NoPermsOK(GtkWidget *aWidget, gpointer aData)
{
    GtkWidget *noPermsDlg = (GtkWidget *) aData;

    if (!noPermsDlg)
        return;

    gtk_widget_destroy(noPermsDlg);
}

void
nsSetupTypeDlg::CreateDestYes(GtkWidget *aWidget, gpointer aData)
{
    DUMP("CreateDestYes");
    int err = 0; 
    char path[PATH_MAX + 1];
    int  pathLen = strlen(gCtx->opt->mDestination);

    if (pathLen > PATH_MAX)
        pathLen = PATH_MAX;
    memcpy(path, gCtx->opt->mDestination, pathLen);
    path[pathLen] = '/';  // for uniform handling

    struct stat buf;

    for (int i = 1; !err && i <= pathLen; i++) 
    {
        if (path[i] == '/') 
        {
            path[i] = '\0';
            if (stat(path, &buf) != 0) 
            {
                err = mkdir(path, 0755);
            }
            path[i] = '/';
        }
    }

    if (gCtx->opt->mMode == nsXIOptions::MODE_DEFAULT)
    {
        gtk_widget_destroy(sCreateDestDlg);
    }

    if (err != 0)
    {
        ErrorHandler(E_MKDIR_FAIL);
    }
    else
    {
        // try to move forward to installer dialog again
        nsSetupTypeDlg::Next((GtkWidget *)NULL, NULL);
    }
}

void
nsSetupTypeDlg::CreateDestNo(GtkWidget *aWidget, gpointer aData)
{
    DUMP("CreateDestNo");

    gtk_widget_destroy(sCreateDestDlg);
}

int
nsSetupTypeDlg::DeleteOldInst()
{
    DUMP("DeleteOldInst");

    const int MAXCHARS = 64; // Maximum chars per line in Delete Dialog
    const int MAXLINES = 20; // Maximum lines in Delete Dialog
    int err = OK;
    struct stat dummy;
    char path[MAXPATHLEN];
    GtkWidget *label = NULL;
    GtkWidget *deleteBtn = NULL; /* delete button */
    GtkWidget *cancelBtn = NULL; /* cancel button */
    char *msgPtr = NULL, *msgChunkPtr = NULL, *msgEndPtr = NULL;
    char msgChunk[MAXCHARS+1];
    char msg[MAXPATHLEN+512];
    nsLegacyCheck *currLC = NULL;

    currLC = sLegacyChecks;
    while (currLC)
    {
      memset(path, 0, MAXPATHLEN);
      ConstructPath(path, gCtx->opt->mDestination, currLC->GetFilename());
      DUMP(path);

      // check if old installation exists
      if (0 == stat(path, &dummy))
      {
          if (gCtx->opt->mMode != nsXIOptions::MODE_DEFAULT)
          {
              DeleteInstDelete((GtkWidget *)NULL, (gpointer) gCtx->sdlg);
              return OK;
          }

          // throw up delete dialog 
          sDelInstDlg = gtk_dialog_new();
          gtk_window_set_modal(GTK_WINDOW(sDelInstDlg), TRUE);
          gtk_window_set_title(GTK_WINDOW(sDelInstDlg), gCtx->opt->mTitle);
          gtk_window_set_position(GTK_WINDOW(sDelInstDlg), GTK_WIN_POS_CENTER);

          deleteBtn = gtk_button_new_with_label(gCtx->Res("DELETE_LABEL"));
          cancelBtn = gtk_button_new_with_label(gCtx->Res("CANCEL_LABEL"));

          gtk_container_add(GTK_CONTAINER(GTK_DIALOG(sDelInstDlg)->action_area), 
              deleteBtn);
          gtk_container_add(GTK_CONTAINER(GTK_DIALOG(sDelInstDlg)->action_area),
              cancelBtn);
          gtk_signal_connect(GTK_OBJECT(deleteBtn), "clicked",
                         GTK_SIGNAL_FUNC(DeleteInstDelete), sDelInstDlg);
          gtk_signal_connect(GTK_OBJECT(cancelBtn), "clicked",
                         GTK_SIGNAL_FUNC(DeleteInstCancel), sDelInstDlg);

          GTK_WIDGET_SET_FLAGS(cancelBtn, GTK_CAN_DEFAULT);
          gtk_widget_grab_default(cancelBtn);

          snprintf(msg, sizeof(msg), currLC->GetMessage(), gCtx->opt->mDestination);
          msgPtr = msg;
          msgEndPtr = msg + strlen(msg);
          // wrap message at MAXCHARS colums (or last space inside MAXCHARS)
          // stop at MAXLINES rows or stop after last char is reached
          for (int i = 0; i < MAXLINES && msgPtr < msgEndPtr; i++)
          {
              // get the next MAXCHARS chars
              memset(msgChunk, 0, MAXCHARS+1);
              strncpy(msgChunk, msgPtr, MAXCHARS);

              // find last space
              msgChunkPtr = strrchr(msgChunk, ' ');
              if (msgChunkPtr)
              {
                  *msgChunkPtr = '\0';
                  msgPtr += (msgChunkPtr - msgChunk + 1);
              }
              else
              {
                  msgPtr += MAXCHARS;
              }
              label = gtk_label_new(msgChunk);
              gtk_box_pack_start(GTK_BOX(GTK_DIALOG(sDelInstDlg)->vbox), label,
                  FALSE, FALSE, 0);
          }
          gtk_widget_show_all(sDelInstDlg);
      
          err = E_OLD_INST;
          break;
      }
      currLC = currLC->GetNext();    
    }

    return err;
}

void         
nsSetupTypeDlg::DeleteInstDelete(GtkWidget *aWidget, gpointer aData)
{
    DUMP("DeleteInstDelete");

    if (!fork())
    {
        execlp("rm", "rm", "-rf", gCtx->opt->mDestination, NULL);
        /* execlp shouldn't return, we need to exit in case it does */
        _exit(0);
    }
    wait(NULL);

    if (gCtx->opt->mMode == nsXIOptions::MODE_DEFAULT)
    {
       gtk_widget_destroy(sDelInstDlg);
    }

    // We just deleted the directory, so the parent exists
    // and we have appropriate perms.
    mkdir(gCtx->opt->mDestination, 0755);

    if (gCtx->opt->mMode == nsXIOptions::MODE_DEFAULT)
    {
        // Try to move forward to installer dialog again.
        nsSetupTypeDlg::Next((GtkWidget *)NULL, NULL);
    }
}

void         
nsSetupTypeDlg::DeleteInstCancel(GtkWidget *aWidget, gpointer aData)
{
    DUMP("DeleteInstCancel");

    gtk_widget_destroy(sDelInstDlg);
}

int
nsSetupTypeDlg::ConstructPath(char *aDest, char *aTrunk, char *aLeaf)
{
    int err = OK;
    int trunkLen;
    char *lastSlash = NULL;
    
    if (!aDest || !aTrunk || !aLeaf)
        return E_PARAM;

    trunkLen = strlen(aTrunk);
    lastSlash = strrchr(aTrunk, '/');
    
    strcpy(aDest, aTrunk);
    if (lastSlash != aTrunk + (trunkLen - 1))
    {
        // need to tack on a slash
        strcat(aDest, "/");
    }
    strcat(aDest, aLeaf);

    return err;
}

int
nsSetupTypeDlg::CheckDestEmpty()
{
    DUMP("DeleteOldInst");

    DIR *destDirD;
    struct dirent *de;
    nsObjectIgnore *currOI = NULL;

    /* check if the destination directory is empty */
    destDirD = opendir(gCtx->opt->mDestination);
    while (de = readdir(destDirD))
    {
        if (strcmp(de->d_name, ".") && strcmp(de->d_name, ".."))
        {
            currOI = sObjectsToIgnore;
            while (currOI)
            {
                // check if this is an Object To Ignore
                if (!strcmp(currOI->GetFilename(),de->d_name))
                    break;

                currOI = currOI->GetNext();    
            }
            if (!currOI)
            {
                closedir(destDirD);
                ErrorHandler(E_DIR_NOT_EMPTY);
                return E_DIR_NOT_EMPTY;
            }
        }
    }
    
    closedir(destDirD);
    return OK;
}

int
nsSetupTypeDlg::VerifyDiskSpace(void)
{
    int err = OK;
    int dsAvail, dsReqd;
    char dsAvailStr[128], dsReqdStr[128];
    char message[512];
    GtkWidget *noDSDlg, *label, *okButton, *packer;

    // find disk space available at destination
    dsAvail = DSAvailable();
    if (dsAvail < 0)
        return OK; // optimistic when statfs failed
                   // or we don't have statfs

    // get disk space required
    dsReqd = DSRequired();

    if (dsReqd > dsAvail)
    {
        if (gCtx->opt->mMode == nsXIOptions::MODE_DEFAULT)
        {
            // throw up not enough ds dlg
            sprintf(dsAvailStr, gCtx->Res("DS_AVAIL"), dsAvail);
            sprintf(dsReqdStr, gCtx->Res("DS_REQD"), dsReqd);
            sprintf(message, "%s\n%s\n\n%s", dsAvailStr, dsReqdStr, 
                    gCtx->Res("NO_DISK_SPACE"));

            noDSDlg = gtk_dialog_new();
            gtk_window_set_modal(GTK_WINDOW(noDSDlg), TRUE);
            label = gtk_label_new(message);
            okButton = gtk_button_new_with_label(gCtx->Res("OK_LABEL"));
            packer = gtk_packer_new(); 

            if (noDSDlg && label && okButton && packer)
            {
                gtk_window_set_title(GTK_WINDOW(noDSDlg), gCtx->opt->mTitle);
                gtk_window_set_position(GTK_WINDOW(noDSDlg), 
                        GTK_WIN_POS_CENTER);
                gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
                gtk_packer_set_default_border_width(GTK_PACKER(packer), 20);
                gtk_packer_add_defaults(GTK_PACKER(packer), label,
                        GTK_SIDE_BOTTOM, GTK_ANCHOR_CENTER, GTK_FILL_X);
                gtk_box_pack_start(GTK_BOX(
                            GTK_DIALOG(noDSDlg)->action_area), okButton,
                        FALSE, FALSE, 10);
                gtk_signal_connect(GTK_OBJECT(okButton), "clicked", 
                        GTK_SIGNAL_FUNC(NoDiskSpaceOK), noDSDlg);
                gtk_box_pack_start(GTK_BOX(
                            GTK_DIALOG(noDSDlg)->vbox), packer, FALSE, FALSE, 10);

                GTK_WIDGET_SET_FLAGS(okButton, GTK_CAN_DEFAULT);
                gtk_widget_grab_default(okButton);

                gtk_widget_show_all(noDSDlg);
            }
        }

        err = E_NO_DISK_SPACE;
    }

    return err;
}

int
nsSetupTypeDlg::DSAvailable(void)
{
    // returns disk space available in kilobytes

    int dsAvail = -1;

#if defined(HAVE_SYS_STATVFS_H) || defined(HAVE_SYS_STATFS_H)
    struct STATFS buf;
    int rv;

    if (gCtx->opt->mDestination)
    {
        rv = STATFS(gCtx->opt->mDestination, &buf);
        if (rv == 0)
        {
            if (buf.f_bsize > 1024 && (buf.f_bsize%1024 == 0))
            {
                // normally the block size is >= 1024 and a multiple
                // so we can shave off the last three digits before 
                // finding the product of the block size and num blocks
                // which is important for large disks

                dsAvail = (buf.f_bsize/1024) * (buf.f_bavail);
            }
            else
            {
                // attempt to stuff into a 32 bit int even though
                // we convert from bytes -> kilobytes later
                // (may fail to compute on very large disks whose
                // block size is not a multiple of 1024 -- highly 
                // improbable)

                dsAvail = (buf.f_bsize * buf.f_bavail)/1024;
            }
        }
    }
#endif // HAVE_SYS_STATVFS_H -or- HAVE_SYS_STATFS_H 

    return dsAvail;
}

int 
nsSetupTypeDlg::DSRequired(void)
{
    // returns disk space required in kilobytes 

    int dsReqd = 0;
    nsComponentList *comps;
    int bCus;

    // find setup type's component list
    bCus = (gCtx->opt->mSetupType == (gCtx->sdlg->GetNumSetupTypes() - 1));
    comps = gCtx->sdlg->GetSelectedSetupType()->GetComponents();

    // loop through all components
    nsComponent *currComp = comps->GetHead();
    while (currComp)
    {
        if ( (bCus == TRUE && currComp->IsSelected()) || (bCus == FALSE) )
        {
            // add to disk space required 
            dsReqd += currComp->GetInstallSize();
            dsReqd += currComp->GetArchiveSize();
        }

        currComp = currComp->GetNext();
    }

    return dsReqd;
}

void
nsSetupTypeDlg::NoDiskSpaceOK(GtkWidget *aWidget, gpointer aData)
{
    GtkWidget *noDSDlg = (GtkWidget *) aData;

    if (!noDSDlg)
        return;

    gtk_widget_destroy(noDSDlg);
}

