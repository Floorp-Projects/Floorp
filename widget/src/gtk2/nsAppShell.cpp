/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 */

#include "nsAppShell.h"

#include <gtk/gtkmain.h>

static PRBool sInitialized = PR_FALSE;

nsAppShell::nsAppShell(void)
{
  NS_INIT_REFCNT();
}

nsAppShell::~nsAppShell(void)
{
}

NS_IMPL_ISUPPORTS1(nsAppShell, nsIAppShell)

NS_IMETHODIMP
nsAppShell::Create(int *argc, char **argv)
{
  if (sInitialized)
    return NS_OK;

  sInitialized = PR_TRUE;

  gtk_init(argc, &argv);

  return NS_OK;
}

NS_IMETHODIMP
nsAppShell::Run(void)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsAppShell::Spinup(void)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsAppShell::Spindown(void)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsAppShell::ListenToEventQueue(nsIEventQueue *aQueue, PRBool aListen)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsAppShell::GetNativeEvent(PRBool &aRealEvent, void * &aEvent)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsAppShell::DispatchNativeEvent(PRBool aRealEvent, void *aEvent)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsAppShell::SetDispatchListener(nsDispatchListener *aDispatchListener)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsAppShell::Exit(void)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
