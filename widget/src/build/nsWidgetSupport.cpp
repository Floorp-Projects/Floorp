/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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


#include "nsWidgetSupport.h"
#include "nsRect.h"
#include "nsIAppShell.h"
#include "nsIButton.h"
#include "nsIEventListener.h"
#include "nsILabel.h"
#include "nsILookAndFeel.h"
#include "nsIMouseListener.h"
#include "nsIToolkit.h"
#include "nsIWidget.h"
#include "nsICheckButton.h"
#include "nsIScrollbar.h"
#include "nsITextWidget.h"


static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);
static NS_DEFINE_IID(kIButtonIID, NS_IBUTTON_IID);
static NS_DEFINE_IID(kITextWidgetIID, NS_ITEXTWIDGET_IID);
static NS_DEFINE_IID(kICheckButtonIID, NS_ICHECKBUTTON_IID);
static NS_DEFINE_IID(kIRadioButtonIID, NS_IRADIOBUTTON_IID);
static NS_DEFINE_IID(kILabelIID, NS_ILABEL_IID);
static NS_DEFINE_IID(kIScrollBarIID, NS_ISCROLLBAR_IID);


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
