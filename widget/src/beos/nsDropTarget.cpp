/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- 
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

//#include "stdafx.h"
//#include "olebase.h"
//#include "idropsrc.h"
#include "nsDropTarget.h"
//#include "contndoc.h"
#include "stdio.h"
#include "nsIWidget.h"

#include "nsWindow.h"
//Don't forget to RegisterDragDrop and to 
//CoLockObjectExternal.
//You must RevokeDragDrop before destroying this object
//
nsDropTarget::nsDropTarget(nsIWidget  * aWindow)
{
  printf("nsDropTarget::nsDropTarget\n");
    m_refs        = 0;
    m_pWin = aWindow; 
    NS_ADDREF(aWindow);
}

//Destructor for nsDropTarget
//Doesn't do a whole lot!
//
nsDropTarget::~nsDropTarget()
{
  printf("nsDropTarget::Drop\n");
    ;    
}

//QueryInterface for IDropTarget
//Notice that this QueryInterface does not refer to another object's QueryInterface,
//this is a totally stand-alone separate object
//
STDMETHODIMP nsDropTarget::QueryInterface(REFIID riid, LPVOID FAR* ppv)
{
  printf("nsDropTarget::QueryInterface\n");
    if (riid == IID_IUnknown)
        *ppv = this;                     
    else if (riid == IID_IDropTarget)
     *ppv = this;
    else{ 
        *ppv = NULL;
         }
    if (*ppv != NULL){
     ((LPUNKNOWN)(*ppv))->AddRef();
     return NOERROR;
    }

//If for some reason our constructor did not create the requested interface
//don't pass the NULL interface without telling the caller that we don't support
//the requested interface.
   return ResultFromScode(E_NOINTERFACE);    
}

//A private AddRef for IDropTarget
//
STDMETHODIMP_(ULONG) nsDropTarget::AddRef(void)
{
  printf("nsDropTarget::AddRef\n");
	return ++m_refs;
}

//A private Release for IDropTarget
//
STDMETHODIMP_(ULONG) nsDropTarget::Release(void)
{
  printf("nsDropTarget::Release\n");
   if(--m_refs == 0){
   //And now we can delete this object 
    delete this;
   }

   return m_refs;
}

//
//This helper function tells us what the drag/drop effect would be at
//the point pointl.  Since our object accepts being a drop target anywhere
//in the window, we will ignore pointl
//
DWORD   nsDropTarget::FindDragDropEffect(DWORD grfKeyState, POINTL /* pointl */)
{
  printf("nsDropTarget::FindDragDropEffect\n");
    DWORD   dwRet;
    
	//          no modifier -- DROPEFFECT_MOVE or source default
	//          SHIFT       -- DROPEFFECT_MOVE
	//          CTRL        -- DROPEFFECT_COPY
	//          CTRL-SHIFT  -- DROPEFFECT_LINK

	dwRet = 0;//OleStdGetDropEffect(grfKeyState);
    if (dwRet == 0)
        dwRet = DROPEFFECT_COPY;
    
    return dwRet;    
}


//Notification that a DropSource is over the current DropTarget
//
STDMETHODIMP nsDropTarget::DragEnter (LPDATAOBJECT pDataObj, 
                                      DWORD grfKeyState, 
                                      POINTL pointl, 
                                      LPDWORD pdwEffect)
{
  printf("nsDropTarget::DragEnter\n");
    *pdwEffect = FindDragDropEffect(grfKeyState, pointl);
    return NOERROR;
}

//Provides user feedback when a DropSource is over a DropTarget
//
STDMETHODIMP nsDropTarget::DragOver  (DWORD grfKeyState, 
                                      POINTL pointl, 
                                      LPDWORD pdwEffect)
{
  printf("nsDropTarget::DragOver\n");
    *pdwEffect = FindDragDropEffect(grfKeyState, pointl);
    return NOERROR;
}


//The DropSource is leaving our target window
//so its cleanup time!!!
//
STDMETHODIMP nsDropTarget::DragLeave (void)
{
  printf("nsDropTarget::DragLeave\n");
    return NOERROR;
}

//Drops the DropSource
//
STDMETHODIMP nsDropTarget::Drop (LPDATAOBJECT pIDataObject, 
                                 DWORD grfKeyState, 
                                 POINTL pointl, 
                                 LPDWORD pdwEffect)
{
  printf("nsDropTarget::Drop\n");
  printf("pIDataObject 0x%x\n", pIDataObject);
    pIDataObject->AddRef();
    if (nsnull != m_pWin) {
      nsEventStatus status;
      nsDragDropEvent event;
      ((nsWindow *)m_pWin)->InitEvent(event, NS_DRAGDROP_EVENT);
      event.mType      = nsDragDropEventStatus_eDrop;
      event.mIsFileURL = PR_FALSE;
      event.mURL       = nsnull;           
      m_pWin->DispatchEvent(&event, status);
    }

   //pdwEffect = FindDragDropEffect(grfKeyState, pointl);


   if (pIDataObject != NULL) {
     IDataObject * presistStorage;
     //printf("QueryInterface for persist\n");
     pIDataObject->QueryInterface(IID_IDataObject,(LPVOID *)&presistStorage);
     //printf("Done QueryInterface for persist\n");
     if (presistStorage != NULL) {
       printf("$$$$$$$$$$$$$$$ Got it!\n");
       FORMATETC pFE;
       presistStorage->QueryGetData(&pFE);

       pFE.tymed = TYMED_FILE;

       STGMEDIUM pSTM;
       HRESULT st = presistStorage->GetDataHere(&pFE, &pSTM);
          printf("st 0x%X\n", st);
	        if (NOERROR != st) {
		        return FALSE;
	        }
//TYMED_STORAGE, TYMED_STREAM, TYMED_HGLOBAL, or TYMED_FILE
       //printf("%s\n", pSTM.lpszFileName);
       {

	        //HRESULT hr = pIDataObject->GetData(&pFE, &pSTM);
          //printf("hr 0x%X\n", hr);
	        //if (NOERROR != hr) {
		      //  return FALSE;
	        //}

	        const CLIPFORMAT format = pFE.cfFormat;
	        switch(format) {
		        case CF_BITMAP:
			        //hr = SetBitmap(*pFE, stm);
			        break;
		        case CF_DIB:
			        //hr = SetDib(*pFE, stm);
			        break;
		        case CF_TEXT:
			        //hr = SetText(*pFE, stm);
			        break;
		        case CF_METAFILEPICT:
			        //hr = SetMetafilePict(*pFE, stm);
			        break;
		        default:
			        //if (format == GetRcfFormat()) {
				      //  hr = SetRcf(*pFE, stm);
			        //}
			        break;
	        }

	        //return (SUCCEEDED(hr));

       }


     } else {
       printf("^^^^^^^^^^^^^^^^^ Didn't!\n");
     }
   }


/*    if (m_pContainDoc->m_pOleSiteObject)
        delete m_pContainDoc->m_pOleSiteObject;
    m_pContainDoc->m_pOleSiteObject = new COleSiteObject(m_pContainDoc);
	FORMATETC fmtetc;
	fmtetc.cfFormat = NULL;            
	fmtetc.ptd = NULL;
	fmtetc.lindex = -1;
	fmtetc.dwAspect = DVASPECT_CONTENT; 
    fmtetc.tymed = TYMED_NULL;
    m_pContainDoc->m_pOleSiteObject->AddSiteFromData(m_pContainDoc,
                                                     pIDataObject,
                                                     &fmtetc); 
*/
    pIDataObject->Release();
    
    DragLeave();

    return NOERROR;
}
