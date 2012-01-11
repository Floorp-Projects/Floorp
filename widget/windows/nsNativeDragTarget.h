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
 * The Original Code is Mozilla Communicator client code.
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
#ifndef _nsNativeDragTarget_h_
#define _nsNativeDragTarget_h_

#include "nsCOMPtr.h"
#include "nsIDragSession.h"
#include <ole2.h>
#include <shlobj.h>

#ifndef IDropTargetHelper
#ifndef __MINGW32__   // MingW does not provide shobjidl.h.
#include <shobjidl.h> // Vista drag image interfaces
#endif  // MingW
#endif

class nsIDragService;
class nsIWidget;

struct IDataObject;

/*
 * nsNativeDragTarget implements the IDropTarget interface and gets most of its
 * behavior from the associated adapter (m_dragDrop).
 */

class nsNativeDragTarget : public IDropTarget
{
public:
  nsNativeDragTarget(nsIWidget * aWnd);
  ~nsNativeDragTarget();

  // IUnknown members - see iunknown.h for documentation
  STDMETHODIMP QueryInterface(REFIID, void**);
  STDMETHODIMP_(ULONG) AddRef();
  STDMETHODIMP_(ULONG) Release();

  // IDataTarget members

  // Set pEffect based on whether this object can support a drop based on
  // the data available from pSource, the key and mouse states specified
  // in grfKeyState, and the coordinates specified by point. This is
  // called by OLE when a drag enters this object's window (as registered
  // by Initialize).
  STDMETHODIMP DragEnter(LPDATAOBJECT pSource, DWORD grfKeyState,
                         POINTL point, DWORD* pEffect);

  // Similar to DragEnter except it is called frequently while the drag
  // is over this object's window.
  STDMETHODIMP DragOver(DWORD grfKeyState, POINTL point, DWORD* pEffect);

  // Release the drag-drop source and put internal state back to the point
  // before the call to DragEnter. This is called when the drag leaves
  // without a drop occurring.
  STDMETHODIMP DragLeave();

  // If point is within our region of interest and pSource's data supports
  // one of our formats, get the data and set pEffect according to
  // grfKeyState (DROPEFFECT_MOVE if the control key was not pressed,
  // DROPEFFECT_COPY if the control key was pressed). Otherwise return
  // E_FAIL.
  STDMETHODIMP Drop(LPDATAOBJECT pSource, DWORD grfKeyState,
                    POINTL point, DWORD* pEffect);
  /**
   * Cancel the current drag session, if any.
   */
  void DragCancel();

protected:

  void GetGeckoDragAction(DWORD grfKeyState, LPDWORD pdwEffect, 
                          PRUint32 * aGeckoAction);
  void ProcessDrag(PRUint32 aEventType, DWORD grfKeyState,
                   POINTL pt, DWORD* pdwEffect);
  void DispatchDragDropEvent(PRUint32 aType, POINTL pt);
  void AddLinkSupportIfCanBeGenerated(LPDATAOBJECT aIDataSource);

  // Native Stuff
  ULONG            m_cRef;      // reference count
  HWND             mHWnd;
  DWORD            mEffectsAllowed;
  DWORD            mEffectsPreferred;
  bool             mTookOwnRef;

  // Gecko Stuff
  nsIWidget      * mWindow;
  nsIDragService * mDragService;

  // Drag target helper 
  IDropTargetHelper * mDropTargetHelper;
};

#endif // _nsNativeDragTarget_h_


