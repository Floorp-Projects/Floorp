/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 */

#include "nsLabel.h"

// XP-com
NS_IMPL_ADDREF(nsLabel)
NS_IMPL_RELEASE(nsLabel)

nsresult nsLabel::QueryInterface( const nsIID &aIID, void **aInstancePtr)
{
   nsresult result = nsWindow::QueryInterface( aIID, aInstancePtr);

   if( result == NS_NOINTERFACE && aIID.Equals( nsILabel::GetIID()))
   {
      *aInstancePtr = (void*) ((nsILabel*)this);
      NS_ADDREF_THIS();
      result = NS_OK;
   }

   return result;
}

// nsLabel constructor
nsLabel::nsLabel()
{
   mAlignment = eAlign_Left;
}

// Creation hook to create with correct aligment
nsresult nsLabel::PreCreateWidget( nsWidgetInitData *aInitData)
{
   if( nsnull != aInitData)
   {
      nsLabelInitData* data = (nsLabelInitData *) aInitData;
      mAlignment = data->mAlignment;
   }
   return NS_OK;
}

ULONG nsLabel::GetPMStyle( nsLabelAlignment aAlignment)
{
   ULONG rc = 0;

   switch( aAlignment)
   {
      case eAlign_Right : rc = DT_RIGHT; break;
      case eAlign_Left  : rc = DT_LEFT;  break;
      case eAlign_Center: rc = DT_CENTER; break;
   }

   return rc;
}

// Label style
nsresult nsLabel::SetAlignment( nsLabelAlignment aAlignment)
{
   if( mWnd && aAlignment != mAlignment)
   {
      // already created a PM window; remove current aligment & add new
      RemoveFromStyle( GetPMStyle( mAlignment));
      AddToStyle( GetPMStyle( aAlignment));
   }

   mAlignment = aAlignment;
   return NS_OK;
}

// Label text
NS_IMPL_LABEL(nsLabel)

// Creation hooks
PCSZ nsLabel::WindowClass()
{
  return WC_STATIC;
}

ULONG nsLabel::WindowStyle()
{ 
   return BASE_CONTROL_STYLE | SS_TEXT | GetPMStyle( mAlignment);
}
