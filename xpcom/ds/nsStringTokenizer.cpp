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

#include "nsStringTokenizer.h"


nsStringTokenizer::nsStringTokenizer(const char* aDataSpec,const char* aFieldSep,const char* aRecordSep) :
  mDataStartDelimiter(""),
  mDataEndDelimiter(""),
  mSubstrStartDelimiter(""),
  mSubstrEndDelimiter(""),
  mFieldSeparator(aFieldSep),
  mRecordSeparator(aRecordSep) 
{
  mBuffer=0;
  mOffset=0;
  mValidChars[0]=mValidChars[1]=mValidChars[2]=mValidChars[3]=0;
  mInvalidChars[0]=mInvalidChars[1]=mInvalidChars[2]=mInvalidChars[3]=0;
  ExpandDataSpecifier(aDataSpec);
  mCharSpec=eGivenChars;
}

nsStringTokenizer::~nsStringTokenizer(){
}

/**
 * This method can tell you whether a given char is in the valid set
 * given by the user in the constructor
 * @update	gess7/10/99
 */
void nsStringTokenizer::SetBuffer(nsString& aBuffer) {
  mBuffer=&aBuffer;
}

/**
 * This method can tell you whether a given char is in the valid set
 * given by the user in the constructor
 * @update	gess7/10/99
 */
inline PRBool nsStringTokenizer::IsValidDataChar(PRUnichar aChar) {
  PRInt32 theByteNum=aChar/32;
  PRInt32 theBitNum=aChar-(theByteNum*32);
  PRInt32 shift=(1<<theBitNum);
  PRInt32 value=PRInt32(mValidChars[theByteNum]&shift);
  return PRBool(value>0);
}

inline void SetChars(PRInt32 array[3],PRUnichar aStart,PRUnichar aStop){
  PRInt32 theChar;
  for(theChar=aStart;theChar<=aStop;theChar++){
    PRInt32 theByteNum=theChar/32;
    PRInt32 theBitNum=theChar-(theByteNum*32);
    PRInt32 shift=(1<<theBitNum);
    array[theByteNum]|=shift;
  }
}

inline void ClearChars(PRInt32 array[3],PRUnichar aStart,PRUnichar aStop){
  PRInt32 theChar;
  for(theChar=aStart;theChar<=aStop;theChar++){
    PRInt32 theByteNum=theChar/32;
    PRInt32 theBitNum=theChar-(theByteNum*32);
    PRInt32 shift=(1<<theBitNum);
    array[theByteNum]&=(~shift);
  }
}

/**
 * This method constructs the legal charset and data delimiter pairs.
 * Specifier rules are:
 *    abc   -- allows a set of characters
 *    [a-z] -- allows all chars in given range
 *    [*-*] -- allows all characters
 *    ^abc  -- disallows a set of characters
 *    [a^z] -- disallows all characters in given range
 *    [a*b] -- specifies a delimiter pair for the entire token
 *    [a+b] -- specifies a delimiter pair for substrings in the token
 * @update	gess7/10/99
 */
void nsStringTokenizer::ExpandDataSpecifier(const char* aDataSpec) {
  if(aDataSpec) {
    PRInt32   theIndex=-1;
    char  theChar=0;
    while(theChar=aDataSpec[++theIndex]) {
      switch(theChar) {
        case '[':
          switch(aDataSpec[theIndex+2]){
          case '-':
            {
              char theStart=aDataSpec[theIndex+1];
              char theEnd=aDataSpec[theIndex+3];
              if(('*'==theStart) && (theStart==theEnd)) {
                mCharSpec=eAllChars;
              }
              else {
                SetChars(mValidChars,theStart,theEnd);
              }
            }
            break;

          case '^': //specify a range of invalid chars
            {
              char theStart=aDataSpec[theIndex+1];
              char theEnd=aDataSpec[theIndex+3];
              SetChars(mInvalidChars,theStart,theEnd);              
            }
            break;

          case '*': //this char signals a delimiter pair
            mDataStartDelimiter+=aDataSpec[theIndex+1];
            mDataEndDelimiter+=aDataSpec[theIndex+3];
            break;

          case '+': //this char signals a delimiter pair for substrings
            mSubstrStartDelimiter+=aDataSpec[theIndex+1];
            mSubstrEndDelimiter+=aDataSpec[theIndex+3];
            break;

          default:
            break;
          }
          theIndex+=4;
          break;

        case '^'://they've given us a list (not a range) of invalid chars
          {
            while(theChar=aDataSpec[++theIndex]) {
              if('['!=theChar) {
                SetChars(mInvalidChars,theChar,theChar);
              }
              else {
                --theIndex;
                break;
              }
            }
          }
          break;
        default:
          SetChars(mValidChars,theChar,theChar);
          break;
      }//switch
    }
  }

/* DEBUG CODE TO SHOW STRING OF GIVEN CHARSET
  CAutoString temp;
  for(PRInt32 theChar=0;theChar<128;theChar++){
    if(IsValidDataChar(theChar)) 
			temp+=theChar;
  }
  PRInt32 x=10;
*/
}

