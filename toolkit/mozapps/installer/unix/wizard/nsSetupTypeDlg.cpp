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

#include "nscore.h"
#include "nsSetupTypeDlg.h"
#include "nsXInstaller.h"


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


static GtkWidget        *sBrowseBtn;
static gint             sBrowseBtnID;
static GtkWidget        *sFolder;
static int              sFilePickerUp = FALSE;

nsSetupTypeDlg::nsSetupTypeDlg() :
    mBox(NULL),
    mExistingMsg(NULL),
    mMsg0(NULL),
    mSetupTypeList(NULL)
{
}

nsSetupTypeDlg::~nsSetupTypeDlg()
{
    FreeSetupTypeList();

    XI_IF_FREE(mMsg0);
    XI_IF_FREE(mExistingMsg);
}

void
nsSetupTypeDlg::Back(GtkWidget *aWidget, gpointer aData)
{
    DUMP("Back");
    if (aData != gCtx->sdlg) return;

    // hide this notebook page
    gCtx->sdlg->Hide(nsXInstallerDlg::BACKWARD_MOVE);

    // disconnect this dlg's nav btn signal handlers
    gtk_signal_disconnect(GTK_OBJECT(gCtx->back), gCtx->backID);
    gtk_signal_disconnect(GTK_OBJECT(gCtx->next), gCtx->nextID);
    gtk_signal_disconnect(GTK_OBJECT(sBrowseBtn), sBrowseBtnID);

    // show the next dlg
    gCtx->ldlg->Show(nsXInstallerDlg::BACKWARD_MOVE);
}

void 
nsSetupTypeDlg::Next(GtkWidget *aWidget, gpointer aData)
{
    DUMP("Next");
    if (aData != gCtx->sdlg) return;

    GSList *s = gCtx->sdlg->mRadioGroup;
    gCtx->opt->mSetupType = 0;
    while (s) {
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(s->data)))
        break;

      ++gCtx->opt->mSetupType;
      s = s->next;
    }

    // verify selected destination directory exists
    if (OK != nsSetupTypeDlg::VerifyDestination())
        return;

    // old installation may exist: delete it
    if (OK != nsSetupTypeDlg::DeleteOldInst())
        return;

    // if not custom setup type verify disk space
    if (gCtx->opt->mSetupType != (gCtx->sdlg->GetNumSetupTypes() - 1))
    {
        if (OK != nsSetupTypeDlg::VerifyDiskSpace())
            return;
    }

    // hide this notebook page
    gCtx->sdlg->Hide(nsXInstallerDlg::FORWARD_MOVE);

    // disconnect this dlg's nav btn signal handlers
    gtk_signal_disconnect(GTK_OBJECT(gCtx->back), gCtx->backID);
    gtk_signal_disconnect(GTK_OBJECT(gCtx->next), gCtx->nextID);
    gtk_signal_disconnect(GTK_OBJECT(sBrowseBtn), sBrowseBtnID);

    // show the last dlg
    if (gCtx->opt->mSetupType == (gCtx->sdlg->GetNumSetupTypes() - 1))
        gCtx->cdlg->Show(nsXInstallerDlg::FORWARD_MOVE);
    else
        gCtx->idlg->Show(nsXInstallerDlg::FORWARD_MOVE);
}

