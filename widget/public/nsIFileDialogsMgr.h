/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsIFileDialogsMgr_h__
#define nsIFileDialogsMgr_h__

#include "nsISupports.h"
#include "nsFileSpec.h"

// {0ef98781-e34b-11d2-b345-00a0cc3c1cde}
#define NS_IFILEDIALOGSMGR_IID \
{ 0xef98781, 0xe34b, 0x11d2, { 0xb3, 0x45, 0x0, 0xa0, 0xcc, 0x3c, 0x1c, 0xde } }

#define NS_FILEDIALOGSMGR_CID      \
{ 0xef98784, 0xe34b, 0x11d2, { 0xb3, 0x45, 0x0, 0xa0, 0xcc, 0x3c, 0x1c, 0xde } }

enum nsFileDlgResults {
  nsFileDlgResults_Cancel,    // User hit cancel, ignore selection
  nsFileDlgResults_OK,        // User hit Ok, process selection 
  nsFileDlgResults_Replace    // User acknowledged file already exists so ok to replace, process selection
};

/**
 * (native) File Dialogs utility.
 * Provides an XP wrapper to platform native file dialogs:
 *  GetFile   - Presents a file browser and returns an nsFileSpec for the selected file
 *  GetFolder - Presents a folder/path selection dialog and returns an nsFileSpec
 *  PutFile   - Presents a file save dialog to the user and returns an nsFileSpec with
 *              the name and path to save the file
 * 
 */

class nsIFileDialogsMgr : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFILEDIALOGSMGR_IID)
    
  NS_IMETHOD GetFile(
    nsFileSpec       & theFileSpec,     // Populate with initial path for file dialog
    nsFileDlgResults & theResult,       // Result from the file selection dialog prompt
    const nsString   * promptString,    // Window title for file selection dialog
    void             * filterList) = 0;
    
  NS_IMETHOD GetFolder(
    nsFileSpec       & theFileSpec,     // Populate with initial path for file dialog 
    nsFileDlgResults & theResult,       // Result from the folder selection dialog prompt
    const nsString   * promptString) = 0;  // Window title for folder selection dialog
    
  NS_IMETHOD PutFile(
    nsFileSpec       & theFileSpec,     // Populate with initial path for file dialog 
    nsFileDlgResults & theResult,       // Result from the file save dialog prompt
    const nsString   * promptString) = 0;  // Window title for file save dialog

};

#endif // nsIFileDialogsMgr_h__
