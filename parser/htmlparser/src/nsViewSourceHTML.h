
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
 * @update  gess 4/8/98
 * 
 *         
 */

#ifndef __NS_VIEWSOURCE_HTML_
#define __NS_VIEWSOURCE_HTML_

#include "nsIDTD.h"
#include "nsISupports.h"
#include "nsHTMLTokens.h"
#include "nshtmlpars.h"
#include "nsIHTMLContentSink.h"

#define NS_VIEWSOURCE_HTML_IID      \
  {0xb6003010, 0x7932, 0x11d2, \
  {0x80, 0x1b, 0x0, 0x60, 0x8, 0xbf, 0xc4, 0x89 }}


class nsIDTDDebug;
class nsIParserNode;
class nsParser;
class nsITokenizer;


class CViewSourceHTML: public nsIDTD {
            
  public:

    NS_DECL_ISUPPORTS


    /**
     *  
     *  
     *  @update  gess 4/9/98
     *  @param   
     *  @return  
     */
    CViewSourceHTML();

    virtual const nsIID&  GetMostDerivedIID(void) const;

    /**
     *  
     *  
     *  @update  gess 4/9/98
     *  @param   
     *  @return  
     */
    virtual ~CViewSourceHTML();


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
    virtual eAutoDetectResult CanParse(nsString& aContentType, nsString& aCommand, nsString& aBuffer, PRInt32 aVersion);

    /**
      * The parser uses a code sandwich to wrap the parsing process. Before
      * the process begins, WillBuildModel() is called. Afterwards the parser
      * calls DidBuildModel(). 
      * @update	gess5/18/98
      * @param	aFilename is the name of the file being parsed.
      * @return	error code (almost always 0)
      */
    NS_IMETHOD WillBuildModel(nsString& aFilename,PRBool aNotifySink,nsString& aSourceType,nsIContentSink* aSink=0);

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
    nsITokenizer* GetTokenizer(void);

    
    /**
     * 
     * @update	gess12/28/98
     * @param 
     * @return
     */
    virtual  nsITokenRecycler* GetTokenRecycler(void);

    /**
     *  This method causes all tokens to be dispatched to the given tag handler.
     *
     *  @update  gess 3/25/98
  	 *  @param   aHandler -- object to receive subsequent tokens...
	   *  @return	 error code (usually 0)
     */
    NS_IMETHOD CaptureTokenPump(nsITagHandler* aHandler);

    /**
     *  This method releases the token-pump capture obtained in CaptureTokenPump()
     *
     *  @update  gess 3/25/98
  	 *  @param   aHandler -- object that received tokens...
	   *  @return	 error code (usually 0)
     */
    NS_IMETHOD ReleaseTokenPump(nsITagHandler* aHandler);

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


    static nsresult WriteText(const nsString& aTextString,nsIContentSink& aSink,PRBool aPreserveText);

    /** These methods are used while setting setting title
     *  in the view source window. Same code fragment as in
     *  CNavDTD.
     *  
     *  @update  02/22/99
     *  
     */
    nsresult OpenHead(const nsIParserNode& aNode);
    nsresult CloseHead(const nsIParserNode& aNode);
    
protected:

    nsParser*           mParser;
    nsIHTMLContentSink* mSink;
    nsString            mFilename;
    PRInt32             mLineNumber;
    PRBool              mIsHTML;
    nsITokenizer*       mTokenizer;
    PRInt32             mHasOpenHead;
    PRBool              mIsPlaintext;

};

extern NS_HTMLPARS nsresult NS_NewViewSourceHTML(nsIDTD** aInstancePtrResult);

#endif 



