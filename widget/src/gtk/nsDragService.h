/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by Christopher Blizzard
 * are Copyright (C) 1998 Christopher Blizzard. All Rights Reserved.
 *
 * Contributor(s):
 * Christopher Blizzard <blizzard@mozilla.org>
*/

#ifndef nsDragService_h__
#define nsDragService_h__

#include "nsBaseDragService.h"
#include "nsIDragSessionGTK.h"
#include <gtk/gtk.h>


/**
 * Native GTK DragService wrapper
 */

class nsDragService : public nsBaseDragService, public nsIDragSessionGTK
{

public:
  nsDragService();
  virtual ~nsDragService();
  
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDragService
  NS_IMETHOD InvokeDragSession (nsISupportsArray * anArrayTransferables,
                                nsIScriptableRegion * aRegion,
                                PRUint32 aActionType);
  NS_IMETHOD StartDragSession();
  NS_IMETHOD EndDragSession();

  // nsIDragSession
  NS_IMETHOD SetCanDrop            (PRBool           aCanDrop);
  NS_IMETHOD GetCanDrop            (PRBool          *aCanDrop);
  NS_IMETHOD GetNumDropItems       (PRUint32 * aNumItems);
  NS_IMETHOD GetData               (nsITransferable * aTransferable, PRUint32 anItemIndex);
  NS_IMETHOD IsDataFlavorSupported (const char *aDataFlavor, PRBool *_retval);

  // nsIDragSessionGTK
  NS_IMETHOD SetLastContext  (GtkWidget          *aWidget,
                              GdkDragContext     *aContext,
                              guint               aTime);
  NS_IMETHOD StartDragMotion (GtkWidget      *aWidget,
                              GdkDragContext *aContext,
                              guint           aTime);
  NS_IMETHOD EndDragMotion   (GtkWidget      *aWidget,
                              GdkDragContext *aContext,
                              guint           aTime);
  NS_IMETHOD SetDataReceived (GtkWidget          *aWidget,
                              GdkDragContext     *context,
                              gint                x,
                              gint                y,
                              GtkSelectionData   *selection_data,
                              guint               info,
                              guint32             time);
  NS_IMETHOD DataGetSignal   (GtkWidget          *widget,
                              GdkDragContext     *context,
                              GtkSelectionData   *selection_data,
                              guint               info,
                              guint32             time,
                              gpointer            data);
  NS_IMETHOD SetTimeCallback (nsIDragSessionGTKTimeCB aCallback);

  //  END PUBLIC API

private:
  // this is the hidden widget where we get our data
  // we start drags from and where we do a grab when we
  // are waiting for our DND data back
  GtkWidget *mHiddenWidget;
  // here's the last drag context
  GdkDragContext *mLastContext;
  // this is the last widget that had a drag event
  GtkWidget *mLastWidget;
  // the last time a drag event happened.
  guint mLastTime;

  // this is the list of data items that we support when we've
  // started a drag
  nsISupportsArray *mDataItems;

  // have we gotten our drag data yet?
  PRBool mDataReceived;
  // this is where the actual drag data is received
  void     *mDragData;
  PRUint32 mDragDataLen;

  // this will reset all of our drag state
  void ResetDragState(void);
  // set our local data items list and addref it properly
  void SetDataItems(nsISupportsArray *anArray);
  // get the data for a particular flavor
  NS_METHOD GetNativeDragData(GdkAtom aFlavor);
  // this is a callback to get the time for the last event that
  // happened
  nsIDragSessionGTKTimeCB mTimeCB;
};

#endif // nsDragService_h__