nsComponent*
nsSetupTypeDlg::ParseComponent(nsINIParser *aParser, char *aCompName)
{
  char *descShort = NULL, *descLong = NULL, *archive = NULL,
    *installSize = NULL, *archiveSize = NULL, *attr = NULL;
  int bufsize = 0, err;
  nsComponent *comp = NULL;

  err = aParser->GetStringAlloc(aCompName, DESC_SHORT, &descShort, &bufsize);
  if (err != OK)
    goto bail;

  err = aParser->GetStringAlloc(aCompName, DESC_LONG, &descLong, &bufsize);
  if (err != OK)
    goto bail;

  err = aParser->GetStringAlloc(aCompName, ARCHIVE, &archive, &bufsize);
  if (err != OK)
    goto bail;

  err = aParser->GetStringAlloc(aCompName, INSTALL_SIZE,
                                &installSize, &bufsize);
  if (err != OK)
    goto bail;

  err = aParser->GetStringAlloc(aCompName, ARCHIVE_SIZE,
                                &archiveSize, &bufsize);
  if (err != OK)
    goto bail;

  err = aParser->GetStringAlloc(aCompName, ATTRIBUTES, &attr, &bufsize);
  if (err != OK && err != nsINIParser::E_NO_KEY) // this is optional
    goto bail;

  comp = new nsComponent();
  if (!comp)
    goto bail;

  comp->SetDescShort(descShort);
  comp->SetDescLong(descLong);
  comp->SetArchive(archive);
  comp->SetInstallSize(atoi(installSize));
  comp->SetArchiveSize(atoi(archiveSize));

  if (strstr(attr, SELECTED_ATTR)) {
    comp->SetSelected();
    comp->DepAddRef();
  }
  else {
    comp->SetUnselected();
  }

  if (strstr(attr, INVISIBLE_ATTR))
    comp->SetInvisible();
  else
    comp->SetVisible();

  if (strstr(attr, LAUNCHAPP_ATTR))
    comp->SetLaunchApp();
  else
    comp->SetDontLaunchApp();

  if (strstr(attr, DOWNLOAD_ONLY_ATTR))
    comp->SetDownloadOnly();

  if (strstr(attr, MAIN_COMPONENT_ATTR))
    comp->SetMainComponent();

  for (int i = 0; i < MAX_URLS; ++i) {
    char urlKey[MAX_URL_LEN], *currURL;
    sprintf(urlKey, URLd, i);
    err = aParser->GetStringAlloc(aCompName, urlKey, &currURL, &bufsize);
    if (err != OK)
      break;

    comp->SetURL(currURL, i);
  }

  for (int j = 0; j < MAX_COMPONENTS; ++j) {
    char *currDep = NULL;
    char key[MAX_DEPENDEE_KEY_LEN];

    sprintf(key, DEPENDEEd, j);

    err = aParser->GetStringAlloc(aCompName, key, &currDep, &bufsize);
    if (bufsize == 0 || err != OK)
      break; // no more dependees

    comp->AddDependee(currDep); // we will scan for invalid dependencies later
  }

  return comp;
 bail:
  return NULL;
}

