/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   
 */

#ifndef nsPrintSettingsX_h__
#define nsPrintSettingsX_h__

#include "nsPrintSettingsImpl.h"  
#include "nsIPrintSettingsX.h"  

//*****************************************************************************
//***    nsPrintSettingsX
//*****************************************************************************

class nsPrintSettingsX : public nsPrintSettings,
                         public nsIPrintSettingsX
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIPRINTSETTINGSX

  nsPrintSettingsX();
  virtual ~nsPrintSettingsX();
  
  nsresult Init();

protected:
  nsPrintSettingsX(const nsPrintSettingsX& src);
  nsPrintSettingsX& operator=(const nsPrintSettingsX& rhs);

  nsresult _Clone(nsIPrintSettings **_retval);
  nsresult _Assign(nsIPrintSettings *aPS);
  
  // The out param has a ref count of 1 on return so caller needs to PMRelase() when done.
  OSStatus CreateDefaultPageFormat(PMPrintSession aSession, PMPageFormat& outFormat);
  OSStatus CreateDefaultPrintSettings(PMPrintSession aSession, PMPrintSettings& outSettings);

  PMPageFormat mPageFormat;
  PMPrintSettings mPrintSettings;
};

#endif /* nsPrintSettingsX_h__ */
