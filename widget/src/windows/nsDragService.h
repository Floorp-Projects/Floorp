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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsDragService_h__
#define nsDragService_h__

#include "nsBaseDragService.h"
#include <windows.h>
#include <shlobj.h>

struct IDropSource;
struct IDataObject;
class  nsNativeDragTarget;
class  nsDataObjCollection;
class  nsString;

// some older versions of MS PSDK do not have definitions for these, even though
// the functions are there and exported from the shell32.dll,
// so I had to add them manually. For example if one tries to build without those
// using VC6 standard libs one will get an error.
#ifndef SHCNE_RENAMEITEM_DEF
#define SHCNE_RENAMEITEM_DEF 0x00000001

// Structure
typedef struct {
    LPCITEMIDLIST pidl;
    BOOL fRecursive;
} SHChangeNotifyStruct;
#endif // SHCNE_RENAMEITEM_DEF

// Register for shell notifications func ptr
typedef WINSHELLAPI ULONG (WINAPI *SHCNRegPtr)(HWND hWnd,                      // Ordinal of 2
                                               int fSources,
                                               LONG fEvents,
                                               UINT wMsg,
                                               int cEntries,
                                               SHChangeNotifyStruct *pschn);
// Unregister form from the shell notifications func ptr
typedef WINSHELLAPI BOOL (WINAPI *SHCNDeregPtr)(ULONG ulID);                   // Ordinal of 4

/**
 * Native Win32 DragService wrapper
 */

class nsDragService : public nsBaseDragService
{
public:
  nsDragService();
  virtual ~nsDragService();
  
  // nsIDragService
  NS_IMETHOD InvokeDragSession(nsIDOMNode *aDOMNode,
                               nsISupportsArray *anArrayTransferables,
                               nsIScriptableRegion *aRegion,
                               PRUint32 aActionType);

  // nsIDragSession
  NS_IMETHOD GetData(nsITransferable * aTransferable, PRUint32 anItem);
  NS_IMETHOD GetNumDropItems(PRUint32 * aNumItems);
  NS_IMETHOD IsDataFlavorSupported(const char *aDataFlavor, PRBool *_retval);
  NS_IMETHOD EndDragSession();

  // native impl.
  NS_IMETHOD SetIDataObject(IDataObject * aDataObj);
  NS_IMETHOD StartInvokingDragSession(IDataObject * aDataObj,
                                      PRUint32 aActionType);

protected:
  nsDataObjCollection* GetDataObjCollection(IDataObject * aDataObj);

  // determine if we have a single data object or one of our private
  // collections
  PRBool IsCollectionObject(IDataObject* inDataObj);

  // Begin monitoring the shell for events (this would allow to figure drop location)
  PRBool StartWatchingShell(const nsAString& aFileName);

  // Should be called after drag is over to get the drop path (if any)
  PRBool GetDropPath(nsAString& aDropPath) const;

  // Hidden window procedure - here we shall receive notification from the shell
  static LRESULT WINAPI HiddenWndProc(HWND aWnd, UINT aMsg, WPARAM awParam, LPARAM alParam);

  IDropSource * mNativeDragSrc;
  nsNativeDragTarget * mNativeDragTarget;
  IDataObject * mDataObject;

  static PRUnichar *mFileName;    // File name to look for
  static PRUnichar *mDropPath;    // Drop path
  HWND            mHiddenWnd;     // Handle to a hidden window for notifications
  PRInt32         mNotifyHandle;  // Handle to installed hook (SHChangeNotify)
};

#endif // nsDragService_h__
