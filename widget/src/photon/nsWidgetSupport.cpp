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


#include "nsWidgetSupport.h"
#include "nsRect.h"
#include "nsITextAreaWidget.h"
#include "nsIFileWidget.h"
#include "nsIAppShell.h"
#include "nsIButton.h"
#include "nsIComboBox.h"
#include "nsIEventListener.h"
#include "nsILabel.h"
#include "nsIListBox.h"
#include "nsIListWidget.h"
#include "nsILookAndFeel.h"
#include "nsIMouseListener.h"
#include "nsITabWidget.h"
#include "nsIToolkit.h"
#include "nsIWidget.h"
#include "nsIDialog.h"
#include "nsICheckButton.h"
#include "nsIScrollbar.h"
#include "nsIRadioButton.h"
#include "nsITooltipWidget.h"
#include "nsITextWidget.h"




static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);
//static NS_DEFINE_IID(kIButtonIID, NS_IBUTTON_IID);
//static NS_DEFINE_IID(kIFileWidgetIID, NS_IFILEWIDGET_IID);
//static NS_DEFINE_IID(kITextWidgetIID, NS_ITEXTWIDGET_IID);
static NS_DEFINE_IID(kIDialogIID, NS_IDIALOG_IID);
//static NS_DEFINE_IID(kICheckButtonIID, NS_ICHECKBUTTON_IID);
//static NS_DEFINE_IID(kIRadioButtonIID, NS_IRADIOBUTTON_IID);
//static NS_DEFINE_IID(kILabelIID, NS_ILABEL_IID);
//static NS_DEFINE_IID(kIScrollBarIID, NS_ISCROLLBAR_IID);

NS_WIDGET nsresult 
NS_CreateDialog(nsISupports* aParent, 
	        nsIDialog* aDialog, 
	        const nsRect& aRect, 
          EVENT_CALLBACK aHandleEventFunction,
	        const nsFont* aFont)
{
    nsIWidget* parent = nsnull;
    if (aParent != nsnull)
      aParent->QueryInterface(kIWidgetIID,(void**)&parent);

    nsIWidget* 	widget;
    if (NS_OK == aDialog->QueryInterface(kIWidgetIID,(void**)&widget)) {
      widget->Create(parent, aRect, aHandleEventFunction, NULL);
      widget->Show(PR_TRUE);
      if (aFont != nsnull)
        widget->SetFont(*aFont);
      NS_IF_RELEASE(widget); 
    }
    if (aParent != nsnull)
      NS_IF_RELEASE(parent);
    return NS_OK;
}

#if 0
NS_WIDGET nsresult 
NS_CreateButton(nsISupports* aParent, 
								nsIButton* aButton, 
								const nsRect& aRect, 
								EVENT_CALLBACK aHandleEventFunction,
								const nsFont* aFont)
{
	nsIWidget* parent = nsnull;
	if (aParent != nsnull)
    aParent->QueryInterface(kIWidgetIID,(void**)&parent);

  nsIWidget* 	widget;
	if (NS_OK == aButton->QueryInterface(kIWidgetIID,(void**)&widget)) {
	  widget->Create(parent, aRect, aHandleEventFunction, NULL);
	  widget->Show(PR_TRUE);
    if (aFont != nsnull)
	  	widget->SetFont(*aFont);
		NS_IF_RELEASE(widget);
	}
  
  if (aParent != nsnull)
    NS_IF_RELEASE(parent);
  return NS_OK;
}



NS_WIDGET nsresult 
NS_CreateCheckButton(nsISupports* aParent, 
											  nsICheckButton* aCheckButton, 
											  const nsRect& aRect, 
											  EVENT_CALLBACK aHandleEventFunction,
											  const nsFont* aFont)
{
	nsIWidget* parent = nsnull;
	if (aParent != nsnull)
    aParent->QueryInterface(kIWidgetIID,(void**)&parent);

 	nsIWidget* 	widget;
	if (NS_OK == aCheckButton->QueryInterface(kIWidgetIID,(void**)&widget)) {
	  widget->Create(parent, aRect, aHandleEventFunction, NULL);
	  widget->Show(PR_TRUE);
    if (aFont != nsnull)
	  	widget->SetFont(*aFont);
		NS_IF_RELEASE(widget);
	}
  if (aParent != nsnull)
    NS_IF_RELEASE(parent);
 return NS_OK;
}



NS_WIDGET nsresult 
NS_CreateRadioButton( nsISupports* aParent, 
												nsIRadioButton* aRadioButton, 
												const nsRect& aRect, 
												EVENT_CALLBACK aHandleEventFunction,
												const nsFont* aFont)
{
	nsIWidget* parent = nsnull;
	if (aParent != nsnull)
    aParent->QueryInterface(kIWidgetIID,(void**)&parent);

 	nsIWidget* 	widget;
	if (NS_OK == aRadioButton->QueryInterface(kIWidgetIID,(void**)&widget)) {
	  widget->Create(parent, aRect, aHandleEventFunction, NULL);
	  widget->Show(PR_TRUE);
    if (aFont != nsnull)
	  	widget->SetFont(*aFont);
		NS_IF_RELEASE(widget);
	}
  if (aParent != nsnull)
    NS_IF_RELEASE(parent);
 return NS_OK;
}


