/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Shaver <shaver@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

