/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#include "nsITokenizer.h"
#include "nsString.h"
#include "nsIParserNode.h"
#include "nsFixedSizeAllocator.h"
#include "nsCOMArray.h"
#include "nsIParserService.h"
#include "nsReadableUtils.h"
#include "nsIHTMLContentSink.h"

#define IF_HOLD(_ptr) \
 PR_BEGIN_MACRO       \
 if(_ptr) {           \
   _ptr->AddRef();    \
 }                    \
 PR_END_MACRO

// recycles _ptr
#define IF_FREE(_ptr, _allocator)                \
  PR_BEGIN_MACRO                                 \
  if(_ptr && _allocator) {                       \
    _ptr->Release((_allocator)->GetArenaPool()); \
    _ptr=0;                                      \
  }                                              \
  PR_END_MACRO   

// release objects and destroy _ptr
#define IF_DELETE(_ptr, _allocator) \
  PR_BEGIN_MACRO                    \
  if(_ptr) {                        \
    _ptr->ReleaseAll(_allocator);   \
    delete(_ptr);                   \
    _ptr=0;                         \
  }                                 \
  PR_END_MACRO

class nsIParserNode;
class nsCParserNode;
class nsNodeAllocator;


#ifdef DEBUG
void DebugDumpContainmentRules(nsIDTD& theDTD,const char* aFilename,const char* aTitle);
void DebugDumpContainmentRules2(nsIDTD& theDTD,const char* aFilename,const char* aTitle);
#endif

/***************************************************************
  First, define the tagstack class
 ***************************************************************/

class nsEntryStack;  //forware declare to make compilers happy.

struct nsTagEntry {
  nsTagEntry ()
    : mTag(eHTMLTag_unknown), mNode(0), mParent(0), mStyles(0){}
  eHTMLTags       mTag;  //for speedier access to tag id
  nsCParserNode*  mNode;
  nsEntryStack*   mParent;
  nsEntryStack*   mStyles;
};

class nsEntryStack {

public:
                  nsEntryStack();
                  ~nsEntryStack();

  nsTagEntry*     PopEntry();
  void            PushEntry(nsTagEntry* aEntry, bool aRefCntNode = true);
  void            EnsureCapacityFor(PRInt32 aNewMax, PRInt32 aShiftOffset=0);
  void            Push(nsCParserNode* aNode,nsEntryStack* aStyleStack=0, bool aRefCntNode = true);
  void            PushTag(eHTMLTags aTag);
  void            PushFront(nsCParserNode* aNode,nsEntryStack* aStyleStack=0, bool aRefCntNode = true);
  void            Append(nsEntryStack *aStack);
  nsCParserNode*  Pop(void);
  nsCParserNode*  Remove(PRInt32 anIndex,eHTMLTags aTag);
  nsCParserNode*  NodeAt(PRInt32 anIndex) const;
  eHTMLTags       First() const;
  eHTMLTags       TagAt(PRInt32 anIndex) const;
  nsTagEntry*     EntryAt(PRInt32 anIndex) const;
  eHTMLTags       operator[](PRInt32 anIndex) const;
  eHTMLTags       Last() const;
  void            Empty(void); 

  /*
   * Release all objects in the entry stack
   */
  void ReleaseAll(nsNodeAllocator* aNodeAllocator);
  
  /**
   * Find the first instance of given tag on the stack.
   * @update	gess 12/14/99
   * @param   aTag
   * @return  index of tag, or kNotFound if not found
   */
  inline PRInt32 FirstOf(eHTMLTags aTag) const {
    PRInt32 index=-1;
    
    if(0<mCount) {
      while(++index<mCount) {
        if(aTag==mEntries[index].mTag) {
          return index;
        }
      } //while
    }
    return kNotFound;
  }


  /**
   * Find the last instance of given tag on the stack.
   * @update	gess 12/14/99
   * @param   aTag
   * @return  index of tag, or kNotFound if not found
   */
  inline PRInt32 LastOf(eHTMLTags aTag) const {
    PRInt32 index=mCount;
    while(--index>=0) {
        if(aTag==mEntries[index].mTag) {
          return index; 
        }
    }
    return kNotFound;
  }

  nsTagEntry* mEntries;
  PRInt32    mCount;
  PRInt32    mCapacity;
};


/**********************************************************
  The table state class is used to store info about each
  table that is opened on the stack. As tables open and
  close on the context, we update these objects to track 
  what has/hasn't been seen on a per table basis. 
 **********************************************************/
class CTableState {
public:
  CTableState(CTableState *aPreviousState=0) {
    mHasCaption=PR_FALSE;
    mHasCols=PR_FALSE;
    mHasTHead=PR_FALSE;
    mHasTFoot=PR_FALSE;
    mHasTBody=PR_FALSE;    
    mPrevious=aPreviousState;
  }

  bool    CanOpenCaption() {
    bool result=!(mHasCaption || mHasCols || mHasTHead || mHasTFoot || mHasTBody);
    return result;
  }

