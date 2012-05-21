/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsScreenWin_h___
#define nsScreenWin_h___

#include <windows.h>
#include "nsBaseScreen.h"

//------------------------------------------------------------------------

class nsScreenWin : public nsBaseScreen
{
public:
  nsScreenWin ( HMONITOR inScreen );
  ~nsScreenWin();

  NS_IMETHOD GetRect(PRInt32* aLeft, PRInt32* aTop, PRInt32* aWidth, PRInt32* aHeight);
  NS_IMETHOD GetAvailRect(PRInt32* aLeft, PRInt32* aTop, PRInt32* aWidth, PRInt32* aHeight);
  NS_IMETHOD GetPixelDepth(PRInt32* aPixelDepth);
  NS_IMETHOD GetColorDepth(PRInt32* aColorDepth);

private:

  HMONITOR mScreen;
};

#endif  // nsScreenWin_h___ 
