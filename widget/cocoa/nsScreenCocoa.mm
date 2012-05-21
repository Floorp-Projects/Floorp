/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsScreenCocoa.h"
#include "nsObjCExceptions.h"
#include "nsCocoaUtils.h"

nsScreenCocoa::nsScreenCocoa (NSScreen *screen)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  mScreen = [screen retain];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsScreenCocoa::~nsScreenCocoa ()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mScreen release];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

NS_IMETHODIMP
nsScreenCocoa::GetRect(PRInt32 *outX, PRInt32 *outY, PRInt32 *outWidth, PRInt32 *outHeight)
{
  nsIntRect r = nsCocoaUtils::CocoaRectToGeckoRect([mScreen frame]);

  *outX = r.x;
  *outY = r.y;
  *outWidth = r.width;
  *outHeight = r.height;

  return NS_OK;
}

NS_IMETHODIMP
nsScreenCocoa::GetAvailRect(PRInt32 *outX, PRInt32 *outY, PRInt32 *outWidth, PRInt32 *outHeight)
{
  nsIntRect r = nsCocoaUtils::CocoaRectToGeckoRect([mScreen visibleFrame]);

  *outX = r.x;
  *outY = r.y;
  *outWidth = r.width;
  *outHeight = r.height;

  return NS_OK;
}

NS_IMETHODIMP
nsScreenCocoa::GetPixelDepth(PRInt32 *aPixelDepth)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSWindowDepth depth = [mScreen depth];
  int bpp = NSBitsPerPixelFromDepth(depth);

  *aPixelDepth = bpp;
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsScreenCocoa::GetColorDepth(PRInt32 *aColorDepth)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSWindowDepth depth = [mScreen depth];
  int bpp = NSBitsPerPixelFromDepth (depth);

  *aColorDepth = bpp;
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}