NS_WIDGET nsresult 
NS_CreateLabel( nsISupports* aParent, 
									nsILabel* aLabel, 
									const nsRect& aRect, 
									EVENT_CALLBACK aHandleEventFunction,
									const nsFont* aFont)
{
	nsIWidget* parent = nsnull;
	if (NS_OK == aParent->QueryInterface(kIWidgetIID,(void**)&parent))
	{
		nsIWidget* 	widget;
		if (NS_OK == aLabel->QueryInterface(kIWidgetIID,(void**)&widget)) {
	  	widget->Create(parent, aRect, aHandleEventFunction, NULL);
	  	widget->Show(PR_TRUE);
      if (aFont != nsnull)
	  	  widget->SetFont(*aFont);
			NS_IF_RELEASE(widget);
		}
		NS_IF_RELEASE(parent);
	}
  return NS_OK;
}



NS_WIDGET nsresult 
NS_CreateTextAreaWidget(nsISupports* aParent, 
									nsITextAreaWidget* aWidget, 
									const nsRect& aRect, 
									EVENT_CALLBACK aHandleEventFunction,
									const nsFont* aFont)
{
	nsIWidget* parent = nsnull;
	if (aParent != nsnull)
    aParent->QueryInterface(kIWidgetIID,(void**)&parent);

  nsIWidget* 	widget = nsnull;
	if (NS_OK == aWidget->QueryInterface(kIWidgetIID,(void**)&widget)) {
	  widget->Create(parent, aRect, aHandleEventFunction, NULL);
	  widget->Show(PR_TRUE);
    if (aFont != nsnull)
	  	widget->SetFont(*aFont);
    NS_IF_RELEASE(widget);
	}
  else
  {
    NS_ERROR("Called QueryInterface on a non kIWidgetIID supported object");
  }

	if (aParent)
	  NS_IF_RELEASE(parent);

  return NS_OK;
}

NS_WIDGET nsresult 
NS_CreateTextWidget(nsISupports* aParent, 
									nsITextWidget* aWidget, 
									const nsRect& aRect, 
									EVENT_CALLBACK aHandleEventFunction,
									const nsFont* aFont)
{
	nsIWidget* parent = nsnull;
	if (aParent != nsnull)
    aParent->QueryInterface(kIWidgetIID,(void**)&parent);

  nsIWidget* 	widget = nsnull;
	if (NS_OK == aWidget->QueryInterface(kIWidgetIID,(void**)&widget)) {
	  widget->Create(parent, aRect, aHandleEventFunction, NULL);
	  widget->Show(PR_TRUE);
    if (aFont != nsnull)
	  	widget->SetFont(*aFont);
    NS_IF_RELEASE(widget);
	}
  else
  {
    NS_ERROR("Called QueryInterface on a non kIWidgetIID supported object");
  }

	if (aParent)
	  NS_IF_RELEASE(parent);

  return NS_OK;
}



NS_WIDGET nsresult 
NS_CreateScrollBar(nsISupports* aParent, 
									nsIScrollbar* aWidget, 
									const nsRect& aRect, 
									EVENT_CALLBACK aHandleEventFunction)
{
	nsIWidget* parent = nsnull;
	if (aParent != nsnull)
    aParent->QueryInterface(kIWidgetIID,(void**)&parent);

  nsIWidget* 	widget = nsnull;
	if (NS_OK == aWidget->QueryInterface(kIWidgetIID,(void**)&widget)) {
	  widget->Create(parent, aRect, aHandleEventFunction, NULL);
	  widget->Show(PR_TRUE);
    NS_IF_RELEASE(widget);
	}
  else
  {
    NS_ERROR("Called QueryInterface on a non kIWidgetIID supported object");
  }

	if (aParent)
	  NS_IF_RELEASE(parent);

  return NS_OK;
}

NS_WIDGET nsresult 
NS_CreateListBox(nsISupports* aParent, 
									nsIListBox* aWidget, 
									const nsRect& aRect, 
                  EVENT_CALLBACK aHandleEventFunction,
                  const nsFont* aFont)
{
	nsIWidget* parent = nsnull;
	if (aParent != nsnull)
    aParent->QueryInterface(kIWidgetIID,(void**)&parent);

  nsIWidget* 	widget = nsnull;
	if (NS_OK == aWidget->QueryInterface(kIWidgetIID,(void**)&widget)) {
	  widget->Create(parent, aRect, aHandleEventFunction, NULL);
	  widget->Show(PR_TRUE);
    if (aFont != nsnull)
	  	widget->SetFont(*aFont);
    NS_IF_RELEASE(widget);
	}
  else
  {
    NS_ERROR("Called QueryInterface on a non kIWidgetIID supported object");
  }

	if (aParent)
	  NS_IF_RELEASE(parent);

  return NS_OK;
}


