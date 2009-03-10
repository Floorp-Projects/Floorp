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

  PRPackedBool UserCancelled() { return mUserCancelled; }

protected:
  // Reference count
  ULONG m_cRef;

  // Data object, hold information about cursor state
  nsCOMPtr<nsIDOMNSDataTransfer> mDataTransfer;

  // Custom drag cursor
  HCURSOR m_hCursor;

  // true if the user cancelled the drag by pressing escape
  PRPackedBool mUserCancelled;
};

#endif // _nsNativeDragSource_h_

