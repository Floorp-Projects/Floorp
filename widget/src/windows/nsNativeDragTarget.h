/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#ifndef _nsNativeDragTarget_h_
#define _nsNativeDragTarget_h_

#include "nsGUIEvent.h"

class nsIDragService;
class nsIWidget;

struct IDataObject;

/*
 * nsNativeDragTarget implements the IDropTarget interface and gets most of its
 * behavior from the associated adapter (m_dragDrop).
 */

class nsNativeDragTarget : public IDropTarget
{
	public: // construction, destruction

		nsNativeDragTarget(nsIWidget * aWnd);
		~nsNativeDragTarget();

	public: // IUnknown members - see iunknown.h for documentation
		STDMETHODIMP         QueryInterface(REFIID, void**);
		STDMETHODIMP_(ULONG) AddRef        ();
		STDMETHODIMP_(ULONG) Release       ();

	public: // IDataTarget members

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

  protected:
    // methods
    void GetGeckoDragAction(DWORD grfKeyState, LPDWORD pdwEffect, PRUint32 * aGeckoAction);
    void ProcessDrag(PRUint32     aEventType, 
                     DWORD        grfKeyState,
							       POINTL       pt, 
                     DWORD*       pdwEffect);
    void DispatchDragDropEvent(PRUint32 aType, POINTL pt);

    // Native Stuff
    ULONG            m_cRef;      // reference count
    HWND             mHWnd;
    IDataObject *    mDataObj;

    // Gecko Stuff
    nsIWidget      * mWindow;
    nsIDragService * mDragService;
};

#endif // _nsNativeDragTarget_h_


