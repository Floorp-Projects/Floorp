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

// C++ interface
#include "nsTabWidget.h"

// C interface
#include "tabapi.h"       // !! TAB-FIX

#include <stdlib.h>

// XP-com
NS_IMPL_ADDREF(nsTabWidget)
NS_IMPL_RELEASE(nsTabWidget)

nsresult nsTabWidget::QueryInterface( const nsIID &aIID, void **aInstancePtr)
{
   nsresult result = nsWindow::QueryInterface( aIID, aInstancePtr);

   if( result == NS_NOINTERFACE && aIID.Equals( nsITabWidget::GetIID()))
   {
      *aInstancePtr = (void*) ((nsITabWidget*)this);
      NS_ADDREF_THIS();
      result = NS_OK;
   }

   return result;
}

// tab methods
nsresult nsTabWidget::SetTabs( PRUint32 aNumberOfTabs,
                               const nsString aTabLabels[])
{
   nsresult rc = NS_ERROR_FAILURE;

   if( mToolkit && mWnd)
   {
      // first remove current tabs...
      // !! oops, can't do this yet :-)    // !! TAB-FIX
      // mOS2Toolkit->SendMsg( mWnd, TABM_REMOVEALLTABS);

      PRUint32   i;
      char     **strings = new char * [aNumberOfTabs];

      for( i = 0; i < aNumberOfTabs; i++)
         strings[i] = strdup( gModuleData.ConvertFromUcs( aTabLabels[i]));

      BOOL bOk = (BOOL) mOS2Toolkit->SendMsg( mWnd, TABM_INSERTMULTTABS,
                                              MPFROMSHORT(aNumberOfTabs),
                                              MPFROMP(strings));
      for( i = 0; i < aNumberOfTabs; i++)
         free( strings[i]);
      delete [] strings;

      if( bOk)
         rc = NS_OK;
   }

   return rc;
}

nsresult nsTabWidget::GetSelectedTab( PRUint32 &aTab)
{
   nsresult rc = NS_ERROR_FAILURE;

   if( mWnd && mToolkit)
   {
      MRESULT mrc = mOS2Toolkit->SendMsg( mWnd, TABM_QUERYHILITTAB);
      aTab = SHORT1FROMMR( mrc);
      if( aTab != TABC_FAILURE)
         rc = NS_OK;
      aTab--; // !! TAB-INDEX
   }

   return rc;
}

// Generate mozilla events when appropriate...
PRBool nsTabWidget::OnControl( MPARAM mp1, MPARAM mp2)
{
   switch( SHORT2FROMMP(mp1))
   {
      case TTN_TABCHANGE:
         DispatchStandardEvent( NS_TABCHANGE);
         break;
   }

   return PR_TRUE;
}

// Creation hooks...
PCSZ nsTabWidget::WindowClass()
{
  return (PCSZ) TabGetTabControlName();
}

ULONG nsTabWidget::WindowStyle()
{ 
   return BASE_CONTROL_STYLE;
}
