/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsIUnixToolkitService.h"

class nsUnixToolkitService : public nsIUnixToolkitService
{
 public:
  nsUnixToolkitService();
  virtual ~nsUnixToolkitService();

  NS_DECL_ISUPPORTS

  NS_IMETHOD SetToolkitName(const nsString & aToolkitName);
  NS_IMETHOD IsValidToolkit(const nsString & aToolkitName,
                            PRBool * aResultOut);
  NS_IMETHOD GetToolkitName(nsString & aToolkitNameOut);
  NS_IMETHOD GetWidgetDllName(nsString & aWidgetDllNameOut);
  NS_IMETHOD GetGfxDllName(nsString & aGfxDllNameOut);

  NS_IMETHOD GetTimerCID(nsCID ** aTimerCIDOut);

private:

  static nsresult GlobalGetToolkitName(nsString & aStringOut);

  static nsresult EnsureToolkitName();

  static nsresult Cleanup();

  static nsString *     sToolkitName;
  static nsString *     sWidgetDllName;
  static nsString *     sGfxDllName;
  static const nsCID *  sTimerCID;

  static const char * ksDefaultToolkit;
  static const char * ksDllSuffix;
  static const char * ksDllPrefix;
  static const char * ksWidgetName;
  static const char * ksGfxName;
};
