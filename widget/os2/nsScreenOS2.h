/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsScreenOS2_h___
#define nsScreenOS2_h___

#include "nsBaseScreen.h"

#define INCL_WIN
#define INCL_DOS
#include <os2.h>

//------------------------------------------------------------------------

class nsScreenOS2 : public nsBaseScreen
{
public:
  nsScreenOS2 ( );
  virtual ~nsScreenOS2();

  NS_IMETHOD GetRect(int32_t* aLeft, int32_t* aTop, int32_t* aWidth, int32_t* aHeight);
  NS_IMETHOD GetAvailRect(int32_t* aLeft, int32_t* aTop, int32_t* aWidth, int32_t* aHeight);
  NS_IMETHOD GetPixelDepth(int32_t* aPixelDepth);
  NS_IMETHOD GetColorDepth(int32_t* aColorDepth);

private:

};

#endif  // nsScreenOS2_h___ 