nsStringTokenizer::eCharTypes nsStringTokenizer::DetermineCharType(PRUnichar ch) {
	eCharTypes result=eUnknown;
	
  if(mRecordSeparator[0]==ch)
    result=eRecordSeparator;
  else if(mFieldSeparator[0]==ch)
    result=eFieldSeparator;
  else if((mDataStartDelimiter[0]==ch) || (mDataEndDelimiter[0]==ch))
    result=eDataDelimiter;
  else if(IsValidDataChar(ch))
    result=eDataChar;
	return result;
}


/**
 * Moves the input stream to the start of the file.
 * @update	gess7/25/98
 * @return  yes if all is well
 */
PRBool nsStringTokenizer::FirstRecord(void){
  mOffset=0;
  return PRBool(mBuffer!=0);
}



/**
 * Seeks to next record
 * @update	gess7/25/98
 * @return  PR_TRUE if there IS a next record
 */
PRBool nsStringTokenizer::NextRecord(void){
  PRBool result=PR_FALSE;

  if(mBuffer) {
    PRInt32  status=SkipOver(mRecordSeparator);
    if(NS_OK==status) {
      if(SkipToValidData()) {
        if(NS_OK==status) {
          result=HasNextToken();
        }
      }
      else result=PR_FALSE;
	  }
  }
  return result;
}


/*
 * LAST MODS:	gess 	12Aug94
 * PARMS:		«»	
 * RETURNS:		YES if there is another field to be read. 
 * PURPOSE:		Allows a client to ask the io system to test for
 					the presence of another field.
 */
PRBool  nsStringTokenizer::HasNextToken(void){
  PRBool result=PR_FALSE;

  if(mBuffer){
	  while(More())	{
		  //Now go test to see if there is any other field data in this record.
		  //The appropriate algorithm here is to scan the file until you
		  //find one of following things occurs:
		  //	1. You find a field separator
		  //	2. You find a record separator
		  //	3.	You hit the end of the file
		  //	4.	You find a valid char.
		  PRUnichar theChar;
		  GetChar(theChar);
		  switch(DetermineCharType(theChar)){

			  case	eUnknown: //ok to skip junk between delimiters...
          if(-1<mSubstrStartDelimiter.Find(theChar)) {
            break;
          }

			  case	eDataChar:
          if(kSpace<theChar) {
            UnGetChar(theChar);
            return PR_TRUE;
          }
				  break;

			  case	eDataDelimiter:
          UnGetChar(theChar);
				  return PR_TRUE;

			  case	eFieldSeparator:
          SkipOver(mFieldSeparator[0]);
          return PR_TRUE;

        case	eRecordSeparator:
          UnGetChar(theChar);
          return PR_FALSE;

			  default:
				  return PR_FALSE;
		  }
	  }
  }//if
  return result;
}


/**
 * LAST MODS:	gess 	4Jul94
 * PARMS:			
 * RETURNS:	error code; 0 means all is well. 
 * PURPOSE:	Gets the next field of data from the stream.
 * NOTES:		This does not currently handle fields that have
 					  field delimiters (ie quotes).	
					
 * WARNING: You should have called HasNextToken prior
					  to calling this method, so that you can
					  fail gracefully if you encounter the end
					  of your input stream (unexpectedly). If
					  this method hits EOF, it returns an error. 
 */
PRInt32   nsStringTokenizer::GetNextToken(nsString& aToken){
  PRInt32 result=0;

  if(mBuffer && More()) {
	  PRUnichar theChar;
    if(mDataStartDelimiter.Length()) {
      result=GetChar(theChar); //skip delimiter...
      if(mFieldSeparator[0]==theChar)
        return result;
      aToken+=theChar;
    }
    if(NS_OK==result) {
      PRUnichar theTerm[]={mFieldSeparator[0],mRecordSeparator[0],0,0};
      if(mDataEndDelimiter.Length()) {
        theTerm[2]=mDataEndDelimiter[0];
      }
      result=ReadUntil(aToken,theTerm,PRBool(0!=mDataEndDelimiter[0]));
      if(NS_OK==result) {
        PRInt32  status=SkipOver(mFieldSeparator[0]);
      }
    }
	}
  return result;
}



/*
 * This method gets called when the system wants to jump over any garbage before that may be in a
 * string. Typically, this happens before, inbetween and after valid data rows.
 *
 * LAST MODS:	gess 	11Aug94
 * RETURNS:		0 if all is well; non-zero for error. If you hit EOF, return 0. 
 */