  bool    CanOpenCols() {
    bool result=!(mHasCols || mHasTHead || mHasTFoot || mHasTBody);
    return result;
  }

  bool    CanOpenTBody() {
    bool result=!(mHasTBody);
    return result;
  }

  bool    CanOpenTHead() {
    bool result=!(mHasTHead || mHasTFoot || mHasTBody);
    return result;
  }

  bool    CanOpenTFoot() {
    bool result=!(mHasTFoot || mHasTBody);
    return result;
  }

  bool          mHasCaption;
  bool          mHasCols;
  bool          mHasTHead;
  bool          mHasTFoot;
  bool          mHasTBody;
  CTableState   *mPrevious;
};

/************************************************************************
  nsTokenAllocator class implementation.
  This class is used to recycle tokens. 
  By using this simple class, we cut WAY down on the number of tokens
  that get created during the run of the system.

  Note: The allocator is created per document. It's been shared 
        ( but not ref. counted ) by objects, tokenizer,dtd,and dtd context,
        that cease to exist when the document is destroyed.
 ************************************************************************/
class nsTokenAllocator
{
public: 

  nsTokenAllocator();
  ~nsTokenAllocator();
  CToken* CreateTokenOfType(eHTMLTokenTypes aType,eHTMLTags aTag, const nsAString& aString);
  CToken* CreateTokenOfType(eHTMLTokenTypes aType,eHTMLTags aTag);

  nsFixedSizeAllocator& GetArenaPool() { return mArenaPool; }

protected:
  nsFixedSizeAllocator mArenaPool;
#ifdef  NS_DEBUG
  int mTotals[eToken_last-1];
#endif
};

/************************************************************************
  CNodeRecycler class implementation.
  This class is used to recycle nodes. 
  By using this simple class, we cut down on the number of nodes
  that get created during the run of the system.
 ************************************************************************/

#ifndef HEAP_ALLOCATED_NODES
class nsCParserNode;
#endif

class nsNodeAllocator
{
public:
  
  nsNodeAllocator();
  ~nsNodeAllocator();
  nsCParserNode* CreateNode(CToken* aToken=nsnull, nsTokenAllocator* aTokenAllocator=0);

  nsFixedSizeAllocator&  GetArenaPool() { return mNodePool; }

#ifdef HEAP_ALLOCATED_NODES
  void Recycle(nsCParserNode* aNode) { mSharedNodes.Push(static_cast<void*>(aNode)); }
protected:
  nsDeque mSharedNodes;
#ifdef DEBUG_TRACK_NODES
  PRInt32 mCount;
#endif
#endif

protected:
  nsFixedSizeAllocator mNodePool;
};

/************************************************************************
  The dtdcontext class defines an ordered list of tags (a context).
 ************************************************************************/

class nsDTDContext
{
public:
                  nsDTDContext();
                  ~nsDTDContext();

  nsTagEntry*     PopEntry();
  void            PushEntry(nsTagEntry* aEntry, bool aRefCntNode = true);
  void            MoveEntries(nsDTDContext& aDest, PRInt32 aCount);
  void            Push(nsCParserNode* aNode,nsEntryStack* aStyleStack=0, bool aRefCntNode = true);
  void            PushTag(eHTMLTags aTag);
  nsCParserNode*  Pop(nsEntryStack*& aChildStack);
  nsCParserNode*  Pop();
  nsCParserNode*  PeekNode() { return mStack.NodeAt(mStack.mCount-1); }
  eHTMLTags       First(void) const;
  eHTMLTags       Last(void) const;
  nsTagEntry*     LastEntry(void) const;
  eHTMLTags       TagAt(PRInt32 anIndex) const;
  eHTMLTags       operator[](PRInt32 anIndex) const {return TagAt(anIndex);}
  bool            HasOpenContainer(eHTMLTags aTag) const;
  PRInt32         FirstOf(eHTMLTags aTag) const {return mStack.FirstOf(aTag);}
  PRInt32         LastOf(eHTMLTags aTag) const {return mStack.LastOf(aTag);}

  void            Empty(void); 
  PRInt32         GetCount(void) const {return mStack.mCount;}
  PRInt32         GetResidualStyleCount(void) {return mResidualStyleCount;}
  nsEntryStack*   GetStylesAt(PRInt32 anIndex) const;
  void            PushStyle(nsCParserNode* aNode);
  void            PushStyles(nsEntryStack *aStyles);
  nsCParserNode*  PopStyle(void);
  nsCParserNode*  PopStyle(eHTMLTags aTag);
  void            RemoveStyle(eHTMLTags aTag);

  static  void    ReleaseGlobalObjects(void);

  void            SetTokenAllocator(nsTokenAllocator* aTokenAllocator) { mTokenAllocator=aTokenAllocator; }
  void            SetNodeAllocator(nsNodeAllocator* aNodeAllocator) { mNodeAllocator=aNodeAllocator; }

  nsEntryStack    mStack; //this will hold a list of tagentries...
  PRInt32         mResidualStyleCount;
  PRInt32         mContextTopIndex;

