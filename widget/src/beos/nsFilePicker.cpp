/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s):
 *   Makoto Hamanaka <VYA04230@nifty.com>
 */

#include "nsCOMPtr.h"
#include "nsNetUtil.h"
#include "nsIServiceManager.h"
#define NS_IMPL_IDS
#include "nsIPlatformCharset.h"
#undef NS_IMPL_IDS
#include "nsFilePicker.h"
#include "nsILocalFile.h"
#include "nsIURL.h"
#include "nsIFileChannel.h"
#include "nsIStringBundle.h"
#include "nsReadableUtils.h"

#include <Window.h>
#include <View.h>

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

NS_IMPL_ISUPPORTS1(nsFilePicker, nsIFilePicker)

//-------------------------------------------------------------------------
//
// nsFilePicker constructor
//
//-------------------------------------------------------------------------
nsFilePicker::nsFilePicker()
  : mParentWindow(nsnull)
  , mUnicodeEncoder(nsnull)
  , mUnicodeDecoder(nsnull)
{
  NS_INIT_REFCNT();
  mDisplayDirectory = do_CreateInstance("@mozilla.org/file/local;1");
}

//-------------------------------------------------------------------------
//
// nsFilePicker destructor
//
//-------------------------------------------------------------------------
nsFilePicker::~nsFilePicker()
{
  NS_IF_RELEASE(mUnicodeEncoder);
  NS_IF_RELEASE(mUnicodeDecoder);
}

//-------------------------------------------------------------------------
//
// Show - Display the file dialog
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::Show(PRInt16 *retval)
{
  PRBool result = PR_TRUE;
  nsFilePanelBeOS *ppanel;
  file_panel_mode panel_mode;
  uint32 node_flavors;

  if (mMode == modeGetFolder) {
    node_flavors = B_DIRECTORY_NODE;
    panel_mode = B_OPEN_PANEL;
  }
  else if (mMode == modeOpen) {
    node_flavors = B_FILE_NODE;
    panel_mode = B_OPEN_PANEL;
  }
  else if (mMode == modeSave) {
    node_flavors = B_FILE_NODE;
    panel_mode = B_SAVE_PANEL;
  }
  else {
    printf("nsFilePicker::Show() wrong mode");
    return PR_FALSE;
  }

  ppanel = new nsFilePanelBeOS(
      panel_mode, //file_panel_mode mode
      node_flavors,  //uint32 node_flavors
      false,         //bool allow_multiple_selection
      false,        //bool modal
      true          //bool hide_when_done
      );
  if (!ppanel) return PR_FALSE;

  // set title
  if (mTitle.Length() > 0) {
    char *title_utf8 = ToNewUTF8String(mTitle);
    ppanel->Window()->SetTitle(title_utf8);
    Recycle(title_utf8);
  }

  // set default text
  if (mDefault.Length() > 0) {
    char *defaultText = ToNewCString(mDefault);
    ppanel->SetSaveText(defaultText);
    Recycle(defaultText);
  }

  // set initial directory
  char * initialDir;
  mDisplayDirectory->GetPath(&initialDir);
  if (initialDir)
    ppanel->SetPanelDirectory(initialDir);

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

  if (mMode == modeOpen && ppanel->IsOpenSelected()) {
    BList *list = ppanel->OpenRefs();
    if ((list) && list->CountItems() >= 1) {
      entry_ref *ref = (entry_ref *)list->ItemAt(0);
      BPath path(ref);
      if (path.InitCheck() == B_OK) {
        mFile.SetLength(0);
        mFile.Append(path.Path());
      } else {
        printf("path.init failed \n");
      }
    } else {
      printf("list not init \n");
    }
  }
  else if (mMode == modeSave && ppanel->IsSaveSelected()) {
    BString savefilename = ppanel->SaveFileName();
    entry_ref ref = ppanel->SaveDirRef();
    BPath path(&ref);
    if (path.InitCheck() == B_OK) {
      path.Append(savefilename.String(), true);
      mFile.SetLength(0);
      mFile.Append(path.Path());
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
  mDisplayDirectory->InitWithPath(dir_path.Path());

  if (ppanel->Lock()) {
    ppanel->Quit();
  }

  if (result) {
    PRInt16 returnOKorReplace = returnOK;

    if (mMode == modeSave) {
      // Windows does not return resultReplace,
      //   we must check if file already exists
      nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));

      NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

      file->InitWithPath(mFile.get());

      PRBool exists = PR_FALSE;
      file->Exists(&exists);
      if (exists)
        returnOKorReplace = returnReplace;
    }
    *retval = returnOKorReplace;
  }
  else {
    *retval = returnCancel;
  }
  return NS_OK;

// TODO: implement filters
}



