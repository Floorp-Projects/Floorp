/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
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

#define NS_HTMLTOKENIZER_IID      \
  {0xe4238ddd, 0x9eb6, 0x11d2, \
  {0xba, 0xa5, 0x0,     0x10, 0x4b, 0x98, 0x3f, 0xd4 }}


/***************************************************************
  Notes: 
 ***************************************************************/

#if defined(XP_PC)
#pragma warning( disable : 4275 )
#endif

CLASS_EXPORT_HTMLPARS nsHTMLTokenizer : public nsITokenizer {
public:
          nsHTMLTokenizer( PRInt32 aParseMode=eDTDMode_quirks,
                           eParserDocType aDocType=eHTML3_Quirks,
                           eParserCommands aCommand=eViewNormal);
  virtual ~nsHTMLTokenizer();

          NS_DECL_ISUPPORTS

  virtual nsresult          WillTokenize(PRBool aIsFinalChunk,nsTokenAllocator* aTokenAllocator);
  virtual nsresult          ConsumeToken(nsScanner& aScanner,PRBool& aFlushTokens);
  virtual nsresult          DidTokenize(PRBool aIsFinalChunk);
  virtual nsTokenAllocator*  GetTokenAllocator(void);

  virtual CToken*           PushTokenFront(CToken* theToken);
  virtual CToken*           PushToken(CToken* theToken);
  virtual CToken*           PopToken(void);
  virtual CToken*           PeekToken(void);
  virtual CToken*           GetTokenAt(PRInt32 anIndex);
  virtual PRInt32           GetCount(void);

  virtual void              PrependTokens(nsDeque& aDeque);

protected:

  virtual nsresult ConsumeScriptContent(nsScanner& aScanner,CToken*& aToken);
  virtual nsresult ConsumeTag(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner,PRBool& aFlushTokens);
  virtual nsresult ConsumeStartTag(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner,PRBool& aFlushTokens);
  virtual nsresult ConsumeEndTag(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner);
  virtual nsresult ConsumeAttributes(PRUnichar aChar,CStartToken* aToken,nsScanner& aScanner);
  virtual nsresult ConsumeEntity(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner);
  virtual nsresult ConsumeWhitespace(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner);
  virtual nsresult ConsumeComment(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner);
  virtual nsresult ConsumeNewline(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner);
  virtual nsresult ConsumeText(CToken*& aToken,nsScanner& aScanner);
  virtual nsresult ConsumeSpecialMarkup(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner);
  virtual nsresult ConsumeProcessingInstruction(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner);

          nsresult ScanDocStructure(PRBool aIsFinalChunk);

  virtual void     RecordTrailingContent(CStartToken* aStartToken, nsScanner& aScanner, nsReadingIterator<PRUnichar> aOrigin);

  static void AddToken(CToken*& aToken,nsresult aResult,nsDeque* aDeque,nsTokenAllocator* aTokenAllocator);

  nsDeque            mTokenDeque;
  PRInt32            mFlags;
  PRBool             mRecordTrailingContent;
  nsTokenAllocator*  mTokenAllocator;
  PRInt32            mTokenScanPos;
  PRBool             mIsFinalChunk;
};

extern NS_HTMLPARS nsresult NS_NewHTMLTokenizer(  nsITokenizer** aInstancePtrResult,
                                                  PRInt32 aMode,
                                                  eParserDocType aDocType,
                                                  eParserCommands aCommand);

#endif


