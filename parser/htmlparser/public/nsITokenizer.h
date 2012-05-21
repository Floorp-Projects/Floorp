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
#include "prtypes.h"

class CToken;
class nsScanner;
class nsDeque;
class nsTokenAllocator;

#define NS_ITOKENIZER_IID      \
  {0xe4238ddc, 0x9eb6,  0x11d2, {0xba, 0xa5, 0x0,     0x10, 0x4b, 0x98, 0x3f, 0xd4 }}


/***************************************************************
  Notes: 
 ***************************************************************/


class nsITokenizer : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ITOKENIZER_IID)

  NS_IMETHOD                     WillTokenize(bool aIsFinalChunk,nsTokenAllocator* aTokenAllocator)=0;
  NS_IMETHOD                     ConsumeToken(nsScanner& aScanner,bool& aFlushTokens)=0;
  NS_IMETHOD                     DidTokenize(bool aIsFinalChunk)=0;
  
  NS_IMETHOD_(CToken*)           PushTokenFront(CToken* aToken)=0;
  NS_IMETHOD_(CToken*)           PushToken(CToken* aToken)=0;
  NS_IMETHOD_(CToken*)           PopToken(void)=0;
  NS_IMETHOD_(CToken*)           PeekToken(void)=0;
  NS_IMETHOD_(CToken*)           GetTokenAt(PRInt32 anIndex)=0;
  NS_IMETHOD_(PRInt32)           GetCount(void)=0;
  NS_IMETHOD_(nsTokenAllocator*) GetTokenAllocator(void)=0;
  NS_IMETHOD_(void)              PrependTokens(nsDeque& aDeque)=0;
  NS_IMETHOD                     CopyState(nsITokenizer* aTokenizer)=0;
  
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsITokenizer, NS_ITOKENIZER_IID)

#define NS_DECL_NSITOKENIZER \
  NS_IMETHOD                     WillTokenize(bool aIsFinalChunk,nsTokenAllocator* aTokenAllocator);\
  NS_IMETHOD                     ConsumeToken(nsScanner& aScanner,bool& aFlushTokens);\
  NS_IMETHOD                     DidTokenize(bool aIsFinalChunk);\
  NS_IMETHOD_(CToken*)           PushTokenFront(CToken* aToken);\
  NS_IMETHOD_(CToken*)           PushToken(CToken* aToken);\
  NS_IMETHOD_(CToken*)           PopToken(void);\
  NS_IMETHOD_(CToken*)           PeekToken(void);\
  NS_IMETHOD_(CToken*)           GetTokenAt(PRInt32 anIndex);\
  NS_IMETHOD_(PRInt32)           GetCount(void);\
  NS_IMETHOD_(nsTokenAllocator*) GetTokenAllocator(void);\
  NS_IMETHOD_(void)              PrependTokens(nsDeque& aDeque);\
  NS_IMETHOD                     CopyState(nsITokenizer* aTokenizer);


#endif