NS_WIDGET nsresult 
NS_CreateComboBox(nsISupports* aParent, 
									nsIComboBox* aWidget, 
									const nsRect& aRect, 
                  EVENT_CALLBACK aHandleEventFunction,
                  const nsFont* aFont)
{
	nsIWidget* parent = nsnull;
	if (aParent != nsnull)
    aParent->QueryInterface(kIWidgetIID,(void**)&parent);

  nsIWidget* 	widget = nsnull;
	if (NS_OK == aWidget->QueryInterface(kIWidgetIID,(void**)&widget)) {
	  widget->Create(parent, aRect, aHandleEventFunction, NULL);
	  widget->Show(PR_TRUE);
    if (aFont != nsnull)
	  	widget->SetFont(*aFont);
    NS_IF_RELEASE(widget);
	}
  else
  {
    NS_ERROR("Called QueryInterface on a non kIWidgetIID supported object");
  }

	if (aParent)
	  NS_IF_RELEASE(parent);

  return NS_OK;
}


NS_WIDGET nsresult 
NS_CreateTabWidget(nsISupports* aParent, 
									nsITabWidget* aWidget, 
									const nsRect& aRect, 
                  EVENT_CALLBACK aHandleEventFunction,
                  const nsFont* aFont)
{
	nsIWidget* parent = nsnull;
	if (aParent != nsnull)
    aParent->QueryInterface(kIWidgetIID,(void**)&parent);

  nsIWidget* 	widget = nsnull;
	if (NS_OK == aWidget->QueryInterface(kIWidgetIID,(void**)&widget)) {
	  widget->Create(parent, aRect, aHandleEventFunction, NULL);
	  widget->Show(PR_TRUE);
    if (aFont != nsnull)
	  	widget->SetFont(*aFont);
    NS_IF_RELEASE(widget);
	}
  else
  {
    NS_ERROR("Called QueryInterface on a non kIWidgetIID supported object");
  }

	if (aParent)
	  NS_IF_RELEASE(parent);

  return NS_OK;
}





extern NS_WIDGET nsresult 
NS_CreateTooltipWidget(nsISupports* aParent,	
                nsITooltipWidget* aWidget, 
                const nsRect& aRect, 
                EVENT_CALLBACK aHandleEventFunction,
                const nsFont* aFont)
{
	nsIWidget* parent = nsnull;
	if (aParent != nsnull)
    aParent->QueryInterface(kIWidgetIID,(void**)&parent);

  nsIWidget* 	widget = nsnull;
	if (NS_OK == aWidget->QueryInterface(kIWidgetIID,(void**)&widget)) {
	  widget->Create(parent, aRect, aHandleEventFunction, NULL);
	  widget->Show(PR_TRUE);
    if (aFont != nsnull)
	  	widget->SetFont(*aFont);
    NS_IF_RELEASE(widget);
	}
  else
  {
    NS_ERROR("Call QueryInterface on a non kIWidgetIID supported object");
  }

	if (aParent)
	  NS_IF_RELEASE(parent);

  return NS_OK;
}
#endif

extern NS_WIDGET nsresult 
NS_ShowWidget(nsISupports* aWidget, PRBool aShow)
{

  nsIWidget* 	widget = nsnull;
	if (NS_OK == aWidget->QueryInterface(kIWidgetIID,(void**)&widget)) {
	  widget->Show(aShow);
	  NS_IF_RELEASE(widget);
	}

  return NS_OK;
}

extern NS_WIDGET nsresult 
NS_MoveWidget(nsISupports* aWidget, PRUint32 aX, PRUint32 aY)
{

  nsIWidget* 	widget = nsnull;
	if (NS_OK == aWidget->QueryInterface(kIWidgetIID,(void**)&widget)) {
	  widget->Move(aX,aY);
	  NS_IF_RELEASE(widget);
	}
  return NS_OK;
}

extern NS_WIDGET nsresult 
NS_EnableWidget(nsISupports* aWidget, PRBool aEnable)
{
	void* 			result = nsnull;
	nsIWidget* 	widget;
	if (NS_OK == aWidget->QueryInterface(kIWidgetIID,(void**)&widget))
	{
		widget->Enable(aEnable);
		NS_RELEASE(widget);
	}
  return NS_OK;
}


extern NS_WIDGET nsresult 
NS_SetFocusToWidget(nsISupports* aWidget)
{

  nsIWidget* 	widget = nsnull;
	if (NS_OK == aWidget->QueryInterface(kIWidgetIID,(void**)&widget)) {
	  widget->SetFocus();
	  NS_IF_RELEASE(widget);
	}
  return NS_OK;
}


extern NS_WIDGET nsresult 
NS_GetWidgetNativeData(nsISupports* aWidget, void** aNativeData)
{
	void* 			result = nsnull;
	nsIWidget* 	widget;
	if (NS_OK == aWidget->QueryInterface(kIWidgetIID,(void**)&widget))
	{
		result = widget->GetNativeData(NS_NATIVE_WIDGET);
		NS_RELEASE(widget);
	}
	*aNativeData = result;
  return NS_OK;

}
