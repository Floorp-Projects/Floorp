/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


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
