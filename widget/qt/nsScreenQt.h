/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsScreenQt_h___
#define nsScreenQt_h___

#include "nsBaseScreen.h"

#ifdef MOZ_ENABLE_QMSYSTEM2
namespace MeeGo
{
    class QmDisplayState;
}
#endif


//------------------------------------------------------------------------

class nsScreenQt : public nsBaseScreen
{
public:
  nsScreenQt (int aScreen);
  virtual ~nsScreenQt();

  NS_IMETHOD GetRect(PRInt32* aLeft, PRInt32* aTop, PRInt32* aWidth, PRInt32* aHeight);
  NS_IMETHOD GetAvailRect(PRInt32* aLeft, PRInt32* aTop, PRInt32* aWidth, PRInt32* aHeight);
  NS_IMETHOD GetPixelDepth(PRInt32* aPixelDepth);
  NS_IMETHOD GetColorDepth(PRInt32* aColorDepth);

#ifdef MOZ_ENABLE_QMSYSTEM2
protected:
  virtual void ApplyMinimumBrightness(PRUint32 aType) MOZ_OVERRIDE;
private:
  MeeGo::QmDisplayState* mDisplayState;
#endif
private:
  int mScreen;
};

#endif  // nsScreenQt_h___
