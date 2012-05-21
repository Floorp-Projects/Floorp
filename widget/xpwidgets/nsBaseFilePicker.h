/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBaseFilePicker_h__
#define nsBaseFilePicker_h__

#include "nsISupports.h"
#include "nsIFilePicker.h"
#include "nsISimpleEnumerator.h"
#include "nsArrayEnumerator.h"
#include "nsCOMPtr.h"

class nsIWidget;

#define BASEFILEPICKER_HAS_DISPLAYDIRECTORY 1

class nsBaseFilePicker : public nsIFilePicker
{
public:
  nsBaseFilePicker(); 
  virtual ~nsBaseFilePicker();

  NS_IMETHOD Init(nsIDOMWindow *aParent,
                  const nsAString& aTitle,
                  PRInt16 aMode);

  NS_IMETHOD AppendFilters(PRInt32 filterMask);
  NS_IMETHOD GetFilterIndex(PRInt32 *aFilterIndex);
  NS_IMETHOD SetFilterIndex(PRInt32 aFilterIndex);
  NS_IMETHOD GetFiles(nsISimpleEnumerator **aFiles);
#ifdef BASEFILEPICKER_HAS_DISPLAYDIRECTORY 
  NS_IMETHOD GetDisplayDirectory(nsILocalFile * *aDisplayDirectory);
  NS_IMETHOD SetDisplayDirectory(nsILocalFile * aDisplayDirectory);
#endif
  NS_IMETHOD GetAddToRecentDocs(bool *aFlag);
  NS_IMETHOD SetAddToRecentDocs(bool aFlag);

protected:

  virtual void InitNative(nsIWidget *aParent, const nsAString& aTitle,
                          PRInt16 aMode) = 0;

  bool mAddToRecentDocs;
#ifdef BASEFILEPICKER_HAS_DISPLAYDIRECTORY 
  nsCOMPtr<nsILocalFile> mDisplayDirectory;
#endif
};

#endif // nsBaseFilePicker_h__
