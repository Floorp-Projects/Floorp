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

#include <gtk/gtk.h>

#include "nsLabel.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"

#include "nsGtkEventHandler.h"

NS_IMPL_ADDREF_INHERITED(nsLabel, nsWidget)
NS_IMPL_RELEASE_INHERITED(nsLabel, nsWidget)

//-------------------------------------------------------------------------
//
// nsLabel constructor
//
//-------------------------------------------------------------------------
nsLabel::nsLabel() : nsWidget(), nsILabel()
{
  NS_INIT_REFCNT();
  mAlignment = eAlign_Left;
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
// Create the nativeLabel widget
//
//-------------------------------------------------------------------------
NS_METHOD  nsLabel::CreateNative(GtkWidget *parentWindow)
{
  unsigned char alignment = GetNativeAlignment();

  mWidget = gtk_label_new("");
  gtk_widget_set_name(mWidget, "nsLabel");
  gtk_misc_set_alignment(GTK_MISC(mWidget), 0.0, alignment);

  return NS_OK;
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
  GtkJustification align;

  mAlignment = aAlignment;

  align = GetNativeAlignment();
  gtk_misc_set_alignment(GTK_MISC(mWidget), 0.0, align);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsLabel::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{

  nsresult result = nsWidget::QueryInterface(aIID, aInstancePtr);

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
GtkJustification nsLabel::GetNativeAlignment()
{
  switch (mAlignment) {
    case eAlign_Right : return GTK_JUSTIFY_RIGHT;
    case eAlign_Left  : return GTK_JUSTIFY_LEFT;
    case eAlign_Center: return GTK_JUSTIFY_CENTER;
    default :
      return GTK_JUSTIFY_LEFT;
  }
  return GTK_JUSTIFY_LEFT;
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsLabel::SetLabel(const nsString& aText)
{
  NS_ALLOC_STR_BUF(label, aText, 256);
  gtk_label_set(GTK_LABEL(mWidget), label);
  NS_FREE_STR_BUF(label);
  return NS_OK;

}

//-------------------------------------------------------------------------
//
// Get this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsLabel::GetLabel(nsString& aBuffer)
{
  char * text;
  gtk_label_get(GTK_LABEL(mWidget), &text);
  aBuffer.SetLength(0);
  aBuffer.Append(text);
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
