/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsFileWidget.h"
#include "nsString.h"

#include <StorageKit.h>
#include <Message.h>
#include <Window.h>
#include <View.h>

NS_DEFINE_IID(kIFileWidgetIID, NS_IFILEWIDGET_IID);
NS_IMPL_ISUPPORTS(nsFileWidget, kIFileWidgetIID);

//-------------------------------------------------------------------------
//
// nsFileWidget constructor
//
//-------------------------------------------------------------------------
nsFileWidget::nsFileWidget() : nsIFileWidget()
   , mParentWindow(nsnull)
   , mTitles(nsnull)
   , mFilters(nsnull)
{
  NS_INIT_REFCNT();
  mNumberOfFilters = 0;
}

//-------------------------------------------------------------------------
//
// Show - Display the file dialog
//
//-------------------------------------------------------------------------

PRBool nsFileWidget::Show()
{
  PRBool result = PR_TRUE;
  nsFilePanelBeOS *ppanel;
  file_panel_mode panel_mode;

  if (mMode == eMode_load) {
    panel_mode = B_OPEN_PANEL;
  }
  else if (mMode == eMode_save) {
    panel_mode = B_SAVE_PANEL;
  }
  else {
    printf("nsFileWidget::Show() wrong mode");
    return PR_FALSE;
  }

  ppanel = new nsFilePanelBeOS(
      panel_mode,   //file_panel_mode mode
      B_FILE_NODE,  //uint32 node_flavors
      false,        //bool allow_multiple_selection
      false,        //bool modal
      true          //bool hide_when_done
      );
  if (!ppanel) return PR_FALSE;

  // set title
  if (mTitle.Length() > 0) {
    char *title_utf8 = mTitle.ToNewUTF8String();
    ppanel->Window()->SetTitle(title_utf8);
    Recycle(title_utf8);
  }

  // set default text
  if (mDefault.Length() > 0) {
    char *defaultText = mDefault.ToNewCString();
    ppanel->SetSaveText(defaultText);
    Recycle(defaultText);
  }

  // set initial directory
  nsString initialDirString;
  mDisplayDirectory.GetNativePathString(initialDirString);
  if (initialDirString.Length() > 0) {
    char *initialDir_utf8 = initialDirString.ToNewUTF8String();
    ppanel->SetPanelDirectory(initialDir_utf8);
    Recycle(initialDir_utf8);
  }

  // set modal feel
  if (ppanel->LockLooper()) {
    ppanel->Window()->SetFeel(B_MODAL_APP_WINDOW_FEEL);
    ppanel->UnlockLooper();
  }

  // Show File Panel
  ppanel->Show();
  ppanel->WaitForSelection();
  
  if (ppanel->IsCancelSelected()) {
    result = PR_FALSE;
  }

  if (mMode == eMode_load && ppanel->IsOpenSelected()) {
    BList *list = ppanel->OpenRefs();
    if ((list) && list->CountItems() >= 1) {
      entry_ref *ref = (entry_ref *)list->ItemAt(0);
      BPath path(ref);
      if (path.InitCheck() == B_OK) {
        mFile.SetLength(0);
        mFile.AppendWithConversion(path.Path());
      }
    }
  }
  else if (mMode == eMode_save && ppanel->IsSaveSelected()) {
    BString savefilename = ppanel->SaveFileName();
    entry_ref ref = ppanel->SaveDirRef();
    BPath path(&ref);
    if (path.InitCheck() == B_OK) {
      path.Append(savefilename.String(), true);
      mFile.SetLength(0);
      mFile.AppendWithConversion(path.Path());
    }
  }
  else {
    result = PR_FALSE;  
  }
  
  // set current directory to mDisplayDirectory
  entry_ref dir_ref;
  ppanel->GetPanelDirectory(&dir_ref);
  BEntry dir_entry(&dir_ref);
  BPath dir_path;
  dir_entry.GetPath(&dir_path);
  mDisplayDirectory = dir_path.Path();

  if (ppanel->Lock()) {
    ppanel->Quit();
  }
  
  return result;

// TODO: implement filters
}

//-------------------------------------------------------------------------
//
// Convert filter titles + filters into a Windows filter string
//
//-------------------------------------------------------------------------

void nsFileWidget::GetFilterListArray(nsString& aFilterList)
{
  aFilterList.SetLength(0);
  for (PRUint32 i = 0; i < mNumberOfFilters; i++) {
    const nsString& title = mTitles[i];
    const nsString& filter = mFilters[i];
    
    aFilterList.Append(title);
    aFilterList.AppendWithConversion("\0");
    aFilterList.Append(filter);
    aFilterList.AppendWithConversion("\0");
  }
  aFilterList.AppendWithConversion("\0"); 
}

//-------------------------------------------------------------------------
//
// Set the list of filters
//
//-------------------------------------------------------------------------

