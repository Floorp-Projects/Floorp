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
 *  Oleg Romashin <romaxa@gmail.com>.
 *
 * Contributor(s):
 *  Oleg Romashin <romaxa@gmail.com>.
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

#include "qmimedata.h"
#include "qwidget.h"

#include "nsDragService.h"
#include "nsISupportsPrimitives.h"
#include "nsXPIDLString.h"
#include "nsIDOMMouseEvent.h"

NS_IMPL_ADDREF_INHERITED(nsDragService, nsBaseDragService)
NS_IMPL_RELEASE_INHERITED(nsDragService, nsBaseDragService)
NS_IMPL_QUERY_INTERFACE2(nsDragService, nsIDragService, nsIDragSession )

nsDragService::nsDragService() : mDrag(NULL)
{
    /* member initializers and constructor code */
    // TODO: Any other better source? (the main window?)
    //mHiddenWidget = new QWidget(0,QWidget::tr("DragDrop"),0);
    mHiddenWidget = new QWidget();
}

nsDragService::~nsDragService()
{
    /* destructor code */
    delete mHiddenWidget;
}

NS_IMETHODIMP
nsDragService::SetDropActionType( PRUint32 aActionType )
{
    mDropAction = Qt::IgnoreAction;

    if (aActionType & DRAGDROP_ACTION_COPY)
    {
        mDropAction |= Qt::CopyAction;
    }
    if (aActionType & DRAGDROP_ACTION_MOVE)
    {
        mDropAction |= Qt::MoveAction;
    }
    if (aActionType & DRAGDROP_ACTION_LINK)
    {
        mDropAction |= Qt::LinkAction;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDragService::SetupDragSession(
                                nsISupportsArray *aTransferables,
                                PRUint32 aActionType)
{
    PRUint32 itemCount = 0;
    aTransferables->Count(&itemCount);
    if (0 == itemCount)
    {
        NS_WARNING("No items to drag?");
        return NS_ERROR_FAILURE;
    }

    if (1 != itemCount)
    {
        NS_WARNING("Dragging more than one item, cannot do (yet?)");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    SetDropActionType(aActionType);

    QMimeData *mimeData = new QMimeData;

    nsCOMPtr<nsISupports> genericItem;
    aTransferables->GetElementAt(0, getter_AddRefs(genericItem));
    nsCOMPtr<nsITransferable> transferable(do_QueryInterface(genericItem));

    if (transferable)
    {
        nsCOMPtr <nsISupportsArray> flavorList;
        transferable->FlavorsTransferableCanExport(getter_AddRefs(flavorList));

        if (flavorList)
        {
            PRUint32 flavorCount;
            flavorList->Count( &flavorCount );

            for (PRUint32 flavor=0; flavor < flavorCount; flavor++)
            {
                nsCOMPtr<nsISupports> genericWrapper;
                flavorList->GetElementAt(flavor, getter_AddRefs(genericWrapper));
                nsCOMPtr<nsISupportsCString> currentFlavor;
                currentFlavor = do_QueryInterface(genericWrapper);

                if (currentFlavor)
                {
                    nsCOMPtr<nsISupports> data;
                    PRUint32 dataLen = 0;
                    nsXPIDLCString flavorStr;
                    currentFlavor->ToString(getter_Copies(flavorStr));

                    // Is it some flavor we think we could support?
                    if (!strcmp(kURLMime, flavorStr.get())
                     || !strcmp(kURLDataMime, flavorStr.get())
                     || !strcmp(kURLDescriptionMime, flavorStr.get())
                     || !strcmp(kHTMLMime, flavorStr.get())
                     || !strcmp(kUnicodeMime, flavorStr.get())
                        )
                    {
                        transferable->GetTransferData(flavorStr,getter_AddRefs(data),&dataLen);

                        nsCOMPtr<nsISupportsString> wideString;
                        wideString = do_QueryInterface(data);
                        if (!wideString)
                        {
                            return NS_ERROR_FAILURE;
                        }

                        nsAutoString utf16string;
                        wideString->GetData(utf16string);
                        QByteArray ba((const char*) utf16string.get(), dataLen);

                        mimeData->setData(flavorStr.get(), ba);
                    }
                }
            }
        }
    }

    mDrag = new QDrag( mHiddenWidget ); // TODO: Better drag source here?
    mDrag->setMimeData(mimeData);

    // mDrag and mimeData SHOULD NOT be destroyed. They are destroyed by QT.

    return NS_OK;
}

/* void invokeDragSession (in nsIDOMNode aDOMNode, in nsISupportsArray aTransferables, in nsIScriptableRegion aRegion, in unsigned long aActionType); */
NS_IMETHODIMP
nsDragService::InvokeDragSession(
                                nsIDOMNode *aDOMNode,
                                nsISupportsArray *aTransferables,
                                nsIScriptableRegion *aRegion,
                                PRUint32 aActionType)
{
    nsBaseDragService::InvokeDragSession( 
                                        aDOMNode,
                                        aTransferables,
                                        aRegion,
                                        aActionType);

    SetupDragSession( aTransferables, aActionType);

    return NS_OK;
}

NS_IMETHODIMP
nsDragService::ExecuteDrag()
{
    Qt::DropAction dropAction = mDrag->exec( mDropAction );

    return NS_OK;
}

/* void invokeDragSessionWithImage ( nsIDOMNode DOMNode , nsISupportsArray transferableArray , nsIScriptableRegion region , PRUint32 actionType , nsIDOMNode image , PRInt32 imageX , PRInt32 imageY , nsIDOMMouseEvent dragEvent ); */
NS_IMETHODIMP
nsDragService::InvokeDragSessionWithImage(
                        nsIDOMNode *aDOMNode,
                        nsISupportsArray*aTransferables,
                        nsIScriptableRegion* aRegion,
                        PRUint32 aActionType,
                        nsIDOMNode* aImage,
                        PRInt32 aImageX,
                        PRInt32 aImageY,
                        nsIDOMDragEvent* aDragEvent,
                        nsIDOMDataTransfer* aDataTransfer)
{
    nsBaseDragService::InvokeDragSessionWithImage(
                                        aDOMNode, aTransferables,
                                        aRegion, aActionType,
                                        aImage,
                                        aImageX, aImageY,
                                        aDragEvent,
                                        aDataTransfer);

    SetupDragSession( aTransferables, aActionType);

    // Setup Image, and other stuff
    if (aImage)
    {
        // Use the custom image 
        // (aImageX,aImageY) specifies the offset "within the image where
        // the cursor would be positioned"

        // So, convert the aImage to QPixmap and X and Y to q QPoint
        // and then:
        // mDrag->setPixmap( pixmap ) or mDrag->setDragCursor( pixmap );
        // mDrad->setHotSpot( point );
        // TODO: Not implemented yet as this cannot be currently tested
        NS_WARNING("Support for drag image not implemented");
    }

    return ExecuteDrag();
}

NS_IMETHODIMP
nsDragService::InvokeDragSessionWithSelection(nsISelection* aSelection,
                                              nsISupportsArray* aTransferableArray,
                                              PRUint32 aActionType,
                                              nsIDOMDragEvent* aDragEvent,
                                              nsIDOMDataTransfer* aDataTransfer)
{
    nsBaseDragService::InvokeDragSessionWithSelection(
                                        aSelection,
                                        aTransferableArray,
                                        aActionType,
                                        aDragEvent,
                                        aDataTransfer);

    SetupDragSession( aTransferableArray, aActionType);

    // Setup selection related properties
    // There is however nothing that needs to be set

    return ExecuteDrag();
}

/* nsIDragSession getCurrentSession (); */
NS_IMETHODIMP
nsDragService::GetCurrentSession(nsIDragSession **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void startDragSession (); */
NS_IMETHODIMP
nsDragService::StartDragSession()
{
    return nsBaseDragService::StartDragSession();
}

/* void endDragSession (in bool aDoneDrag); */
NS_IMETHODIMP
nsDragService::EndDragSession(bool aDoneDrag)
{
    return nsBaseDragService::EndDragSession(aDoneDrag);
}

/* void fireDragEventAtSource (in unsigned long aMsg); */
NS_IMETHODIMP
nsDragService::FireDragEventAtSource(PRUint32 aMsg)
{
    return nsBaseDragService::FireDragEventAtSource(aMsg);
}

/* TODO: What is this? */
NS_IMETHODIMP
nsDragService::Suppress()
{
    return nsBaseDragService::Suppress();
}

/* TODO: What is this? */
NS_IMETHODIMP
nsDragService::Unsuppress()
{
    return nsBaseDragService::Unsuppress();
}

NS_IMETHODIMP
nsDragService::DragMoved(PRInt32 aX, PRInt32 aY)
{
    return nsBaseDragService::DragMoved(aX, aY);
}
