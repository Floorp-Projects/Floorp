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

#include <ctype.h> 
#include <time.h>
#include <stdio.h>  
#include "nsScanner.h"
#include "nsToken.h" 
#include "nsHTMLTokens.h"
#include "nsParserTypes.h"
#include "prtypes.h"
#include "nsDebug.h"
#include "nsHTMLTags.h"

//#define GESS_MACHINE
#ifdef GESS_MACHINE
#include "nsEntityEx.cpp"
#endif

static nsString     gIdentChars("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-");
static nsString     gAttrTextChars("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-%.");
static nsString     gAlphaChars("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
static nsAutoString gDigits("0123456789");
static nsAutoString gWhitespace(" \t\b");
static nsAutoString gOperatorChars("/?.<>[]{}~^+=-!%&*(),|:");
static const char*  gUserdefined = "userdefined";
static const char*  gEmpty = "";


const PRInt32 kMAXNAMELEN=10;
struct StrToUnicodeStruct
{
  char    mName[kMAXNAMELEN+1];
  PRInt32 mValue;
};


  // KEEP THIS LIST SORTED!
  // NOTE: This names table is sorted in ascii collating order. If you
  // add a new entry, make sure you put it in the right spot otherwise
  // the binary search code above will break! 
static StrToUnicodeStruct gStrToUnicodeTable[] =
{
  {"AElig", 0x00c6},  {"AMP",   0x0026},  {"Aacute",0x00c1},  
  {"Acirc", 0x00c2},  {"Agrave",0x00c0},  {"Aring", 0x00c5},  
  {"Atilde",0x00c3},  {"Auml",  0x00c4},  {"COPY",  0x00a9},  
  {"Ccedil",0x00c7},  {"ETH",   0x00d0},  {"Eacute",0x00c9},
  {"Ecirc", 0x00ca},  {"Egrave",0x00c8},  {"Euml",  0x00cb},  
  {"GT",    0x003e},  {"Iacute",0x00cd},  {"Icirc", 0x00ce},  
  {"Igrave",0x00cc},  {"Iuml",  0x00cf},  {"LT",    0x003c},  
  {"Ntilde",0x00d1},  {"Oacute",0x00d3},  {"Ocirc", 0x00d4},
  {"Ograve",0x00d2},  {"Oslash",0x00d8},  {"Otilde",0x00d5},  
  {"Ouml",  0x00d6},  {"QUOT",  0x0022},  {"REG",   0x00ae},  
  {"THORN", 0x00de},  {"Uacute",0x00da},  {"Ucirc", 0x00db},  
  {"Ugrave",0x00d9},  {"Uuml",  0x00dc},  {"Yacute",0x00dd},
  {"aacute",0x00e1},  {"acirc", 0x00e2},  {"acute", 0x00b4},  
  {"aelig", 0x00e6},  {"agrave",0x00e0},  {"amp",   0x0026},  
  {"aring", 0x00e5},  {"atilde",0x00e3},  {"auml",  0x00e4},  
  {"brvbar",0x00a6},  {"ccedil",0x00e7},  {"cedil", 0x00b8},
  {"cent",  0x00a2},  {"copy",  0x00a9},  {"curren",0x00a4},  
  {"deg",   0x00b0},  {"divide",0x00f7},  {"eacute",0x00e9},  
  {"ecirc", 0x00ea},  {"egrave",0x00e8},  {"eth",   0x00f0},  
  {"euml",  0x00eb},  {"frac12",0x00bd},  {"frac14",0x00bc},
  {"frac34",0x00be},  {"gt",    0x003e},  {"iacute",0x00ed},  
  {"icirc", 0x00ee},  {"iexcl", 0x00a1},  {"igrave",0x00ec},  
  {"iquest",0x00bf},  {"iuml",  0x00ef},  {"laquo",  0x00ab},  
  {"lt",    0x003c},  {"macr",  0x00af},  {"micro",  0x00b5},
  {"middot",0x00b7},  {"nbsp",  0x00a0},  {"not",   0x00ac}, 
  {"ntilde",0x00f1},  {"oacute",0x00f3},  {"ocirc", 0x00f4},  
  {"ograve",0x00f2},  {"ordf",  0x00aa},  {"ordm",  0x00ba},  
  {"oslash",0x00f8},  {"otilde",0x00f5},  {"ouml",  0x00f6},
  {"para",  0x00b6},  {"plusmn",0x00b1},  {"pound", 0x00a3},  
  {"quot",  0x0022},  {"raquo", 0x00bb},  {"reg",   0x00ae},  
  {"sect",  0x00a7},  {"shy",   0x00ad},  {"sup1",  0x00b9},  
  {"sup2",  0x00b2},  {"sup3",  0x00b3},  {"szlig", 0x00df},
  {"thorn", 0x00fe},  {"times", 0x00d7},  {"uacute",0x00fa},  
  {"ucirc", 0x00fb},  {"ugrave",0x00f9},  {"uml",   0x00a8},  
  {"uuml",  0x00fc},  {"yacute",0x00fd},  {"yen",   0x00a5},  
  {"yuml",  0x00ff}
};

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
CHTMLToken::CHTMLToken(const nsString& aName) : CToken(aName) {
  mTypeID=eHTMLTag_unknown;
}

/*
 *  constructor from tag id
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
CHTMLToken::CHTMLToken(eHTMLTags aTag) : CToken(aTag) {

}

/**
 * Setter method that changes the string value of this token
 * @update	gess5/11/98
 * @param   name is a char* value containing new string value
 */
void CHTMLToken::SetStringValue(const char* name){
  mTextValue=name;
  mStringInit=PR_TRUE;
}

/**
 *  This method retrieves the value of this internal string. 
 *  
 *  @update gess 3/25/98
 *  @return nsString reference to internal string value
 */
static nsAutoString gTagName;
nsString& CHTMLToken::GetStringValueXXX(void) {

  if(!mStringInit) {
    if((mTypeID>eHTMLTag_unknown) && (mTypeID<eHTMLTag_userdefined)) {
      const char* str=GetTagName(mTypeID);
      if(str)
        gTagName=str;
      else gTagName="";
      return gTagName;
    }
  }
  return mTextValue;
}

/**
 *  This method retrieves the value of this internal string
 *  as a cstring.
 *  
 *  @update gess 3/25/98
 *  @return char* rep of internal string value
 */
char* CHTMLToken::GetCStringValue(char* aBuffer, PRInt32 aMaxLen) {

  if(!mStringInit) {
    if((mTypeID>eHTMLTag_unknown) && (mTypeID<eHTMLTag_userdefined)) {
      const char* str=GetTagName(mTypeID);
      if(str) 
        strcpy(aBuffer,str);
      else aBuffer[0]=0;
    }
  }
  else mTextValue.ToCString(aBuffer,aMaxLen);
  return aBuffer;
}


/*
 *  constructor from tag id
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
CStartToken::CStartToken(eHTMLTags aTag) : CHTMLToken(aTag) {
  mAttributed=PR_FALSE;
  mEmpty=PR_FALSE;
}

/*
 *  constructor from tag id
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
CStartToken::CStartToken(nsString& aString) : CHTMLToken(aString) {
  mAttributed=PR_FALSE;
  mEmpty=PR_FALSE;
}


/*
 *  This method returns the typeid (the tag type) for this token.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CStartToken::GetTypeID(){
  if(eHTMLTag_unknown==mTypeID) {
    nsAutoString tmp(mTextValue);
    tmp.ToUpperCase();
    char cbuf[20];
    tmp.ToCString(cbuf, sizeof(cbuf));
    mTypeID = NS_TagToEnum(cbuf);
    switch(mTypeID) {
      case eHTMLTag_dir:
      case eHTMLTag_menu:
        mTypeID=eHTMLTag_ul;
        break;
      default:
        break;
    }
  }
  return mTypeID;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
const char*  CStartToken::GetClassName(void) {
  return "start";
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CStartToken::GetTokenType(void) {
  return eToken_start;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
void CStartToken::SetAttributed(PRBool aValue) {
  mAttributed=aValue;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRBool CStartToken::IsAttributed(void) {
  return mAttributed;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
void CStartToken::SetEmpty(PRBool aValue) {
  mEmpty=aValue;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRBool CStartToken::IsEmpty(void) {
  return mEmpty;
}

/*
 *  Consume the identifier portion of the start tag
 *  
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CStartToken::Consume(PRUnichar aChar, CScanner& aScanner) {

  //if you're here, we've already Consumed the < char, and are
   //ready to Consume the rest of the open tag identifier.
   //Stop consuming as soon as you see a space or a '>'.
   //NOTE: We don't Consume the tag attributes here, nor do we eat the ">"

  mTextValue=aChar;
  nsresult result=aScanner.ReadWhile(mTextValue,gIdentChars,PR_FALSE);
  char buffer[300];
  mTextValue.ToCString(buffer,sizeof(buffer)-1);
  eHTMLTags theTag= NS_TagToEnum(buffer);
  if((theTag>eHTMLTag_unknown) && (theTag<eHTMLTag_userdefined)) {
    mTypeID=theTag;
  } 
  else mStringInit=PR_TRUE;


   //Good. Now, let's skip whitespace after the identifier,
   //and see if the next char is ">". If so, we have a complete
   //tag without attributes.
  if(NS_OK==result) {  
    result=aScanner.SkipWhitespace();
    if(NS_OK==result) {
      result=aScanner.GetChar(aChar);
      if(NS_OK==result) {
        if(kGreaterThan!=aChar) { //look for '>' 
         //push that char back, since we apparently have attributes...
          aScanner.PutBack(aChar);
          mAttributed=PR_TRUE;
        } //if
      } //if
    }//if
  }
  return result;
};


/*
 *  Dump contents of this token to givne output stream
 *  
 *  @update  gess 3/25/98
 *  @param   out -- ostream to output content
 *  @return  
 */
void CStartToken::DebugDumpSource(ostream& out) {
  char buffer[200];
  mTextValue.ToCString(buffer,sizeof(buffer)-1);
  out << "<" << buffer;
  if(!mAttributed)
    out << ">";
}


/*
 *  constructor from tag id
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
CEndToken::CEndToken(eHTMLTags aTag) : CHTMLToken(aTag) {
}


/*
 *  default constructor for end token
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- char* containing token name
 *  @return  
 */
CEndToken::CEndToken(const nsString& aName) : CHTMLToken(aName) {
}

/*
 *  Consume the identifier portion of the end tag
 *  
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CEndToken::Consume(PRUnichar aChar, CScanner& aScanner) {

  //if you're here, we've already Consumed the <! chars, and are
   //ready to Consume the rest of the open tag identifier.
   //Stop consuming as soon as you see a space or a '>'.
   //NOTE: We don't Consume the tag attributes here, nor do we eat the ">"

  mTextValue="";
  static nsAutoString terminals(">");
  nsresult result=aScanner.ReadUntil(mTextValue,terminals,PR_FALSE);

  char buffer[300];
  mTextValue.ToCString(buffer,sizeof(buffer)-1);
  eHTMLTags theTag= NS_TagToEnum(buffer);
  if((theTag>eHTMLTag_unknown) && (theTag<eHTMLTag_userdefined)) {
    mTypeID=theTag;
  } 
  else mStringInit=PR_TRUE;

  if(NS_OK==result)
    result=aScanner.GetChar(aChar); //eat the closing '>;
  return result;
};


/*
 *  Asks the token to determine the <i>HTMLTag type</i> of
 *  the token. This turns around and looks up the tag name
 *  in the tag dictionary.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  eHTMLTag id of this endtag
 */
PRInt32 CEndToken::GetTypeID(){
  if(eHTMLTag_unknown==mTypeID) {
    nsAutoString tmp(mTextValue);
    tmp.ToUpperCase();
    char cbuf[200];
    tmp.ToCString(cbuf, sizeof(cbuf));
    mTypeID = NS_TagToEnum(cbuf);
    switch(mTypeID) {
      case eHTMLTag_dir:
      case eHTMLTag_menu:
        mTypeID=eHTMLTag_ul;
        break;
      default:
        break;
    }
  }
  return mTypeID;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
const char*  CEndToken::GetClassName(void) {
  return "/end";
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CEndToken::GetTokenType(void) {
  return eToken_end;
}

/*
 *  Dump contents of this token to givne output stream
 *  
 *  @update  gess 3/25/98
 *  @param   out -- ostream to output content
 *  @return  
 */
void CEndToken::DebugDumpSource(ostream& out) {
  char buffer[200];
  mTextValue.ToCString(buffer,sizeof(buffer)-1);
  out << "</" << buffer << ">";
}

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CTextToken::CTextToken() : CHTMLToken(eHTMLTag_text) {
}


/*
 *  string based constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CTextToken::CTextToken(const nsString& aName) : CHTMLToken(aName) {
  mTypeID=eHTMLTag_text;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
const char*  CTextToken::GetClassName(void) {
  return "text";
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CTextToken::GetTokenType(void) {
  return eToken_text;
}

/*
 *  Consume as much clear text from scanner as possible.
 *
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CTextToken::Consume(PRUnichar, CScanner& aScanner) {
  static nsAutoString terminals("&<\r\n");
  mStringInit=PR_TRUE;
  nsresult result=aScanner.ReadUntil(mTextValue,terminals,PR_FALSE);
  return result;
};

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CCommentToken::CCommentToken() : CHTMLToken(eHTMLTag_comment) {
}


/*
 *  Default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
CCommentToken::CCommentToken(const nsString& aName) : CHTMLToken(aName) {
  mTypeID=eHTMLTag_comment;
}

/*
 *  Consume the identifier portion of the comment. 
 *  Note that we've already eaten the "<!" portion.
 *  
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CCommentToken::Consume(PRUnichar, CScanner& aScanner) {

  PRUnichar ch,ch2;
  nsresult   result=NS_OK;
  
  static nsAutoString terminals(">");
  
  aScanner.GetChar(ch);
  mTextValue="<!";
  if(kMinus==ch) {
    result=aScanner.GetChar(ch2);
    if(NS_OK==result) {
      if(kMinus==ch2) {
           //in this case, we're reading a long-form comment <-- xxx -->
        mTextValue+="--";
        PRInt32 findpos=-1;
        while((findpos==kNotFound) && (NS_OK==result)) {
          result=aScanner.ReadUntil(mTextValue,terminals,PR_TRUE);
          findpos=mTextValue.RFind("-->");
        }
      }
    }
    return result;
  }
     //if you're here, we're consuming a "short-form" comment
  mTextValue+=ch;
  result=aScanner.ReadUntil(mTextValue,terminals,PR_TRUE);
  mStringInit=PR_TRUE;
  return result;
};

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
const char* CCommentToken::GetClassName(void){
  return "/**/";
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CCommentToken::GetTokenType(void) {
  return eToken_comment;
}

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CNewlineToken::CNewlineToken() : CHTMLToken(eHTMLTag_newline) {
}


/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string value to init token name with
 *  @return  
 */
CNewlineToken::CNewlineToken(const nsString& aName) : CHTMLToken(aName) {
  mTypeID=eHTMLTag_newline;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
const char*  CNewlineToken::GetClassName(void) {
  return "crlf";
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CNewlineToken::GetTokenType(void) {
  return eToken_newline;
}

/**
 *  This method retrieves the value of this internal string. 
 *  
 *  @update gess 3/25/98
 *  @return nsString reference to internal string value
 */
nsString& CNewlineToken::GetStringValueXXX(void) {
  static nsAutoString theStr("\n");
  return theStr;
}

/*
 *  Consume as many cr/lf pairs as you can find.
 *  
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CNewlineToken::Consume(PRUnichar aChar, CScanner& aScanner) {
  mTextValue=aChar;

    //we already read the \r or \n, let's see what's next!
  PRUnichar nextChar;
  nsresult result=aScanner.Peek(nextChar);

  if(NS_OK==result) {
    switch(aChar) {
      case kNewLine:
        if(kCR==nextChar) {
          result=aScanner.GetChar(nextChar);
          mTextValue+=nextChar;
        }
        break;
      case kCR:
        if(kNewLine==nextChar) {
          result=aScanner.GetChar(nextChar);
          mTextValue+=nextChar;
        }
        break;
      default:
        break;
    }
  }  
  mStringInit=PR_TRUE;
  return result;
}

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CAttributeToken::CAttributeToken() : CHTMLToken(eHTMLTag_unknown) {
}

/*
 *  string based constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string value to init token name with
 *  @return  
 */
CAttributeToken::CAttributeToken(const nsString& aName) : CHTMLToken(aName),  
  mTextKey() {
  mLastAttribute=PR_FALSE;
}

/*
 *  construct initializing data to 
 *  key value pair
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string value to init token name with
 *  @return  
 */
CAttributeToken::CAttributeToken(const nsString& aKey, const nsString& aName) : CHTMLToken(aName) {
  mTextKey = aKey;
  mLastAttribute=PR_FALSE;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
const char*  CAttributeToken::GetClassName(void) {
  return "attr";
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CAttributeToken::GetTokenType(void) {
  return eToken_attribute;
}

/*
 *  Dump contents of this token to givne output stream
 *  
 *  @update  gess 3/25/98
 *  @param   out -- ostream to output content
 *  @return  
 */
void CAttributeToken::DebugDumpToken(ostream& out) {
  char buffer[200];
  mTextKey.ToCString(buffer,sizeof(buffer)-1);
  out << "[" << GetClassName() << "] " << buffer << "=";
  mTextValue.ToCString(buffer,sizeof(buffer)-1);
  out << buffer << ": " << mTypeID << endl;
}


/*
 *  This general purpose method is used when you want to
 *  consume a known quoted string. 
 *  
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
PRInt32 ConsumeQuotedString(PRUnichar aChar,nsString& aString,CScanner& aScanner){
  static nsAutoString terminals1(">'");
  static nsAutoString terminals2(">\"");

  PRInt32 result=kNotFound;
  switch(aChar) {
    case kQuote:
      result=aScanner.ReadUntil(aString,terminals2,PR_TRUE);
      break;
    case kApostrophe:
      result=aScanner.ReadUntil(aString,terminals1,PR_TRUE);
      break;
    default:
      break;
  }
  PRUnichar ch=aString.Last();
  if(ch!=aChar)
    aString+=aChar;
  return result;
}

/*
 *  This general purpose method is used when you want to
 *  consume attributed text value.
 *  
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
PRInt32 ConsumeAttributeValueText(PRUnichar,nsString& aString,CScanner& aScanner){

  PRInt32 result=kNotFound;
  static nsAutoString terminals(" \t\b\r\n>");
  result=aScanner.ReadUntil(aString,terminals,PR_FALSE);
  return result;
}


/*
 *  Consume the key and value portions of the attribute.
 *  
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CAttributeToken::Consume(PRUnichar aChar, CScanner& aScanner) {

  aScanner.SkipWhitespace();             //skip leading whitespace                                      
  nsresult result=aScanner.Peek(aChar);
  if(NS_OK==result) {
    if(kQuote==aChar) {               //if you're here, handle quoted key...
      result=aScanner.GetChar(aChar);        //skip the quote sign...
      if(NS_OK==result) {
        mTextKey=aChar;
        result=ConsumeQuotedString(aChar,mTextKey,aScanner);
      }
    }
    else if(kHashsign==aChar) {
      result=aScanner.GetChar(aChar);        //skip the hash sign...
      if(NS_OK==result) {
        mTextKey=aChar;
        result=aScanner.ReadWhile(mTextKey,gDigits,PR_TRUE);
      }
    }
    else {
        //If you're here, handle an unquoted key.
        //Don't forget to reduce entities inline!
      static nsAutoString terminals(" >=\t\b\r\n\"");
      result=aScanner.ReadUntil(mTextKey,terminals,PR_FALSE);
    }

      //now it's time to Consume the (optional) value...
    if(NS_OK == (result=aScanner.SkipWhitespace())) { 
      if(NS_OK == (result=aScanner.Peek(aChar))) {
        if(kEqual==aChar){
          result=aScanner.GetChar(aChar);  //skip the equal sign...
          if(NS_OK==result) {
            result=aScanner.SkipWhitespace();     //now skip any intervening whitespace
            if(NS_OK==result) {
              result=aScanner.GetChar(aChar);  //and grab the next char.    
              if(NS_OK==result) {
                if((kQuote==aChar) || (kApostrophe==aChar)) {
                  mTextValue=aChar;
                  result=ConsumeQuotedString(aChar,mTextValue,aScanner);
                }
                else {      
                  mTextValue=aChar;       //it's an alphanum attribute...
                  result=ConsumeAttributeValueText(aChar,mTextValue,aScanner);
                } 
              }//if
              if(NS_OK==result)
                result=aScanner.SkipWhitespace();     
            }//if
          }//if
        }//if
      }//if
    }
    if(NS_OK==result) {
      result=aScanner.Peek(aChar);
      mLastAttribute= PRBool((kGreaterThan==aChar) || (kEOF==result));
    }
  }
  mStringInit=PR_TRUE;
  return result;
}

/*
 *  Dump contents of this token to givne output stream
 *  
 *  @update  gess 3/25/98
 *  @param   out -- ostream to output content
 *  @return  
 */
void CAttributeToken::DebugDumpSource(ostream& out) {
  char buffer[200];
  mTextKey.ToCString(buffer,sizeof(buffer)-1);
  out << " " << buffer;
  if(mTextValue.Length()){
    mTextValue.ToCString(buffer,sizeof(buffer)-1);
    out << "=" << buffer;
  }
  if(mLastAttribute)
    out<<">";
}

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CWhitespaceToken::CWhitespaceToken() : CHTMLToken(eHTMLTag_whitespace) {
}


/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string value to init token name with
 *  @return  
 */
CWhitespaceToken::CWhitespaceToken(const nsString& aName) : CHTMLToken(aName) {
  mTypeID=eHTMLTag_whitespace;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
const char*  CWhitespaceToken::GetClassName(void) {
  return "ws";
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CWhitespaceToken::GetTokenType(void) {
  return eToken_whitespace;
}

/*
 *  This general purpose method is used when you want to
 *  consume an aribrary sequence of whitespace. 
 *  
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CWhitespaceToken::Consume(PRUnichar aChar, CScanner& aScanner) {
  
  mTextValue=aChar;

  nsresult result=aScanner.ReadWhile(mTextValue,gWhitespace,PR_FALSE);
  if(NS_OK==result) {
    mTextValue.StripChars("\r");
  }
  mStringInit=PR_TRUE;
  return result;
}

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CEntityToken::CEntityToken() : CHTMLToken(eHTMLTag_entity) {
}

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string value to init token name with
 *  @return  
 */
CEntityToken::CEntityToken(const nsString& aName) : CHTMLToken(aName) {
  mTypeID=eHTMLTag_entity;
#ifdef VERBOSE_DEBUG
  if(!VerifyEntityTable())  {
    cout<<"Entity table is invalid!" << endl;
  }
#endif
}

/*
 *  Consume the rest of the entity. We've already eaten the "&".
 *  
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CEntityToken::Consume(PRUnichar aChar, CScanner& aScanner) {
  if(aChar)
    mTextValue=aChar;
  nsresult result=ConsumeEntity(aChar,mTextValue,aScanner);
  mStringInit=PR_TRUE;
  return result;
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
const char*  CEntityToken::GetClassName(void) {
  return "&entity";
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CEntityToken::GetTokenType(void) {
  return eToken_entity;
}

/*
 *  This general purpose method is used when you want to
 *  consume an entity &xxxx;. Keep in mind that entities
 *  are <i>not</i> reduced inline.
 *  
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
PRInt32 CEntityToken::ConsumeEntity(PRUnichar aChar,nsString& aString,CScanner& aScanner){

  PRInt32 result=aScanner.Peek(aChar);
  if(kNoError==result) {
    if(kLeftBrace==aChar) {
      //you're consuming a script entity...
      static nsAutoString terminals("}>");
      result=aScanner.ReadUntil(aString,terminals,PR_FALSE);
      if(kNoError==result) {
        result=aScanner.Peek(aChar);
        if(kNoError==result) {
          if(kRightBrace==aChar) {
            aString+=kRightBrace;   //append rightbrace, and...
            result=aScanner.GetChar(aChar);//yank the closing right-brace
          }
        }
      }
    } //if
    else {
      result=aScanner.ReadWhile(aString,gIdentChars,PR_FALSE);
      if(kNoError==result) {
        result=aScanner.Peek(aChar);
        if(kNoError==result) {
          if (kSemicolon == aChar) {
            // consume semicolon that stopped the scan
            result=aScanner.GetChar(aChar);
          }
        }
      }//if
    } //else
  } //if
  return result;
}


/*
 *  This method converts this entity into its underlying
 *  unicode equivalent.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CEntityToken::TranslateToUnicodeStr(nsString& aString) {
  PRInt32 index=0;
  if(aString.IsDigit(mTextValue[0])) {
    PRInt32 err=0;
    index=mTextValue.ToInteger(&err);
    if(0==err)
      aString.Append(PRUnichar(index));
  }
  else {
    index=FindEntityIndex(mTextValue);
    if(kNotFound!=index) {
      PRUnichar ch=gStrToUnicodeTable[index].mValue;
      aString=ch;
    } 
    else {
#ifdef GESS_MACHINE
      index=TranslateExtendedEntity(mTextValue,aString);
#endif
    }
  }
  return index;
}

 

/*
 *  This method ensures that the entity table doesn't get
 *  out of sync. Make sure you call this at least once.
 *  
 *  @update  gess 3/25/98
 *  @return  PR_TRUE if valid (ordered correctly)
 */
PRBool CEntityToken::VerifyEntityTable(){
  PRInt32  count=sizeof(gStrToUnicodeTable)/sizeof(StrToUnicodeStruct);
  PRInt32 i,j;
  for(i=1;i<count-1;i++)
  {
    j=strcmp(gStrToUnicodeTable[i-1].mName,gStrToUnicodeTable[i].mName);
    if(j>0)
      return PR_FALSE;
  }
  return PR_TRUE;
}


/*
 *  This method is used to convert from a given string (char*)
 *  into a entity index (offset within entity table).
 *  
 *  @update  gess 3/25/98
 *  @param   aBuffer -- string to be converted
 *  @param   aBuflen -- optional string length
 *  @return  integer offset of string in table, or kNotFound
 */
PRInt32 CEntityToken::FindEntityIndex(nsString& aString) {
  PRInt32  result=kNotFound;
  PRInt32  cnt=sizeof(gStrToUnicodeTable)/sizeof(StrToUnicodeStruct);
  PRInt32  low=0; 
  PRInt32  high=cnt-1;
  PRInt32  middle=kNotFound;
    
  if(cnt) {
    while(low<=high)
    {
      middle=(PRInt32)(low+high)/2;
      result=aString.Compare(gStrToUnicodeTable[middle].mName);
//      result=strcmp(aBuffer,gStrToUnicodeTable[middle].mName);
      if (result==0) {
        return middle;
      }
      if (result<0) {
        high=middle-1;
      }
      else low=middle+1; 
    }
  }
  return kNotFound;
}


/*
 *  This method is used to convert from a given string (char*)
 *  into a entity index (offset within entity table).
 *  
 *  @update  gess 3/25/98
 *  @param   aBuffer -- string to be converted
 *  @param   aBuflen -- optional string length
 *  @return  integer offset of string in table, or kNotFound
 */
PRInt32 CEntityToken::FindEntityIndexMax(const char* aBuffer,PRInt32 aBufLen) {
  PRInt32  result=kNotFound;
  PRInt32  cnt=sizeof(gStrToUnicodeTable)/sizeof(StrToUnicodeStruct);
  PRInt32  low=0; 
  PRInt32  high=cnt-1;
  PRInt32  middle=kNotFound;
  
  if(aBuffer) {
    if(-1==aBufLen) {
      aBufLen=strlen(aBuffer);
    }
  
    if(aBufLen && cnt) {
      while(low<=high)
      {
        middle=(PRInt32)(low+high)/2;
        result=strcmp(aBuffer,gStrToUnicodeTable[middle].mName);
        if (result==0) {
          return middle;
        }
        if (result<0) {
          high=middle-1;
        }
        else low=middle+1; 
      }
    }
  }
  return kNotFound;
}


/*
 *  This method reduces all text entities into their char
 *  representation.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CEntityToken::ReduceEntities(nsString& aString) {
  PRInt32 result=0;
  PRInt32 amppos=0;
  PRBool done=PR_FALSE;
  PRInt32 offset=0;

  while(!done) {
    if(kNotFound!=(amppos=aString.Find('&',offset))) {
      if(!nsString::IsSpace(aString[amppos+1])) { //have we found a genuine entity?
        PRInt32 endpos=aString.FindLastCharInSet(gIdentChars,amppos+1);
        PRInt32 cnt;
        if(kNotFound==endpos) 
          cnt=aString.Length()-1-amppos;
        else cnt=endpos-amppos;
        PRInt32 index=FindEntityIndexMax((const char*)&aString[amppos+1],cnt);
        if(kNotFound!=index) {
          aString[amppos]=gStrToUnicodeTable[index].mValue;
          aString.Cut(amppos+1,cnt+(endpos!=kNotFound));
        }
        else offset=amppos+1;
      }
    }
    else done=PR_TRUE;
  }
  return result;
}

/*
 *  Dump contents of this token to givne output stream
 *  
 *  @update  gess 3/25/98
 *  @param   out -- ostream to output content
 *  @return  
 */
void CEntityToken::DebugDumpSource(ostream& out) {
  char* cp=mTextValue.ToNewCString();
  out << "&" << *cp;
  delete cp;
}

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CScriptToken::CScriptToken() : CHTMLToken(eHTMLTag_script) {
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
const char*  CScriptToken::GetClassName(void) {
  return "script";
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CScriptToken::GetTokenType(void) {
  return eToken_script;
}

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CStyleToken::CStyleToken() : CHTMLToken(eHTMLTag_style) {
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
const char*  CStyleToken::GetClassName(void) {
  return "style";
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 CStyleToken::GetTokenType(void) {
  return eToken_style;
}


/*
 *  string based constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string value to init token name with
 *  @return  
 */
CSkippedContentToken::CSkippedContentToken(const nsString& aName) : CAttributeToken(aName) {
  mTextKey = "$skipped-content";/* XXX need a better answer! */
}

/*
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
const char*  CSkippedContentToken::GetClassName(void) {
  return "skipped";
}

/*
 *  Retrieve the token type as an int.
 *  @update  gess 3/25/98 
 *  @return  
 */
PRInt32 CSkippedContentToken::GetTokenType(void) {
  return eToken_skippedcontent;
}

/*
 *  Consume content until you find an end sequence that matches
 *  this objects current mTextValue. Note that this is complicated 
 *  by the fact that you can be parsing content that itself 
 *  contains quoted content of the same type (like <SCRIPT>). 
 *  That means we have to look for quote-pairs, and ignore the 
 *  content inside them.
 *  
 *  @update  gess 7/25/98
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CSkippedContentToken::Consume(PRUnichar,CScanner& aScanner) {
  PRBool      done=PR_FALSE;
  PRInt32     result=kNoError;
  nsString    temp;

  while((!done) && (kNoError==result)) {
    static nsAutoString terminals(">");
    result=aScanner.ReadUntil(temp,terminals,PR_TRUE);
    done=PRBool(kNotFound!=temp.RFind(mTextValue,PR_TRUE));
  }
  mTextValue=temp;
  return result;
}

#if 0
/*
 *  This method iterates the tagtable to ensure that is 
 *  is proper sort order. This method only needs to be
 *  called once.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
class CTagTableVerifier {
public:
  CTagTableVerifier(){
    PRInt32  count=sizeof(gHTMLTagTable)/sizeof(HTMLTagEntry);
    PRInt32 i,j;
    for(i=1;i<count-1;i++)
    {
      j=strcmp(gHTMLTagTable[i-1].mName,gHTMLTagTable[i].mName);
      if(j>0) {
        cout << "Tag Table names are out of order at " << i << "!" << endl;
      }
      if(gHTMLTagTable[i-1].mTagID>=gHTMLTagTable[i].mTagID) {
        cout << "Tag table ID's are out of order at " << i << "!" << endl;;
      }
    }
  }
};
#endif

/**
 * 
 * @update	gess4/25/98
 * @param 
 * @return
 */
const char* GetTagName(PRInt32 aTag) {
  const char* result = NS_EnumToTag((nsHTMLTag) aTag);
  if (0 == result) {
    if(aTag>=eHTMLTag_userdefined)
      result = gUserdefined;
    else result= gEmpty;
  }
  return result;
}


#if 0
/*
 *  This method iterates the attribute-table to ensure that is 
 *  is proper sort order. This method only needs to be
 *  called once.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
class CAttributeTableVerifier {
public:
  CAttributeTableVerifier(){
    PRInt32  count=sizeof(gHTMLAttributeTable)/sizeof(HTMLAttrEntry);
    PRInt32 i,j;
    for(i=1;i<count-1;i++)
    {
      j=strcmp(gHTMLAttributeTable[i-1].mName,gHTMLAttributeTable[i].mName);
      if(j>0) {
#ifdef VERBOSE_DEBUG
        cout << "Attribute table is out of order at " << j << "!" << endl;
#endif
        return;
      }
    }
    return;
  }
};


/*
 *  These objects are here to force the validation of the
 *  tag and attribute tables.
 */

CAttributeTableVerifier gAttributeTableVerifier;
CTagTableVerifier gTableVerifier;



#endif
