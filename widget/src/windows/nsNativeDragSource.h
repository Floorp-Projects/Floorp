/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef _nsNativeDragSource_h_
#define _nsNativeDragSource_h_

#include <OLE2.h>
#include <OLEIDL.h>

//class nsIDragSource;

/*
 * nsNativeDragSource implements the IDropSource interface and gets most of its
 * behavior from the associated adapter (m_dragDrop).
 */
class nsNativeDragSource : public IDropSource
{
	public: // construction, destruction

		// construct an nsNativeDragSource referencing adapter
		//nsNativeDragSource(nsIDragSource * adapter);
		nsNativeDragSource();
		~nsNativeDragSource();

	public: // IUnknown methods - see iunknown.h for documentation

		STDMETHODIMP         QueryInterface(REFIID, void**);
		STDMETHODIMP_(ULONG) AddRef        ();
		STDMETHODIMP_(ULONG) Release       ();

	public: // IDropSource methods

		// Return DRAGDROP_S_USEDEFAULTCURSORS if this object lets OLE provide
		// default cursors, otherwise return NOERROR. This method gets called in
		// response to changes that the target makes to dEffect (DragEnter,
		// DragOver).
		STDMETHODIMP GiveFeedback (DWORD dEffect);

		// If fESC is TRUE return DRAGDROP_S_CANCEL. If the left mouse button
		// is released return DRAGDROP_S_DROP. Otherwise return NOERROR. This
      // method gets called if there is any change in the mouse or key state.
		STDMETHODIMP QueryContinueDrag (BOOL fESC, DWORD grfKeyState);

	protected:
		ULONG        m_cRef;     // reference count
		//nsIDragSource *  mDragSource; // adapter
		//CfDragDrop *  mDragSource; // adapter

};

#endif // _nsNativeDragSource_h_

