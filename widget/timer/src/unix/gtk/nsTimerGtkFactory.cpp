/* -*- Mode: C++; tab-width: 2; indentT-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "nsCOMPtr.h"

#include "nsUnixTimerCIID.h"
#include "nsTimerGtk.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsTimerGtk)

static nsModuleComponentInfo components[] =
{
  { NS_TIMER_GTK_CID, &nsTimerGtkConstructor, "component://netscape/timer/unix/gtk", "GTK timer", },
};

NS_IMPL_MODULE(nsGtkTimerModule, components)
NS_IMPL_NSGETMODULE(nsGtkTimerModule)

