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
#include "nsIObserver.h"
#include <gtk/gtk.h>


/**
 * Native GTK DragService wrapper
 */

class nsDragService : public nsBaseDragService,
                      public nsIDragSessionGTK,
                      public nsIObserver
{

public:
  nsDragService();
  virtual ~nsDragService();
  
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIOBSERVER

  // nsIDragService
  NS_IMETHOD InvokeDragSession (nsIDOMNode *aDOMNode,
                                nsISupportsArray * anArrayTransferables,
                                nsIScriptableRegion * aRegion,
                                PRUint32 aActionType);
  NS_IMETHOD StartDragSession();
  NS_IMETHOD EndDragSession();

  // nsIDragSession
  NS_IMETHOD SetCanDrop            (PRBool           aCanDrop);
  NS_IMETHOD GetCanDrop            (PRBool          *aCanDrop);
  NS_IMETHOD GetNumDropItems       (PRUint32 * aNumItems);
  NS_IMETHOD GetData               (nsITransferable * aTransferable,
                                    PRUint32 aItemIndex);
  NS_IMETHOD IsDataFlavorSupported (const char *aDataFlavor, PRBool *_retval);

  // nsIDragSessionGTK

  NS_IMETHOD TargetSetLastContext  (GtkWidget      *aWidget,
                                    GdkDragContext *aContext,
                                    guint           aTime);
  NS_IMETHOD TargetStartDragMotion (void);
  NS_IMETHOD TargetEndDragMotion   (GtkWidget      *aWidget,
                                    GdkDragContext *aContext,
                                    guint           aTime);
  NS_IMETHOD TargetDataReceived    (GtkWidget         *aWidget,
                                    GdkDragContext    *aContext,
                                    gint               aX,
                                    gint               aY,
                                    GtkSelectionData  *aSelection_data,
                                    guint              aInfo,
                                    guint32            aTime);
  
  NS_IMETHOD TargetSetTimeCallback (nsIDragSessionGTKTimeCB aCallback);

  // This is called when the drag started with the invisible widget
  // finishes.  It's called from within the drag service code but from
  // a callback - it needs to be public.
  void           SourceEndDrag(void);
  void           SourceDataGet(GtkWidget        *widget,
                               GdkDragContext   *context,
                               GtkSelectionData *selection_data,
                               guint             info,
                               guint32           aTime);

  //  END PUBLIC API

private:

  // target side vars

  // the last widget that was the target of a drag
  GtkWidget      *mTargetWidget;
  GdkDragContext *mTargetDragContext;
  guint           mTargetTime;
  // is it OK to drop on us?
  PRBool          mCanDrop;
  // have we received our drag data?
  PRBool          mTargetDragDataReceived;
  // last data received and its length
  void           *mTargetDragData;
  PRUint32        mTargetDragDataLen;
  // is the current target drag context contain a list?
  PRBool         IsTargetContextList(void);
  // this will get the native data from the last target given a
  // specific flavor
  void           GetTargetDragData(GdkAtom aFlavor);
  // this will reset all of the target vars
  void           TargetResetData(void);

  // source side vars
  
  // the source of our drags
  GtkWidget     *mHiddenWidget;
  // our source data items
  nsCOMPtr<nsISupportsArray> mSourceDataItems;
  // get a list of the sources in gtk's format
  GtkTargetList *GetSourceList(void);

  // this is our callback to get the most recent event's time
  nsIDragSessionGTKTimeCB mTimeCB;

};

#endif // nsDragService_h__
