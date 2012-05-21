/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _nsNativeDragSource_h_
#define _nsNativeDragSource_h_

#include "nscore.h"
#include "nsIDOMDataTransfer.h"
#include "nsCOMPtr.h"
#include <ole2.h>
#include <oleidl.h>

//class nsIDragSource;

/*
 * nsNativeDragSource implements the IDropSource interface and gets
 * most of its behavior from the associated adapter (m_dragDrop).
 */
class nsNativeDragSource : public IDropSource
{
public:

  // construct an nsNativeDragSource referencing adapter
  // nsNativeDragSource(nsIDragSource * adapter);
  nsNativeDragSource(nsIDOMDataTransfer* aDataTransfer);
  ~nsNativeDragSource();

  // IUnknown methods - see iunknown.h for documentation

  STDMETHODIMP QueryInterface(REFIID, void**);
  STDMETHODIMP_(ULONG) AddRef();
  STDMETHODIMP_(ULONG) Release();

  // IDropSource methods - see idropsrc.h for documentation

  // Return DRAGDROP_S_USEDEFAULTCURSORS if this object lets OLE provide
  // default cursors, otherwise return NOERROR. This method gets called in
  // response to changes that the target makes to dEffect (DragEnter,
  // DragOver).
  STDMETHODIMP GiveFeedback(DWORD dEffect);

  // This method gets called if there is any change in the mouse or key
  // state.  Return DRAGDROP_S_CANCEL to stop the drag, DRAGDROP_S_DROP
  // to execute the drop, otherwise NOERROR.
  STDMETHODIMP QueryContinueDrag(BOOL fESC, DWORD grfKeyState);

  bool UserCancelled() { return mUserCancelled; }

protected:
  // Reference count
  ULONG m_cRef;

  // Data object, hold information about cursor state
  nsCOMPtr<nsIDOMDataTransfer> mDataTransfer;

  // Custom drag cursor
  HCURSOR m_hCursor;

  // true if the user cancelled the drag by pressing escape
  bool mUserCancelled;
};

#endif // _nsNativeDragSource_h_

