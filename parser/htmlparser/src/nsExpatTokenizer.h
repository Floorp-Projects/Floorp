/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


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
#include "xmlparse.h"

#define NS_EXPATTOKENIZER_IID      \
  {0x483836aa, 0xcabe, 0x11d2, { 0xab, 0xcb, 0x0, 0x10, 0x4b, 0x98, 0x3f, 0xd4 }}


/***************************************************************
  Notes: 
 ***************************************************************/

#if defined(XP_PC)
#pragma warning( disable : 4275 )
#endif

CLASS_EXPORT_HTMLPARS nsExpatTokenizer : public nsHTMLTokenizer {
public:
          nsExpatTokenizer();      
  virtual ~nsExpatTokenizer();

          NS_DECL_ISUPPORTS

  /* nsITokenizer methods */
  virtual nsresult ConsumeToken(nsScanner& aScanner);

  virtual void    FrontloadMisplacedContent(nsDeque& aDeque);


protected:

    /**
     * Parse an XML buffer using expat
     * @update	nra 2/29/99
     * @return  NS_ERROR_FAILURE if expat encounters an error, else NS_OK
     */
    nsresult ParseXMLBuffer(const char *aBuffer, PRUint32 aLength);

    /**
     * Sets up the callbacks for the expat parser      
     * @update  nra 2/24/99
     * @param   none
     * @return  none
     */
    void SetupExpatCallbacks(void);

    void PushXMLErrorToken(const char *aBuffer, PRUint32 aLength);
	void SetErrorContextInfo(nsParserError* aError, PRUint32 aByteIndex,
		const char* aSourceBuffer, PRUint32 aLength);

    /* The callback handlers that get called from the expat parser */
    static void HandleStartElement(void *userData, const XML_Char *name, const XML_Char **atts);
    static void HandleEndElement(void *userData, const XML_Char *name);
    static void HandleCharacterData(void *userData, const XML_Char *s, int len);
    static void HandleProcessingInstruction(void *userData, 
      const XML_Char *target, 
      const XML_Char *data);
    static void HandleDefault(void *userData, const XML_Char *s, int len);
    static void HandleUnparsedEntityDecl(void *userData, 
      const XML_Char *entityName, 
      const XML_Char *base, 
      const XML_Char *systemId, 
      const XML_Char *publicId,
      const XML_Char *notationName);
    static void HandleNotationDecl(void *userData,
      const XML_Char *notationName,
      const XML_Char *base,
      const XML_Char *systemId,
      const XML_Char *publicId);
    static int HandleExternalEntityRef(XML_Parser parser,
      const XML_Char *openEntityNames,
      const XML_Char *base,
      const XML_Char *systemId,
      const XML_Char *publicId);
    static int HandleUnknownEncoding(void *encodingHandlerData,
      const XML_Char *name,
      XML_Encoding *info);
  
    XML_Parser mExpatParser;
	PRUint32 mBytesParsed;
};

extern NS_HTMLPARS nsresult NS_Expat_Tokenizer(nsIDTD** aInstancePtrResult);

#endif
