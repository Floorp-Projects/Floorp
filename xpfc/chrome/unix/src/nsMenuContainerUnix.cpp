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

#include "nscore.h"
#include "nsMenuContainerUnix.h"

#include <Xm/Xm.h>
#include <Xm/CascadeBG.h>
#include <Xm/PushBG.h>
#include <Xm/SeparatoG.h>
#include <Xm/RowColumn.h>
#include <Xm/CascadeBG.h>

static NS_DEFINE_IID(kCIMenuContainerIID, NS_IMENUCONTAINER_IID);
static NS_DEFINE_IID(kCIMenuItemIID, NS_IMENUITEM_IID);

nsMenuContainerUnix::nsMenuContainerUnix() : nsMenuContainer()
{
  mMenuBar = nsnull;
}

nsMenuContainerUnix::~nsMenuContainerUnix()
{
}

void* nsMenuContainerUnix::GetNativeHandle()
{
  return nsnull;
}

nsresult nsMenuContainerUnix :: AddMenuItem(nsIMenuItem * aMenuItem)
{
  return NS_OK;
}


nsresult nsMenuContainerUnix :: Update()
{
  return NS_OK;
}

nsresult nsMenuContainerUnix :: SetShellInstance(nsIShellInstance * aShellInstance)
{
  nsIWidget * window = aShellInstance->GetApplicationWidget();

  Widget parent = XtParent((Widget)window->GetNativeData(NS_NATIVE_WIDGET));

  mMenuBar = ::XmCreateMenuBar(parent, "menubar", nsnull, 0);

  XtManageChild(mMenuBar);

  return (NS_OK);
}
