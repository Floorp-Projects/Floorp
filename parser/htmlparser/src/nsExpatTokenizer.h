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

#ifndef __nsExpatTokenizer
#define __nsExpatTokenizer

#include "nsISupports.h"
#include "nsHTMLTokenizer.h"
#include "prtypes.h"

// Enable unicode characters in expat.
#define UNICODE
#include "xmlparse.h"

#define NS_EXPATTOKENIZER_IID      \
  {0x483836aa, 0xcabe, 0x11d2, { 0xab, 0xcb, 0x0, 0x10, 0x4b, 0x98, 0x3f, 0xd4 }}


// {575C063A-AE9C-11d3-B9FD-001083023C0E}
#define NS_EXPATTOKENIZER_CID  \
{ 0x575c063a, 0xae9c, 0x11d3, \
  {0xb9, 0xfd, 0x0, 0x10, 0x83, 0x2, 0x3c, 0xe}}


typedef struct _XMLParserState XMLParserState;

/***************************************************************
  Notes: 
 ***************************************************************/

#if defined(XP_PC)
#pragma warning( disable : 4275 )
#endif

#ifdef __cplusplus
extern "C" {
#endif
  /* The callback handlers that get called from the expat parser */
  void Tokenizer_HandleStartElement(void *userData, const XML_Char *name, const XML_Char **atts);
  void Tokenizer_HandleEndElement(void *userData, const XML_Char *name);
  void Tokenizer_HandleCharacterData(void *userData, const XML_Char *s, int len);
  void Tokenizer_HandleComment(void *userData, const XML_Char *name);
  void Tokenizer_HandleProcessingInstruction(void *userData, 
    const XML_Char *target, 
    const XML_Char *data);
  void Tokenizer_HandleDefault(void *userData, const XML_Char *s, int len);
  void Tokenizer_HandleStartCdataSection(void *userData);
  void Tokenizer_HandleEndCdataSection(void *userData);
  void Tokenizer_HandleUnparsedEntityDecl(void *userData, 
    const XML_Char *entityName, 
    const XML_Char *base, 
    const XML_Char *systemId, 
    const XML_Char *publicId,
    const XML_Char *notationName);
  void Tokenizer_HandleNotationDecl(void *userData,
    const XML_Char *notationName,
    const XML_Char *base,
    const XML_Char *systemId,
    const XML_Char *publicId);
  int Tokenizer_HandleExternalEntityRef(XML_Parser parser,
    const XML_Char *openEntityNames,
    const XML_Char *base,
    const XML_Char *systemId,
    const XML_Char *publicId);
  int Tokenizer_HandleUnknownEncoding(void *encodingHandlerData,
    const XML_Char *name,
    XML_Encoding *info);
  void Tokenizer_HandleStartDoctypeDecl(void *userData,
    const XML_Char *doctypeName);
  void Tokenizer_HandleEndDoctypeDecl(void *userData);
#ifdef __cplusplus
}
#endif

class nsExpatTokenizer : public nsHTMLTokenizer {
public:
          nsExpatTokenizer(nsString* aURL = nsnull);      
  virtual ~nsExpatTokenizer();

  virtual const   nsIID& GetCID();
  static  const   nsIID& GetIID();

          NS_DECL_ISUPPORTS

  /* nsITokenizer methods */
  virtual nsresult WillTokenize(PRBool aIsFinalChunk,nsTokenAllocator* aTokenAllocator);
  virtual nsresult ConsumeToken(nsScanner& aScanner,PRBool& aFlushTokens);  
  virtual nsresult DidTokenize(PRBool aIsFinalChunk);

  virtual void    FrontloadMisplacedContent(nsDeque& aDeque);

protected:

  /**
   * Parse an XML buffer using expat
   * @update	nra 2/29/99
   * @return  NS_ERROR_FAILURE if expat encounters an error, else NS_OK
   */
  nsresult ParseXMLBuffer(const char *aBuffer, PRUint32 aLength, PRBool aIsFinal=PR_FALSE);

  /**
   * Sets up the callbacks and user data for the expat parser      
   * @update  nra 2/24/99
   * @param   none
   * @return  none
   */
  void SetupExpatParser(void);

  // Propagate XML errors to the content sink
  nsresult PushXMLErrorTokens(const char *aBuffer, PRUint32 aLength, PRBool aIsFinal);
  nsresult AddErrorMessageTokens(nsParserError* aError);

	void GetLine(const char* aSourceBuffer, PRUint32 aLength, 
    PRUint32 aByteIndex, nsString& aLine);

  // Load up an external stream to get external entity information
  static nsresult OpenInputStream(const XML_Char* aURLStr, 
                                  const XML_Char* aBaseURL, 
                                  nsIInputStream** in, 
                                  nsString* aAbsURL);

  static nsresult LoadStream(nsIInputStream* in, 
                             PRUnichar* &uniBuf, 
                             PRUint32 &retLen);

  /* The callback handlers that get called from the expat parser */
  friend void Tokenizer_HandleStartElement(void *userData, const XML_Char *name, const XML_Char **atts);
  friend void Tokenizer_HandleEndElement(void *userData, const XML_Char *name);
  friend void Tokenizer_HandleCharacterData(void *userData, const XML_Char *s, int len);
  friend void Tokenizer_HandleComment(void *userData, const XML_Char *name);
  friend void Tokenizer_HandleProcessingInstruction(void *userData, 
    const XML_Char *target, 
    const XML_Char *data);
  friend void Tokenizer_HandleDefault(void *userData, const XML_Char *s, int len);
  friend void Tokenizer_HandleStartCdataSection(void *userData);
  friend void Tokenizer_HandleEndCdataSection(void *userData);
  friend void Tokenizer_HandleUnparsedEntityDecl(void *userData, 
    const XML_Char *entityName, 
    const XML_Char *base, 
    const XML_Char *systemId, 
    const XML_Char *publicId,
    const XML_Char *notationName);
  friend void Tokenizer_HandleNotationDecl(void *userData,
    const XML_Char *notationName,
    const XML_Char *base,
    const XML_Char *systemId,
    const XML_Char *publicId);
  friend int Tokenizer_HandleExternalEntityRef(XML_Parser parser,
    const XML_Char *openEntityNames,
    const XML_Char *base,
    const XML_Char *systemId,
    const XML_Char *publicId);
  friend int Tokenizer_HandleUnknownEncoding(void *encodingHandlerData,
    const XML_Char *name,
    XML_Encoding *info);
  friend void Tokenizer_HandleStartDoctypeDecl(void *userData,
    const XML_Char *doctypeName);
  friend void Tokenizer_HandleEndDoctypeDecl(void *userData);

  XML_Parser mExpatParser;
	PRUint32 mBytesParsed;
  nsString mLastLine;
  XMLParserState* mState;
};

#endif
