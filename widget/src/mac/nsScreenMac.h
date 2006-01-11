/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsScreenMac_h___
#define nsScreenMac_h___

#include "nsIScreen.h"

#include <Quickdraw.h>

//------------------------------------------------------------------------

class nsScreenMac : public nsIScreen
{
public:
  nsScreenMac ( GDHandle inScreen );
  ~nsScreenMac();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISCREEN

private:

#if !TARGET_CARBON
    // Take out the menu bar from the reported size of this screen.
  void SubtractMenuBar ( const Rect & inScreenRect, Rect* outAdjustedRect ) ;
#endif
  
    // are we the primary screen? Needed so we can sub out the menubar if
    // asked.
  PRBool IsPrimaryScreen ( ) const { return (mScreen == ::GetMainDevice()); }
  
  GDHandle mScreen;   // the device that represents this screen
};

#endif  // nsScreenMac_h___ 
