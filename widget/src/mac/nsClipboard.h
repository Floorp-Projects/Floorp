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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

//
// Mike Pinkerton
// Netscape Communications
//
// Native MacOS Clipboard Implementation of nsIClipboard. This contains
// the FE-specific implementations of the pure virtuals in nsBaseClipboard
//

#ifndef nsClipboard_h__
#define nsClipboard_h__

#include "nsBaseClipboard.h"

#include "Types.h"

class nsITransferable;


class nsClipboard : public nsBaseClipboard
{

public:
  nsClipboard();
  virtual ~nsClipboard();

  // nsIClipboard  
  //NS_IMETHOD ForceDataToClipboard();
  NS_IMETHOD  HasDataMatchingFlavors(nsISupportsArray *aFlavorList, PRInt32 aWhichClipboard, PRBool *_retval); 

protected:

  // impelement the native clipboard behavior
  NS_IMETHOD SetNativeClipboardData ( PRInt32 aWhichClipboard );
  NS_IMETHOD GetNativeClipboardData ( nsITransferable * aTransferable, PRInt32 aWhichClipboard );

  // helper to get the data off the clipboard. Caller responsible for deleting
  // |outData| with delete[].
  nsresult GetDataOffClipboard ( ResType inMacFlavor, void** outData, PRInt32* outDataSize ) ;

  // helper to check if the data is really there
  PRBool CheckIfFlavorPresent ( ResType inMacFlavor ) ;

  // actually places data on the clipboard
  nsresult PutOnClipboard ( ResType inFlavor, const void* inData, short inLen ) ;

}; // nsClipboard

#endif // nsClipboard_h__
