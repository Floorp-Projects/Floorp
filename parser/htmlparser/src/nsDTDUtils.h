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



#ifndef DTDUTILS_
#define DTDUTILS_

#include "nsHTMLTags.h"
#include "nsHTMLTokens.h"
#include "nsIParser.h"
#include "nsCRT.h"
#include "nsDeque.h"
#include "nsIDTD.h"
#include <fstream.h>
#include "nsITokenizer.h"
#include "nsString.h"

/***************************************************************
  Before digging into the NavDTD, we'll define a helper 
  class called CTagStack.

  Simply put, we've built ourselves a little data structure that
  serves as a stack for htmltags (and associated bits). 
  What's special is that if you #define rickgdebug 1, the stack
  size can grow dynamically (like you'ld want in a release build.)
  If you don't #define rickgdebug 1, then the stack is a fixed size,
  equal to the eStackSize enum. This makes debugging easier, because
  you can see the htmltags on the stack if its not dynamic.
 ***************************************************************/

void DebugDumpContainmentRules(nsIDTD& theDTD,const char* aFilename,const char* aTitle);
void DebugDumpContainmentRules2(nsIDTD& theDTD,const char* aFilename,const char* aTitle);
PRUint32 AccumulateCRC(PRUint32 crc_accum, char *data_blk_ptr, int data_blk_size);

/**************************************************************
  This is the place to store the "bad-content" tokens, and the 
  also the regular tags.
 **************************************************************/

struct nsTagEntry {
  eHTMLTags mTag;
  PRInt8    mBankIndex;
  PRInt8    mStyleIndex;
};

class nsTagStack {
public:
              nsTagStack();
              ~nsTagStack();
  void        Push(eHTMLTags aTag);
  eHTMLTags   Pop();
  eHTMLTags   First() const;
  eHTMLTags   TagAt(PRUint32 anIndex) const;
  nsTagEntry& EntryAt(PRUint32 anIndex) const;
  PRInt32     GetTopmostIndexOf(eHTMLTags aTag) const;
  eHTMLTags   operator[](PRUint32 anIndex) const;
  eHTMLTags   Last() const;
  void        Empty(void); 
  PRInt32     GetSize(void) const {return mCount;}

  nsTagEntry* mEntries;
  PRUint32    mCount;
  PRUint32    mCapacity;
};


/***************************************************************
  The dtdcontext class defines the current tag context (both
  structural and stylistic). This small utility class allows us
  to push/pop contexts at will, which makes handling styles in
  certainly (unfriendly) elements (like tables) much easier.
 ***************************************************************/

class nsDTDContext {
public:
                nsDTDContext();
                ~nsDTDContext();
  void          Push(eHTMLTags aTag);
  eHTMLTags     Pop();
  eHTMLTags     First() const;
  eHTMLTags     TagAt(PRInt32 anIndex) const;
  eHTMLTags     operator[](PRInt32 anIndex) const;
  eHTMLTags     Last() const;
  void          Empty(void); 
  PRInt32       GetCount(void);
  nsTagStack*   GetStyles(void) const;

  void          SaveToken(CToken* aToken, PRInt32 aID);
  CToken*       RestoreTokenFrom(PRInt32 aID);
  PRInt32       TokenCountAt(PRInt32 aID);

  nsTagStack    mStack;
  nsDeque       mSkipped; //each entry will hold a deque full of skipped tokens...
  nsDeque       mStyles;  //each entry will hold a tagstack full of style tags...
#ifdef  NS_DEBUG
  eHTMLTags   mTags[100];
#endif
};


/**************************************************************
  Now define the token deallocator class...
 **************************************************************/
class CTokenDeallocator: public nsDequeFunctor{
public:
  virtual void* operator()(void* anObject) {
    CToken* aToken = (CToken*)anObject;
    delete aToken;
    return 0;
  }
};


/************************************************************************
  CTokenRecycler class implementation.
  This class is used to recycle tokens. 
  By using this simple class, we cut WAY down on the number of tokens
  that get created during the run of the system.
 ************************************************************************/
class CTokenRecycler : public nsITokenRecycler {
public:
  
//      enum {eCacheMaxSize=100}; 

                  CTokenRecycler();
  virtual         ~CTokenRecycler();
  virtual void    RecycleToken(CToken* aToken);
  virtual CToken* CreateTokenOfType(eHTMLTokenTypes aType,eHTMLTags aTag, const nsString& aString);
  virtual CToken* CreateTokenOfType(eHTMLTokenTypes aType,eHTMLTags aTag);

protected:
    nsDeque*  mTokenCache[eToken_last-1];
#ifdef  NS_DEBUG
    int       mTotals[eToken_last-1];
#endif
};

/************************************************************************
  ITagHandler class offers an API for taking care of specific tokens.
 ************************************************************************/
class nsITagHandler {
public:
  
  virtual void          SetString(const nsString &aTheString)=0;
  virtual nsString*     GetString()=0;
  virtual PRBool        HandleToken(CToken* aToken,nsIDTD* aDTD)=0;
  virtual PRBool        HandleCapturedTokens(CToken* aToken,nsIDTD* aDTD)=0;
};

/************************************************************************
  Here are a few useful utility methods...
 ************************************************************************/

/**
 * This method quickly scans the given set of tags,
 * looking for the given tag.
 * @update	gess8/27/98
 * @param   aTag -- tag to be search for in set
 * @param   aTagSet -- set of tags to be searched
 * @return
 */
inline PRInt32 IndexOfTagInSet(PRInt32 aTag,const eHTMLTags aTagSet[],PRInt32 aCount)  {
  PRInt32 index;

  for(index=0;index<aCount;index++)
    if(aTag==aTagSet[index]) {
      return index;
    }
  return kNotFound;
}

/**
 * This method quickly scans the given set of tags,
 * looking for the given tag.
 * @update	gess8/27/98
 * @param   aTag -- tag to be search for in set
 * @param   aTagSet -- set of tags to be searched
 * @return
 */
inline PRBool FindTagInSet(PRInt32 aTag,const eHTMLTags aTagSet[],PRInt32 aCount)  {
  return PRBool(-1<IndexOfTagInSet(aTag,aTagSet,aCount));
}

/**
 * Called from various DTD's to determine the type of data in the buffer...
 * @update	gess11/20/98
 * @param 
 * @return
 */
inline PRBool BufferContainsHTML(nsString& aBuffer){
  PRBool result=PR_FALSE;
  nsString temp;
  aBuffer.Left(temp,200);
  temp.ToLowerCase();
  if((-1<temp.Find("<html ") || (-1<temp.Find("!doctype html public")))) {
    result=PR_TRUE;
  }
  return result;
}


/******************************************************************************
  This little structure is used to compute CRC32 values for our debug validator
 ******************************************************************************/

struct CRCStruct {
  CRCStruct(eHTMLTags aTag,PRInt32 anOp) {mTag=aTag; mOperation=anOp;}
  eHTMLTags mTag; 
  PRInt32   mOperation; //usually open or close
};

/******************************************************************************
  This class is used to store ref's to token observers during the parse phase.
  Note that for simplicity, this is a singleton that is constructed in the
  CNavDTD and shared for the duration of the application session. Later on it
  might be nice to use a more dynamic approach that would permit observers to
  come and go on a document basis.
 ******************************************************************************/
class CObserverDictionary {
public:
  CObserverDictionary();
  ~CObserverDictionary();

  void      RegisterObservers();
  void      UnregisterObservers();
  nsDeque*  GetObserversForTag(eHTMLTags aTag);

protected:
  nsDeque*  mObservers[NS_HTML_TAG_MAX];
};

#endif


