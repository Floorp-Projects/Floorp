/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsDragService.h"
#include "nsITransferable.h"
#include "nsIServiceManager.h"

#include "nsWidgetsCID.h"
#include "nslog.h"

NS_IMPL_LOG(nsDragServiceLog)
#define PRINTF NS_LOG_PRINTF(nsDragServiceLog)
#define FLUSH  NS_LOG_FLUSH(nsDragServiceLog)

static NS_DEFINE_IID(kIDragServiceIID,   NS_IDRAGSERVICE_IID);
static NS_DEFINE_CID(kCDragServiceCID,   NS_DRAGSERVICE_CID);

// The class statics:
//GtkWidget* nsDragService::sWidget = 0;

NS_IMPL_ADDREF_INHERITED(nsDragService, nsBaseDragService)
NS_IMPL_RELEASE_INHERITED(nsDragService, nsBaseDragService)

//-------------------------------------------------------------------------
// static variables
//-------------------------------------------------------------------------
  //static PRBool gHaveDrag = PR_FALSE;

//-------------------------------------------------------------------------
//
// DragService constructor
//
//-------------------------------------------------------------------------
nsDragService::nsDragService()
{
    NS_INIT_REFCNT();
}

//-------------------------------------------------------------------------
//
// DragService destructor
//
//-------------------------------------------------------------------------
nsDragService::~nsDragService()
{
}

/**
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 * 
*/ 
nsresult nsDragService::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{

    if (NULL == aInstancePtr) 
    {
        return NS_ERROR_NULL_POINTER;
    }

    nsresult rv = NS_NOINTERFACE;

    if (aIID.Equals(NS_GET_IID(nsIDragService))) 
    {
        *aInstancePtr = (void*) ((nsIDragService*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }

return rv;
}

//---------------------------------------------------------
NS_IMETHODIMP nsDragService::StartDragSession(nsITransferable * aTransferable,
                                              PRUint32 aActionType)

{
    return NS_OK;
}

#if 0 //JCG
//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::GetData(nsITransferable * aTransferable,
                                     PRUint32 aItemIndex)
{
    return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
void nsDragService::SetTopLevelWidget(QWidget* w)
{
    PRINTF("  nsDragService::SetTopLevelWidget\n");
  
    // Don't set up any more event handlers if we're being called twice
    // for the same toplevel widget
    if (sWidget == w)
        return;

    sWidget = w;

    // Get the DragService from the service manager.
    nsresult rv;
    NS_WITH_SERVICE(nsIDragService, dragService, kCDragServiceCID, &rv);

    if (NS_FAILED(rv)) 
    {
        return;
    }

    // We're done with our reference to the dragService.
    //NS_IF_RELEASE(dragService);
}

//-------------------------------------------------------------------------
void  
nsDragService::DragLeave(QWidget	*widget,
			 QDragContext   *context,
                         unsigned int   time)
{
    printf("leave\n");
}

//-------------------------------------------------------------------------
PRBool
nsDragService::DragMotion(QWidget	 *widget,
			  QDragContext   *context,
			  int            x,
			  int            y,
			  unsigned int   time)
{
    printf("drag motion\n");
    return PR_TRUE;
}

//-------------------------------------------------------------------------
PRBool
nsDragService::DragDrop(QWidget	       *widget,
			QDragContext   *context,
			int            x,
			int            y,
			unsigned int   time)
{
    printf("drop\n");
    return PR_FALSE;
}

//-------------------------------------------------------------------------
void  
nsDragService::DragDataReceived(QWidget          *widget,
                                QDragContext     *context,
                                int              x,
                                int              y,
                                QSelectionData   *data,
                                unsigned int     info,
                                guint               time)
{
    printf("Data Received!\n");
}
  
//-------------------------------------------------------------------------
void  
nsDragService::DragDataGet(QWidget          *widget,
		           QDragContext     *context,
		           QSelectionData   *selection_data,
		           unsigned int     info,
		           unsigned int     time,
		           void             *data)
{
    printf("Get the data!\n");
}

//-------------------------------------------------------------------------
void  
nsDragService::DragDataDelete(QWidget          *widget,
			      QDragContext     *context,
			      void             *data)
{
    printf("Delete the data!\n");
}
#endif /* JCG */
