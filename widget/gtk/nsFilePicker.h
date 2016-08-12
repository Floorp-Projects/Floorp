/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFilePicker_h__
#define nsFilePicker_h__

#include <gtk/gtk.h>

#include "nsBaseFilePicker.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsCOMArray.h"

class nsIWidget;
class nsIFile;

class nsFilePicker : public nsBaseFilePicker
{
public:
  nsFilePicker();

  NS_DECL_ISUPPORTS

  // nsIFilePicker (less what's in nsBaseFilePicker)
  NS_IMETHOD Open(nsIFilePickerShownCallback *aCallback) override;
  NS_IMETHOD AppendFilters(int32_t aFilterMask) override;
  NS_IMETHOD AppendFilter(const nsAString& aTitle,
                          const nsAString& aFilter) override;
  NS_IMETHOD SetDefaultString(const nsAString& aString) override;
  NS_IMETHOD GetDefaultString(nsAString& aString) override;
  NS_IMETHOD SetDefaultExtension(const nsAString& aExtension) override;
  NS_IMETHOD GetDefaultExtension(nsAString& aExtension) override;
  NS_IMETHOD GetFilterIndex(int32_t *aFilterIndex) override;
  NS_IMETHOD SetFilterIndex(int32_t aFilterIndex) override;
  NS_IMETHOD GetFile(nsIFile **aFile) override;
  NS_IMETHOD GetFileURL(nsIURI **aFileURL) override;
  NS_IMETHOD GetFiles(nsISimpleEnumerator **aFiles) override;
  NS_IMETHOD Show(int16_t *aReturn) override;

  // nsBaseFilePicker
  virtual void InitNative(nsIWidget *aParent,
                          const nsAString& aTitle) override;

  static void Shutdown();

protected:
  virtual ~nsFilePicker();

  void ReadValuesFromFileChooser(GtkWidget *file_chooser);

  static void OnResponse(GtkWidget* dialog, gint response_id,
                         gpointer user_data);
  static void OnDestroy(GtkWidget* dialog, gpointer user_data);
  void Done(GtkWidget* dialog, gint response_id);

  nsCOMPtr<nsIWidget>    mParentWidget;
  nsCOMPtr<nsIFilePickerShownCallback> mCallback;
  nsCOMArray<nsIFile> mFiles;

  int16_t   mSelectedType;
  int16_t   mResult;
  bool      mRunning;
  bool      mAllowURLs;
  nsCString mFileURL;
  nsString  mTitle;
  nsString  mDefault;
  nsString  mDefaultExtension;

  nsTArray<nsCString> mFilters;
  nsTArray<nsCString> mFilterNames;

private:
  static nsIFile *mPrevDisplayDirectory;

#if (MOZ_WIDGET_GTK == 3)
  GtkFileChooserWidget *mFileChooserDelegate;
#endif
};

#endif
