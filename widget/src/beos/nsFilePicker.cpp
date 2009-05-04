/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Makoto Hamanaka <VYA04230@nifty.com>
 *   Paul Ashford <arougthopher@lizardland.net>
 *   Sergei Dolgov <sergei_d@fi.tartu.ee>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsCOMPtr.h"
#include "nsNetUtil.h"
#include "nsIServiceManager.h"
#include "nsIPlatformCharset.h"
#include "nsFilePicker.h"
#include "nsILocalFile.h"
#include "nsIURL.h"
#include "nsIStringBundle.h"
#include "nsReadableUtils.h"
#include "nsEscape.h"
#include "nsEnumeratorUtils.h"
#include "nsString.h"
#include <Window.h>
#include <View.h>
#include <Button.h>

NS_IMPL_THREADSAFE_ISUPPORTS1(nsFilePicker, nsIFilePicker)

#ifdef FILEPICKER_SAVE_LAST_DIR
char nsFilePicker::mLastUsedDirectory[B_PATH_NAME_LENGTH+1] = { 0 };
#endif

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
	bool allow_multiple_selection = false;
	uint32 node_flavors;

	if (mMode == modeGetFolder) {
		node_flavors = B_DIRECTORY_NODE;
		panel_mode = B_OPEN_PANEL;
	}
	else if (mMode == modeOpen) {
		node_flavors = B_FILE_NODE;
		panel_mode = B_OPEN_PANEL;
	}
	else if (mMode == modeOpenMultiple) {
		node_flavors = B_FILE_NODE;
		panel_mode = B_OPEN_PANEL;
		allow_multiple_selection = true;
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
	             allow_multiple_selection,  //bool allow_multiple_selection
	             false, //bool modal
	             true //bool hide_when_done
	         );
	if (!ppanel) return PR_FALSE;

	// set title
	if (!mTitle.IsEmpty()) {
		char *title_utf8 = ToNewUTF8String(mTitle);
		ppanel->Window()->SetTitle(title_utf8);
		Recycle(title_utf8);
	}

	// set default text
	if (!mDefault.IsEmpty()) {
		char *defaultText = ToNewUTF8String(mDefault);
		ppanel->SetSaveText(defaultText);
		Recycle(defaultText);
	}

	// set initial directory
	nsCAutoString initialDir;
	if (mDisplayDirectory)
		mDisplayDirectory->GetNativePath(initialDir);
	if(initialDir.IsEmpty()) {
#ifdef FILEPICKER_SAVE_LAST_DIR		
		if (strlen(mLastUsedDirectory) < 2)
			initialDir.Assign("/boot/home");
		else
			initialDir.Assign(mLastUsedDirectory);
#else
		ppanel->SetPanelDirectory(initialDir.get());
#endif			
	}

#ifdef FILEPICKER_SAVE_LAST_DIR
	ppanel->SetPanelDirectory(initialDir.get());
