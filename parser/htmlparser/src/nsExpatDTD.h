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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */

/**
 * MODULE NOTES:
 * @update  gess 4/8/98
 * 
 *         
 */

#ifndef __NS_EXPAT_DTD
#define __NS_EXPAT_DTD

#include "nsIDTD.h"
#include "nsITokenizer.h"
#include "nsISupports.h"
#include "nsHTMLTokens.h"
#include "nshtmlpars.h"
#include "nsVoidArray.h"
#include "nsDeque.h"
#include "nsIContentSink.h"
#include "xmlparse.h"


#define NS_EXPAT_DTD_IID      \
  {0x5ad4b431, 0xcabb, 0x11d2, \
  {0xab, 0xcb, 0x0, 0x10, 0x4b, 0x98, 0x3f, 0xd4}}


class nsIDTDDebug;
class nsIParserNode;
class nsParser;



class nsExpatDTD : public nsIDTD {
            
  public:

    NS_DECL_ISUPPORTS


    /**
     *  
     *  
     *  @update  gess 4/9/98
     *  @param   
     *  @return  
     */
    nsExpatDTD();

    /**
     *  
     *  
     *  @update  gess 4/9/98
     *  @param   
     *  @return  
     */
    virtual ~nsExpatDTD();

    virtual const nsIID&  GetMostDerivedIID(void) const;

    /**
     * Call this method if you want the DTD to construct a clone of itself.
     * @update	gess7/23/98
     * @param 
     * @return
     */
    virtual nsresult CreateNewInstance(nsIDTD** aInstancePtrResult);

    /**
     * This method is called to determine if the given DTD can parse
     * a document in a given source-type. 
     * NOTE: Parsing always assumes that the end result will involve
     *       storing the result in the main content model.
     * @update	gess6/24/98
     * @param   
     * @return  TRUE if this DTD can satisfy the request; FALSE otherwise.
     */
    virtual eAutoDetectResult CanParse(CParserContext& aParserContext,nsString& aBuffer, PRInt32 aVersion);

    /**
      * The parser uses a code sandwich to wrap the parsing process. Before
      * the process begins, WillBuildModel() is called. Afterwards the parser
      * calls DidBuildModel(). 
      * @update	rickg 03.20.2000
      * @param	aParserContext
      * @param	aSink
      * @return	error code (almost always 0)
      */
    NS_IMETHOD WillBuildModel(  const CParserContext& aParserContext,nsIContentSink* aSink);


    /**
      * The parser uses a code sandwich to wrap the parsing process. Before
      * the process begins, WillBuildModel() is called. Afterwards the parser
      * calls DidBuildModel(). 
      * @update	gess5/18/98
      * @param	aFilename is the name of the file being parsed.
      * @return	error code (almost always 0)
      */
    NS_IMETHOD BuildModel(nsIParser* aParser,nsITokenizer* aTokenizer,nsITokenObserver* anObserver=0,nsIContentSink* aSink=0);

   /**
     * The parser uses a code sandwich to wrap the parsing process. Before
     * the process begins, WillBuildModel() is called. Afterwards the parser
     * calls DidBuildModel(). 
     * @update	gess5/18/98
     * @param	anErrorCode contans the last error that occured
     * @return	error code
     */
    NS_IMETHOD DidBuildModel(nsresult anErrorCode,PRBool aNotifySink,nsIParser* aParser,nsIContentSink* aSink=0);

    /**
     *  
     *  @update  gess 3/25/98
     *  @param   aToken -- token object to be put into content model
     *  @return  0 if all is well; non-zero is an error
     */
    NS_IMETHOD HandleToken(CToken* aToken,nsIParser* aParser);

    /**
     * 
     * @update	gess12/28/98
     * @param 
     * @return
     */
    NS_IMETHOD  GetTokenizer(nsITokenizer*& aTokenizer);

    /**
     * 
     * @update	gess5/18/98
     * @param 
     * @return
     */
    NS_IMETHOD WillResumeParse(void);

    /**
     * 
     * @update	gess5/18/98
     * @param 
     * @return
     */
    NS_IMETHOD WillInterruptParse(void);

    /**
     * Called by the parser to initiate dtd verification of the
     * internal context stack.
     * @update	gess 7/23/98
     * @param 
     * @return
     */
    virtual PRBool Verify(nsString& aURLRef,nsIParser* aParser);

