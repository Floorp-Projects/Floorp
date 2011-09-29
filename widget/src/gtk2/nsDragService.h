/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
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
 * Christopher Blizzard <blizzard@mozilla.org>.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
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
    NS_IMETHOD EndDragSession(bool aDoneDrag);

    // nsIDragSession
    NS_IMETHOD SetCanDrop            (bool             aCanDrop);
    NS_IMETHOD GetCanDrop            (bool            *aCanDrop);
    NS_IMETHOD GetNumDropItems       (PRUint32 * aNumItems);
    NS_IMETHOD GetData               (nsITransferable * aTransferable,
                                      PRUint32 aItemIndex);
    NS_IMETHOD IsDataFlavorSupported (const char *aDataFlavor, bool *_retval);

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

    //  END PUBLIC API

    // These methods are public only so that they can be called from functions
    // with C calling conventions.  They are called for drags started with the
    // invisible widget.
    void           SourceEndDragSession(GdkDragContext *aContext,
                                        gint            aResult);
    void           SourceDataGet(GtkWidget        *widget,
                                 GdkDragContext   *context,
                                 GtkSelectionData *selection_data,
                                 guint             info,
                                 guint32           aTime);

    // set the drag icon during drag-begin
    void SetDragIcon(GdkDragContext* aContext);

private:

    // target side vars

    // the last widget that was the target of a drag
    GtkWidget      *mTargetWidget;
    GdkDragContext *mTargetDragContext;
    guint           mTargetTime;
    // is it OK to drop on us?
    bool            mCanDrop;
    // have we received our drag data?
    bool            mTargetDragDataReceived;
    // last data received and its length
    void           *mTargetDragData;
    PRUint32        mTargetDragDataLen;
    // is the current target drag context contain a list?
    bool           IsTargetContextList(void);
    // this will get the native data from the last target given a
    // specific flavor
    void           GetTargetDragData(GdkAtom aFlavor);
    // this will reset all of the target vars
    void           TargetResetData(void);

    // source side vars

    // the source of our drags
    GtkWidget     *mHiddenWidget;
    // the widget receiving mouse events
    GtkWidget     *mGrabWidget;
    // our source data items
    nsCOMPtr<nsISupportsArray> mSourceDataItems;

    nsCOMPtr<nsIScriptableRegion> mSourceRegion;

    // get a list of the sources in gtk's format
    GtkTargetList *GetSourceList(void);

    // attempts to create a semi-transparent drag image. Returns TRUE if
    // successful, FALSE if not
    bool SetAlphaPixmap(gfxASurface     *aPixbuf,
                          GdkDragContext  *aContext,
                          PRInt32          aXOffset,
                          PRInt32          aYOffset,
                          const nsIntRect &dragRect);

};

#endif // nsDragService_h__

