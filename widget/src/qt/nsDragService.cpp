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

#include "nsDragService.h"

NS_IMPL_ISUPPORTS1(nsDragService, nsIDragService)

nsDragService::nsDragService()
{
    /* member initializers and constructor code */
    qDebug("nsDragService::nsDragService");
}

nsDragService::~nsDragService()
{
    /* destructor code */
    qDebug("nsDragService::~nsDragService");
}

/* void invokeDragSession (in nsIDOMNode aDOMNode, in nsISupportsArray aTransferables, in nsIScriptableRegion aRegion, in unsigned long aActionType); */
NS_IMETHODIMP
nsDragService::InvokeDragSession(nsIDOMNode *aDOMNode, nsISupportsArray *aTransferables, nsIScriptableRegion *aRegion, PRUint32 aActionType)
{
    qDebug("nsDragService::InvokeDragSession");

    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void invokeDragSessionWithImage ( nsIDOMNode DOMNode , nsISupportsArray transferableArray , nsIScriptableRegion region , PRUint32 actionType , nsIDOMNode image , PRInt32 imageX , PRInt32 imageY , nsIDOMMouseEvent dragEvent ); */
NS_IMETHODIMP
nsDragService::InvokeDragSessionWithImage(nsIDOMNode *aDOMNode, nsISupportsArray*aTransferables, nsIScriptableRegion* aRegion, PRUint32 aActionType, nsIDOMNode* aImage, PRInt32 aImageX, PRInt32 aImageY, nsIDOMMouseEvent* aDragEvent)
{
    qDebug("nsDragService::InvokeDragSessionWithImage");

    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void invokeDragSessionWithSelection ( nsISelection selection , nsISupportsArray transferableArray , PRUint32 actionType , nsIDOMMouseEvent dragEvent ) */
NS_IMETHODIMP
nsDragService::InvokeDragSessionWithSelection(nsISelection* aSelection, nsISupportsArray* aTransferables, PRUint32 aActionType, nsIDOMMouseEvent* aDragEvent)
{
    qDebug("nsDragService::InvokeDragSessionWithSelection");

    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIDragSession getCurrentSession (); */
NS_IMETHODIMP
nsDragService::GetCurrentSession(nsIDragSession **_retval)
{
    qDebug("nsDragService::GetCurrentSession");

    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void startDragSession (); */
NS_IMETHODIMP
nsDragService::StartDragSession()
{
    qDebug("nsDragService::StartDragSession");

    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void endDragSession (in PRBool aDoneDrag); */
NS_IMETHODIMP
nsDragService::EndDragSession(PRBool aDoneDrag)
{
    qDebug("nsDragService::EndDragSession");

    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void fireDragEventAtSource (in unsigned long aMsg); */
NS_IMETHODIMP
nsDragService::FireDragEventAtSource(PRUint32 aMsg)
{
    qDebug("nsDragService::FireDragEventAtSource");

    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDragService::Suppress()
{
    qDebug("nsDragService::Suppress");

    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDragService::Unsuppress()
{
    qDebug("nsDragService::Unsuppress");

    return NS_ERROR_NOT_IMPLEMENTED;
}

