/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsILoadAttribs_h___
#define nsILoadAttribs_h___

#include "nscore.h"
#include "prtypes.h"
#include "nsISupports.h"

// Class ID for an implementation of nsILoadAttribs
// {8942D321-48D3-11d2-9E7A-006008BF092E}
#define NS_ILOAD_ATTRIBS_IID \
 { 0x8942d321, 0x48d3, 0x11d2,{0x9e, 0x7a, 0x00, 0x60, 0x08, 0xbf, 0x09, 0x2e}}

/*
 * The nsReloadType represents the following:
 *    nsURLReload                    - normal reload (uses cache)
 *    nsURLReloadBypassCache         - bypass the cache
 *    nsURLReloadBypassCacheAndProxy - bypass the proxy (not yet implemented)
 */
typedef enum {
  nsURLReload = 0,
  nsURLReloadBypassCache,
  nsURLReloadBypassProxy,
  nsURLReloadBypassCacheAndProxy,
  nsURLReloadMax
} nsURLReloadType;

/*
 * The nsLoadType represents the following:
 *    nsURLLoadNormal      - Load the URL normally.
 *    nsURLLoadBackground  - Supress all notifications when loading the URL.
 */
typedef enum {
  nsURLLoadNormal = 0,
  nsURLLoadBackground,
  nsURLLoadMax
} nsURLLoadType;

// Defining attributes of a url's load behavior.
class nsILoadAttribs : public nsISupports {
public:
    // Copy the state of another nsILoadAttribs instance.
  NS_IMETHOD Clone(nsILoadAttribs* aLoadAttribs) = 0;

  // Bypass Proxy.
  NS_IMETHOD SetBypassProxy(PRBool aBypass) = 0;
  NS_IMETHOD GetBypassProxy(PRBool *aBypass) = 0;

  // Local IP address.
  NS_IMETHOD SetLocalIP(const PRUint32 aLocalIP) = 0;
  NS_IMETHOD GetLocalIP(PRUint32 *aLocalIP) = 0;

  // Reload method.
  NS_IMETHOD SetReloadType(nsURLReloadType aType) = 0;
  NS_IMETHOD GetReloadType(nsURLReloadType* aResult) = 0;

  // Load method
  NS_IMETHOD SetLoadType(nsURLLoadType aType) = 0;
  NS_IMETHOD GetLoadType(nsURLLoadType* aResult) = 0;
};

/* Creation routines. */

extern NS_NET nsresult NS_NewLoadAttribs(nsILoadAttribs** aInstancePtrResult);

#endif // nsILoadAttribs_h___
