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
#include "nsIParser.h"
#include "prlog.h"

#include "prmem.h"
#include "nsIUnicharInputStream.h"
#ifdef NECKO
#include "nsNeckoUtil.h"
#else
#include "nsINetService.h"
#endif
#include "nsIServiceManager.h"

 /************************************************************************
  And now for the main class -- nsExpatTokenizer...
 ************************************************************************/

static NS_DEFINE_IID(kISupportsIID,       NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kITokenizerIID,      NS_ITOKENIZER_IID);
static NS_DEFINE_IID(kHTMLTokenizerIID,   NS_HTMLTOKENIZER_IID);
static NS_DEFINE_IID(kClassIID,           NS_EXPATTOKENIZER_IID);
#ifndef NECKO
static NS_DEFINE_IID(kNetServiceCID,      NS_NETSERVICE_CID);
static NS_DEFINE_IID(kINetServiceIID,     NS_INETSERVICE_IID);
#endif


static CTokenRecycler* gTokenRecycler=0;
static nsDeque* gTokenDeque=0;
static XML_Parser gExpatParser=0;
static const char* kDocTypeDeclPrefix = "<!DOCTYPE";

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
    XML_SetDefaultHandlerExpand(mExpatParser, NULL);
    XML_SetUnparsedEntityDeclHandler(mExpatParser, HandleUnparsedEntityDecl);
    XML_SetNotationDeclHandler(mExpatParser, HandleNotationDecl);
    XML_SetExternalEntityRefHandler(mExpatParser, HandleExternalEntityRef);
    XML_SetCommentHandler(mExpatParser, HandleComment);
    XML_SetUnknownEncodingHandler(mExpatParser, HandleUnknownEncoding, NULL);
    XML_SetDoctypeDeclHandler(mExpatParser, HandleStartDoctypeDecl, HandleEndDoctypeDecl);
  }
}


/**
 *  Default constructor
 *   
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
nsExpatTokenizer::nsExpatTokenizer(nsString* aURL) : nsHTMLTokenizer() {  
  NS_INIT_REFCNT();
  mBytesParsed = 0;
  nsAutoString buffer("UTF-16");
  const PRUnichar* encoding = buffer.GetUnicode();
  if (encoding) {
    mExpatParser = XML_ParserCreate((const XML_Char*) encoding);
#ifdef XML_DTD
    XML_SetParamEntityParsing(mExpatParser, XML_PARAM_ENTITY_PARSING_ALWAYS);
#endif
    if (aURL)
      XML_SetBase(mExpatParser, (const XML_Char*) aURL->GetUnicode());
    gTokenRecycler=(CTokenRecycler*)GetTokenRecycler();
    if (mExpatParser) {
      SetupExpatCallbacks();
    }
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
  if (mExpatParser) {    
    XML_ParserFree(mExpatParser);
    mExpatParser = nsnull;
  }
}


/*******************************************************************
  Here begins the real working methods for the tokenizer.
 *******************************************************************/

/* 
 * Parameters:
 * 
 * aSourceBuffer (in): String buffer.
 * aLength (in): Length of input buffer.
 * aOffset (in): Offset in buffer
 * aLine (out): Line on which the character, aSourceBuffer[aOffset], is located.
 */
