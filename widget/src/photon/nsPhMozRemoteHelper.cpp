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
 *   Adrian Mardare <amardare@qnx.com>
 */

/* the XRemoteService.cpp insists on getting the "widget helper service" */

#include <stdlib.h>
#include <nsIWidget.h>
#include <nsIXRemoteService.h>
#include <nsCOMPtr.h>
#include "nsPhMozRemoteHelper.h"

nsPhXRemoteWidgetHelper::nsPhXRemoteWidgetHelper()
{
}

nsPhXRemoteWidgetHelper::~nsPhXRemoteWidgetHelper()
{
}

NS_IMPL_ISUPPORTS1(nsPhXRemoteWidgetHelper, nsIXRemoteWidgetHelper)

NS_IMETHODIMP
nsPhXRemoteWidgetHelper::EnableXRemoteCommands(nsIWidget *aWidget)
{
  return NS_OK;
}