int
nsSetupTypeDlg::Parse(nsINIParser *aParser)
{
    int err = OK;
    int bufsize = 0;
    char *showDlg = NULL;
    int i, j;
    char *currSec = (char *) malloc(strlen(SETUP_TYPE) + 3); // e.g. SetupType12
    if (!currSec) return E_MEM;
    char *currKey = (char *) malloc(1 + 3); // e.g. C0, C1, C12
    if (!currKey) return E_MEM;
    char *currVal = NULL;
    nsComponent *currComp = NULL;
    int currNumComps = 0;
    nsSetupType *currST = NULL;
    char *currDescShort = NULL;
    char *currDescLong = NULL;

    XI_VERIFY(gCtx);

    XI_ERR_BAIL(aParser->GetStringAlloc(DLG_SETUP_TYPE, MSGEXISTING,
                                        &mExistingMsg, &bufsize));
    if (bufsize == 0)
      XI_IF_FREE(mExistingMsg);

    /* optional keys */
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

    aParser->GetStringAlloc(DLG_SETUP_TYPE, SUBTITLE, &mSubTitle, &bufsize);
    if (bufsize == 0)
      XI_IF_FREE(mSubTitle);

    /* setup types */
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

        bufsize = 0;
        err = aParser->GetStringAlloc(currSec, DESC_LONG, &currDescLong,
                                      &bufsize);
        if (err != OK || bufsize == 0) goto fin_iter;

        currST = new nsSetupType();
        if (!currST) goto fin_iter;

        // The short description may contain mnemonics by prefixing
        // characters with "&".  We need to change these to "_" for GTK.
        for (char *c = currDescShort; *c != '\0'; ++c) {
          if (*c == '&')  // XXX this doesn't quite handle "&&"
            *c = '_';
        }

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
        
            currComp = ParseComponent(aParser, currVal);
            if (currComp) {
              currST->SetComponent(currComp);
              ++currNumComps;
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

    return err;
}

int
nsSetupTypeDlg::Show(int aDirection)
{
    int err = OK;
    int numSetupTypes = 0;
    GtkWidget *radbtn;
    nsSetupType *currST = NULL;
    GtkWidget *frame = NULL;

    XI_VERIFY(gCtx);
    XI_VERIFY(gCtx->notebook);

    if (mWidgetsInit == FALSE)
    {
      // add a vbox as a page of the notebook
      mBox = gtk_vbox_new(FALSE, 0);
      gtk_container_set_border_width(GTK_CONTAINER(mBox), 12);
      gtk_notebook_append_page(GTK_NOTEBOOK(gCtx->notebook), mBox, NULL);
      mPageNum = gtk_notebook_get_current_page(GTK_NOTEBOOK(gCtx->notebook));

      // add the top text label
      GtkWidget *msg0 = gtk_label_new(mMsg0);
      gtk_misc_set_alignment(GTK_MISC(msg0), 0.0, 0.5);
      gtk_box_pack_start(GTK_BOX(mBox), msg0, FALSE, FALSE, 12);

      // for each setup type, pack into the vbox:
      //  an hbox containing:  (this is to pad the radio button on the right)
      //   the radio button with short description
      //  a label with the long description

      numSetupTypes = GetNumSetupTypes();
      currST = GetSetupTypeList();

      for (int i = 0; i < numSetupTypes; ++i, currST = currST->GetNext()) {
        if (i == 0) {
          radbtn = gtk_radio_button_new_with_mnemonic(NULL,
                                                      currST->GetDescShort());
          mRadioGroup = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radbtn));
        } else {
          radbtn = gtk_radio_button_new_with_mnemonic(mRadioGroup,
                                                      currST->GetDescShort());
        }

        
        GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), radbtn, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(mBox), hbox, FALSE, FALSE, 6);

        GtkWidget *desc = gtk_label_new(currST->GetDescLong());
        gtk_label_set_line_wrap(GTK_LABEL(desc), TRUE);
        gtk_misc_set_alignment(GTK_MISC(desc), 0.0, 0.5);

        // Pad the labels so that they line up with the radio button text
        gint ind_size, ind_spacing;
        gtk_widget_style_get(radbtn,
                             "indicator_size", &ind_size,
                             "indicator_spacing", &ind_spacing,
                             NULL);

        gtk_misc_set_padding(GTK_MISC(desc), ind_size + ind_spacing * 3, 0);
        gtk_box_pack_start(GTK_BOX(mBox), desc, FALSE, FALSE, 6);
      }


      frame = gtk_frame_new(gCtx->Res("DEST_DIR"));
      gtk_box_pack_start(GTK_BOX(mBox), frame, FALSE, FALSE, 12);

      GtkWidget *frame_hbox = gtk_hbox_new(FALSE, 6);
      gtk_container_set_border_width(GTK_CONTAINER(frame_hbox), 6);
      gtk_container_add(GTK_CONTAINER(frame), frame_hbox);

      if (!gCtx->opt->mDestination) {
        gCtx->opt->mDestination = (char*)malloc(MAXPATHLEN * sizeof(char));
        getcwd(gCtx->opt->mDestination, MAXPATHLEN);
      }

      sFolder = gtk_label_new(gCtx->opt->mDestination);
      gtk_label_set_line_wrap(GTK_LABEL(sFolder), TRUE);
      gtk_misc_set_alignment(GTK_MISC(sFolder), 0.0, 0.5);
      gtk_box_pack_start(GTK_BOX(frame_hbox), sFolder, TRUE, TRUE, 0);

      sBrowseBtn = gtk_button_new_with_label(gCtx->Res("BROWSE"));
      gtk_box_pack_start(GTK_BOX(frame_hbox), sBrowseBtn, FALSE, FALSE, 0);

      mWidgetsInit = TRUE;
    } else {
        gtk_notebook_set_page(GTK_NOTEBOOK(gCtx->notebook), mPageNum);
    }

    // <b>title</b>\0
    char *titleBuf = new char[strlen(mTitle) + 9];
    sprintf(titleBuf, "<b>%s</b>", mTitle);

    gtk_label_set_markup(GTK_LABEL(gCtx->header_title), titleBuf);
    gtk_label_set_text(GTK_LABEL(gCtx->header_subtitle), mSubTitle);

    delete[] titleBuf;

    gtk_widget_show_all(mBox);

    // signal connect the buttons
    // NOTE: back button disfunctional in this dlg since user accepted license
    gCtx->backID = gtk_signal_connect(GTK_OBJECT(gCtx->back), "clicked",
                   GTK_SIGNAL_FUNC(nsSetupTypeDlg::Back), gCtx->sdlg);
    gCtx->nextID = gtk_signal_connect(GTK_OBJECT(gCtx->next), "clicked",
                   GTK_SIGNAL_FUNC(nsSetupTypeDlg::Next), gCtx->sdlg);
    sBrowseBtnID = gtk_signal_connect(GTK_OBJECT(sBrowseBtn), "clicked",
                   GTK_SIGNAL_FUNC(nsSetupTypeDlg::SelectFolder), NULL);  

    if (aDirection == nsXInstallerDlg::FORWARD_MOVE)
    {
        // change the button titles back to Back/Next
      gtk_button_set_label(GTK_BUTTON(gCtx->next), GTK_STOCK_GO_FORWARD);
      gtk_button_set_label(GTK_BUTTON(gCtx->back), GTK_STOCK_GO_BACK);
    }
        // from install dlg
    if (aDirection == nsXInstallerDlg::BACKWARD_MOVE && 
        // not custom setup type
        gCtx->opt->mSetupType != (gCtx->sdlg->GetNumSetupTypes() - 1))
    {
        DUMP("Back from Install to Setup Type");
        gtk_button_set_label(GTK_BUTTON(gCtx->next), GTK_STOCK_GO_FORWARD);
    }     

    gtk_widget_set_sensitive(gCtx->back, FALSE);

    return err;
}

