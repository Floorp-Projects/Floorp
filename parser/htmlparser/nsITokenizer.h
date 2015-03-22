/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 * MODULE NOTES:
 * @update  gess 4/1/98
 * 
 */

#ifndef __NSITOKENIZER__
#define __NSITOKENIZER__

#include "nsISupports.h"

class nsScanner;

#define NS_ITOKENIZER_IID      \
{ 0Xae98a348, 0X5e91, 0X41a8, \
  { 0Xa5, 0Xb4, 0Xd2, 0X20, 0Xf3, 0X1f, 0Xc4, 0Xab } }

/***************************************************************
  Notes: 
 ***************************************************************/


class nsITokenizer : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ITOKENIZER_IID)

  NS_IMETHOD                     WillTokenize(bool aIsFinalChunk)=0;
  NS_IMETHOD                     ConsumeToken(nsScanner& aScanner,bool& aFlushTokens)=0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsITokenizer, NS_ITOKENIZER_IID)

#define NS_DECL_NSITOKENIZER \
  NS_IMETHOD                     WillTokenize(bool aIsFinalChunk) override;\
  NS_IMETHOD                     ConsumeToken(nsScanner& aScanner,bool& aFlushTokens) override;\


#endif
