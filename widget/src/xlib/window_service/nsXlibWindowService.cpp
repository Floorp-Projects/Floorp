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

nsXlibWindowService::nsXlibWindowService()
{
  NS_INIT_REFCNT();
}

nsXlibWindowService::~nsXlibWindowService()
{
}

NS_IMPL_ADDREF(nsXlibWindowService)
NS_IMPL_RELEASE(nsXlibWindowService)
NS_IMPL_QUERY_INTERFACE(nsXlibWindowService, nsCOMTypeInfo<nsIXlibWindowService>::GetIID())

nsXlibWindowCallback         nsXlibWindowService::gsWindowCreateCallback = nsnull;
nsXlibWindowCallback         nsXlibWindowService::gsWindowDestroyCallback = nsnull;
nsXlibEventDispatcher        nsXlibWindowService::gsEventDispatcher = nsnull;
nsXlibTimeToNextTimeoutFunc  nsXlibWindowService::gsTimeToNextTimeoutFunc = nsnull;
nsXlibProcessTimeoutsProc    nsXlibWindowService::gsProcessTimeoutsProc = nsnull;

NS_IMETHODIMP
nsXlibWindowService::SetWindowCreateCallback(nsXlibWindowCallback aCallback)
{
  NS_ASSERTION(nsnull != aCallback,"null in ptr.");

  gsWindowCreateCallback = aCallback;

  return NS_OK;
}
//////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsXlibWindowService::SetWindowDestroyCallback(nsXlibWindowCallback aCallback)
{
  NS_ASSERTION(nsnull != aCallback,"null in ptr.");

  gsWindowDestroyCallback = aCallback;

  return NS_OK;
}
//////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsXlibWindowService::SetEventDispatcher(nsXlibEventDispatcher aDispatcher)
{
  NS_ASSERTION(nsnull != aDispatcher,"null in ptr.");

  gsEventDispatcher = aDispatcher;

  return NS_OK;
}
//////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsXlibWindowService::GetWindowCreateCallback(nsXlibWindowCallback * aCallbackOut)
{
  NS_ASSERTION(nsnull != aCallbackOut,"null out ptr.");

  *aCallbackOut = gsWindowCreateCallback;

  return NS_OK;
}
//////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsXlibWindowService::GetWindowDestroyCallback(nsXlibWindowCallback * aCallbackOut)
{
  NS_ASSERTION(nsnull != aCallbackOut,"null out ptr.");

  *aCallbackOut = gsWindowDestroyCallback;

  return NS_OK;
}
//////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsXlibWindowService::GetEventDispatcher(nsXlibEventDispatcher * aDispatcherOut)
{
  NS_ASSERTION(nsnull != aDispatcherOut,"null out ptr.");

  *aDispatcherOut = gsEventDispatcher;

  return NS_OK;
}
//////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsXlibWindowService::SetTimeToNextTimeoutFunc(nsXlibTimeToNextTimeoutFunc aFunc)
{
  NS_ASSERTION(nsnull != aFunc,"null in ptr.");

  gsTimeToNextTimeoutFunc = aFunc;

  return NS_OK;
}
//////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsXlibWindowService::GetTimeToNextTimeoutFunc(nsXlibTimeToNextTimeoutFunc * aFuncOut)
{
  NS_ASSERTION(nsnull != aFuncOut,"null out ptr.");

  *aFuncOut = gsTimeToNextTimeoutFunc;

  return NS_OK;
}
//////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsXlibWindowService::SetProcessTimeoutsProc(nsXlibProcessTimeoutsProc aProc)
{
  NS_ASSERTION(nsnull != aProc,"null in ptr.");

  gsProcessTimeoutsProc = aProc;

  return NS_OK;
}
//////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsXlibWindowService::GetProcessTimeoutsProc(nsXlibProcessTimeoutsProc * aProcOut)
{
  NS_ASSERTION(nsnull != aProcOut,"null out ptr.");

  *aProcOut = gsProcessTimeoutsProc;

  return NS_OK;
}
//////////////////////////////////////////////////////////////////////////