void nsExpatTokenizer::GetLine(const char* aSourceBuffer, PRUint32 aLength, 
                                PRUint32 aOffset, nsString& aLine)
{
  /* Figure out the line inside aSourceBuffer that contains character specified by aOffset.
     Copy it into aLine. */
  PR_ASSERT(aOffset > 0 && aOffset < aLength);
  /* Assert that the byteIndex and the length of the buffer is even */
  PR_ASSERT(aOffset % 2 == 0 && aLength % 2 == 0);  
  PRUnichar* start = (PRUnichar* ) &aSourceBuffer[aOffset];  /* Will try to find the start of the line */
  PRUnichar* end = (PRUnichar* ) &aSourceBuffer[aOffset];    /* Will try to find the end of the line */
  PRUint32 startIndex = aOffset / sizeof(PRUnichar);          /* Track the position of the 'start' pointer into the buffer */
  PRUint32 endIndex = aOffset / sizeof(PRUnichar);          /* Track the position of the 'end' pointer into the buffer */
  PRUint32 numCharsInBuffer = aLength / sizeof(PRUnichar);
  PRBool reachedStart;
  PRBool reachedEnd;
  

  /* Use start to find the first new line before the error position and
     end to find the first new line after the error position */
  reachedStart = ('\n' == *start || '\r' == *start || startIndex <= 0);
  reachedEnd = ('\n' == *end || '\r' == *end || endIndex >= numCharsInBuffer);
  while (!reachedStart || !reachedEnd) {
    if (!reachedStart) {
      start--;
      startIndex--;
      reachedStart = ('\n' == *start || '\r' == *start || startIndex <= 0);
    }
    if (!reachedEnd) {
      end++;
      endIndex++;
      reachedEnd = ('\n' == *end || '\r' == *end || endIndex >= numCharsInBuffer);
    }
  }

  aLine.Truncate(0);
  if (startIndex == endIndex) {
    /* Special case if the error is on a line where the only character is a newline */
    aLine.Append("");
  }
  else {
    PR_ASSERT(endIndex - startIndex >= sizeof(PRUnichar));
    /* At this point, there are two cases.  Either the error is on the first line or
       on subsequent lines.  If the error is on the first line, startIndex will decrement
       all the way to zero.  If not, startIndex will decrement to the position of the
       newline character on the previous line.  So, in the first case, the start position
       of the error line = startIndex (== 0).  In the second case, the start position of the
       error line = startIndex + 1.  In both cases, the end position of the error line will be 
       (endIndex - 1).  */
    PRUint32 startPosn = (startIndex <= 0) ? startIndex : startIndex + 1;
        
    /* At this point, the substring starting at startPosn and ending at (endIndex - 1),
       is the line on which the error occurred. Copy that substring into the error structure. */
    const PRUnichar* unicodeBuffer = (const PRUnichar*) aSourceBuffer;
    aLine.Append(&unicodeBuffer[startPosn], endIndex - startPosn);
  }
}

/* 
 * Called immediately after an error has occurred in expat.  Creates
 * an error token and pushes it onto the token queue.
 *
 */
void nsExpatTokenizer::PushXMLErrorToken(const char *aBuffer, PRUint32 aLength, PRBool aIsFinal)
{
  CErrorToken* token= (CErrorToken *) gTokenRecycler->CreateTokenOfType(eToken_error, eHTMLTag_unknown);
  nsParserError *error = new nsParserError;
  
  if(error){  
    error->code = XML_GetErrorCode(mExpatParser);
    error->lineNumber = XML_GetCurrentLineNumber(mExpatParser);
    error->colNumber = XML_GetCurrentColumnNumber(mExpatParser);  
    error->description = XML_ErrorString(error->code);
    if (!aIsFinal) {
      PRInt32 byteIndexRelativeToFile = 0;
      byteIndexRelativeToFile = XML_GetCurrentByteIndex(mExpatParser);
      GetLine(aBuffer, aLength, (byteIndexRelativeToFile - mBytesParsed), error->sourceLine);
    }
    else {
      error->sourceLine.Append(mLastLine);
    }

    token->SetError(error);

    CToken* theToken = (CToken* )token;
    AddToken(theToken, NS_OK, *gTokenDeque,gTokenRecycler);
  }
}

nsresult nsExpatTokenizer::ParseXMLBuffer(const char* aBuffer, PRUint32 aLength, PRBool aIsFinal)
{
  nsresult result=NS_OK;
  PR_ASSERT((aBuffer && aLength) || (aBuffer == nsnull && aLength == 0));
  if (mExpatParser) {
    if (!XML_Parse(mExpatParser, aBuffer, aLength, aIsFinal)) {
      PushXMLErrorToken(aBuffer, aLength, aIsFinal);
      result=NS_ERROR_HTMLPARSER_STOPPARSING;
    }
    else if (aBuffer && aLength) {
      // Cache the last line in the buffer
      GetLine(aBuffer, aLength, aLength - sizeof(PRUnichar), mLastLine);
    }
	  mBytesParsed += aLength;    
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
  PRUint32 bufLength = theBuffer.Length() * sizeof(PRUnichar);
  const PRUnichar* expatBuffer = (bufLength) ? theBuffer.GetUnicode() : nsnull;
  
  gTokenDeque=&mTokenDeque;
  gExpatParser = mExpatParser;

  result = ParseXMLBuffer((const char *)expatBuffer, bufLength);
    
  theBuffer.Truncate(0);
  
  if(NS_OK==result)
    result=aScanner.Eof();
  return result;
}

nsresult nsExpatTokenizer::DidTokenize(PRBool aIsFinalChunk)
{
  return ParseXMLBuffer(nsnull, 0, aIsFinalChunk);
}

/**
 * 
 * @update	gess12/29/98
 * @param 
 * @return
 */
void nsExpatTokenizer::FrontloadMisplacedContent(nsDeque& aDeque){
}

/***************************************/
/* Expat Callback Functions start here */
/***************************************/

void nsExpatTokenizer::HandleStartElement(void *userData, const XML_Char *name, const XML_Char **atts){
  CToken* theToken=gTokenRecycler->CreateTokenOfType(eToken_start,eHTMLTag_unknown);
  if(theToken) {
    nsString& theString=theToken->GetStringValueXXX();
    theString.SetString((PRUnichar *) name);
    AddToken(theToken,NS_OK,*gTokenDeque,gTokenRecycler);
    int theAttrCount=0;
    while(*atts){
      theAttrCount++;
      CAttributeToken* theAttrToken= (CAttributeToken*)gTokenRecycler->CreateTokenOfType(eToken_attribute,eHTMLTag_unknown);
      if(theAttrToken){
        nsString& theKey=theAttrToken->GetKey();
        theKey.SetString((PRUnichar *) (*atts++));
        nsString& theValue=theAttrToken->GetStringValueXXX();
        theValue.SetString((PRUnichar *) (*atts++));
      }
      CToken* theTok=(CToken*)theAttrToken;
      AddToken(theTok,NS_OK,*gTokenDeque,gTokenRecycler);
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
    theString.SetString((PRUnichar *) name);
    AddToken(theToken,NS_OK,*gTokenDeque,gTokenRecycler);
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
    AddToken(tempCDATAToken,NS_OK,*gTokenDeque,gTokenRecycler);
    currentCDataToken = 0;
    XML_SetUserData(gExpatParser, 0);
  }
  else if (currentCDataToken) {
    // While there exists a current CDATA token, keep appending all strings from expat into it.
    nsString& theString = currentCDataToken->GetStringValueXXX();
    theString.Append((PRUnichar *) s,len);
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
        theString.Append((PRUnichar *) s,len);
      }
      AddToken(newToken,NS_OK,*gTokenDeque,gTokenRecycler);
    }
    else {
      //THROW A HUGE ERROR IF WE CANT CREATE A TOKEN!
    }
  }  
}

