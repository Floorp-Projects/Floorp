/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 */

#include "nsButton.h"
#if TARGET_CARBON || (UNIVERSAL_INTERFACES_VERSION >= 0x0330)
#include <ControlDefinitions.h>
#endif

NS_IMPL_ADDREF(nsButton);
NS_IMPL_RELEASE(nsButton);

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
nsButton::nsButton() : nsMacControl(), nsIButton()
{
  NS_INIT_REFCNT();
  WIDGET_SET_CLASSNAME("nsButton");
  SetControlType(pushButProc);
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
nsButton::~nsButton()
{
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_INTERFACE_MAP_BEGIN(nsButton)
  NS_INTERFACE_MAP_ENTRY(nsIButton)
NS_INTERFACE_MAP_END_INHERITING(nsWindow)

#pragma mark -
//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsButton::SetLabel(const nsString& aText)
{
	mLabel = aText;
	Invalidate(PR_TRUE);
	return NS_OK;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsButton::GetLabel(nsString& aBuffer)
{
	aBuffer = mLabel;
  return NS_OK;
}

