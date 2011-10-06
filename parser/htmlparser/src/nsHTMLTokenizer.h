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

#ifndef __NSHTMLTOKENIZER
#define __NSHTMLTOKENIZER

#include "nsISupports.h"
#include "nsITokenizer.h"
#include "nsIDTD.h"
#include "prtypes.h"
#include "nsDeque.h"
#include "nsScanner.h"
#include "nsHTMLTokens.h"
#include "nsDTDUtils.h"

/***************************************************************
  Notes: 
 ***************************************************************/

#ifdef _MSC_VER
#pragma warning( disable : 4275 )
#endif

class nsHTMLTokenizer : public nsITokenizer {
public:
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSITOKENIZER
  nsHTMLTokenizer(nsDTDMode aParseMode = eDTDMode_quirks,
                  eParserDocType aDocType = eHTML_Quirks,
                  eParserCommands aCommand = eViewNormal,
                  PRUint32 aFlags = 0);
  virtual ~nsHTMLTokenizer();

  static PRUint32 GetFlags(const nsIContentSink* aSink);

protected:

  nsresult ConsumeTag(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner,bool& aFlushTokens);
  nsresult ConsumeStartTag(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner,bool& aFlushTokens);
  nsresult ConsumeEndTag(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner);
  nsresult ConsumeAttributes(PRUnichar aChar, CToken* aToken, nsScanner& aScanner);
  nsresult ConsumeEntity(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner);
  nsresult ConsumeWhitespace(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner);
  nsresult ConsumeComment(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner);
  nsresult ConsumeNewline(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner);
  nsresult ConsumeText(CToken*& aToken,nsScanner& aScanner);
  nsresult ConsumeSpecialMarkup(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner);
  nsresult ConsumeProcessingInstruction(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner);

  nsresult ScanDocStructure(bool aIsFinalChunk);

  static void AddToken(CToken*& aToken,nsresult aResult,nsDeque* aDeque,nsTokenAllocator* aTokenAllocator);

  nsDeque            mTokenDeque;
  bool               mIsFinalChunk;
  nsTokenAllocator*  mTokenAllocator;
  // This variable saves the position of the last tag we inspected in
  // ScanDocStructure. We start scanning the general well-formedness of the
  // document starting at this position each time.
  PRInt32            mTokenScanPos;
  PRUint32           mFlags;
};

#endif


