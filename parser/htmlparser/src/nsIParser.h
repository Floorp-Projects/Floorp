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
#ifndef NS_IPARSER___
#define NS_IPARSER___


/**
 * MODULE NOTES:
 * @update  gess 4/1/98
 *  
 *  This class defines the iparser interface. This XPCOM
 *  inteface is all that parser clients ever need to see.
 *
 **/


#include "nshtmlpars.h"
#include "nsISupports.h"
#include "nsIStreamListener.h"
#include "nsIDTD.h"
#include "nsIInputStream.h"
#include "nsError.h"
#include <fstream.h>

#define NS_IPARSER_IID      \
  {0x355cbba0, 0xbf7d,  0x11d1,  \
  {0xaa, 0xd9, 0x00,    0x80, 0x5f, 0x8a, 0x3e, 0x14}}

/**
 * Return codes for parsing routines.
 * NS_ERROR_HTMLPARSER_BLOCK indicates that the parser should
 * block further parsing until it gets a Unblock() method call.
 * NS_ERROR_HTMLPARSER_CONTINUE indicates that the parser should
 * continue parsing
 */
#define NS_ERROR_HTMLPARSER_BLOCK       NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_HTMLPARSER,1)
#define NS_ERROR_HTMLPARSER_CONTINUE    NS_OK

class nsIContentSink;
class nsIStreamObserver;
class nsString;
class nsIURL;


enum  eParseMode {
  
  eParseMode_unknown=0,
  eParseMode_raptor,    //5.0 version of nav. and greater
  eParseMode_navigator, //pre 5.0 versions
  eParseMode_noquirks,  //pre 5.0 without quirks (as best as we can...)
  eParseMode_other,
  eParseMode_autodetect
};

/**
 *  This class defines the iparser interface. This XPCOM
 *  inteface is all that parser clients ever need to see.
 *  
 *  @update  gess 3/25/98
 */
class nsIParser : public nsISupports {
  public:

    /**
     *  Call this method if you have a DTD that you want to share with the parser.
	   *  Registered DTD's get remembered until the system shuts down.
     *  
     *  @update  gess 3/25/98
     *  @param   aDTD -- ptr DTD that you're publishing the services of
     */
    virtual void RegisterDTD(nsIDTD* aDTD)=0;


    /**
     * Select given content sink into parser for parser output
     * @update	gess5/11/98
     * @param   aSink is the new sink to be used by parser
     * @return  old sink, or NULL
     */
    virtual nsIContentSink* SetContentSink(nsIContentSink* aSink)=0;


    /**
     * retrive the sink set into the parser 
     * @update	gess5/11/98
     * @param   aSink is the new sink to be used by parser
     * @return  old sink, or NULL
     */
    virtual nsIContentSink* GetContentSink(void)=0;

    /**
     *  Call this method once you've created a parser, and want to instruct it
	   *  about the command which caused the parser to be constructed. For example,
     *  this allows us to select a DTD which can do, say, view-source.
     *  
     *  @update  gess 3/25/98
     *  @param   aCommand -- ptrs to string that contains command
     *  @return	 nada
     */
    virtual void SetCommand(const char* aCommand)=0;

    /**
     *  This internal method is used when the parser needs to determine the
	   *  type of content it's being asked to parse.
     *  
     *  @update  gess 3/25/98
     *  @param   aBuffer -- contains data to be tested (autodetected) for type
	   *  @param	 aType -- string where you store the detected type (if any)
     *  @return  autodetect enum (valid, invalid, unknown)
     */
    virtual eAutoDetectResult AutoDetectContentType(nsString& aBuffer,nsString& aType)=0;


    /******************************************************************************************
     *  Parse methods always begin with an input source, and perform conversions 
     *  until you wind up being emitted to the given contentsink (which may or may not
	   *  be a proxy for the NGLayout content model).
     ******************************************************************************************/
    virtual PRBool  EnableParser(PRBool aState) = 0;
    virtual PRInt32 Parse(nsIURL* aURL,nsIStreamObserver* aListener = nsnull,PRBool aEnableVerify=PR_FALSE) = 0;
    virtual PRInt32 Parse(fstream& aStream,PRBool aEnableVerify=PR_FALSE) = 0;
    virtual PRInt32 Parse(nsString& aSourceBuffer,PRBool anHTMLString,PRBool aEnableVerify=PR_FALSE) = 0;

    /**
     * This method gets called when the tokens have been consumed, and it's time
     * to build the model via the content sink.
     * @update	gess5/11/98
     * @return  error code -- 0 if model building went well .
     */
    virtual PRInt32 BuildModel(void)=0;


    /**
     *  Retrieve the parse mode from the parser...
     *  
     *  @update  gess 6/9/98
     *  @return  ptr to scanner
     */
    virtual eParseMode GetParseMode(void)=0;


};

#endif 
