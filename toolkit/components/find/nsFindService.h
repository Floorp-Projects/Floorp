/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
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

  NS_DECL_ISUPPORTS
  NS_DECL_NSIFINDSERVICE
 
protected:

  virtual ~nsFindService();

  nsString        mSearchString;
  nsString        mReplaceString;
  
  bool            mFindBackwards;
  bool            mWrapFind;
  bool            mEntireWord;
  bool            mMatchCase;
};
