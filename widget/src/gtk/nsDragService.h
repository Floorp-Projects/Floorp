/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsDragService_h__
#define nsDragService_h__

#include "nsBaseDragService.h"
#include <gtk/gtk.h>


/**
 * Native GTK DragService wrapper
 */

class nsDragService : public nsBaseDragService
{

public:
  nsDragService();
  virtual ~nsDragService();
  
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDragService
  NS_IMETHOD InvokeDragSession (nsISupportsArray * anArrayTransferables,
                                nsIScriptableRegion * aRegion, PRUint32 aActionType);
  NS_IMETHOD GetCurrentSession (nsIDragSession ** aSession);

  // nsIDragSession
  NS_IMETHOD GetData (nsITransferable * aTransferable, PRUint32 anItem);
  NS_IMETHOD GetNumDropItems (PRUint32 * aNumItems);
  NS_IMETHOD IsDataFlavorSupported(const char *aDataFlavor, PRBool *_retval);

  NS_IMETHOD StartDragSession();
  NS_IMETHOD EndDragSession();
    

  GtkTargetList *RegisterDragItemsAndFlavors(nsISupportsArray *inArray);

protected:

  static void SelectionReceivedCB (GtkWidget        *aWidget,
                                   GdkDragContext   *aContext,
                                   gint              aX,
                                   gint              aY,
                                   GtkSelectionData *aSelectionData,
                                   guint             aInfo,
                                   guint             aTime);
  void SelectionReceiver (GtkWidget *aWidget, GtkSelectionData *aSD);


  PRBool DoConvert(GdkAtom type);

  static PRBool gHaveDrag;

  static void DragLeave(GtkWidget      *widget,
                        GdkDragContext *context,
                        guint           time);

  static PRBool DragMotion(GtkWidget      *widget,
                           GdkDragContext *context,
                           gint            x,
                           gint            y,
                           guint           time);

  static PRBool DragDrop(GtkWidget   *widget,
                         GdkDragContext *context,
                         gint            x,
                         gint            y,
                         guint            time);

  static void DragDataReceived(GtkWidget        *widget,
			                         GdkDragContext   *context,
			                         gint              x,
			                         gint              y,
			                         GtkSelectionData *data,
			                         guint             info,
			                         guint             time);

  static void DragDataGet(GtkWidget          *widget,
		                      GdkDragContext     *context,
		                      GtkSelectionData   *selection_data,
		                      guint               info,
		                      guint               time,
		                      gpointer            data);

  static void  DragDataDelete(GtkWidget          *widget,
			                        GdkDragContext     *context,
			                        gpointer            data);

private:
  GdkDragAction mActionType;
  PRUint32 mNumFlavors;
  GtkWidget *mWidget;
  GdkDragContext *mDragContext;
  GtkSelectionData mSelectionData;
  PRBool mBlocking;
};

#endif // nsDragService_h__