#endif

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

	if ((mMode == modeOpen || mMode == modeOpenMultiple || mMode == modeGetFolder) && ppanel->IsOpenSelected()) {
		BList *list = ppanel->OpenRefs();
		uint32 numfiles = list->CountItems();
		if ((list) && numfiles >= 1) {
			nsresult rv;
			for (uint32 i = 0; i< numfiles; i++) {
				BPath *path = (BPath *)list->ItemAt(i);

				if (path->InitCheck() == B_OK) {
					mFile.Truncate();
					// Single and Multiple are exclusive now, though, maybe there is sense
					// to assign also first list element to mFile even in openMultiple case ?
					if (mMode == modeOpenMultiple) {
						nsCOMPtr<nsILocalFile> file = do_CreateInstance("@mozilla.org/file/local;1", &rv);
						NS_ENSURE_SUCCESS(rv,rv);
						rv = file->InitWithNativePath(nsDependentCString(path->Path()));
						NS_ENSURE_SUCCESS(rv,rv);
						rv = mFiles.AppendObject(file);
						NS_ENSURE_SUCCESS(rv,rv);
					} else {
						if (i == 0) mFile.Assign(path->Path());
					}
				} else {
					printf("path.init failed \n");
				}
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
			mFile.Assign(path.Path());
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
	if (!mDisplayDirectory)
		mDisplayDirectory = do_CreateInstance("@mozilla.org/file/local;1");
	if (mDisplayDirectory)
		mDisplayDirectory->InitWithNativePath(nsDependentCString(dir_path.Path()));

	if (ppanel->Lock()) {
		ppanel->Quit();
	}

	if (result) {
		PRInt16 returnOKorReplace = returnOK;

#ifdef FILEPICKER_SAVE_LAST_DIR
		strncpy(mLastUsedDirectory, dir_path.Path(), B_PATH_NAME_LENGTH+1);
		if (mDisplayDirectory)
			mDisplayDirectory->InitWithNativePath( nsDependentCString(mLastUsedDirectory) );
#endif

		if (mMode == modeSave) {
			//   we must check if file already exists
			PRBool exists = PR_FALSE;
			nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
			NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

			file->InitWithNativePath(mFile);
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

	file->InitWithNativePath(mFile);

	NS_ADDREF(*aFile = file);

	return NS_OK;
}

NS_IMETHODIMP nsFilePicker::GetFiles(nsISimpleEnumerator **aFiles)
{
	NS_ENSURE_ARG_POINTER(aFiles);
	return NS_NewArrayEnumerator(aFiles, mFiles);
}

//-------------------------------------------------------------------------

NS_IMETHODIMP nsFilePicker::GetFileURL(nsIURI **aFileURL)
{
	*aFileURL = nsnull;
	nsCOMPtr<nsILocalFile> file;
	nsresult rv = GetFile(getter_AddRefs(file));
	if (!file)
		return rv;

	return NS_NewFileURI(aFileURL, file);
}

//-------------------------------------------------------------------------
//
// Get the file + path
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::SetDefaultString(const nsAString& aString)
{
	mDefault = aString;
	return NS_OK;
}

NS_IMETHODIMP nsFilePicker::GetDefaultString(nsAString& aString)
{
	return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
//
// The default extension to use for files
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetDefaultExtension(nsAString& aExtension)
{
	aExtension.Truncate();
	return NS_OK;
}

NS_IMETHODIMP nsFilePicker::SetDefaultExtension(const nsAString& aExtension)
{
	return NS_OK;
}

//-------------------------------------------------------------------------
void nsFilePicker::InitNative(nsIWidget *aParent,
                              const nsAString& aTitle,
                              PRInt16 aMode)
{
	mParentWindow = 0;

  BView *view = (BView *) aParent->GetNativeData(NS_NATIVE_WIDGET);
  if (view && view->LockLooper()) {
    mParentWindow = view->Window();
    view->UnlockLooper();
  }

	mTitle.Assign(aTitle);
	mMode = aMode;
}

NS_IMETHODIMP
nsFilePicker::AppendFilter(const nsAString& aTitle, const nsAString& aFilter)
{
	mFilterList.Append(aTitle);
	mFilterList.Append(PRUnichar('\0'));
	mFilterList.Append(aFilter);
	mFilterList.Append(PRUnichar('\0'));

	return NS_OK;
}

//-------------------------------------------------------------------------
//
// BeOS native File Panel
//
//-------------------------------------------------------------------------

// Internal message for when the 'Select <dir>' button is used
const uint32 MSG_DIRECTORY = 'mDIR';

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
	
	if ( mode == B_OPEN_PANEL && node_flavors == B_DIRECTORY_NODE ) 
	{
		// Add a 'Select <dirname>' button to the open dialog
		Window()->Lock();
		
		BView *background = Window()->ChildAt(0); 
		entry_ref ref;
		char label[10+B_FILE_NAME_LENGTH];
		GetPanelDirectory(&ref);
		sprintf(label, "Select '%s'", ref.name);
		mDirectoryButton = new BButton(
			BRect(113, background->Bounds().bottom-35, 269, background->Bounds().bottom-10),
			"directoryButton", label, new BMessage(MSG_DIRECTORY), B_FOLLOW_LEFT | B_FOLLOW_BOTTOM);
		
		if(mDirectoryButton)
		{
			background->AddChild(mDirectoryButton);
			mDirectoryButton->SetTarget(Messenger());
		}
		else
			NS_ASSERTION(false, "Out of memory: failed to create mDirectoryButton");
		
		SetButtonLabel(B_DEFAULT_BUTTON, "Select");
		
		Window()->Unlock();
	}
	else 
		mDirectoryButton = nsnull;

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
				// XXX change - adding BPaths * to list instead entry_refs,
				// entry_refs are too unsafe objects in our case.
				entry_ref ref;
				if (msg->FindRef("refs", i, &ref) == B_OK) {
					BPath *path = new BPath(&ref);
					mOpenRefs.AddItem((void *) path);
				}
			}
		} else {
			printf("nsFilePanelBeOS::MessageReceived() no ref!\n");
		}
		mSelectedActivity = OPEN_SELECTED;
		mIsSelected = true;
		release_sem(wait_sem);
		break;
	
	case MSG_DIRECTORY: // Directory selected
	{
		entry_ref ref;
		GetPanelDirectory(&ref);
		BPath *path = new BPath(&ref);
		mOpenRefs.AddItem((void *) path);
		mSelectedActivity = OPEN_SELECTED;
		mIsSelected = true;
		release_sem(wait_sem);
		break;
	}

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

void nsFilePanelBeOS::SelectionChanged(void)
{
	if(mDirectoryButton)
	{
		//Update the 'Select <dir>' button
		entry_ref ref;
		char label[50];
		GetPanelDirectory(&ref);
		Window()->Lock();
		sprintf(label, "Select '%s'", ref.name);
		mDirectoryButton->SetLabel(label);
		Window()->Unlock();
	}
	BFilePanel::SelectionChanged();
}

