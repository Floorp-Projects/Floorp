/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsXPFCDialog.h"
#include "nsxpfcCIID.h"
#include "nspr.h"
#include "nsXPFCMethodInvokerCommand.h"
#include "nsXPFCTextWidget.h"
#include "nsITextWidget.h"
#include "nsWidgetsCID.h"
#include "nsVoidArray.h"
#include "nsXPFCDataCollectionManager.h"
#include "nsIServiceManager.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCXPFCDialogCID, NS_XPFC_DIALOG_CID);
static NS_DEFINE_IID(kCIXPFCDialogIID, NS_IXPFC_DIALOG_IID);
static NS_DEFINE_IID(kCXPFCDataCollectionManager, NS_XPFCDATACOLLECTION_MANAGER_CID);
static NS_DEFINE_IID(kIXPFCDataCollectionManager, NS_IXPFCDATACOLLECTION_MANAGER_IID);

#define DEFAULT_WIDTH  300
#define DEFAULT_HEIGHT 300

nsXPFCDialog :: nsXPFCDialog(nsISupports* outer) : nsXPFCCanvas(outer)
{
  NS_INIT_REFCNT();
}

nsXPFCDialog :: ~nsXPFCDialog()
{
}

nsresult nsXPFCDialog::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kCXPFCDialogCID);                         
  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) (nsXPFCDialog *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kCIXPFCDialogIID)) {                                          
    *aInstancePtr = (void*) (nsIXPFCDialog *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  return (nsXPFCCanvas::QueryInterface(aIID, aInstancePtr));
}

NS_IMPL_ADDREF(nsXPFCDialog)
NS_IMPL_RELEASE(nsXPFCDialog)

nsresult nsXPFCDialog :: Init()
{
  nsresult res = nsXPFCCanvas::Init();    

  return res;
}

nsresult nsXPFCDialog :: SetParameter(nsString& aKey, nsString& aValue)
{
  return (nsXPFCCanvas::SetParameter(aKey, aValue));
}

nsresult nsXPFCDialog :: GetClassPreferredSize(nsSize& aSize)
{
  aSize.width  = DEFAULT_WIDTH;
  aSize.height = DEFAULT_HEIGHT;
  return (NS_OK);
}

static NS_DEFINE_IID(kInsTextWidgetIID,     NS_ITEXTWIDGET_IID);
#define MAX_SIZE 2048

nsresult nsXPFCDialog :: CollectDataInCanvas(nsIXPFCCanvas* host_canvas, nsVoidArray* DataMembers)
{
  nsresult res ;
  nsIIterator * iterator;

  res = host_canvas->CreateIterator(&iterator);

  if (NS_OK != res)
    return NS_OK;

  iterator->Init();
  while(!(iterator->IsDone()))
  {
      nsIXPFCCanvas * canvas = (nsIXPFCCanvas *) iterator->CurrentItem();
      nsString canvas_name;
      nsITextWidget * text_widget = nsnull;
      nsIWidget * widget = nsnull;

      canvas_name = host_canvas->GetNameID();
      widget = canvas->GetWidget();
      if (widget)
      {
        res = widget->QueryInterface(kInsTextWidgetIID,(void**)&text_widget);
        NS_RELEASE(widget);

        if (NS_OK == res)
        {
          CollectedDataPtr newData;
          nsString text;
          nsString name;
          nsString colon = ":";
          PRUint32 size;

          text_widget->GetText(text, MAX_SIZE, size);
          name = canvas->GetNameID();

          NS_RELEASE(text_widget);

          newData = new CollectedData;
          newData->LabelName = canvas_name;
          newData->LabelName += colon;
          newData->LabelName += name;
          newData->Value = text;
          DataMembers->AppendElement((void *)newData);
        }
      }

    CollectDataInCanvas(canvas, DataMembers);
    iterator->Next();
  }

  NS_RELEASE(iterator);
}

nsresult nsXPFCDialog :: CollectData()
{
  nsCollectedData*   cdata = new nsCollectedData;
  nsVoidArray*      lDataMembers = cdata->GetDataArray();
  nsIXPFCDataCollectionManager *theMan = nsnull;

  CollectDataInCanvas(this, lDataMembers);

  for(PRInt32 x = 0; x < lDataMembers->Count(); x++)
  {
    nsString a,b;
    CollectedDataPtr c;
    c = (CollectedDataPtr)(lDataMembers->ElementAt(x));
    a = c->LabelName;
    b = c->Value;
  }
  
  nsServiceManager::GetService(kCXPFCDataCollectionManager, kIXPFCDataCollectionManager, (nsISupports**)&theMan);
  if (theMan)
  {
    nsString ce = nsString("CreateEvent");
    theMan->CallDataHandler(ce, cdata);
  }

  return NS_OK;
}

/*
 *  If we cannot process the command, pass it up the food chain
 */

nsEventStatus nsXPFCDialog::ProcessCommand(nsIXPFCCommand* aCommand)
{
  static NS_DEFINE_IID(kXPFCDialogDataHandlerCommandCID, NS_XPFC_DIALOG_DATA_HANDLER_COMMAND_CID);
  nsXPFCMethodInvokerCommand * methodinvoker_command = nsnull;

  if (NS_OK == aCommand->QueryInterface(kXPFCDialogDataHandlerCommandCID, (void**)&methodinvoker_command))
  {
    CollectData();
    // NS_RELEASE(this);
  } else
  {
    nsXPFCCanvas::ProcessCommand(aCommand);
  }
  return (nsEventStatus_eIgnore);
}