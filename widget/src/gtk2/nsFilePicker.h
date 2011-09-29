/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Mozilla GTK2 File Chooser.
 *
 * The Initial Developer of the Original Code is Red Hat, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Aillon <caillon@redhat.com>
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

#include <gtk/gtk.h>

#include "nsBaseFilePicker.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsCOMArray.h"

class nsIWidget;
class nsILocalFile;

class nsFilePicker : public nsBaseFilePicker
{
public:
  nsFilePicker();
  virtual ~nsFilePicker();

  NS_DECL_ISUPPORTS

  // nsIFilePicker (less what's in nsBaseFilePicker)
  NS_IMETHODIMP AppendFilters(PRInt32 aFilterMask);
  NS_IMETHODIMP AppendFilter(const nsAString& aTitle, const nsAString& aFilter);
  NS_IMETHODIMP SetDefaultString(const nsAString& aString);
  NS_IMETHODIMP GetDefaultString(nsAString& aString);
  NS_IMETHODIMP SetDefaultExtension(const nsAString& aExtension);
  NS_IMETHODIMP GetDefaultExtension(nsAString& aExtension);
  NS_IMETHODIMP GetFilterIndex(PRInt32 *aFilterIndex);
  NS_IMETHODIMP SetFilterIndex(PRInt32 aFilterIndex);
  NS_IMETHODIMP GetFile(nsILocalFile **aFile);
  NS_IMETHODIMP GetFileURL(nsIURI **aFileURL);
  NS_IMETHODIMP GetFiles(nsISimpleEnumerator **aFiles);
  NS_IMETHODIMP Show(PRInt16 *aReturn);

  virtual void InitNative(nsIWidget *aParent, const nsAString& aTitle, PRInt16 aMode);

  static void Shutdown();

protected:

  void ReadValuesFromFileChooser(GtkWidget *file_chooser);

  nsCOMPtr<nsIWidget>    mParentWidget;
  nsCOMArray<nsILocalFile> mFiles;

  PRInt16   mMode;
  PRInt16   mSelectedType;
  bool      mAllowURLs;
  nsCString mFileURL;
  nsString  mTitle;
  nsString  mDefault;
  nsString  mDefaultExtension;

  nsTArray<nsCString> mFilters;
  nsTArray<nsCString> mFilterNames;

private:
  static nsILocalFile *mPrevDisplayDirectory;
};

#endif
