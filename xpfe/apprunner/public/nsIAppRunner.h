/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef nsIAppRunner_h__
#define nsIAppRunner_h__

#include "nsISupports.h"

// {5A298F60-939B-11d2-805C-00600811A9C3}
#define NS_IAPPRUNNER_IID \
    { 0x5a298f60, 0x939b, 0x11d2, \
        { 0x80, 0x5c, 0x0, 0x60, 0x8, 0x11, 0xa9, 0xc3 } };

// {5A298F60-939B-11d2-805C-00600811A9C3}
#define NS_APPRUNNER_CID \
    { 0x5a298f60, 0x939b, 0x11d2, \
        { 0x80, 0x5c, 0x0, 0x60, 0x8, 0x11, 0xa9, 0xc3 } };

class nsIAppRunner : public nsISupports {
public:

  NS_IMETHOD main( int argc, char *argv[] ) = 0;

}; // nsIAppRunner

#endif
