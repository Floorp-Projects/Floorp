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

#ifndef nsIClipboard_h__
#define nsIClipboard_h__

#include "nsISupports.h"

class nsITransferable;
class nsIClipboardOwner;
class nsIDataFlavor;

// {8B5314BA-DB01-11d2-96CE-0060B0FB9956}
#define NS_ICLIPBOARD_IID      \
{ 0x8b5314ba, 0xdb01, 0x11d2, { 0x96, 0xce, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56 } }

class nsIClipboard : public nsISupports {

  public:

   /**
    * Given a transferable, set the data on the native clipboard
    *
    * @param  aTransferable The transferable
    * @param  anOwner The owner of the transferable
    * @result NS_Ok if no errors
    */
  
    NS_IMETHOD SetData(nsITransferable * aTransferable, nsIClipboardOwner * anOwner) = 0;

   /**
    * Given a transferable, get the clipboard data.
    *
    * @param  aTransferable The transferable
    * @result NS_Ok if no errors
    */
  
    NS_IMETHOD GetData(nsITransferable ** aTransferable) = 0;

   /**
    * Gets the data from the clipboard and put it into the transferable object
    *
    * @param  aTransferable The transferable
    * @result NS_Ok if no errors
    */
  
    NS_IMETHOD GetClipboard() = 0;

   /**
    * Sets the clipboard from the transferable object
    *
    * @param  aTransferable The transferable
    * @result NS_Ok if no errors
    */
  
    NS_IMETHOD SetClipboard() = 0;

   /**
    * Check to set if ant of the native data on the clipboard matches this data flavor
    *
    * @result NS_Ok if if the data flavor is supported and, NS_ERROR_FAILURE is it is not
    */
  
    NS_IMETHOD IsDataFlavorSupported(nsIDataFlavor * aDataFlavor) = 0;

   /**
    * This empties the clipboard and notifies the clipboard owner
    * This empties the "logical" clipboard it does not clear the native clipboard
    *
    * @result NS_OkK if successfull
    */
  
    NS_IMETHOD EmptyClipboard() = 0;

   /**
    * Some platforms support deferred notification for putting data on the clipboard
    * This method forces the data onto the clipboard in its various formats
    * This may be used if the application going away.
    *
    * @result NS_OkK if successfull
    */
  
    NS_IMETHOD ForceDataToClipboard() = 0;

};

#endif
