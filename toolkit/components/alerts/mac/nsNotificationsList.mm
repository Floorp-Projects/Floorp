/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Growl implementation of nsIAlertsService.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006-2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsNotificationsList.h"
#include "nsObjCExceptions.h"
#include "nsStringAPI.h"

NS_IMPL_ADDREF(nsNotificationsList)
NS_IMPL_RELEASE(nsNotificationsList)

NS_INTERFACE_MAP_BEGIN(nsNotificationsList)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsINotificationsList)
  NS_INTERFACE_MAP_ENTRY(nsINotificationsList)
NS_INTERFACE_MAP_END

nsNotificationsList::nsNotificationsList()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  mNames   = [[NSMutableArray alloc] init];
  mEnabled = [[NSMutableArray alloc] init];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsNotificationsList::~nsNotificationsList()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mNames release];
  [mEnabled release];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

NS_IMETHODIMP
nsNotificationsList::AddNotification(const nsAString &aName, bool aEnabled)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSString *name = [NSString stringWithCharacters: aName.BeginReading()
                                           length: aName.Length()];

  [mNames addObject: name];

  if (aEnabled)
    [mEnabled addObject: name];

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsNotificationsList::IsNotification(const nsAString &aName, bool *retVal)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSString *name = [NSString stringWithCharacters: aName.BeginReading()
                                           length: aName.Length()];

  *retVal = [mNames containsObject: name] ? PR_TRUE : PR_FALSE;
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

void
nsNotificationsList::informController(mozGrowlDelegate *aController)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [aController addNotificationNames: mNames];
  [aController addEnabledNotifications: mEnabled];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

