/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Simon Fraser <sfraser@netscape.com>
 */
 
/*
 * The sole purpose of the Find service is to store globally the
 * last used Find settings
 *
 */
 
#include "nsString.h"

#include "nsIFindService.h"


// {5060b803-340e-11d5-be5b-b3e063ec6a3c}
#define NS_FIND_SERVICE_CID \
 {0x5060b803, 0x340e, 0x11d5, {0xbe, 0x5b, 0xb3, 0xe0, 0x63, 0xec, 0x6a, 0x3c}}


#define NS_FIND_SERVICE_CONTRACTID \
 "@mozilla.org/find/find_service;1"


class nsFindService : public nsIFindService
{
public:

                      nsFindService();
  virtual             ~nsFindService();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIFINDSERVICE
  
  static nsFindService*     GetSingleton();
  static void               FreeSingleton();

protected:

  static nsFindService*   gFindService;
  
protected:

  nsString        mSearchString;
  nsString        mReplaceString;
  
  PRPackedBool    mFindBackwards;
  PRPackedBool    mWrapFind;
  PRPackedBool    mEntireWord;
  PRPackedBool    mMatchCase;

};
