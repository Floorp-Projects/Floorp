/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   Lars Knoll <knoll@kde.org>
 *   Zack Rusin <zack@kde.org>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Denis Issoupov <denis@macadamian.com>
 *   John C. Griggs <johng@corel.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifdef NDEBUG
#define NO_DEBUG
#endif

#include "nsDragService.h"
#include "nsIServiceManager.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsPrimitiveHelpers.h"
#include "nsMime.h"
#include "nsWidgetsCID.h"
#include "nsString.h"

// static NS_DEFINE_IID(kIDragServiceIID,   NS_IDRAGSERVICE_IID);
// static NS_DEFINE_IID(kIDragSessionQtIID, NS_IDRAGSESSIONQT_IID);
// static NS_DEFINE_CID(kCDragServiceCID,   NS_DRAGSERVICE_CID);

NS_IMPL_ADDREF_INHERITED(nsDragService, nsBaseDragService)
NS_IMPL_RELEASE_INHERITED(nsDragService, nsBaseDragService)
NS_IMPL_QUERY_INTERFACE3(nsDragService, nsIDragService, nsIDragSession, nsIDragSessionQt )

//-------------------------------------------------------------------------
// static variables
//-------------------------------------------------------------------------
static PRBool gHaveDrag = PR_FALSE;

//-------------------------------------------------------------------------
//
// DragService constructor
//
//-------------------------------------------------------------------------
nsDragService::nsDragService()
{
  // our hidden source widget
  mHiddenWidget = new QWidget(0,QWidget::tr("DragDrop"),0);
}

//-------------------------------------------------------------------------
//
// DragService destructor
//
//-------------------------------------------------------------------------
nsDragService::~nsDragService()
{
  delete mHiddenWidget;
}

//---------------------------------------------------------
NS_IMETHODIMP
nsDragService::InvokeDragSession(nsIDOMNode *aDOMNode,
                                 nsISupportsArray *aArrayTransferables,
                                 nsIScriptableRegion *aRegion,
                                 PRUint32 aActionType)
{
  PRUint32 numItemsToDrag = 0;

  nsBaseDragService::InvokeDragSession(aDOMNode, aArrayTransferables,
                                       aRegion, aActionType);

  // make sure that we have an array of transferables to use
  if (!aArrayTransferables) {
    return NS_ERROR_INVALID_ARG;
  }
  // set our reference to the transferables.  this will also addref
  // the transferables since we're going to hang onto this beyond the
  // length of this call
  mSourceDataItems = aArrayTransferables;

  mSourceDataItems->Count(&numItemsToDrag);
  if (!numItemsToDrag) {
    return NS_ERROR_FAILURE;
  }
  if (numItemsToDrag > 1) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsISupports> genericItem;

  mSourceDataItems->GetElementAt(0,getter_AddRefs(genericItem));

  nsCOMPtr<nsITransferable> transferable(do_QueryInterface(genericItem));

  mDragObject = RegisterDragFlavors(transferable);
  gHaveDrag = PR_TRUE;

  if (aActionType == DRAGDROP_ACTION_MOVE)
    mDragObject->dragMove();
  else
    mDragObject->dragCopy();

  gHaveDrag = PR_FALSE;
  mDragObject = 0;
  return NS_OK;
}

QDragObject *nsDragService::RegisterDragFlavors(nsITransferable *transferable)
{
  nsMimeStore *pMimeStore = new nsMimeStore();
  nsCOMPtr<nsISupportsArray> flavorList;

  if (NS_SUCCEEDED(transferable->FlavorsTransferableCanExport(getter_AddRefs(flavorList)))) {
    PRUint32 numFlavors;

    flavorList->Count(&numFlavors);

    for (PRUint32 flavorIndex = 0; flavorIndex < numFlavors; ++flavorIndex) {
      nsCOMPtr<nsISupports> genericWrapper;

      flavorList->GetElementAt(flavorIndex,getter_AddRefs(genericWrapper));

      nsCOMPtr<nsISupportsCString> currentFlavor(do_QueryInterface(genericWrapper));

      if (currentFlavor) {
	nsXPIDLCString flavorStr;

	currentFlavor->ToString(getter_Copies(flavorStr));

	PRUint32   len;
	void* data;
	nsCOMPtr<nsISupports> clip;

	transferable->GetTransferData(flavorStr,getter_AddRefs(clip),&len);
        nsPrimitiveHelpers::CreateDataFromPrimitive(flavorStr,clip,&data,len);
        pMimeStore->AddFlavorData(flavorStr,data,len);
      }
    } // foreach flavor in item
  } // if valid flavor list
#ifdef NS_DEBUG
  else
   printf(" DnD ERROR: cannot export any flavor\n");
#endif
  return new nsDragObject(pMimeStore,mHiddenWidget);
} // RegisterDragItemsAndFlavors

