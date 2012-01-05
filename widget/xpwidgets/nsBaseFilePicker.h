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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Mike Pinkerton   <pinkerton@netscape.com>
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