PRBool nsStringTokenizer::SkipToValidData(void){
  PRInt32 result=0;
	PRUnichar	ch;

  if(mBuffer) {
	  while(More()) {
		  result=GetChar(ch);

		  switch(DetermineCharType(ch)){
			  case	eDataChar:
          if(!mDataStartDelimiter[0]) {
            UnGetChar(ch);
            return PR_TRUE;
          }
          break;
        
        case eDataDelimiter:
          if(ch==mDataStartDelimiter[0]) {
            UnGetChar(ch);
            return PR_TRUE;
          }
          break;

			  case	eFieldSeparator:
        case  eRecordSeparator:
				  UnGetChar(ch);
				  return PR_TRUE;

			  default:
				  break;
      } //switch
    }	 //while
  }//if
  return PR_FALSE;
}

PRInt32 nsStringTokenizer::SkipOver(PRUnichar aSkipChar) {
  PRUnichar	theChar=0;
  PRInt32			result=NS_OK;

  if(mBuffer) {
    while(NS_OK==result) {
      result=GetChar(theChar);
      if(NS_OK == result) {
        if(theChar!=aSkipChar) {
          UnGetChar(theChar);
          break;
        }
      } 
      else break;
    } //while
  }//if
  return result;
}

PRInt32 nsStringTokenizer::SkipOver(nsString& aString) {
  PRUnichar	theChar=0;
  PRInt32			result=NS_OK;

  if(mBuffer) {
    while(NS_OK==result) {
      result=GetChar(theChar);
      if(NS_OK == result) {
    
        PRInt32 index=aString.Find(theChar);
        if(-1==index) {
          UnGetChar(theChar);
          break;
        }
      } 
      else break;
    } //while
  } //if
  return result;
}

PRInt32 nsStringTokenizer::ReadUntil(nsString& aString,PRUnichar* aTermSet,PRBool addTerminal){
  PRInt32 result=NS_OK;

  PRUnichar   theChar=0;
  PRBool      theCharIsValid;

  if(mBuffer) {
    while(NS_OK == result) {
      result=GetChar(theChar);
      if(NS_OK==result) {

        PRBool found=PR_FALSE;
        PRInt32 index=-1;
        while(aTermSet[++index]){
          if(theChar==aTermSet[index]){
            found=PR_TRUE;
            break;
          }
        }

        if(found) {
          if(addTerminal)
            aString+=theChar;
          else UnGetChar(theChar);
          break;
        }
        else {
          PRInt32 pos=mSubstrStartDelimiter.Find(theChar);
          if(-1<pos) {
            aString+=theChar;
            result=ReadUntil(aString,mSubstrEndDelimiter[pos],PR_TRUE);
          }
          else if(theCharIsValid){
            if(IsValidDataChar(theChar)){
              aString+=theChar;
            }
            else theCharIsValid=PR_FALSE;
          }
        } //else
      } //if
    } //while
  }//if

  return result;
}

PRInt32 nsStringTokenizer::ReadUntil(nsString& aString,PRUnichar aTerminalChar,PRBool addTerminal){
  PRInt32 result=NS_OK;

  PRUnichar   theChar=0;
  if(mBuffer) {
    while(NS_OK == result) {
      result=GetChar(theChar);
      if(NS_OK==result) {

        if(theChar==aTerminalChar){
          if(addTerminal)
            aString+=theChar;
          else UnGetChar(theChar);
          break;
        }
        else aString+=theChar;
      }//if
    } //while
  }//if

  return result;
}

PRBool nsStringTokenizer::More(void){
  PRBool result=PR_FALSE;
  if(mBuffer) {
    if(mOffset<mBuffer->Length())
      result=PR_TRUE;
  }
  return result;
}

PRInt32 nsStringTokenizer::GetChar(PRUnichar& aChar){
  PRInt32 result=kEOF;
  if(mBuffer) {
    if(mOffset<mBuffer->Length()) {
      aChar=(*mBuffer)[mOffset++];
      result=0;
    }
  }
  return result;
}

void nsStringTokenizer::UnGetChar(PRUnichar aChar){
  if(mOffset>0)
    mOffset--;
}


/*
 * Call this method if you want the tokenizer to iterate your string
 * and automatically call you back with each token
 *
 * @parm    aFunctor is the object you want me to notify
 * @update  gess  07/10/99
 * RETURNS:	0 if all went well
 */
PRInt32 nsStringTokenizer::Iterate(nsString& aBuffer,ITokenizeFunctor& aFunctor) {
  PRInt32 result=0;
  PRInt32 theRecordNum=-1;
  
  nsString* theOldBuffer=mBuffer;
  mBuffer=&aBuffer;
  FirstRecord(); 
  while(HasNextToken()){
    theRecordNum++;
    PRInt32 theTokenNum=-1;
    while(HasNextToken()){
      theTokenNum++;
      nsAutoString theString;
      GetNextToken(theString);
      aFunctor(theString,theRecordNum,theTokenNum);
    }
    NextRecord();
  }
  mBuffer=theOldBuffer;
  return result;
}