NS_IMETHODIMP nsDragService::StartDragSession()
{
#ifdef NS_DEBUG
  printf(" DnD: StartDragSession\n");
#endif
  return nsBaseDragService::StartDragSession();
}

NS_IMETHODIMP nsDragService::EndDragSession()
{
#ifdef NS_DEBUG
  printf(" DnD: EndDragSession\n");
#endif
  mDragObject = 0;
  return nsBaseDragService::EndDragSession();
}

// nsIDragSession
NS_IMETHODIMP nsDragService::SetCanDrop(PRBool aCanDrop)
{
  mCanDrop = aCanDrop;
  return NS_OK;
}

NS_IMETHODIMP nsDragService::GetCanDrop(PRBool *aCanDrop)
{
  *aCanDrop = mCanDrop;
  return NS_OK;
}

NS_IMETHODIMP nsDragService::GetNumDropItems(PRUint32 *aNumItems)
{
  *aNumItems = 1;
  return NS_OK;
}

NS_IMETHODIMP nsDragService::GetData(nsITransferable *aTransferable,
                                     PRUint32 aItemIndex)
{
  // make sure that we have a transferable
  if (!aTransferable)
    return NS_ERROR_INVALID_ARG;

  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsISupportsArray> flavorList;

  rv = aTransferable->FlavorsTransferableCanImport(getter_AddRefs(flavorList));
  if (NS_FAILED(rv))
    return rv;

  // count the number of flavors
  PRUint32 cnt;

  flavorList->Count(&cnt);

  // Now walk down the list of flavors. When we find one that is
  // actually present, copy out the data into the transferable in that
  // format. SetTransferData() implicitly handles conversions.
  for (unsigned int i = 0; i < cnt; ++i) {
    nsCAutoString foundFlavor;
    nsCOMPtr<nsISupports> genericWrapper;

    flavorList->GetElementAt(i,getter_AddRefs(genericWrapper));

    nsCOMPtr<nsISupportsCString> currentFlavor;

    currentFlavor = do_QueryInterface(genericWrapper);
    if (currentFlavor) {
      nsXPIDLCString flavorStr;

      currentFlavor->ToString(getter_Copies(flavorStr));
      foundFlavor = nsCAutoString(flavorStr);

      if (mDragObject && mDragObject->provides(flavorStr)) {
	QByteArray ba = mDragObject->encodedData((const char*)flavorStr);
        nsCOMPtr<nsISupports> genericDataWrapper;
	PRUint32 len = (PRUint32)ba.count();

        nsPrimitiveHelpers::CreatePrimitiveForData(foundFlavor.get(),
 						   (void*)ba.data(),len,
                                                   getter_AddRefs(genericDataWrapper));

        aTransferable->SetTransferData(foundFlavor.get(),genericDataWrapper,len);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsDragService::IsDataFlavorSupported(const char *aDataFlavor,
                                                   PRBool *_retval)
{
  if (!_retval)
    return NS_ERROR_INVALID_ARG;

  *_retval = PR_FALSE;

  if (mDragObject)
     *_retval = mDragObject->provides(aDataFlavor);

#ifdef NS_DEBUG
  if (!*_retval)
    printf("nsDragService::IsDataFlavorSupported not provides [%s] \n", aDataFlavor);
#endif
  return NS_OK;
}

NS_IMETHODIMP nsDragService::SetDragReference(QMimeSource* aDragRef)
{
   nsMimeStore*  pMimeStore = new nsMimeStore();
   int c = 0;
   const char* format;

   while ((format = aDragRef->format(c++)) != 0) {
     // this is usualy between different processes
     // so, we need to copy datafrom one to onother

     QByteArray ba = aDragRef->encodedData(format);
     char *data = new char[ba.size()];
     memcpy(data,ba.data(),ba.size());
     pMimeStore->AddFlavorData(format,data,ba.size());
   }
   mDragObject = new nsDragObject(pMimeStore,mHiddenWidget);
   return NS_OK;
}
