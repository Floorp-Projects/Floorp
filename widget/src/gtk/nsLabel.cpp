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

#include <gtk/gtk.h>

#include "nsLabel.h"
#include "nsString.h"

NS_IMPL_ADDREF_INHERITED(nsLabel, nsWidget)
NS_IMPL_RELEASE_INHERITED(nsLabel, nsWidget)
NS_IMPL_QUERY_INTERFACE2(nsLabel, nsILabel, nsIWidget)

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
NS_IMETHODIMP  nsLabel::CreateNative(GtkObject *parentWindow)
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
NS_IMETHODIMP nsLabel::PreCreateWidget(nsWidgetInitData *aInitData)
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
NS_IMETHODIMP nsLabel::SetAlignment(nsLabelAlignment aAlignment)
{
  GtkJustification align;

  mAlignment = aAlignment;

  align = GetNativeAlignment();
  gtk_misc_set_alignment(GTK_MISC(mWidget), 0.0, align);
  return NS_OK;
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
  gtk_label_set(GTK_LABEL(mWidget),
                NS_LossyConvertUCS2toASCII(aText).get());
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
  aBuffer.AppendWithConversion(text);
  return NS_OK;
}