int
nsSetupTypeDlg::Hide(int aDirection)
{
  //    gtk_widget_hide(mTable);
  gtk_widget_hide(mBox);

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
nsSetupTypeDlg::SelectFolder(GtkWidget *aWidget, gpointer aData)
{
    DUMP("SelectFolder");

    if (sFilePickerUp)
        return;

    GtkWidget *fileSel = NULL;
    char *selDir = gCtx->opt->mDestination;

    fileSel = gtk_file_selection_new(gCtx->Res("SELECT_DIR"));
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(fileSel), selDir);
    gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fileSel)->ok_button),
                       "clicked", (GtkSignalFunc) SelectFolderOK, fileSel);
    gtk_signal_connect_object(GTK_OBJECT(
                                GTK_FILE_SELECTION(fileSel)->cancel_button),
                                "clicked", (GtkSignalFunc) SelectFolderCancel,
                                GTK_OBJECT(fileSel));
    gtk_widget_show(fileSel); 
    sFilePickerUp = TRUE;
}

void
nsSetupTypeDlg::SelectFolderOK(GtkWidget *aWidget, GtkFileSelection *aFileSel)
{
    DUMP("SelectFolderOK");

    struct stat destStat;
    const gchar *selDir =
      gtk_file_selection_get_filename(GTK_FILE_SELECTION(aFileSel));

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
    sFilePickerUp = FALSE;
}

void
nsSetupTypeDlg::SelectFolderCancel(GtkWidget *aWidget, 
                                   GtkFileSelection *aFileSel)
{
    // tear down file sel dlg
    gtk_object_destroy(GTK_OBJECT(aWidget)); 
    gtk_object_destroy(GTK_OBJECT(aFileSel)); 
    sFilePickerUp = FALSE;
}