NS_METHOD nsFileWidget::SetFilterList(PRUint32 aNumberOfFilters,const nsString aTitles[],const nsString aFilters[])
{
  mNumberOfFilters  = aNumberOfFilters;
  mTitles           = aTitles;
  mFilters          = aFilters;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the file + path
//
//-------------------------------------------------------------------------

NS_METHOD  nsFileWidget::GetFile(nsFileSpec& aFile)
{
  nsFilePath filePath(mFile);
  nsFileSpec fileSpec(filePath);

  aFile = filePath;
  return NS_OK;

}


//-------------------------------------------------------------------------
//
// Get the file + path
//
//-------------------------------------------------------------------------
NS_METHOD  nsFileWidget::SetDefaultString(const nsString& aString)
{
  mDefault = aString;
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set the display directory
//
//-------------------------------------------------------------------------
NS_METHOD  nsFileWidget::SetDisplayDirectory(const nsFileSpec& aDirectory)
{
  mDisplayDirectory = aDirectory;
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get the display directory
//
//-------------------------------------------------------------------------
NS_METHOD  nsFileWidget::GetDisplayDirectory(nsFileSpec& aDirectory)
{
  aDirectory = mDisplayDirectory;
  return NS_OK;
}


//-------------------------------------------------------------------------
NS_METHOD nsFileWidget::Create(nsIWidget *aParent,
                       const nsString& aTitle,
                       nsFileDlgMode aMode,
                       nsIDeviceContext *aContext,
                       nsIAppShell *aAppShell,
                       nsIToolkit *aToolkit,
                       void *aInitData)
{
  mParentWindow = 0;
  if (aParent) {
    BView *view = (BView *) aParent->GetNativeData(NS_NATIVE_WIDGET);
    if (view && view->LockLooper()) {
      mParentWindow = view->Window();
      view->UnlockLooper();
    }
  }
  mTitle.SetLength(0);
  mTitle.Append(aTitle);
  mMode = aMode;
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// nsFileWidget destructor
//
//-------------------------------------------------------------------------
nsFileWidget::~nsFileWidget()
{
}


nsFileDlgResults nsFileWidget::GetFile(nsIWidget *aParent,
                                       const nsString &promptString,
                                       nsFileSpec &theFileSpec)
{
  Create(aParent, promptString, eMode_load, nsnull, nsnull);
  PRBool result = Show();
  nsFileDlgResults status = nsFileDlgResults_Cancel;
  if (result && mFile.Length() > 0) {
    nsFilePath filePath(mFile);
    nsFileSpec fileSpec(filePath);
    theFileSpec = fileSpec;
    status = nsFileDlgResults_OK;
  }
  return status;

}

nsFileDlgResults nsFileWidget::GetFolder(nsIWidget *aParent,
                                         const nsString &promptString,
                                         nsFileSpec &theFileSpec)
{
	Create(aParent, promptString, eMode_getfolder, nsnull, nsnull);
	if (Show() == PR_TRUE)
	{
		GetFile(theFileSpec);
		return nsFileDlgResults_OK;
	}

  return nsFileDlgResults_Cancel;
}

nsFileDlgResults nsFileWidget::PutFile(nsIWidget *aParent,
                                       const nsString &promptString,
                                       nsFileSpec &theFileSpec)
{ 
	Create(aParent, promptString, eMode_save, nsnull, nsnull);
	if (Show() == PR_TRUE)
	{
		GetFile(theFileSpec);
		return nsFileDlgResults_OK;
	}
  return nsFileDlgResults_Cancel; 
}

NS_METHOD  nsFileWidget::GetSelectedType(PRInt16& theType)
{
  theType = mSelectedType;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// BeOS native File Panel
//
//-------------------------------------------------------------------------

nsFilePanelBeOS::nsFilePanelBeOS(file_panel_mode mode, 
      uint32 node_flavors,
      bool allow_multiple_selection, 
      bool modal, 
      bool hide_when_done)
  : BLooper()
  , BFilePanel(mode,
      NULL, NULL,
      node_flavors,
      allow_multiple_selection,
      NULL, NULL,
      modal,
      hide_when_done)
  , mSelectedActivity(nsFilePanelBeOS::NOT_SELECTED)
  , mIsSelected(false)
  , mSaveFileName("")
  , mSaveDirRef()
  , mOpenRefs()
{
  if ((wait_sem = create_sem(1,"FilePanel")) < B_OK) 
  	printf("nsFilePanelBeOS::nsFilePanelBeOS : create_sem error\n");
  if (wait_sem > 0) acquire_sem(wait_sem);

  SetTarget(BMessenger(this));
  
  this->Run();
}

nsFilePanelBeOS::~nsFilePanelBeOS()
{
  int count = mOpenRefs.CountItems();
  for (int i=0 ; i<count ; i++) {
    delete mOpenRefs.ItemAt(i);
  }
  if (wait_sem > 0) {
    delete_sem(wait_sem);
  }
}

void nsFilePanelBeOS::MessageReceived(BMessage *msg)
{
  switch ( msg->what ) {
  case B_REFS_RECEIVED: // open
    int32 count;
    type_code code;
    msg->GetInfo("refs", &code, &count);
    if (code == B_REF_TYPE) {
      for (int i=0 ; i<count ; i++) {
        entry_ref *ref = new entry_ref;
        if (msg->FindRef("refs", i, ref) == B_OK) {
          mOpenRefs.AddItem((void *) ref);
        } else {
          delete ref;
        }
      }
    } else {
      printf("nsFilePanelBeOS::MessageReceived() no ref!\n");
    }
    mSelectedActivity = OPEN_SELECTED;
    mIsSelected = true;
    release_sem(wait_sem);
    break;

  case B_SAVE_REQUESTED: // save
    msg->FindString("name", &mSaveFileName);
    msg->FindRef("directory", &mSaveDirRef);
    mSelectedActivity = SAVE_SELECTED;
    mIsSelected = true;
    release_sem(wait_sem);
    break;

  case B_CANCEL: // cancel
    if (mIsSelected) break;
    mSelectedActivity = CANCEL_SELECTED;
    mIsSelected = true;
    release_sem(wait_sem);
    break;
  default:
    break;
  }
}

void nsFilePanelBeOS::WaitForSelection()
{
  if (wait_sem > 0) {
    acquire_sem(wait_sem);
    release_sem(wait_sem);
  }
}

uint32 nsFilePanelBeOS::SelectedActivity()
{
  uint32 result = 0;
  result = mSelectedActivity;

  return result;
}
