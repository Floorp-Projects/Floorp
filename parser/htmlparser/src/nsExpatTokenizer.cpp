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

#include "nsExpatTokenizer.h"
#include "nsScanner.h"
#include "nsDTDUtils.h"
#include "nsParserError.h"
// #include "nsParser.h"
#include "nsIParser.h"
#include "prlog.h"
#include <string.h>


 /************************************************************************
  And now for the main class -- nsExpatTokenizer...
 ************************************************************************/

static NS_DEFINE_IID(kISupportsIID,       NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kITokenizerIID,      NS_ITOKENIZER_IID);
static NS_DEFINE_IID(kHTMLTokenizerIID,   NS_HTMLTOKENIZER_IID);
static NS_DEFINE_IID(kClassIID,           NS_EXPATTOKENIZER_IID);

static CTokenRecycler* gTokenRecycler=0;
static nsDeque* gTokenDeque=0;
static XML_Parser gExpatParser=0;

/**
 *  This method gets called as part of our COM-like interfaces.
 *  Its purpose is to create an interface to parser object
 *  of some type.
 *  
 *  @update   gess 4/8/98
 *  @param    nsIID  id of object to discover
 *  @param    aInstancePtr ptr to newly discovered interface
 *  @return   NS_xxx result code
 */
nsresult nsExpatTokenizer::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsExpatTokenizer*)(this);                                        
  }
  else if(aIID.Equals(kITokenizerIID)) {  //do ITokenizer base class...    
    *aInstancePtr = (nsITokenizer*)(this);
  }
  else if(aIID.Equals(kHTMLTokenizerIID)) {  //do nsHTMLTokenizer base class...
    *aInstancePtr = (nsHTMLTokenizer*)(this);                                        
  }
  else if(aIID.Equals(kClassIID)) {  //do this class...
    *aInstancePtr = (nsExpatTokenizer*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  NS_ADDREF_THIS();
  return NS_OK;                                                        
}


/**
 *  This method is defined in nsIParser. It is used to 
 *  cause the COM-like construction of an nsParser.
 *  
 *  @update  gess 4/8/98
 *  @param   nsIParser** ptr to newly instantiated parser
 *  @return  NS_xxx error result
 */
NS_HTMLPARS nsresult NS_New_Expat_Tokenizer(nsIDTD** aInstancePtrResult) {
  nsExpatTokenizer* it = new nsExpatTokenizer();
  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}


NS_IMPL_ADDREF(nsExpatTokenizer)
NS_IMPL_RELEASE(nsExpatTokenizer)

/**
 * Sets up the callbacks for the expat parser      
 * @update  nra 2/24/99
 * @param   none
 * @return  none
 */
void nsExpatTokenizer::SetupExpatCallbacks(void) {
  if (mExpatParser) {
    XML_SetElementHandler(mExpatParser, HandleStartElement, HandleEndElement);    
    XML_SetCharacterDataHandler(mExpatParser, HandleCharacterData);
    XML_SetProcessingInstructionHandler(mExpatParser, HandleProcessingInstruction);
    XML_SetDefaultHandler(mExpatParser, nsnull);
    XML_SetUnparsedEntityDeclHandler(mExpatParser, HandleUnparsedEntityDecl);
    XML_SetNotationDeclHandler(mExpatParser, HandleNotationDecl);
    XML_SetExternalEntityRefHandler(mExpatParser, HandleExternalEntityRef);
    XML_SetUnknownEncodingHandler(mExpatParser, HandleUnknownEncoding, NULL);    
  }
}


/**
 *  Default constructor
 *   
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
nsExpatTokenizer::nsExpatTokenizer() : nsHTMLTokenizer() {
  NS_INIT_REFCNT();
  mExpatParser = XML_ParserCreate(NULL);
  if (mExpatParser) {
    SetupExpatCallbacks();
  }
}

/**
 *  Destructor
 *   
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
nsExpatTokenizer::~nsExpatTokenizer(){
  if (mExpatParser)
    XML_ParserFree(mExpatParser);  
}


/*******************************************************************
  Here begins the real working methods for the tokenizer.
 *******************************************************************/


static void SetErrorContextInfo(nsParserError* aError, PRUint32 aByteIndex, 
                                const char* aSourceBuffer, PRUint32 aLength)
{
  /* Figure out the substring inside aSourceBuffer that contains the line on which the error
     occurred.  Copy the line into error->sourceLine */
  PR_ASSERT(aByteIndex > 0 && aByteIndex < aLength);
  char* start = (char* ) &aSourceBuffer[aByteIndex];  /* Will try to find the start of the line */
  char* end = (char* ) &aSourceBuffer[aByteIndex];    /* Will try to find the end of the line */
  PRUint32 startIndex = aByteIndex;          /* Track the position of the 'start' pointer into the buffer */
  PRUint32 endIndex = aByteIndex;          /* Track the position of the 'end' pointer into the buffer */
  PRBool reachedStart;
  PRBool reachedEnd;

  /* Use start to find the first new line before the error position and
     end to find the first new line after the error position */
  reachedStart = ('\n' == *start || '\r' == *start || startIndex <= 0);
  reachedEnd = ('\n' == *end || '\r' == *end || endIndex >= aLength);
  while (!reachedStart || !reachedEnd) {
    if (!reachedStart) {
      start--;
      startIndex--;
    }
    if (!reachedEnd) {
      end++;
      endIndex++;
    }
    reachedStart = ('\n' == *start || '\r' == *start || startIndex <= 0);
    reachedEnd = ('\n' == *end || '\r' == *end || endIndex >= aLength);
  }

  if (startIndex == endIndex) {
    /* Special case if the error is on a line where the only character is a newline */
    aError->sourceLine.Append("");
  }
  else {
    PR_ASSERT(endIndex - startIndex >= 2);
    
    /* At this point, the substring starting at (startIndex + 1) and ending at (endIndex - 1),
       is the line on which the error occurred. Copy that substring into the error structure. */
    char* tempLine = new char[endIndex - startIndex];
    strncpy(tempLine, &aSourceBuffer[startIndex + 1], (endIndex - 1) - startIndex);
    tempLine[endIndex - startIndex - 1] = '\0';
    aError->sourceLine.Append(tempLine);
    delete [] tempLine;
  }
}

/* 
 * Called immediately after an error has occurred in expat.  Creates
 * an error token and pushes it onto the token queue.
 *
 */
void nsExpatTokenizer::PushXMLErrorToken(const char *aBuffer, PRUint32 aLength)
{
  CErrorToken* token= (CErrorToken *) gTokenRecycler->CreateTokenOfType(eToken_error, eHTMLTag_unknown);
  nsParserError *error = new nsParserError;
  
  error->code = XML_GetErrorCode(mExpatParser);
  error->lineNumber = XML_GetCurrentLineNumber(mExpatParser);
  error->colNumber = XML_GetCurrentColumnNumber(mExpatParser);  
  error->description = XML_ErrorString(error->code);  
  SetErrorContextInfo(error, (PRUint32) XML_GetCurrentByteIndex(mExpatParser), aBuffer, aLength);  

  token->SetError(error);

  CToken* theToken = (CToken* )token;
  AddToken(theToken, NS_OK, *gTokenDeque);
}

nsresult nsExpatTokenizer::ParseXMLBuffer(const char *aBuffer, PRUint32 aLength){
  nsresult result=NS_OK;
  if (mExpatParser) {
    PR_ASSERT(aLength == strlen(aBuffer));
    if (!XML_Parse(mExpatParser, aBuffer, aLength, PR_FALSE)) {      
      PushXMLErrorToken(aBuffer, aLength);      
    }
  }
  else {
    result = NS_ERROR_FAILURE;
  }
  return result;
}


/**
 *  This method repeatedly called by the tokenizer. 
 *  Each time, we determine the kind of token were about to 
 *  read, and then we call the appropriate method to handle
 *  that token type.
 *  
 *  @update gess 3/25/98
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null 
 */
nsresult nsExpatTokenizer::ConsumeToken(nsScanner& aScanner) {
  
  // return nsHTMLTokenizer::ConsumeToken(aScanner);

  // Ask the scanner to send us all the data it has
  // scanned and pass that data to expat.
  nsresult result = NS_OK;
  nsString& theBuffer = aScanner.GetBuffer();
  PRInt32 length = theBuffer.Length();
  if(0 < length) {
    char* expatBuffer = theBuffer.ToNewCString();
    if (expatBuffer) {      
      gTokenRecycler=(CTokenRecycler*)GetTokenRecycler();
      gTokenDeque=&mTokenDeque;
      gExpatParser = mExpatParser;
      result = ParseXMLBuffer(expatBuffer, length);
      delete [] expatBuffer;
    }
    theBuffer.Truncate(0);    
  }
  if(NS_OK==result)
    result=aScanner.Eof();
  return result;
}

/***************************************/
/* Expat Callback Functions start here */
/***************************************/
 
void nsExpatTokenizer::HandleStartElement(void *userData, const XML_Char *name, const XML_Char **atts){
  CToken* theToken=gTokenRecycler->CreateTokenOfType(eToken_start,eHTMLTag_unknown);
  if(theToken) {
    nsString& theString=theToken->GetStringValueXXX();
    theString.SetString(name);
    AddToken(theToken,NS_OK,*gTokenDeque);
    int theAttrCount=0;
    while(*atts){
      theAttrCount++;
      CAttributeToken* theAttrToken= (CAttributeToken*)gTokenRecycler->CreateTokenOfType(eToken_attribute,eHTMLTag_unknown);
      if(theAttrToken){
        nsString& theKey=theAttrToken->GetKey();
        theKey.SetString(*atts++);
        nsString& theValue=theAttrToken->GetStringValueXXX();
        theValue.SetString(*atts++);
      }
      CToken* theTok=(CToken*)theAttrToken;
      AddToken(theTok,NS_OK,*gTokenDeque);
    }
    theToken->SetAttributeCount(theAttrCount);
  }
  else{
    //THROW A HUGE ERROR IF WE CANT CREATE A TOKEN!
  }
}

void nsExpatTokenizer::HandleEndElement(void *userData, const XML_Char *name) {
  CToken* theToken=gTokenRecycler->CreateTokenOfType(eToken_end,eHTMLTag_unknown);
  if(theToken) {
    nsString& theString=theToken->GetStringValueXXX();
    theString.SetString(name);
    AddToken(theToken,NS_OK,*gTokenDeque);
  }
  else{
    //THROW A HUGE ERROR IF WE CANT CREATE A TOKEN!
  }
}

void nsExpatTokenizer::HandleCharacterData(void *userData, const XML_Char *s, int len) { 
  CCDATASectionToken* currentCDataToken = (CCDATASectionToken*) userData;
  PRBool StartOfCDataSection = (!currentCDataToken && len == 0);
  PRBool EndOfCDataSection = (currentCDataToken && len == 0);

  // Either create a new token (if not currently within a CDATA section) or add the
  // current string from expat to the current CDATA token.

  if (StartOfCDataSection) {
    // Set up state so that we know that we are within a CDATA section.
    currentCDataToken = (CCDATASectionToken*) gTokenRecycler->CreateTokenOfType(eToken_cdatasection,eHTMLTag_unknown);
    XML_SetUserData(gExpatParser, (void *) currentCDataToken);
  }
  else if (EndOfCDataSection) {
    // We've reached the end of the current CDATA section. Push the current CDATA token
    // onto the token queue and reset state to being outside a CDATA section.
    CToken* tempCDATAToken = (CToken*) currentCDataToken;
    AddToken(tempCDATAToken,NS_OK,*gTokenDeque);
    currentCDataToken = 0;
    XML_SetUserData(gExpatParser, 0);
  }
  else if (currentCDataToken) {
    // While there exists a current CDATA token, keep appending all strings from expat into it.
    nsString& theString = currentCDataToken->GetStringValueXXX();
    theString.Append(s,len);
  }
  else {
    CToken* newToken = 0;

    switch(s[0]){
      case kNewLine:
      case CR:
        newToken=gTokenRecycler->CreateTokenOfType(eToken_newline,eHTMLTag_unknown); break;
      case kSpace:
      case kTab:
        newToken=gTokenRecycler->CreateTokenOfType(eToken_whitespace,eHTMLTag_unknown); break;
      default:
        newToken=gTokenRecycler->CreateTokenOfType(eToken_text,eHTMLTag_unknown);
    }
    
    if(newToken) {
      if ((s[0] != kNewLine) && (s[0] != CR)) {
        nsString& theString=newToken->GetStringValueXXX();
        theString.Append(s,len);
      }
      AddToken(newToken,NS_OK,*gTokenDeque);
    }
    else {
      //THROW A HUGE ERROR IF WE CANT CREATE A TOKEN!
    }
  }  
}

void nsExpatTokenizer::HandleProcessingInstruction(void *userData, const XML_Char *target, const XML_Char *data){
  CToken* theToken=gTokenRecycler->CreateTokenOfType(eToken_instruction,eHTMLTag_unknown);
  if(theToken) {
    nsString& theString=theToken->GetStringValueXXX();
    theString.Append("<?");
    theString.Append(target);
    if(data) {
      theString.Append(" ");
      theString.Append(data);
    }
    theString.Append("?>");
    AddToken(theToken,NS_OK,*gTokenDeque);
  }
  else{
    //THROW A HUGE ERROR IF WE CANT CREATE A TOKEN!
  }
}

void nsExpatTokenizer::HandleDefault(void *userData, const XML_Char *s, int len) {
//  NS_NOTYETIMPLEMENTED("Error: nsExpatTokenizer::HandleDefault() not yet implemented.");
}

void nsExpatTokenizer::HandleUnparsedEntityDecl(void *userData, 
                                          const XML_Char *entityName, 
                                          const XML_Char *base, 
                                          const XML_Char *systemId, 
                                          const XML_Char *publicId,
                                          const XML_Char *notationName) {
  NS_NOTYETIMPLEMENTED("Error: nsExpatTokenizer::HandleUnparsedEntityDecl() not yet implemented.");
}

void nsExpatTokenizer::HandleNotationDecl(void *userData,
                                    const XML_Char *notationName,
                                    const XML_Char *base,
                                    const XML_Char *systemId,
                                    const XML_Char *publicId){
  NS_NOTYETIMPLEMENTED("Error: nsExpatTokenizer::HandleNotationDecl() not yet implemented.");
}

int nsExpatTokenizer::HandleExternalEntityRef(XML_Parser parser,
                                         const XML_Char *openEntityNames,
                                         const XML_Char *base,
                                         const XML_Char *systemId,
                                         const XML_Char *publicId){
  NS_NOTYETIMPLEMENTED("Error: nsExpatTokenizer::HandleExternalEntityRef() not yet implemented.");
  int result=0;
  return result;
}

int nsExpatTokenizer::HandleUnknownEncoding(void *encodingHandlerData,
                                       const XML_Char *name,
                                       XML_Encoding *info) {
  NS_NOTYETIMPLEMENTED("Error: nsExpatTokenizer::HandleUnknownEncoding() not yet implemented.");
  int result=0;
  return result;
}

