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

#include "nsLabel.h"
#include "nsILabel.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"

#include "nsXtEventHandler.h"

#include <Xm/Label.h>

NS_IMPL_ADDREF(nsLabel)
NS_IMPL_RELEASE(nsLabel)

//-------------------------------------------------------------------------
//
// nsLabel constructor
//
//-------------------------------------------------------------------------
nsLabel::nsLabel() : nsWindow(), nsILabel()
{
  NS_INIT_REFCNT();
  mAlignment = eAlign_Left;
}


NS_METHOD nsLabel::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData) 
{
  aParent->AddChild(this);
  Widget parentWidget = nsnull;

  if (aParent) {
    parentWidget = (Widget) aParent->GetNativeData(NS_NATIVE_WIDGET);
  } else if (aAppShell) {
    parentWidget = (Widget) aAppShell->GetNativeData(NS_NATIVE_SHELL);
  }

  InitToolkit(aToolkit, aParent);
  InitDeviceContext(aContext, parentWidget);

  unsigned char alignment = GetNativeAlignment();

  mWidget = ::XtVaCreateManagedWidget("label",
                                    xmLabelWidgetClass, 
                                    parentWidget,
                                    XmNwidth, aRect.width,
                                    XmNheight, aRect.height,
                                    XmNrecomputeSize, False,
                                    XmNhighlightOnEnter, False,
		                    XmNx, aRect.x,
		                    XmNy, aRect.y, 
                                    XmNalignment, alignment,
                                    nsnull);

  // save the event callback function
  mEventCallback = aHandleEventFunction;

  InitCallbacks("nsLabel");
  return NS_OK;
}

NS_METHOD nsLabel::Create(nsNativeWidget aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsLabel::PreCreateWidget(nsWidgetInitData *aInitData)
{
  if (nsnull != aInitData) {
    nsLabelInitData* data = (nsLabelInitData *) aInitData;
    mAlignment = data->mAlignment;
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set alignment
//
//-------------------------------------------------------------------------
NS_METHOD nsLabel::SetAlignment(nsLabelAlignment aAlignment)
{
  mAlignment = aAlignment;
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// nsLabel destructor
//
//-------------------------------------------------------------------------
nsLabel::~nsLabel()
{
}

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsLabel::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  nsresult result = nsWindow::QueryInterface(aIID, aInstancePtr);

  static NS_DEFINE_IID(kILabelIID, NS_ILABEL_IID);
  if (result == NS_NOINTERFACE && aIID.Equals(kILabelIID)) {
      *aInstancePtr = (void*) ((nsILabel*)this);
      AddRef();
      result = NS_OK;
  }

  return result;
}


//-------------------------------------------------------------------------
//
// 
//
//-------------------------------------------------------------------------
unsigned char nsLabel::GetNativeAlignment()
{
  switch (mAlignment) {
    case eAlign_Right : return XmALIGNMENT_END;
    case eAlign_Left  : return XmALIGNMENT_BEGINNING;
    case eAlign_Center: return XmALIGNMENT_CENTER;
    default :
      return XmALIGNMENT_BEGINNING;
  }
  return XmALIGNMENT_BEGINNING;
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsLabel::SetLabel(const nsString& aText)
{
  NS_ALLOC_STR_BUF(label, aText, 256);
  XmString str;
  str = XmStringCreate(label, XmFONTLIST_DEFAULT_TAG);
  XtVaSetValues(mWidget, XmNlabelString, str, nsnull);
  NS_FREE_STR_BUF(label);
  XmStringFree(str);
  return NS_OK;

}

//-------------------------------------------------------------------------
//
// Get this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsLabel::GetLabel(nsString& aBuffer)
{
  XmString str;
  XtVaGetValues(mWidget, XmNlabelString, &str, nsnull);
  char * text;
  if (XmStringGetLtoR(str, XmFONTLIST_DEFAULT_TAG, &text)) {
    aBuffer.SetLength(0);
    aBuffer.Append(text);
    XtFree(text);
  }

  XmStringFree(str);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// paint message. Don't send the paint out
//
//-------------------------------------------------------------------------
PRBool nsLabel::OnMove(PRInt32, PRInt32)
{
  return PR_FALSE;
}

PRBool nsLabel::OnPaint(nsPaintEvent &aEvent)
{
  //printf("** nsLabel::OnPaint **\n");
  return PR_FALSE;
}

PRBool nsLabel::OnResize(nsSizeEvent &aEvent)
{
    return PR_FALSE;
}


