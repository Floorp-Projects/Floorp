/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Dreftool.
 *
 * The Initial Developer of the Original Code is
 * Rick Gessner <rick@gessner.com>.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kenneth Herron <kjh-5727@comcast.net>
 *   Bernd Mielke <bmlk@gmx.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "CScanner.h"

const int   kBufsize=1024;


/**
 *  Use this constructor if you want i/o to be stream based.
 *
 *  @update  gess 5/12/98
 *  @param   aStream --
 *  @param   assumeOwnership --
 *  @param   aFilename --
 *  @return  
 */
CScanner::CScanner(istream& aStream)
: mFileStream(aStream),
  mBuffer(""),
  mOffset(0),
  mTotalRead(0),
  mIncremental(true)
{
}


/**
 *  default destructor
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
CScanner::~CScanner() {
}

/** 
 * Grab data from underlying stream.
 *
 * @update  gess4/3/98
 * @return  error code
 */
int CScanner::fillBuffer(void) {
  int anError=kNoError;

  mBuffer="";
  if(!mFileStream) {
    //This is DEBUG code!!!!!!  XXX DEBUG XXX
    //If you're here, it means someone tried to load a
    //non-existent document. So as a favor, we emit a
    //little bit of HTML explaining the error.
    anError=(mIncremental) ? kInterrupted : kEOF;
  }
  else {
    int numread=0;
    char buf[kBufsize+1];
    buf[kBufsize]=0;

    if(mFileStream) {
      mFileStream.read(buf,kBufsize);
      numread=mFileStream.gcount();
      buf[numread]=0;
    }
    mOffset=0;
    if((0<numread) && (0==anError))
      mBuffer=buf;
    mTotalRead+=mBuffer.Length();
  }

  return anError;
}

/**
 *  determine if the scanner has reached EOF
 *  
 *  @update  gess 5/12/98
 *  @param   
 *  @return  0=!eof 1=eof kInterrupted=interrupted
 */
int CScanner::endOfFile() {
  int theError=kNoError;

  if(mOffset>=(int) mBuffer.Length()) {
    theError=fillBuffer();  
  }
  
  if(kNoError==theError) {
    if (0==mBuffer.Length()) {
      return kEOF;
    }
  }

  return theError;
}

/**
 *  retrieve next char from scanners internal input stream
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error code reflecting read status
 */
int CScanner::getChar(char& aChar) {
  int result=kNoError;
  
  if(mOffset>=(int) mBuffer.Length()) 
    result=endOfFile();

  if(kNoError == result) {
    aChar=mBuffer.CharAt(mOffset++);
  }
  return result;
}



/**
 *  peek ahead to consume next char from scanner's internal
 *  input buffer
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
int CScanner::peek(PRUnichar& aChar) {
  int result=kNoError;
  
  if(mOffset>=(int) mBuffer.Length()) 
    result=endOfFile();

  if(kNoError == result) {
    aChar=mBuffer.CharAt(mOffset);
  }
  return result;
}

/**
 *  Push the given char back onto the scanner
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error code
 */
int CScanner::push(PRUnichar aChar) {
  if(mOffset>0)
    mOffset--;
  else {
    nsCAutoString buf;
    buf.Append((char)aChar);
    mBuffer = buf + mBuffer;
  }
  return kNoError;
}


/**
 *  Consume chars as long as they are <i>in</i> the 
 *  given validSet of input chars.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error code
 */
int CScanner::readWhile(nsAFlatCString& aString,nsAFlatCString& aValidSet,bool addTerminal){
  char theChar=0;
  int result=kNoError;

  while(kNoError==result) {
    result=getChar(theChar);
    if(kNoError==result) {
      int pos=aValidSet.FindChar(theChar);
      if(kNotFound==pos) {
        if(addTerminal)
          aString+=theChar;
        else push(theChar);
        break;
      }
      else aString+=theChar;
    }
  }
  return result;
}


/**
 *  Consume characters until you find one contained in given
 *  input set.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error code
 */
int CScanner::readUntil(nsAFlatCString& aString,nsAFlatCString& aTerminalSet,bool addTerminal){
  char theChar=0;
  int result=kNoError;

  while(kNoError == result) {
    result=getChar(theChar);
    if(kNoError==result) {
      int pos=aTerminalSet.FindChar(theChar);
      if(kNotFound!=pos) {
        if(addTerminal)
          aString+=theChar;
        else push(theChar);
        break;
      }
      else aString+=theChar;
    }
  }
  return result;
}


/**
 *  Consumes chars until you see the given terminalChar
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error code
 */
int CScanner::readUntil(nsAFlatCString& aString, PRUnichar aTerminalChar, bool addTerminal) {
  char ch=0;
  int result=kNoError;

  while(kNoError==result) {
    result=getChar(ch);
    if(ch==aTerminalChar) {
      if(addTerminal)
        aString+=ch;
      else push(ch);
      break;
    }
    else aString+=ch;
  }
  return result;
}