  nsTokenAllocator  *mTokenAllocator;
  nsNodeAllocator   *mNodeAllocator;

#ifdef  NS_DEBUG
  enum { eMaxTags = MAX_REFLOW_DEPTH };
  eHTMLTags       mXTags[eMaxTags];
#endif
};

/**************************************************************
  Now define the token deallocator class...
 **************************************************************/
class CTokenDeallocator: public nsDequeFunctor{
protected:
  nsFixedSizeAllocator& mArenaPool;

public:
  CTokenDeallocator(nsFixedSizeAllocator& aArenaPool)
    : mArenaPool(aArenaPool) {}

  virtual void* operator()(void* anObject) {
    CToken* aToken = (CToken*)anObject;
    aToken->Release(mArenaPool);
    return 0;
  }
};


/************************************************************************
  ITagHandler class offers an API for taking care of specific tokens.
 ************************************************************************/
class nsITagHandler {
public:
  
  virtual void          SetString(const nsString &aTheString)=0;
  virtual nsString*     GetString()=0;
  virtual bool          HandleToken(CToken* aToken,nsIDTD* aDTD)=0;
  virtual bool          HandleCapturedTokens(CToken* aToken,nsIDTD* aDTD)=0;
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
inline PRInt32 IndexOfTagInSet(PRInt32 aTag,const eHTMLTags* aTagSet,PRInt32 aCount)  {

  const eHTMLTags* theEnd=aTagSet+aCount;
  const eHTMLTags* theTag=aTagSet;

  while(theTag<theEnd) {
    if(aTag==*theTag) {
      return theTag-aTagSet;
    }
    ++theTag;
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
inline bool FindTagInSet(PRInt32 aTag,const eHTMLTags *aTagSet,PRInt32 aCount)  {
  return bool(-1<IndexOfTagInSet(aTag,aTagSet,aCount));
}

/**************************************************************
  This defines the topic object used by the observer service.
  The observerService uses a list of these, 1 per topic when
  registering tags.
 **************************************************************/

class nsObserverEntry : public nsIObserverEntry {
public:
  NS_DECL_ISUPPORTS
            nsObserverEntry(const nsAString& aString);
  virtual   ~nsObserverEntry();

  NS_IMETHOD Notify(nsIParserNode* aNode,
                    nsIParser* aParser,
                    nsISupports* aDocShell,
                    const PRUint32 aFlags);

  nsresult   AddObserver(nsIElementObserver* aObserver,eHTMLTags aTag);
  void       RemoveObserver(nsIElementObserver* aObserver);
  bool       Matches(const nsAString& aTopic);

protected:
  nsString mTopic;
  nsCOMArray<nsIElementObserver>* mObservers[NS_HTML_TAG_MAX + 1];
  friend class nsMatchesTopic;
};

/*********************************************************************************************/


struct TagList {
  size_t mCount;
  const eHTMLTags *mTags;
};

/**
 * Find the last member of given taglist on the given context
 * @update	gess 12/14/99
 * @param   aContext
 * @param   aTagList
 * @return  index of tag, or kNotFound if not found
 */
inline PRInt32 LastOf(nsDTDContext& aContext, const TagList& aTagList){
  int max = aContext.GetCount();
  int index;
  for(index=max-1;index>=0;index--){
    bool result=FindTagInSet(aContext[index],aTagList.mTags,aTagList.mCount);
    if(result) {
      return index;
    }
  }
  return kNotFound;
}
 
/**
 * Find the first member of given taglist on the given context
 * @update	gess 12/14/99
 * @param   aContext
 * @param   aStartOffset
 * @param   aTagList
 * @return  index of tag, or kNotFound if not found
 */
inline PRInt32 FirstOf(nsDTDContext& aContext,PRInt32 aStartOffset,TagList& aTagList){
  int max = aContext.GetCount();
  int index;
  for(index=aStartOffset;index<max;++index){
    bool result=FindTagInSet(aContext[index],aTagList.mTags,aTagList.mCount);
    if(result) {
      return index;
    }
  }
  return kNotFound;
}


/**
 * Call this to find out whether the DTD thinks the tag requires an END tag </xxx>
 * @update	gess 01/04/99
 * @param   id of tag
 * @return  TRUE of the element's end tag is optional
 */
inline bool HasOptionalEndTag(eHTMLTags aTag) {
  static eHTMLTags gHasOptionalEndTags[]={eHTMLTag_body,eHTMLTag_colgroup,eHTMLTag_dd,eHTMLTag_dt,
                                                    eHTMLTag_head,eHTMLTag_li,eHTMLTag_option,
                                                    eHTMLTag_p,eHTMLTag_tbody,eHTMLTag_td,eHTMLTag_tfoot,
                                                    eHTMLTag_th,eHTMLTag_thead,eHTMLTag_tr,
                                                    eHTMLTag_userdefined,eHTMLTag_unknown};
  return FindTagInSet(aTag,gHasOptionalEndTags,sizeof(gHasOptionalEndTags)/sizeof(eHTMLTag_body));
}
#endif