int
nsSetupTypeDlg::VerifyDestination()
{
    int stat_err = 0;
    struct stat stbuf; 
  
    stat_err = stat(gCtx->opt->mDestination, &stbuf);
    if (stat_err == 0)
    {
      if (access(gCtx->opt->mDestination, R_OK | W_OK | X_OK ) != 0)
      {
        GtkWidget *noPermsDlg =
          gtk_message_dialog_new(GTK_WINDOW(gCtx->window),
             GtkDialogFlags(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                 GTK_MESSAGE_ERROR,
                                 GTK_BUTTONS_OK,
                                 gCtx->Res("NO_PERMS"),
                                 gCtx->opt->mDestination);

        gtk_dialog_run(GTK_DIALOG(noPermsDlg));
        gtk_widget_destroy(noPermsDlg);

        return E_NO_PERMS;
      }
      else
      {
        // perms OK, we can proceed
        return OK;
      }
    }

    // destination doesn't exist so ask user if we should create it
    GtkWidget *createDestDlg =
      gtk_message_dialog_new(GTK_WINDOW(gCtx->window),
               GtkDialogFlags(GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT),
                             GTK_MESSAGE_QUESTION,
                             GTK_BUTTONS_YES_NO,
                             gCtx->Res("DOESNT_EXIST"),
                             gCtx->opt->mDestination);

    gint result = gtk_dialog_run(GTK_DIALOG(createDestDlg));
    gtk_widget_destroy(createDestDlg);

    if (result == GTK_RESPONSE_YES) {
      // Create the destination directory.
      char path[PATH_MAX + 1];
      int  pathLen = strlen(gCtx->opt->mDestination);
      int  dirErr = 0;

      if (pathLen > PATH_MAX)
        pathLen = PATH_MAX;
      memcpy(path, gCtx->opt->mDestination, pathLen);
      path[pathLen] = '/';  // for uniform handling

      struct stat buf;
      mode_t oldPerms = umask(022);

      for (int i = 1; !dirErr && i <= pathLen; i++) {
        if (path[i] == '/') {
          path[i] = '\0';
          if (stat(path, &buf) != 0) {
            dirErr = mkdir(path, (0777 & ~oldPerms));
          }
          path[i] = '/';
        }
      }

      umask(oldPerms); // restore original umask
      if (dirErr != 0)
        ErrorHandler(E_MKDIR_FAIL);
      else
        return OK;
    }

    return E_NO_DEST;
}

int
nsSetupTypeDlg::DeleteOldInst()
{
    DUMP("DeleteOldInst");

    int err = OK;
    struct stat dummy;
    char path[MAXPATHLEN];

    memset(path, 0, MAXPATHLEN);
    ConstructPath(path, gCtx->opt->mDestination, gCtx->opt->mProgramName);

    DUMP(path);

    // check if old installation exists
    if (0 == stat(path, &dummy)) {
      // throw up delete dialog 
      GtkWidget *delInstDlg =
        gtk_message_dialog_new(GTK_WINDOW(gCtx->window),
             GtkDialogFlags(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                               GTK_MESSAGE_QUESTION,
                               GTK_BUTTONS_NONE,
                               gCtx->sdlg->mExistingMsg,
                               gCtx->opt->mDestination);

      gtk_dialog_add_buttons(GTK_DIALOG(delInstDlg),
                             GTK_STOCK_DELETE, GTK_RESPONSE_ACCEPT,
                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);
                            
      gint result = gtk_dialog_run(GTK_DIALOG(delInstDlg));
      if (result == GTK_RESPONSE_ACCEPT) {
        // Delete the old installation
        char cwd[MAXPATHLEN];
        getcwd(cwd, MAXPATHLEN);
        chdir(gCtx->opt->mDestination);
        system("rm -rf *");
        chdir(cwd);
        err = OK;
      } else {
        err = E_OLD_INST;
      }

      gtk_widget_destroy(delInstDlg);
    }
    
    return err;
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
nsSetupTypeDlg::VerifyDiskSpace(void)
{
    int err = OK;
    int dsAvail, dsReqd;
    char dsAvailStr[128], dsReqdStr[128];
    char message[512];
    GtkWidget *noDSDlg, *label;

    // find disk space available at destination
    dsAvail = DSAvailable();
    if (dsAvail < 0)
        return OK; // optimistic when statfs failed
                   // or we don't have statfs

    // get disk space required
    dsReqd = DSRequired();

    if (dsReqd > dsAvail)
    {
        // throw up not enough ds dlg
        sprintf(dsAvailStr, gCtx->Res("DS_AVAIL"), dsAvail);
        sprintf(dsReqdStr, gCtx->Res("DS_REQD"), dsReqd);
        sprintf(message, "%s\n%s\n\n%s", dsAvailStr, dsReqdStr, 
                gCtx->Res("NO_DISK_SPACE"));

        noDSDlg = gtk_dialog_new_with_buttons(gCtx->opt->mTitle,
                                              NULL, (GtkDialogFlags) 0,
                                              GTK_STOCK_OK,
                                              GTK_RESPONSE_NONE,
                                              NULL);
        label = gtk_label_new(message);

        g_signal_connect_swapped(GTK_OBJECT(noDSDlg), "response",
                                 G_CALLBACK(NoDiskSpaceOK),
                                 GTK_OBJECT(noDSDlg));

        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(noDSDlg)->vbox), label);
        gtk_widget_show_all(noDSDlg);
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

