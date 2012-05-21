/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  NS_IMETHOD IsDataFlavorSupported(const char *aDataFlavor, bool *_retval);
  NS_IMETHOD EndDragSession(bool aDoneDrag);

  // native impl.
  NS_IMETHOD SetIDataObject(IDataObject * aDataObj);
  NS_IMETHOD StartInvokingDragSession(IDataObject * aDataObj,
                                      PRUint32 aActionType);

  // A drop occurred within the application vs. outside of it.
  void SetDroppedLocal();

protected:
  nsDataObjCollection* GetDataObjCollection(IDataObject * aDataObj);

  // determine if we have a single data object or one of our private
  // collections
  bool IsCollectionObject(IDataObject* inDataObj);

  // Create a bitmap for drag operations
  bool CreateDragImage(nsIDOMNode *aDOMNode,
                         nsIScriptableRegion *aRegion,
                         SHDRAGIMAGE *psdi);

  nsNativeDragTarget * mNativeDragTarget;
  IDataObject * mDataObject;
  bool mSentLocalDropEvent;
};

#endif // nsDragService_h__
