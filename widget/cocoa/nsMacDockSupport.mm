/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#include "nsMacDockSupport.h"
#include "nsObjCExceptions.h"

NS_IMPL_ISUPPORTS1(nsMacDockSupport, nsIMacDockSupport)

NS_IMETHODIMP
nsMacDockSupport::GetDockMenu(nsIStandaloneNativeMenu ** aDockMenu)
{
  *aDockMenu = nullptr;

  if (mDockMenu)
    return mDockMenu->QueryInterface(NS_GET_IID(nsIStandaloneNativeMenu),
                                     reinterpret_cast<void **>(aDockMenu));
  return NS_OK;
}

NS_IMETHODIMP
nsMacDockSupport::SetDockMenu(nsIStandaloneNativeMenu * aDockMenu)
{
  nsresult rv;
  mDockMenu = do_QueryInterface(aDockMenu, &rv);
  return rv;
}

NS_IMETHODIMP
nsMacDockSupport::ActivateApplication(bool aIgnoreOtherApplications)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  [[NSApplication sharedApplication] activateIgnoringOtherApps:aIgnoreOtherApplications];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsMacDockSupport::SetBadgeText(const nsAString& aBadgeText)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSDockTile *tile = [[NSApplication sharedApplication] dockTile];
  mBadgeText = aBadgeText;
  if (aBadgeText.IsEmpty())
    [tile setBadgeLabel: nil];
  else
    [tile setBadgeLabel:[NSString stringWithCharacters:mBadgeText.get() length:mBadgeText.Length()]];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsMacDockSupport::GetBadgeText(nsAString& aBadgeText)
{
  aBadgeText = mBadgeText;
  return NS_OK;
}