void nsExpatTokenizer::HandleComment(void *userData, const XML_Char *name) {
  CToken* theToken=gTokenRecycler->CreateTokenOfType(eToken_comment, eHTMLTag_unknown);
  if(theToken) {
    nsString& theString=theToken->GetStringValueXXX();
    theString.SetString((PRUnichar *) name);
    AddToken(theToken,NS_OK,*gTokenDeque,gTokenRecycler);
  }
  else{
    //THROW A HUGE ERROR IF WE CANT CREATE A TOKEN!
  }
}

void nsExpatTokenizer::HandleProcessingInstruction(void *userData, const XML_Char *target, const XML_Char *data){
  CToken* theToken=gTokenRecycler->CreateTokenOfType(eToken_instruction,eHTMLTag_unknown);
  if(theToken) {
    nsString& theString=theToken->GetStringValueXXX();
    theString.Append("<?");
    theString.Append((PRUnichar *) target);
    if(data) {
      theString.Append(" ");
      theString.Append((PRUnichar *) data);
    }
    theString.Append("?>");
    AddToken(theToken,NS_OK,*gTokenDeque,gTokenRecycler);
  }
  else{
    //THROW A HUGE ERROR IF WE CANT CREATE A TOKEN!
  }
}

void nsExpatTokenizer::HandleDefault(void *userData, const XML_Char *s, int len) {
  NS_NOTYETIMPLEMENTED("Error: nsExpatTokenizer::HandleDefault() not yet implemented.");
}

void nsExpatTokenizer::HandleUnparsedEntityDecl(void *userData, 
                                          const XML_Char *entityName, 
                                          const XML_Char *base, 
                                          const XML_Char *systemId, 
                                          const XML_Char *publicId,
                                          const XML_Char *notationName) {
  NS_NOTYETIMPLEMENTED("Error: nsExpatTokenizer::HandleUnparsedEntityDecl() not yet implemented.");
}

nsresult
nsExpatTokenizer::OpenInputStream(const nsString& aURLStr, 
                                  const nsString& aBaseURL, 
                                  nsIInputStream** in, 
                                  nsString* aAbsURL) 
{
  nsresult rv;
#ifndef NECKO
  nsINetService* pNetService = nsnull;

  aAbsURL->Truncate(0);
  rv = nsServiceManager::GetService(kNetServiceCID,
                                     kINetServiceIID, (nsISupports**) &pNetService);
  
  if (NS_SUCCEEDED(rv)) {
    nsIURI* contextURL = nsnull;
    rv = pNetService->CreateURL(&contextURL, aBaseURL);
    if (NS_SUCCEEDED(rv)) {
      nsIURI* url = nsnull;
      rv = pNetService->CreateURL(&url, aURLStr, contextURL);
      if (NS_SUCCEEDED(rv)) {
        rv = pNetService->OpenBlockingStream(url, nsnull, in);
        const char* absURL = nsnull;
        url->GetSpec(&absURL);
        aAbsURL->Append(absURL);
        NS_RELEASE(url);
      }    
      NS_RELEASE(contextURL);
    }
    NS_RELEASE(pNetService);
  }
#else // NECKO
  nsIURI* uri;
  nsIURI* baseURI;
  
  rv = NS_NewURI(&baseURI, aBaseURL);
  if (NS_SUCCEEDED(rv)) {
    rv = NS_NewURI(&uri, aURLStr, baseURI);
    if (NS_SUCCEEDED(rv)) {
      rv = NS_OpenURI(in, uri);
      char* absURL = nsnull;
      uri->GetSpec(&absURL);
      aAbsURL->Append(absURL);
      nsCRT::free(absURL);
      NS_RELEASE(uri);
    }
    NS_RELEASE(baseURI);
  }
#endif // NECKO
  return rv;
}

