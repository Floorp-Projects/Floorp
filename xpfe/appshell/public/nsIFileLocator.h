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

#ifndef nsIFileLocator_h__
#define nsIFileLocator_h__

#include "nsISupports.h"
#include "nsFileLocations.h"

/* Forward declarations... */
class nsIFactory;

// {7e44eb01-e600-11d2-915f-f08a208628fc}
#define NS_IFILELOCATOR_IID \
{ 0x7e44eb01, 0xe600, 0x11d2, \
  {0x91, 0x5f, 0xf0, 0x8a, 0x20, 0x86, 0x28, 0xfc} }

// {78043e01-e603-11d2-915f-f08a208628fc}
#define NS_FILELOCATOR_CID \
{ 0x78043e01, 0xe603, 0x11d2, \
  {0x91, 0x5f, 0xf0, 0x8a, 0x20, 0x86, 0x28, 0xfc} }


class nsIFileLocator : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFILELOCATOR_IID)

  NS_IMETHOD GetFileLocation(
      nsSpecialFileSpec::LocationType aType,
      nsFileSpec* outSpec) = 0;
};

extern "C" NS_APPSHELL nsresult
NS_NewFileLocatorFactory(nsIFactory** aFactory);

#endif /* nsIFileLocator_h__ */
