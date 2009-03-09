/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsScreenCocoa.h"
#include "nsObjCExceptions.h"
#include "nsCocoaUtils.h"

NS_IMPL_ISUPPORTS1(nsScreenCocoa, nsIScreen)

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
