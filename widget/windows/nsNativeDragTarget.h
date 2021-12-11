/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _nsNativeDragTarget_h_
#define _nsNativeDragTarget_h_

#include "nsCOMPtr.h"
#include <ole2.h>
#include <shlobj.h>

#ifndef IDropTargetHelper
#  include <shobjidl.h>  // Vista drag image interfaces
#  undef LogSeverity     // SetupAPI.h #defines this as DWORD
#endif

#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"

class nsIDragService;
class nsIWidget;

/*
 * nsNativeDragTarget implements the IDropTarget interface and gets most of its
 * behavior from the associated adapter (m_dragDrop).
 */

class nsNativeDragTarget final : public IDropTarget {
 public:
  explicit nsNativeDragTarget(nsIWidget* aWidget);
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
  STDMETHODIMP DragEnter(LPDATAOBJECT pSource, DWORD grfKeyState, POINTL point,
                         DWORD* pEffect);

  // Similar to DragEnter except it is called frequently while the drag
  // is over this object's window.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY STDMETHODIMP DragOver(DWORD grfKeyState,
                                                    POINTL point,
                                                    DWORD* pEffect);

  // Release the drag-drop source and put internal state back to the point
  // before the call to DragEnter. This is called when the drag leaves
  // without a drop occurring.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY STDMETHODIMP DragLeave();

  // If point is within our region of interest and pSource's data supports
  // one of our formats, get the data and set pEffect according to
  // grfKeyState (DROPEFFECT_MOVE if the control key was not pressed,
  // DROPEFFECT_COPY if the control key was pressed). Otherwise return
  // E_FAIL.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY STDMETHODIMP Drop(LPDATAOBJECT pSource,
                                                DWORD grfKeyState, POINTL point,
                                                DWORD* pEffect);
  /**
   * Cancel the current drag session, if any.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void DragCancel();

  static void DragImageChanged() { gDragImageChanged = true; }

 protected:
  void GetGeckoDragAction(DWORD grfKeyState, LPDWORD pdwEffect,
                          uint32_t* aGeckoAction);
  void ProcessDrag(mozilla::EventMessage aEventMessage, DWORD grfKeyState,
                   POINTL pt, DWORD* pdwEffect);
  void DispatchDragDropEvent(mozilla::EventMessage aEventMessage,
                             const POINTL& aPT);
  void AddLinkSupportIfCanBeGenerated(LPDATAOBJECT aIDataSource);

  // Native Stuff
  ULONG m_cRef;  // reference count
  HWND mHWnd;
  DWORD mEffectsAllowed;
  DWORD mEffectsPreferred;
  bool mTookOwnRef;

  // Gecko Stuff
  nsIWidget* mWidget;
  nsIDragService* mDragService;
  // Drag target helper
  IDropTargetHelper* GetDropTargetHelper();

 private:
  // Drag target helper
  IDropTargetHelper* mDropTargetHelper;

  static bool gDragImageChanged;
};

#endif  // _nsNativeDragTarget_h_
