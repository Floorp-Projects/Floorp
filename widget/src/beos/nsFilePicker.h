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

#ifndef nsFilePicker_h__
#define nsFilePicker_h__

#ifndef DEBUG
// XXX adding mLastUsedDirectory and code for handling last used folder on File Open/Save
// caused crashes in DEBUG builds on some machines - see comments for bug 177720 
// works well on "normal" builds
#define FILEPICKER_SAVE_LAST_DIR 1
#endif

#include "nsICharsetConverterManager.h"
#include "nsBaseFilePicker.h"
#include "nsString.h"
#include "nsdefs.h"
#include "nsIFileChannel.h"
#include "nsILocalFile.h"
#include "nsISimpleEnumerator.h"
#include "nsCOMArray.h"

//
// Native BeOS FileSelector wrapper
//

#include <StorageKit.h>
#include <Message.h>
#include <Window.h>
#include <String.h>
class BButton;

class nsFilePicker : public nsBaseFilePicker
{
public:
  NS_DECL_ISUPPORTS

  nsFilePicker();
  virtual ~nsFilePicker();

  // nsIFilePicker (less what's in nsBaseFilePicker)
  NS_IMETHOD GetDefaultString(nsAString& aDefaultString);
  NS_IMETHOD SetDefaultString(const nsAString& aDefaultString);
  NS_IMETHOD GetDefaultExtension(nsAString& aDefaultExtension);
  NS_IMETHOD SetDefaultExtension(const nsAString& aDefaultExtension);
  NS_IMETHOD GetFile(nsILocalFile * *aFile);
  NS_IMETHOD GetFileURL(nsIURI * *aFileURL);
  NS_IMETHOD GetFiles(nsISimpleEnumerator **aFiles);
  NS_IMETHOD Show(PRInt16 *_retval);
  NS_IMETHOD AppendFilter(const nsAString& aTitle, const nsAString& aFilter);

protected:
  // method from nsBaseFilePicker
  virtual void InitNative(nsIWidget *aParent, const nsAString& aTitle,
                          PRInt16 aMode);


  void GetFilterListArray(nsString& aFilterList);

  BWindow*                      mParentWindow;
  nsString                      mTitle;
  PRInt16                       mMode;
  nsCString                     mFile;
  nsString                      mDefault;
  nsString                      mFilterList;
  nsIUnicodeEncoder*            mUnicodeEncoder;
  nsIUnicodeDecoder*            mUnicodeDecoder;
  PRInt16                       mSelectedType;
  nsCOMArray<nsILocalFile>      mFiles;

#ifdef FILEPICKER_SAVE_LAST_DIR
  static char                      mLastUsedDirectory[];
#endif
};

class nsFilePanelBeOS : public BLooper, public BFilePanel
{
public:
  nsFilePanelBeOS(file_panel_mode mode,
                  uint32 node_flavors,
                  bool allow_multiple_selection,
                  bool modal,
                  bool hide_when_done);
  virtual ~nsFilePanelBeOS();

  virtual void MessageReceived(BMessage *message);
  virtual void WaitForSelection();

  virtual void SelectionChanged();

  virtual bool IsOpenSelected() {
    return (SelectedActivity() == OPEN_SELECTED);
  }
  virtual bool IsSaveSelected() {
    return (SelectedActivity() == SAVE_SELECTED);
  }
  virtual bool IsCancelSelected() {
    return (SelectedActivity() == CANCEL_SELECTED);
  }
  virtual uint32 SelectedActivity();

  virtual BList *OpenRefs() { return &mOpenRefs; }
  virtual BString SaveFileName() { return mSaveFileName; }
  virtual entry_ref SaveDirRef() { return mSaveDirRef; }

  enum {
      NOT_SELECTED    = 0,
      OPEN_SELECTED   = 1,
      SAVE_SELECTED   = 2,
      CANCEL_SELECTED = 3
  };

protected:
  BButton *mDirectoryButton;
  sem_id wait_sem ;
  uint32 mSelectedActivity;
  bool mIsSelected;
  BString mSaveFileName;
  entry_ref mSaveDirRef;
  BList mOpenRefs;
};

#endif // nsFilePicker_h__
