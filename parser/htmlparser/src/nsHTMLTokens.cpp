
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
#include "nsIParser.h"
#include "prtypes.h"
#include "nsDebug.h"
#include "nsHTMLTags.h"
#include "nsHTMLEntities.h"
#include "nsCRT.h"

//#define GESS_MACHINE
#ifdef GESS_MACHINE
#include "nsEntityEx.cpp"
#endif

static const char*  gUserdefined = "userdefined";

const PRInt32 kMAXNAMELEN=10;


/**************************************************************
  And now for the token classes...
 **************************************************************/

/*
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
CHTMLToken::CHTMLToken(const nsString& aName,eHTMLTags aTag) : CToken(aName) {
  mTypeID=aTag;
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
  if(name) {
    mTextValue=name;
    mTypeID = NS_TagToEnum(name);
  }
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
CStartToken::CStartToken(nsString& aString,eHTMLTags aTag) : CHTMLToken(aString,aTag) {
  mAttributed=PR_FALSE;
  mEmpty=PR_FALSE;
}


/**
 * 
 * @update	gess8/4/98
 * @param 
 * @return
 */
void CStartToken::Reinitialize(PRInt32 aTag, const nsString& aString){
  CToken::Reinitialize(aTag,aString);
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
    char cbuf[20];
    tmp.ToCString(cbuf, sizeof(cbuf));
    mTypeID = NS_TagToEnum(cbuf);
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

static
nsString& GetIdentChars(void) {
  static nsString gIdentChars("-0123456789:ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz");
  return gIdentChars;
}

/*
 *  Consume the identifier portion of the start tag
 *  
 *  @update  gess 3/25/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CStartToken::Consume(PRUnichar aChar, nsScanner& aScanner) {

  //if you're here, we've already Consumed the < char, and are
   //ready to Consume the rest of the open tag identifier.
   //Stop consuming as soon as you see a space or a '>'.
   //NOTE: We don't Consume the tag attributes here, nor do we eat the ">"

  mTextValue=aChar;
  nsresult result=aScanner.ReadWhile(mTextValue,GetIdentChars(),PR_TRUE,PR_FALSE);
  char buffer[300];
  mTextValue.ToCString(buffer,sizeof(buffer)-1);
  mTypeID = NS_TagToEnum(buffer);

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
          result=aScanner.PutBack(aChar);
          mAttributed=PR_TRUE;
        } //if
      } //if
    }//if
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
void CStartToken::DebugDumpSource(ostream& out) {
  char buffer[1000];
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
nsresult CEndToken::Consume(PRUnichar aChar, nsScanner& aScanner) {

  //if you're here, we've already Consumed the <! chars, and are
   //ready to Consume the rest of the open tag identifier.
   //Stop consuming as soon as you see a space or a '>'.
   //NOTE: We don't Consume the tag attributes here, nor do we eat the ">"

  mTextValue="";
  nsresult result=aScanner.ReadUntil(mTextValue,kGreaterThan,PR_FALSE);

  if(NS_OK==result){
    char buffer[300];
    mTextValue.ToCString(buffer,sizeof(buffer)-1);

    //This code was added to fix Bug#1125.
    //The problem occurs in bad tags like this: </font size>.
    //"font size" was being viewed as the tag, which of course doesn't exist.
    //Instead, we only look at the first word.
    int theBufPos=-1;
    while(buffer[++theBufPos]){
      if(kSpace==buffer[theBufPos]){ 
        buffer[theBufPos]=0;
        break;
      }
    }
    mTypeID= NS_TagToEnum(buffer);
    result=aScanner.GetChar(aChar); //eat the closing '>;
  }
  return result;
}


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
  char buffer[1000];
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
nsresult CTextToken::Consume(PRUnichar aChar, nsScanner& aScanner) {
  static    nsAutoString terminals("&<\r");
  nsresult  result=NS_OK;
  PRBool    done=PR_FALSE;

  while((NS_OK==result) && (!done)) {
    result=aScanner.ReadUntil(mTextValue,terminals,PR_FALSE,PR_FALSE);
    if(NS_OK==result) {
      result=aScanner.Peek(aChar);
      if((kCR==aChar) && (NS_OK==result)) {
        result=aScanner.GetChar(aChar); //strip off the \r
        result=aScanner.Peek(aChar);    //then see what's next.
        if(NS_OK==result) {
          switch(aChar) {
            case kCR:
              result=aScanner.GetChar(aChar); //strip off the \r
              mTextValue.Append("\n\n");
              break;
            case kNewLine:
               //which means we saw \r\n, which becomes \n
              result=aScanner.GetChar(aChar); //strip off the \n
                  //now fall through on purpose...
            default:
              mTextValue.Append("\n");
              break;
          }//switch
        }//if
      }
      else done=PR_TRUE;
    }
  }
  return result;
}

/*
 *  default constructor
 *  
 *  @update  vidur 11/12/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CCDATASectionToken::CCDATASectionToken() : CHTMLToken(eHTMLTag_unknown) {
}


/*
 *  string based constructor
 *  
 *  @update  vidur 11/12/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CCDATASectionToken::CCDATASectionToken(const nsString& aName) : CHTMLToken(aName) {
  mTypeID=eHTMLTag_unknown;
}

/*
 *  
 *  
 *  @update  vidur 11/12/98
 *  @param   
 *  @return  
 */
const char*  CCDATASectionToken::GetClassName(void) {
  return "cdatasection";
}

/*
 *  
 *  @update  vidur 11/12/98
 *  @param   
 *  @return  
 */
PRInt32 CCDATASectionToken::GetTokenType(void) {
  return eToken_cdatasection;
}

/*
 *  Consume as much marked test from scanner as possible.
 *
 *  @update  vidur 11/12/98
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CCDATASectionToken::Consume(PRUnichar aChar, nsScanner& aScanner) {
  static    nsAutoString terminals("]\r");
  nsresult  result=NS_OK;
  PRBool    done=PR_FALSE;

  while((NS_OK==result) && (!done)) {
    result=aScanner.ReadUntil(mTextValue,terminals,PR_FALSE,PR_FALSE);
    if(NS_OK==result) {
      result=aScanner.Peek(aChar);
      if((kCR==aChar) && (NS_OK==result)) {
        result=aScanner.GetChar(aChar); //strip off the \r
        result=aScanner.Peek(aChar);    //then see what's next.
        if(NS_OK==result) {
          switch(aChar) {
            case kCR:
              result=aScanner.GetChar(aChar); //strip off the \r
              mTextValue.Append("\n\n");
              break;
            case kNewLine:
               //which means we saw \r\n, which becomes \n
              result=aScanner.GetChar(aChar); //strip off the \n
                  //now fall through on purpose...
            default:
              mTextValue.Append("\n");
              break;
          } //switch
        } //if
      }
      else if (kRightSquareBracket==aChar) {
        result=aScanner.GetChar(aChar); //strip off the ]
        result=aScanner.Peek(aChar);    //then see what's next.
        if((NS_OK==result) && (kRightSquareBracket==aChar)) {
          result=aScanner.GetChar(aChar);    //strip off the second ]
          result=aScanner.Peek(aChar);    //then see what's next.
          if(NS_OK==result) {
            if (kGreaterThan==aChar) {
              result=aScanner.GetChar(aChar); //strip off the >
              done=PR_TRUE;
            }
            else {
              // This isn't the end of the CDATA section so go on
              mTextValue.Append("]");
            }
          }//if
        }
        else {
          // This isn't the end of the CDATA section so go on
          mTextValue.Append("]");
        }
      }
      else done=PR_TRUE;
    }
  }
  return result;
}


/*
 *  Default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aName -- string to init token name with
 *  @return  
 */
CCommentToken::CCommentToken() : CHTMLToken(eHTMLTag_comment) {
}


/*
 *  Copy constructor
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
CCommentToken::CCommentToken(const nsString& aName) : CHTMLToken(aName) {
  mTypeID=eHTMLTag_comment;
}

/*
 *  This method consumes a comment using the (CORRECT) comment parsing
 *  algorithm supplied by W3C. 
 *  
 *  @update  gess 01/04/99
 *  @param   
 *  @param   
 *  @return  
 */
static
nsresult ConsumeStrictComment(PRUnichar aChar, nsScanner& aScanner,nsString& aString) {
  static    nsAutoString gMinus("-");
  nsresult  result=NS_OK;
 
  /*********************************************************
    NOTE: This algorithm does a fine job of handling comments
          when they're formatted per spec, but if they're not
          we don't handle them well. For example, we gack
          on the following:

          <!-- xx -- xx --> 
   *********************************************************/

  aString="<!";
  while(NS_OK==result) {
    result=aScanner.GetChar(aChar);
    if(NS_OK==result) {
      aString+=aChar;
      if(kMinus==aChar) {
        result=aScanner.GetChar(aChar);
        if(NS_OK==result) {
          if(kMinus==aChar) {
               //in this case, we're reading a long-form comment <-- xxx -->
            aString+=aChar;
            result=aScanner.ReadWhile(aString,gMinus,PR_TRUE,PR_FALSE);  //get all available '---'
            if(NS_OK==result) {
              PRInt32 findpos=-1;
              nsAutoString temp("");
              //Read to the first ending sequence '--'
              while((kNotFound==findpos) && (NS_OK==result)) {
                result=aScanner.ReadUntil(temp,kMinus,PR_TRUE);
                findpos=temp.RFind("--");
              } 
              aString+=temp;
              if(NS_OK==result) {
                result=aScanner.ReadWhile(aString,gMinus,PR_TRUE,PR_FALSE);  //get all available '---'
                if(NS_OK==result) {
                  temp="->";
                  result=aScanner.ReadUntil(aString,temp,PR_FALSE,PR_FALSE);
                }
              } 
            }
          } //
          else break; //go find '>'
        }
      }//if
      else if(kGreaterThan==aChar) {
        return result;
      }
      else break; //go find '>'
    }//if
  }//while
  if(NS_OK==result) {
     //if you're here, we're consuming a "short-form" comment
    result=aScanner.ReadUntil(aString,kGreaterThan,PR_TRUE);
  }
  return result;
}

/*
 *  This method consumes a comment using common (actually non-standard)
 *  algorithm that seems to work against the content on the web.
 *  
 *  @update  gess 01/04/99
 *  @param   
 *  @param   
 *  @return  
 */
static
nsresult ConsumeComment(PRUnichar aChar, nsScanner& aScanner,nsString& aString) {
  static    nsAutoString gEdibles("!-");
  static    nsAutoString gMinus("-");
  static    nsAutoString gWhitespace("\b\t\n\r ");
  nsresult  result=NS_OK;
 
  /*********************************************************
    NOTE: This algorithm does a fine job of handling comments
          commonly used, but it doesn't really consume them
          per spec (But then, neither does IE or Nav).
   *********************************************************/

  aString="<!";
  result=aScanner.GetChar(aChar);
  if(NS_OK==result) {
    aString+=aChar;
    if(kMinus==aChar) {
      result=aScanner.GetChar(aChar);
      if(NS_OK==result) {
        if(kMinus==aChar) {
             //in this case, we're reading a long-form comment <-- xxx -->
          aString+=aChar;
          PRBool done=PR_FALSE;
          PRInt32 findpos=kNotFound;
          result=aScanner.ReadWhile(aString,gMinus,PR_TRUE,PR_TRUE);  //get all available '---'
          findpos=aString.RFind("-->");
          //nsAutoString temp("");
          //nsAutoString ws("");
          while((kNotFound==findpos) && (NS_OK==result)) {
            result=aScanner.ReadUntil(aString,kMinus,PR_TRUE);

            if(NS_OK==result) {
              result=aScanner.ReadWhile(aString,gMinus,PR_TRUE,PR_FALSE);  //get all available '---'
              if(NS_OK==result)
                result=aScanner.ReadWhile(aString,gWhitespace,PR_TRUE,PR_FALSE);  //get all available whitespace
            }
            
            if(NS_OK==result) {
              result=aScanner.GetChar(aChar);
              aString+=aChar;
            }

            findpos=aString.RFind("-->");
            if(kNotFound==findpos)
              findpos=aString.RFind("!>");
          } //while
          return result;
        } //if
      }//if
    }//if
  }//if
  if(NS_OK==result) {
     //Read up to the closing '>'
    result=aScanner.ReadUntil(aString,kGreaterThan,PR_TRUE);
  }
  return result;
}

/*
 *  Consume the identifier portion of the comment. 
 *  Note that we've already eaten the "<!" portion.
 *  
 *  @update  gess 1/27/99
 *  @param   aChar -- last char consumed from stream
 *  @param   aScanner -- controller of underlying input source
 *  @return  error result
 */
nsresult CCommentToken::Consume(PRUnichar aChar, nsScanner& aScanner) {
  PRBool theStrictForm=PR_FALSE;
  nsresult result=(theStrictForm) ? ConsumeStrictComment(aChar,aScanner,mTextValue) : ConsumeComment(aChar,aScanner,mTextValue);
  return result;
}


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
nsresult CNewlineToken::Consume(PRUnichar aChar, nsScanner& aScanner) {
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

/**
 * 
 * @update	gess8/4/98
 * @param 
 * @return
 */
void CAttributeToken::Reinitialize(PRInt32 aTag, const nsString& aString){
  CHTMLToken::Reinitialize(aTag,aString);
  mTextKey.Truncate();
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
 *  Dump contents of this token to given output stream
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
nsresult ConsumeQuotedString(PRUnichar aChar,nsString& aString,nsScanner& aScanner){
  nsresult result=NS_OK;
  switch(aChar) {
    case kQuote:
      result=aScanner.ReadUntil(aString,kQuote,PR_TRUE);
      if(NS_OK==result)
        result=aScanner.SkipOver(kQuote);  //this code is here in case someone mistakenly adds multiple quotes...
      break;
    case kApostrophe:
      result=aScanner.ReadUntil(aString,kApostrophe,PR_TRUE);
      if(NS_OK==result)
        result=aScanner.SkipOver(kApostrophe); //this code is here in case someone mistakenly adds multiple apostrophes...
      break;
    default:
      break;
  }
  PRUnichar ch=aString.Last();
  if(ch!=aChar)
    aString+=aChar;
  aString.ReplaceChar(PRUnichar('\n'),PRUnichar(' '));
  aString.StripChars("\r"); //per the HTML spec, ignore linefeeds...
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
static
nsresult ConsumeAttributeValueText(PRUnichar,nsString& aString,nsScanner& aScanner){
  static nsAutoString terminals("\b\t\n\r >");
  nsresult result=aScanner.ReadUntil(aString,terminals,PR_FALSE,PR_FALSE);
  
  //Let's force quotes if either the first or last char is quoted.
  PRUnichar theLast=aString.Last();
  PRUnichar theFirst=aString.First();
  if(kQuote==theLast) {
    if(kQuote!=theFirst) {
      aString.Insert(kQuote,0);;
    }
  }
  else if(kQuote==theFirst) {
    if(kQuote!=theLast) {
      aString+=kQuote;
    }
  }

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
nsresult CAttributeToken::Consume(PRUnichar aChar, nsScanner& aScanner) {

  nsresult result=aScanner.SkipWhitespace();             //skip leading whitespace 
  if(NS_OK==result) {
    result=aScanner.Peek(aChar);
    if(NS_OK==result) {
      if(kQuote==aChar) {               //if you're here, handle quoted key...
        result=aScanner.GetChar(aChar);        //skip the quote sign...
        if(NS_OK==result) {
          result=aScanner.Peek(aChar);  //peek ahead to make sure the next char is a legal attr-key
          if(NS_OK==result) {
            if(nsString::IsAlpha(aChar) || nsString::IsDigit(aChar)){
              mTextKey=aChar;
              result=ConsumeQuotedString(aChar,mTextKey,aScanner);
            }
            else {
              return NS_ERROR_HTMLPARSER_BADATTRIBUTE;
            }
          } //if
        }//if
      }
      else if(kHashsign==aChar) {
        result=aScanner.GetChar(aChar);        //skip the hash sign...
        if(NS_OK==result) {
          mTextKey=aChar;
          static nsAutoString gDigits("0123456789");
          result=aScanner.ReadWhile(mTextKey,gDigits,PR_TRUE,PR_FALSE);
        }
      }
      else {
          //If you're here, handle an unquoted key.
          //Don't forget to reduce entities inline!
        static nsAutoString terminals("\b\t\n\r \"=>");
        result=aScanner.ReadUntil(mTextKey,terminals,PR_TRUE,PR_FALSE);
      }

        //now it's time to Consume the (optional) value...
      if(NS_OK==result) {
        result=aScanner.SkipWhitespace();
        if(NS_OK==result) { 
          result=aScanner.Peek(aChar);       //Skip ahead until you find an equal sign or a '>'...
          if(NS_OK==result) {  
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
                    else if(kGreaterThan==aChar){      
                      result=aScanner.PutBack(aChar);
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
            else {
              //This is where we have to handle fairly busted content.
              //If you're here, it means we saw an attribute name, but couldn't find 
              //the following equal sign.  <tag NAME=....
          
              //Doing this right in all cases is <i>REALLY</i> ugly. 
              //My best guess is to grab the next non-ws char. We know it's not '=',
              //so let's see what it is. If it's a '"', then assume we're reading
              //from the middle of the value. Try stripping the quote and continuing...

              if(kQuote==aChar){
                result=aScanner.SkipOver(aChar); //strip quote.
              }
            }
          }//if
        } //if
      }//if
      if(NS_OK==result) {
        result=aScanner.Peek(aChar);
        mLastAttribute= PRBool((kGreaterThan==aChar) || (kEOF==result));
      }
    } //if
  }//if
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
  static char buffer[1000];
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
nsresult CWhitespaceToken::Consume(PRUnichar aChar, nsScanner& aScanner) {
  
  mTextValue=aChar;
  static nsAutoString theWhitespace("\b\t ");
  nsresult result=aScanner.ReadWhile(mTextValue,theWhitespace,PR_FALSE,PR_FALSE);
  if(NS_OK==result) {
    mTextValue.StripChars("\r");
  }
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
nsresult CEntityToken::Consume(PRUnichar aChar, nsScanner& aScanner) {
  if(aChar)
    mTextValue=aChar;
  nsresult result=ConsumeEntity(aChar,mTextValue,aScanner);
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
PRInt32 CEntityToken::ConsumeEntity(PRUnichar aChar,nsString& aString,nsScanner& aScanner){

  PRInt32 result=aScanner.Peek(aChar);
  if(NS_OK==result) {
    if(kLeftBrace==aChar) {
      //you're consuming a script entity...
      static nsAutoString terminals("}>");
      result=aScanner.ReadUntil(aString,terminals,PR_FALSE,PR_FALSE);
      if(NS_OK==result) {
        result=aScanner.Peek(aChar);
        if(NS_OK==result) {
          if(kRightBrace==aChar) {
            aString+=kRightBrace;   //append rightbrace, and...
            result=aScanner.GetChar(aChar);//yank the closing right-brace
          }
        }
      }
    } //if
    else {
      result=aScanner.ReadWhile(aString,GetIdentChars(),PR_TRUE,PR_FALSE);
      if(NS_OK==result) {
        result=aScanner.Peek(aChar);
        if(NS_OK==result) {
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

#define PA_REMAP_128_TO_160_ILLEGAL_NCR 1

#ifdef PA_REMAP_128_TO_160_ILLEGAL_NCR
/**
 * Map some illegal but commonly used numeric entities into their
 * appropriate unicode value.
 */
#define NOT_USED 0xfffd

static PRUint16 PA_HackTable[] = {
	NOT_USED,
	NOT_USED,
	0x201a,  /* SINGLE LOW-9 QUOTATION MARK */
	0x0192,  /* LATIN SMALL LETTER F WITH HOOK */
	0x201e,  /* DOUBLE LOW-9 QUOTATION MARK */
	0x2026,  /* HORIZONTAL ELLIPSIS */
	0x2020,  /* DAGGER */
	0x2021,  /* DOUBLE DAGGER */
	0x02c6,  /* MODIFIER LETTER CIRCUMFLEX ACCENT */
	0x2030,  /* PER MILLE SIGN */
	0x0160,  /* LATIN CAPITAL LETTER S WITH CARON */
	0x2039,  /* SINGLE LEFT-POINTING ANGLE QUOTATION MARK */
	0x0152,  /* LATIN CAPITAL LIGATURE OE */
	NOT_USED,
	NOT_USED,
	NOT_USED,
	NOT_USED,
	0x2018,  /* LEFT SINGLE QUOTATION MARK */
	0x2019,  /* RIGHT SINGLE QUOTATION MARK */
	0x201c,  /* LEFT DOUBLE QUOTATION MARK */
	0x201d,  /* RIGHT DOUBLE QUOTATION MARK */
	0x2022,  /* BULLET */
	0x2013,  /* EN DASH */
	0x2014,  /* EM DASH */
	0x02dc,  /* SMALL TILDE */
	0x2122,  /* TRADE MARK SIGN */
	0x0161,  /* LATIN SMALL LETTER S WITH CARON */
	0x203a,  /* SINGLE RIGHT-POINTING ANGLE QUOTATION MARK */
	0x0153,  /* LATIN SMALL LIGATURE OE */
	NOT_USED,
	NOT_USED,
	0x0178   /* LATIN CAPITAL LETTER Y WITH DIAERESIS */
};
#endif /* PA_REMAP_128_TO_160_ILLEGAL_NCR */


/*
 *  This method converts this entity into its underlying
 *  unicode equivalent.
 *  
 *  @update  gess 3/25/98
 *  @param   aString will hold the resulting string value
 *  @return  numeric (unichar) value
 */
PRInt32 CEntityToken::TranslateToUnicodeStr(nsString& aString) {
  PRInt32 value=0;
  PRInt32 theRadix[2]={16,10};

  if(mTextValue.Length()>1) {
    PRUnichar theChar0=mTextValue[0];
    PRBool    isDigit0=nsString::IsDigit(theChar0);

    char cbuf[30];
    mTextValue.ToCString(cbuf, sizeof(cbuf)-1);
    value = NS_EntityToUnicode(cbuf);
    if(-1<value) {
      //we found a named entity...
      aString=PRUnichar(value);
    }
    else {

      if(isDigit0 || ('x'==theChar0) || ('X'==theChar0)) {
        PRInt32 err=0;
        value=mTextValue.ToInteger(&err,theRadix[isDigit0]);
        if(0==err) {
    #ifdef PA_REMAP_128_TO_160_ILLEGAL_NCR
          /* for some illegal, but popular usage */
          if ((value >= 0x0080) && (value <= 0x009f)) {
            value = PA_HackTable[value - 0x0080];
          }
    #endif
          aString.Append(PRUnichar(value));
        }//if
      }//if
    }//else
  }//if

  return value;
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
  delete[] cp;
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
nsresult CSkippedContentToken::Consume(PRUnichar aChar,nsScanner& aScanner) { 
  PRBool      done=PR_FALSE; 
  nsresult    result=NS_OK; 
  nsString    temp; 
  PRUnichar   theChar;

  //We're going to try a new algorithm here. Rather than scan for the matching 
 //end tag like we used to do, we're now going to scan for whitespace and comments. 
 //If we find either, just eat them. If we find text or a tag, then go to the 
 //target endtag, or the start of another comment. 

  static nsAutoString theWhitespace2("\b\t ");

  while((!done) && (NS_OK==result)) { 
    result=aScanner.GetChar(aChar); 
    if((NS_OK==result) && (kLessThan==aChar)) { 
      //we're reading a tag or a comment... 
      result=aScanner.GetChar(theChar); 
      if((NS_OK==result) && (kExclamation==theChar)) { 
        //read a comment... 
        static CCommentToken theComment; 
        result=theComment.Consume(aChar,aScanner); 
        if(NS_OK==result) { 
          result=aScanner.SkipWhitespace();
          temp.Append(theComment.GetStringValueXXX()); 
        } 
      } else { 
        //read a tag... 
        temp+=aChar; 
        temp+=theChar; 
        result=aScanner.ReadUntil(temp,kGreaterThan,PR_TRUE); 
      } 
    } 
    else if(0<=theWhitespace2.BinarySearch(aChar)) { 
      static CWhitespaceToken theWS; 
      result=theWS.Consume(aChar,aScanner); 
      if(NS_OK==result) { 
        temp.Append(theWS.GetStringValueXXX()); 
      } 
    } 
    else { 
      temp+=aChar; 
      result=aScanner.ReadUntil(temp,kLessThan,PR_FALSE); 
    } 
    done=PRBool(kNotFound!=temp.RFind(mTextValue,PR_TRUE)); 
  } 
  int len=temp.Length(); 
  temp.Truncate(len-mTextValue.Length()); 
  mTextKey=temp; 
  return result; 
} 

/*
 *  Dump contents of this token to givne output stream
 *  
 *  @update  gess 3/25/98
 *  @param   out -- ostream to output content
 *  @return  
 */
void CSkippedContentToken::DebugDumpSource(ostream& out) {
  static char buffer[1000];
  mTextKey.ToCString(buffer,sizeof(buffer)-1);
  out << " " << buffer;
  if(mLastAttribute)
    out<<">";
}


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
    else result=0;
  }
  return result;
}


/**
 *  
 *  
 *  @update  gess 9/23/98
 *  @param   
 *  @return  
 */
CInstructionToken::CInstructionToken() : CHTMLToken(eHTMLTag_unknown) {
}

/**
 *  
 *  
 *  @update  gess 9/23/98
 *  @param   
 *  @return  
 */
CInstructionToken::CInstructionToken(const nsString& aString) : CHTMLToken(aString) {
}

/**
 *  
 *  
 *  @update  gess 9/23/98
 *  @param   
 *  @return  
 */
nsresult CInstructionToken::Consume(PRUnichar aChar,nsScanner& aScanner){
  mTextValue="<?";
  nsresult result=aScanner.ReadUntil(mTextValue,kGreaterThan,PR_TRUE);
  return result;
}

/**
 *  
 *  
 *  @update  gess 9/23/98
 *  @param   
 *  @return  
 */
const char* CInstructionToken::GetClassName(void){
  return "instruction";
}

/**
 *  
 *  
 *  @update  gess 9/23/98
 *  @param   
 *  @return  
 */
PRInt32 CInstructionToken::GetTokenType(void){
  return eToken_instruction;
}


CErrorToken::CErrorToken(nsParserError *aError) : CHTMLToken(eHTMLTag_unknown)
{
  mError = aError;
}

CErrorToken::~CErrorToken() 
{
  delete mError;
}

PRInt32 CErrorToken::GetTokenType(void){
  return eToken_error;
}

const char* CErrorToken::GetClassName(void){
  return "error";
}

void CErrorToken::SetError(nsParserError *aError) {
  mError = aError;
}

const nsParserError * CErrorToken::GetError(void) 
{ 
  return mError; 
}
