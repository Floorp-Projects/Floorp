/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef nsINetOStream_h___
#define nsINetOStream_h___

#include "nsIOutputStream.h"

class nsINetOStream : public nsIOutputStream {   
public:

    NS_IMETHOD
    Complete(void) = 0;

    NS_IMETHOD
    Abort(PRInt32 status) = 0;

    NS_IMETHOD
    WriteReady(PRUint32 *aReadyCount) = 0;
};





/* 7f13b870-e95f-11d2-beae-00805f8a66dc */
#define NS_INETOSTREAM_IID   \
{ 0x7f13b870, 0xe95f, 0x11d2, \
  {0xbe, 0xae, 0x00, 0x80, 0x5f, 0x8a, 0x66, 0xdc} }

#endif /* nsINetOStream_h___ */
