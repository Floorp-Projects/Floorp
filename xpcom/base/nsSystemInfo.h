/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *   Mike Shaver <shaver@mozilla.org>
 */

#include "nsISystemInfo.h"

#define NS_SYSTEMINFO_CID_STR "3c443856-1dd2-11b2-bfa1-83125c3f3887"

#define NS_SYSTEMINFO_CID \
  {0x3c443856, 0x1dd2, 0x11b2, \
    { 0xbf, 0xa1, 0x83, 0x12, 0x5c, 0x3f, 0x38, 0x87 }}

#define NS_SYSTEMINFO_CLASSNAME "System Info Service"
#define NS_SYSTEMINFO_CONTRACTID    "@mozilla.org/sysinfo;1"

class nsSystemInfo : public nsISystemInfo
{
  NS_DECL_ISUPPORTS
public:
  NS_DECL_NSISYSTEMINFO

  nsSystemInfo();
  virtual ~nsSystemInfo();
  static NS_METHOD Create(nsISupports *outer, const nsIID& aIID,
			  void **aInstancePtr);
};