    /**
     * Set this to TRUE if you want the DTD to verify its
     * context stack.
     * @update	gess 7/23/98
     * @param 
     * @return
     */
    virtual void SetVerification(PRBool aEnable);

    /**
     *  This method is called to determine whether or not a tag
     *  of one type can contain a tag of another type.
     *  
     *  @update  gess 3/25/98
     *  @param   aParent -- int tag of parent container
     *  @param   aChild -- int tag of child container
     *  @return  PR_TRUE if parent can contain child
     */
    virtual PRBool CanContain(PRInt32 aParent,PRInt32 aChild) const;

    /**
     *  This method gets called to determine whether a given 
     *  tag is itself a container
     *  
     *  @update  gess 3/25/98
     *  @param   aTag -- tag to test for containership
     *  @return  PR_TRUE if given tag can contain other tags
     */
    virtual PRBool IsContainer(PRInt32 aTag) const;

    /**
     * Use this id you want to stop the building content model
     * --------------[ Sets DTD to STOP mode ]----------------
     * It's recommended to use this method in accordance with
     * the parser's terminate() method.
     *
     * @update	harishd 07/22/99
     * @param 
     * @return
     */
    virtual nsresult  Terminate(nsIParser* aParser=nsnull);

    /**
     * Give rest of world access to our tag enums, so that CanContain(), etc,
     * become useful.
     */
    NS_IMETHOD StringTagToIntTag(nsString &aTag, PRInt32* aIntTag) const;

    NS_IMETHOD IntTagToStringTag(PRInt32 aIntTag, nsString& aTag) const;

    NS_IMETHOD ConvertEntityToUnicode(const nsString& aEntity, PRInt32* aUnicode) const;

    virtual PRBool  IsBlockElement(PRInt32 aTagID,PRInt32 aParentID) const {return PR_FALSE;}
    virtual PRBool  IsInlineElement(PRInt32 aTagID,PRInt32 aParentID) const {return PR_FALSE;}

    /**
     * Retrieve a ptr to the global token recycler...
     * @update	gess8/4/98
     * @return  ptr to recycler (or null)
     */
    virtual nsTokenAllocator* GetTokenAllocator(void);

    /**
     * Parse an XML buffer using expat
     * @update	nra 2/29/99
     * @return  NS_ERROR_FAILURE if expat encounters an error, else NS_OK
     */
    nsresult ParseXMLBuffer(const char *buffer);
    
protected:
    /**
     * Sets up the callbacks for the expat parser      
     * @update  nra 2/24/99
     * @param   none
     * @return  none
     */
    void SetupExpatCallbacks(void);

    /* The callback handlers that get called from the expat parser */
    static void PR_CALLBACK HandleStartElement(void *userData, const XML_Char *name, const XML_Char **atts);
    static void PR_CALLBACK HandleEndElement(void *userData, const XML_Char *name);
    static void PR_CALLBACK HandleCharacterData(void *userData, const XML_Char *s, int len);
    static void PR_CALLBACK HandleProcessingInstruction(void *userData, 
      const XML_Char *target, 
      const XML_Char *data);
    static void PR_CALLBACK HandleDefault(void *userData, const XML_Char *s, int len);
    static void PR_CALLBACK HandleUnparsedEntityDecl(void *userData, 
      const XML_Char *entityName, 
      const XML_Char *base, 
      const XML_Char *systemId, 
      const XML_Char *publicId,
      const XML_Char *notationName);
    static void PR_CALLBACK HandleNotationDecl(void *userData,
      const XML_Char *notationName,
      const XML_Char *base,
      const XML_Char *systemId,
      const XML_Char *publicId);
    static void PR_CALLBACK HandleExternalEntityRef(XML_Parser parser,
      const XML_Char *openEntityNames,
      const XML_Char *base,
      const XML_Char *systemId,
      const XML_Char *publicId);
    static void PR_CALLBACK HandleUnknownEncoding(void *encodingHandlerData,
      const XML_Char *name,
      XML_Encoding *info);
  
    XML_Parser          mExpatParser;
    nsParser*           mParser;
    nsIContentSink*     mSink;
    nsString            mFilename;
    PRInt32             mLineNumber;
    nsITokenizer*       mTokenizer;
};

extern NS_HTMLPARS nsresult NS_New_Expat_DTD(nsIDTD** aInstancePtrResult);


#endif
