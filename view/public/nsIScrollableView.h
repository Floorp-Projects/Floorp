/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsIScrollableView_h___
#define nsIScrollableView_h___

#include "nsISupports.h"

// IID for the nsIView interface
#define NS_ISCROLLABLEVIEW_IID    \
{ 0xc95f1830, 0xc376, 0x11d1, \
{ 0xb7, 0x21, 0x0, 0x60, 0x8, 0x91, 0xd8, 0xc9 } }

class nsIScrollableView : public nsISupports
{
public:
  /**
   * Set the height of the container in nscoords.
   * @param aSize height of container in twips.
   */
  virtual void SetContainerSize(PRInt32 aSize) = 0;

  /**
   * Get the height of the container
   * @result height of container
   */
  virtual PRInt32 GetContainerSize(void) = 0;

  /**
   * Set the offset into the container of the
   * topmost visible coordinate
   * @param aOffset offset in twips
   */
  virtual void SetVisibleOffset(PRInt32 aOffset) = 0;

  /**
   * Get the offset of the topmost visible coordinate
   * @result coordinate in twips
   */
  virtual PRInt32 GetVisibleOffset(void) = 0;
};

#endif
