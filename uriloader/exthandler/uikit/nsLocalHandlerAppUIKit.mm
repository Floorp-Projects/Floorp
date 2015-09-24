/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:expandtab:shiftwidth=2:tabstop=2:cin:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <CoreFoundation/CoreFoundation.h>

#include "nsLocalHandlerAppUIKit.h"
#include "nsIURI.h"

NS_IMETHODIMP
nsLocalHandlerAppUIKit::LaunchWithURI(nsIURI* aURI,
                                      nsIInterfaceRequestor* aWindowContext)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