nsresult nsExpatTokenizer::LoadStream(nsIInputStream* in, 
                                      PRUnichar*& uniBuf, 
                                      PRUint32& retLen)
{
  // read it
  PRUint32               aCount = 1024,
                         bufsize = aCount*sizeof(PRUnichar);  
  nsIUnicharInputStream *uniIn = nsnull;
  nsString *utf8 = new nsString("UTF-8");

  nsresult res = NS_NewConverterStream(&uniIn,
                                       nsnull,
                                       in,
                                       aCount,
                                       utf8);
  if (NS_FAILED(res)) return res;

  PRUint32 aReadCount = 0;
  uniBuf = (PRUnichar *) PR_Malloc(bufsize);

  while (NS_OK == (res=uniIn->Read(uniBuf, retLen, aCount, &aReadCount))) {
    retLen += aReadCount;
    if (((aReadCount+32) >= aCount) &&
        ((retLen+aCount) >= bufsize)) {

      bufsize += aCount;
      uniBuf = (PRUnichar *) PR_Realloc(uniBuf, bufsize*sizeof(PRUnichar));
    }
  }/* while */
  if (NS_BASE_STREAM_EOF == res)
    res = NS_OK;
  return res;
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
                                         const XML_Char *publicId)
{
  int result = 0;

#ifdef XML_DTD
  // Create a parser for parsing the external entity
  nsAutoString encoding("UTF-16");  
  XML_Parser entParser = nsnull;

  entParser = XML_ExternalEntityParserCreate(parser, 
                                   0, 
                                   (const XML_Char*) encoding.GetUnicode());

  // Load the external entity into a buffer
  nsIInputStream *in = nsnull;
  nsString urlSpec = (const PRUnichar*) systemId;
  nsString baseURL = (const PRUnichar*) base;
  nsString absURL;

  nsresult rv = OpenInputStream(urlSpec, baseURL, &in, &absURL);

  if (entParser && NS_SUCCEEDED(rv)) {
    PRUint32 retLen = 0;
    PRUnichar *uniBuf = nsnull;
    rv = LoadStream(in, uniBuf, retLen);
    NS_RELEASE(in);

    // Pass the buffer to expat for parsing
    if (NS_SUCCEEDED(rv)) {    
      XML_SetBase(entParser, (const XML_Char*) absURL.GetUnicode());
      result = XML_Parse(entParser, (char *)uniBuf,  retLen * sizeof(PRUnichar), 1);
      XML_ParserFree(entParser);
      PR_FREEIF(uniBuf);      
    }
  }
#else /* ! XML_DTD */

  NS_NOTYETIMPLEMENTED("Error: nsExpatTokenizer::HandleExternalEntityRef() not yet implemented.");  

#endif /* XML_DTD */

  return result;
}

int nsExpatTokenizer::HandleUnknownEncoding(void *encodingHandlerData,
                                       const XML_Char *name,
                                       XML_Encoding *info) {
  NS_NOTYETIMPLEMENTED("Error: nsExpatTokenizer::HandleUnknownEncoding() not yet implemented.");
  int result=0;
  return result;
}

void nsExpatTokenizer::HandleStartDoctypeDecl(void *userData, 
                                              const XML_Char *doctypeName)
{  
  CToken* token=gTokenRecycler->CreateTokenOfType(eToken_doctypeDecl,eHTMLTag_unknown);
  if (token) {
    nsString& str = token->GetStringValueXXX();
    str.Append(kDocTypeDeclPrefix);
    str.Append(" ");
    str.Append((PRUnichar*) doctypeName);
    str.Append(">");
    AddToken(token,NS_OK,*gTokenDeque,gTokenRecycler);
  }
}

void nsExpatTokenizer::HandleEndDoctypeDecl(void *userData)
{
  // Do nothing
}
