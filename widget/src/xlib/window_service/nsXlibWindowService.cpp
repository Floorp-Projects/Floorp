/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C) 1999
 * Christopher Blizzard.  All Rights Reserved.
 */

#include "nsXlibWindowService.h"
// yes, these are from the parent directory.
#include "nsWidget.h"
#include "nsAppShell.h"

nsXlibWindowService::nsXlibWindowService()
{
}

nsXlibWindowService::~nsXlibWindowService()
{
}

NS_IMPL_ADDREF(nsXlibWindowService)
NS_IMPL_RELEASE(nsXlibWindowService)
NS_IMPL_QUERY_INTERFACE(nsXlibWindowService, nsCOMTypeInfo<nsIXlibWindowService>::GetIID())

NS_IMETHODIMP
nsXlibWindowService::SetCreateCallback(nsXlibWindowCallback *aFunc)
{
  nsWidget::SetXlibWindowCallback(aFunc);
  return NS_OK;
}

NS_IMETHODIMP
nsXlibWindowService::DispatchNativeXlibEvent(void *aNativeEvent)
{
  nsAppShell::DispatchXEvent((XEvent *)aNativeEvent);
  return NS_OK;
}