NS_IMETHODIMP nsFilePicker::GetFile(nsILocalFile **aFile)
{
  NS_ENSURE_ARG_POINTER(aFile);

  if (mFile.IsEmpty())
      return NS_OK;

  nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
    
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

  file->InitWithPath(mFile);

  NS_ADDREF(*aFile = file);

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetFileURL(nsIFileURL **aFileURL)
{
  nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);
  file->InitWithPath(mFile);

  nsCOMPtr<nsIURI> uri;
  NS_NewFileURI(getter_AddRefs(uri), file);
  nsCOMPtr<nsIFileURL> fileURL(do_QueryInterface(uri));
  NS_ENSURE_TRUE(fileURL, NS_ERROR_FAILURE);
  
  NS_ADDREF(*aFileURL = fileURL);

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the file + path
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::SetDefaultString(const PRUnichar *aString)
{
  mDefault = aString;
  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::GetDefaultString(PRUnichar **aString)
{
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
//
// Set the display directory
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::SetDisplayDirectory(nsILocalFile *aDirectory)
{
  mDisplayDirectory = aDirectory;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the display directory
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetDisplayDirectory(nsILocalFile **aDirectory)
{
  *aDirectory = mDisplayDirectory;
  NS_IF_ADDREF(*aDirectory);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::InitNative(nsIWidget *aParent,
                                       const PRUnichar *aTitle,
                                       PRInt16 aMode)
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


#if 0
//-------------------------------------------------------------------------
void nsFilePicker::GetFileSystemCharset(nsString & fileSystemCharset)
{
  static nsAutoString aCharset;
  nsresult rv;

  if (aCharset.Length() < 1) {
    nsCOMPtr <nsIPlatformCharset> platformCharset = do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv);
	  if (NS_SUCCEEDED(rv)) 
		  rv = platformCharset->GetCharset(kPlatformCharsetSel_FileName, aCharset);

    NS_ASSERTION(NS_SUCCEEDED(rv), "error getting platform charset");
	  if (NS_FAILED(rv)) 
		  aCharset.AssignWithConversion("UTF-8");// XXX ok?
  }
  fileSystemCharset = aCharset;
}

//-------------------------------------------------------------------------
char * nsFilePicker::ConvertToFileSystemCharset(const PRUnichar *inString)
{
  char *outString = nsnull;
  nsresult rv = NS_OK;

  // get file system charset and create a unicode encoder
  if (!mUnicodeEncoder) {
    nsAutoString fileSystemCharset;
    GetFileSystemCharset(fileSystemCharset);

    nsCOMPtr<nsICharsetConverterManager> ccm = 
             do_GetService(kCharsetConverterManagerCID, &rv); 
    if (NS_SUCCEEDED(rv)) {
      rv = ccm->GetUnicodeEncoder(&fileSystemCharset, &mUnicodeEncoder);
    }
  }

  // converts from unicode to the file system charset
  if (NS_SUCCEEDED(rv)) {
    PRInt32 inLength = nsCRT::strlen(inString);
    PRInt32 outLength;
    rv = mUnicodeEncoder->GetMaxLength(inString, inLength, &outLength);
    if (NS_SUCCEEDED(rv)) {
      outString = NS_STATIC_CAST( char*, nsMemory::Alloc( outLength+1 ) );
      if (!outString) {
        return nsnull;
      }
      rv = mUnicodeEncoder->Convert(inString, &inLength, outString, &outLength);
      if (NS_SUCCEEDED(rv)) {
        outString[outLength] = '\0';
      }
    }
  }
  
  return NS_SUCCEEDED(rv) ? outString : nsnull;
}

//-------------------------------------------------------------------------
PRUnichar * nsFilePicker::ConvertFromFileSystemCharset(const char *inString)
{
  PRUnichar *outString = nsnull;
  nsresult rv = NS_OK;

  // get file system charset and create a unicode encoder
  if (!mUnicodeDecoder) {
    nsAutoString fileSystemCharset;
    GetFileSystemCharset(fileSystemCharset);

    nsCOMPtr<nsICharsetConverterManager> ccm = 
             do_GetService(kCharsetConverterManagerCID, &rv); 
    if (NS_SUCCEEDED(rv)) {
      rv = ccm->GetUnicodeDecoder(&fileSystemCharset, &mUnicodeDecoder);
    }
  }

  // converts from the file system charset to unicode
  if (NS_SUCCEEDED(rv)) {
    PRInt32 inLength = nsCRT::strlen(inString);
    PRInt32 outLength;
    rv = mUnicodeDecoder->GetMaxLength(inString, inLength, &outLength);
    if (NS_SUCCEEDED(rv)) {
      outString = NS_STATIC_CAST( PRUnichar*, nsMemory::Alloc( (outLength+1) * sizeof( PRUnichar ) ) );
      if (!outString) {
        return nsnull;
      }
      rv = mUnicodeDecoder->Convert(inString, &inLength, outString, &outLength);
      if (NS_SUCCEEDED(rv)) {
        outString[outLength] = 0;
      }
    }
  }

  NS_ASSERTION(NS_SUCCEEDED(rv), "error charset conversion");
  return NS_SUCCEEDED(rv) ? outString : nsnull;
}
#endif

NS_IMETHODIMP
nsFilePicker::AppendFilter(const PRUnichar *aTitle, const PRUnichar *aFilter)
{
  mFilterList.Append(aTitle);
  mFilterList.AppendWithConversion('\0');
  mFilterList.Append(aFilter);
  mFilterList.AppendWithConversion('\0');

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
