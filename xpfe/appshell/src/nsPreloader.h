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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 */

#include "nsICmdLineHandler.h"

#define NS_PRELOADER_CID \
{ /* 5ad506ee-393e-45d9-8d6f-53066fa8dc3a */ \
    0x5ad506ee, \
    0x393e, \
    0x45d9, \
    {0x8d, 0x6f, 0x53, 0x06, 0x6f, 0xa8, 0xdc, 0x3a} \
  } \

#define NS_PRELOADER_CONTRACTID \
  "@mozilla.org/commandlinehandler/general-startup;1?type=preloader"

class nsPreloader : public nsICmdLineHandler
{
 public:
    nsPreloader();
    virtual ~nsPreloader();

    NS_DECL_ISUPPORTS
    NS_DECL_NSICMDLINEHANDLER
    CMDLINEHANDLER_REGISTERPROC_DECLS

    nsresult Init();
};

