/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date           Modified by     Description of modification
 * 03/28/2000   IBM Corp.        Changes to make os2.h file similar to windows.h file
 */

// This class is part of the strange printing `architecture'.
// The `CreateDeviceContextSpec' method basically selects a print queue,
//   known here as an `nsIDeviceContextSpec'.  This is given to a method
//   in nsIDeviceContext which creates a fresh device context for that
//   printer.

#ifndef _nsDeviceContextSpecFactoryOS2_h
#define _nsDeviceContextSpecFactoryOS2_h

#include "nsIDeviceContextSpecFactory.h"
#include "nsIDeviceContextSpec.h"

class nsDeviceContextSpecFactoryOS2 : public nsIDeviceContextSpecFactory
{
 public:
   nsDeviceContextSpecFactoryOS2();

   NS_DECL_ISUPPORTS

   NS_IMETHOD Init(void);
   NS_IMETHOD CreateDeviceContextSpec( nsIDeviceContextSpec *aOldSpec,
                                       nsIDeviceContextSpec *&aNewSpec,
                                       PRBool aQuiet);

 protected:
   virtual ~nsDeviceContextSpecFactoryOS2() {}
};

#endif
