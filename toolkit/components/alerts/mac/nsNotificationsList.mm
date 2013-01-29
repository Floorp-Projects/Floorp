/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

  *retVal = [mNames containsObject: name] ? true : false;
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

