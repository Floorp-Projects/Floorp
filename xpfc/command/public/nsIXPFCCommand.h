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

#ifndef nsIXPFCCommand_h___
#define nsIXPFCCommand_h___

#include "nsxpfc.h"
#include "nsISupports.h"

class nsIXPFCCommandReceiver;

//2cfc9370-1f46-11d2-bed9-00805f8a8dbd
#define NS_IXPFC_COMMAND_IID   \
{ 0x2cfc9370, 0x1f46, 0x11d2,    \
{ 0xbe, 0xd9, 0x00, 0x80, 0x5f, 0x8a, 0x8d, 0xbd } }


CLASS_EXPORT_XPFC nsIXPFCCommand : public nsISupports

{

public:

  NS_IMETHOD Init() = 0;

  NS_IMETHOD Execute() = 0;

  NS_IMETHOD SetReceiver(nsIXPFCCommandReceiver * aReceiver) = 0;
  NS_IMETHOD_(nsIXPFCCommandReceiver *) GetReceiver() = 0;

};

#endif /* nsIXPFCCommand_h___ */
