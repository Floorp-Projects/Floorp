/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
       
  
#include "nsDTDUtils.h" 
#include "CNavDTD.h" 
#include "nsIParserNode.h" 
#include "nsParserNode.h" 
#include "nsIChannel.h"
#include "nsIServiceManager.h"

MOZ_DECL_CTOR_COUNTER(nsEntryStack)
MOZ_DECL_CTOR_COUNTER(nsDTDContext)
MOZ_DECL_CTOR_COUNTER(nsTokenAllocator)
MOZ_DECL_CTOR_COUNTER(CNodeRecycler)
 
/**************************************************************************************
  A few notes about how residual style handling is performed:
   
    1. The style stack contains nsTagEntry elements. 
    2. Every tag on the containment stack can have it's own residual style stack.
    3. When a style leaks, it's mParent member is set to the level on the stack where 
       it originated. A node with an mParent of 0 is not opened on tag stack, 
       but is open on stylestack.
    4. An easy way to tell that a container on the element stack is a residual style tag
       is that it's use count is >1.

 **************************************************************************************/


/**
 * Default constructor
 * @update	harishd 04/04/99
 * @update  gess 04/22/99
 */
nsEntryStack::nsEntryStack()  {

  MOZ_COUNT_CTOR(nsEntryStack);
  
  mCapacity=0;
  mCount=0;
  mEntries=0;
}

/**
 * Default destructor
 * @update  harishd 04/04/99
 * @update  gess 04/22/99
 */
nsEntryStack::~nsEntryStack() {

  MOZ_COUNT_DTOR(nsEntryStack);

  if(mEntries) {
    //add code here to recycle the node if you have one...  
    delete [] mEntries;
    mEntries=0;
  }

  mCount=mCapacity=0;
}

/**
 * Release all objects in the entry stack
 */
void 
nsEntryStack::ReleaseAll(nsNodeAllocator* aNodeAllocator)
{
  NS_WARN_IF_FALSE(aNodeAllocator,"no allocator? - potential leak!");

  if(aNodeAllocator) {
    NS_WARN_IF_FALSE(mCount >= 0,"count should not be negative");
    while(mCount > 0) {
      nsCParserNode* node=this->Pop();
      IF_FREE(node,aNodeAllocator);
    }
  }
}

/**
 * Resets state of stack to be empty.
 * @update harishd 04/04/99
 */
void nsEntryStack::Empty(void) {
  mCount=0;
}


/**
 * 
 * @update  gess 04/22/99
 */
void nsEntryStack::EnsureCapacityFor(PRInt32 aNewMax,PRInt32 aShiftOffset) {
  if(mCapacity<aNewMax){ 

    const int kDelta=16;

    PRInt32 theSize = kDelta * ((aNewMax / kDelta) + 1);
    nsTagEntry* temp=new nsTagEntry[theSize]; 
    mCapacity=theSize;

    if(temp){ 
      PRInt32 index=0; 
      for(index=0;index<mCount;index++) {
        temp[aShiftOffset+index]=mEntries[index];
      }
      if(mEntries) delete [] mEntries;
      mEntries=temp;
    }
    else{
      //XXX HACK! This is very bad! We failed to get memory.
    }
  } //if
}

/**
 * 
 * @update  gess 04/22/99
 */
void nsEntryStack::Push(const nsCParserNode* aNode,nsEntryStack* aStyleStack) {
  if(aNode) {

    EnsureCapacityFor(mCount+1);

    ((nsCParserNode*)aNode)->mUseCount++;

    mEntries[mCount].mTag=(eHTMLTags)aNode->GetNodeType();
    mEntries[mCount].mNode=NS_CONST_CAST(nsCParserNode*,aNode);
    
    IF_HOLD(mEntries[mCount].mNode);
    
    mEntries[mCount].mParent=aStyleStack;
    mEntries[mCount++].mStyles=0;
  }
}


/**
 * This method inserts the given node onto the front of this stack
 *
 * @update  gess 11/10/99
 */
void nsEntryStack::PushFront(const nsCParserNode* aNode,nsEntryStack* aStyleStack) {
  if(aNode) {

    if(mCount<mCapacity) {
      PRInt32 index=0; 
      for(index=mCount;index>0;index--) {
        mEntries[index]=mEntries[index-1];
      }
    }
    else EnsureCapacityFor(mCount+1,1);


    ((nsCParserNode*)aNode)->mUseCount++;

    mEntries[0].mTag=(eHTMLTags)aNode->GetNodeType();
    mEntries[0].mNode=NS_CONST_CAST(nsCParserNode*,aNode);
    
    IF_HOLD(mEntries[0].mNode);
    
    mEntries[0].mParent=aStyleStack;
    mEntries[0].mStyles=0;
    mCount++;
  }
}

/**
 * 
 * @update  gess 11/10/99
 */
void nsEntryStack::Append(nsEntryStack *aStack) {
  if(aStack) {

    PRInt32 theCount=aStack->mCount;

    EnsureCapacityFor(mCount+aStack->mCount,0);

    PRInt32 theIndex=0;
    for(theIndex=0;theIndex<theCount;theIndex++){
      mEntries[mCount]=aStack->mEntries[theIndex];
      mEntries[mCount++].mParent=0;
    }
  }
} 
 
/**
 * This method removes the node for the given tag
 * from anywhere within this entry stack, and shifts
 * other entries down.
 * 
 * NOTE: It's odd to be removing an element from the middle
 *       of a stack, but it's necessary because of how MALFORMED
 *       html can be. 
 * 
 * anIndex: the index within the stack of the tag to be removed
 * aTag: the id of the tag to be removed
 * @update  gess 02/25/00
 */
nsCParserNode* nsEntryStack::Remove(PRInt32 anIndex,eHTMLTags aTag) {
  nsCParserNode* result=0;

  if((0<mCount) && (anIndex<mCount)){
    result=mEntries[anIndex].mNode;

    ((nsCParserNode*)result)->mUseCount--;

    PRInt32 theIndex=0;
    mCount-=1;
    for(theIndex=anIndex;theIndex<mCount;theIndex++){
      mEntries[theIndex]=mEntries[theIndex+1];
    }

    mEntries[mCount].mNode=0;
    mEntries[mCount].mStyles=0;

    nsEntryStack* theStyleStack=mEntries[anIndex].mParent;

    if(theStyleStack) {
      //now we have to tell the residual style stack where this tag
      //originated that it's no longer in use.
      PRUint32 scount=theStyleStack->mCount;
      PRUint32 sindex=0;

      nsTagEntry *theStyleEntry=theStyleStack->mEntries;
      for(sindex=scount-1;sindex>0;sindex--){            
        if(theStyleEntry->mTag==aTag) {
          theStyleEntry->mParent=0;  //this tells us that the style is not open at any level
          break;
        }
        theStyleEntry++;
      } //for
    }
  }
  return result;
}

/**
 * 
 * @update  harishd 04/04/99
 * @update  gess 04/21/99
 */
nsCParserNode* nsEntryStack::Pop(void) {

  nsCParserNode* result=0;

  if(0<mCount) {
    result=mEntries[--mCount].mNode;

    ((nsCParserNode*)result)->mUseCount--;

    mEntries[mCount].mNode=0;
    mEntries[mCount].mStyles=0;

    nsEntryStack* theStyleStack=mEntries[mCount].mParent;

    if(theStyleStack) {
      //now we have to tell the residual style stack where this tag
      //originated that it's no longer in use.
      PRUint32 scount=theStyleStack->mCount;
      PRUint32 sindex=0;

      nsTagEntry *theStyleEntry=theStyleStack->mEntries;
      for(sindex=scount-1;sindex>0;sindex--){            
        if(theStyleEntry->mTag==mEntries[mCount].mTag) {
          theStyleEntry->mParent=0;  //this tells us that the style is not open at any level
          break;
        }
        theStyleEntry++;
      } //for
    }
  }
  return result;
} 

/**
 * 
 * @update  harishd 04/04/99
 * @update  gess 04/21/99
 */
eHTMLTags nsEntryStack::First() const {
  eHTMLTags result=eHTMLTag_unknown;
  if(0<mCount){
    result=mEntries[0].mTag;
  }
  return result;
}

/**
 * 
 * @update  harishd 04/04/99
 * @update  gess 04/21/99
 */
nsCParserNode* nsEntryStack::NodeAt(PRInt32 anIndex) const {
  nsCParserNode* result=0;
  if((0<mCount) && (anIndex<mCount)) {
    result=mEntries[anIndex].mNode;
  }
  return result;
}

/**
 * 
 * @update  harishd 04/04/99
 * @update  gess 04/21/99
 */
eHTMLTags nsEntryStack::TagAt(PRInt32 anIndex) const {
  eHTMLTags result=eHTMLTag_unknown;
  if((0<mCount) && (anIndex<mCount)) {
    result=mEntries[anIndex].mTag;
  }
  return result;
}

/**
 * 
 * @update  gess 04/21/99
 */
nsTagEntry* nsEntryStack::EntryAt(PRInt32 anIndex) const {
  nsTagEntry *result=0;
  if((0<mCount) && (anIndex<mCount)) {
    result=&mEntries[anIndex];
  }
  return result;
}


/**
 * 
 * @update  harishd 04/04/99
 * @update  gess 04/21/99
 */
eHTMLTags nsEntryStack::operator[](PRInt32 anIndex) const {
  eHTMLTags result=eHTMLTag_unknown;
  if((0<mCount) && (anIndex<mCount)) {
    result=mEntries[anIndex].mTag;
  }
  return result;
}


/**
 * 
 * @update  harishd 04/04/99
 * @update  gess 04/21/99
 */
eHTMLTags nsEntryStack::Last() const {
  eHTMLTags result=eHTMLTag_unknown;
  if(0<mCount) {
    result=mEntries[mCount-1].mTag;
  }
  return result;
}

/***************************************************************
  Now define the dtdcontext class
 ***************************************************************/


/**
 * 
 * @update	gess 04.21.2000
 */
nsDTDContext::nsDTDContext() : mStack(), mEntities(0){

  MOZ_COUNT_CTOR(nsDTDContext);
  mResidualStyleCount=0;
  mContextTopIndex=-1;
  mTableStates=0;
  mCounters=0;
  mTokenAllocator=0;
  mNodeAllocator=0;
  mAllBits=0;

#ifdef  NS_DEBUG
  memset(mXTags,0,sizeof(mXTags));
#endif
} 
 


class CEntityDeallocator: public nsDequeFunctor{
public:
  virtual void* operator()(void* anObject) {
    CNamedEntity *theEntity=(CNamedEntity*)anObject;
    delete theEntity;
    return 0;
  }
};


/**
 * 
 * @update	gess9/10/98
 */
nsDTDContext::~nsDTDContext() {
  MOZ_COUNT_DTOR(nsDTDContext);

  while(mTableStates) {
    //pop the current state and restore it's predecessor, if any...
    CTableState *theState=mTableStates;      
    mTableStates=theState->mPrevious;
    delete theState;
  } 

  CEntityDeallocator theDeallocator;
  mEntities.ForEach(theDeallocator);
  if(mCounters) {
    delete [] mCounters;
    mCounters = 0;
  }
}


CNamedEntity* nsDTDContext::GetEntity(const nsString& aName)const {
  PRInt32 theCount=mEntities.GetSize();
  PRInt32 theIndex=0;

  PRInt32 theLen=aName.Length();
  PRUnichar theChar=aName.Last();

  if(theLen>2) {
    if(kSemicolon==theChar) {
      theLen--;
    }

    const PRUnichar *theBuf=aName.get();
    if(kQuote==theBuf[0]) {
      theBuf++;
      theLen--;
    }
    if(kQuote==theChar) {
      theLen--;
    }
  
    for(theIndex=0;theIndex<theCount;theIndex++) {
      CNamedEntity *theResult=(CNamedEntity*)mEntities.ObjectAt(theIndex);
      if(theResult && theResult->mName.EqualsWithConversion(theBuf,PR_TRUE,theLen)){
        return theResult;
      }
    }
  }
  return 0;
}

CNamedEntity*  nsDTDContext::RegisterEntity(const nsString& aName,const nsString& aValue) {
  CNamedEntity *theEntity=GetEntity(aName);
  if(!GetEntity(aName)){
    theEntity=new CNamedEntity(aName,aValue);
    mEntities.Push(theEntity);
  }
  return theEntity;
}


/****************************************************************
  The abacus class is useful today for debug purposes, but it
  will eventually serve as the implementation for css counters.

  This implementation is fine for static documents, but woefully
  inadequate for dynamic documents. (This about what happens if
  someone inserts a new counter using the DOM? -- The other
  numbers in that "group" should be renumbered.)

  In order to be dynamic, we need a counter "group" manager that
  is aware of layout (geometry at least) -- and that has a 
  mechanism for notifying markers that need to be updated, along
  with the ability to cause incremental reflow to occur in a
  localized context (so the counters display correctly).

 ****************************************************************/

class CAbacus {
public:

  enum eNumFormat {eUnknown,eAlpha,eDecimal,eRoman,eSpelled,eHex,eBinary,eFootnote,eUserSeries};

  CAbacus(PRInt32 aDefaultValue=0,eNumFormat aFormat=eDecimal) {
    mUserSeries=0;
    mFormat=aFormat;
    mCase=PR_FALSE;
    mValue=0;
    mUserBase=0;
  }

  ~CAbacus() {
  }

  void  SetValue(int aStartValue) {mValue=aStartValue;}
  void  SetNumberingStyle(eNumFormat aFormat) {mFormat=aFormat;}
  void  SetUserSeries(const char* aSeries,int aUserBase) {mUserSeries=aSeries; mUserBase=aUserBase;}
  void  SetCase(PRBool alwaysUpper) {mCase=alwaysUpper;}

  void  GetNextValueAsString(nsString& aString) {
    GetFormattedString(mFormat,mValue++,aString,mUserSeries,0,mUserBase);
  }

  void  GetValueAsString(nsString& aString) {
    GetFormattedString(mFormat,mValue,aString,mUserSeries,0,mUserBase);
  }


  /**
   * Get a counter string in the given style for the given value.
   * 
   * @update	rickg 6June2000
   * 
   * @param   aFormat -- format of choice
   * @param   aValue  -- cardinal value of string
   * @param   aString -- will hold result
   */
  static void  GetFormattedString(eNumFormat aFormat,PRInt32 aValue, nsString& aString,const char* aCharSet, int anOffset, int aBase) {
	  switch (aFormat) {
		  case	eDecimal:	    DecimalString(aValue,aString);  break;
		  case	eHex:			    HexString(aValue,aString);      break;
		  case	eBinary:		  BinaryString(aValue,aString);   break;
		  case	eAlpha:		    AlphaString(aValue,aString);    break;
		  case	eSpelled:		  SpelledString(aValue,aString);  break;
		  case	eRoman:		    RomanString(aValue,aString);    break;
		  case	eFootnote:	  FootnoteString(aValue,aString); break;
		  case	eUserSeries:	SeriesString(aValue,aString,aCharSet,anOffset,aBase); break;
      default:
        DecimalString(aValue,aString);  break;
	  }
  }

  /**
   * Compute a counter string in the casted-series style for the given value.
   * 
   * @update	rickg 6June2000
   * 
   * @param   aValue  -- cardinal value of string
   * @param   aString -- will hold result
   */
  static void  SeriesString(PRInt32 aValue,nsString& aString,const char* aCharSet, int offset, int base) {
	  int	ndex=0;
	  int	root=1;
	  int	next=base;
	  int	expn=1;

    aString.Truncate();
    if(aValue<0)
      aString.AppendWithConversion('-');

	  aValue=abs(aValue);	  // must be positive here...
    while(next<=aValue)	{	// scale up in baseN; exceed current value.
		  root=next;
		  next*=base;
		  expn++;
	  }
		    
	  while(expn--) {
		  ndex = ((root<=aValue) && (root)) ? (aValue/root): 0;
		  aValue%=root;
      aString.AppendWithConversion(aCharSet[ndex+((root>1)*offset)]);
		  root/=base;
    }
  }

  /**
   * Compute a counter string in the spelled style for the given value.
   * 
   * @update	rickg 6June2000
   * 
   * @param   aValue  -- cardinal value of string
   * @param   aString -- will hold result
   */
  static void SpelledString(PRInt32 aValue,nsString& aString) {

    static	char	ones[][12]=   {"zero","one ","two ","three ","four ","five ","six ","seven ","eight ","nine ","ten "};
    static	char	teens[][12]=	{"ten ","eleven ","twelve ","thirteen ","fourteen ","fifteen ","sixteen ","seventeen ","eighteen ","nineteen "};
    static	char	tens[][12]=   {"","ten ","twenty ","thirty ","fourty ","fifty ","sixty ","seventy ","eighty ","ninety ","hundred "};
    static	char	bases[][20]=  {"","hundred ","thousand ","million ","billion ","trillion ","quadrillion ","quintillion ","bajillion "};

    aString.Truncate();
    if(aValue<0)
      aString.AppendWithConversion('-');

	  PRInt32	root=1000000000;
	  PRInt32	expn=4;
	  PRInt32	modu=0;
    
	  aValue=abs(aValue);
    if(0<aValue) {      

	    while(root && aValue) {		
        PRInt32 temp=aValue/root;
		    if(temp) {
          PRInt32 theDiv=temp/100;
          if (theDiv) {//start with hundreds part
            aString.AppendWithConversion(ones[theDiv]);
            aString.AppendWithConversion(bases[1]);
		      }
			    modu=(temp%10);
          theDiv=(temp%100)/10;
          if (theDiv) {
				    if (theDiv<2) {
              aString.AppendWithConversion(teens[modu]);
						  modu=0;
					  }
            else aString.AppendWithConversion(tens[theDiv]);
          }
			    if (modu) 
            aString.AppendWithConversion(ones[modu]); //do remainder
			    aValue-=(temp*root);
			    if (expn>1)	
            aString.AppendWithConversion(bases[expn]);
		    }
		    expn--;
		    root/=1000;
	    }
    }
    else aString.AppendWithConversion(ones[0]);
  }

  /**
   * Compute a counter string in the decimal format for the given value.
   * 
   * @update	rickg 6June2000
   * 
   * @param   aValue  -- cardinal value of string
   * @param   aString -- will hold result
   */
  static void DecimalString(PRInt32 aValue,nsString& aString) {
    aString.Truncate();
    aString.AppendInt(aValue);
  }

  /**
   * Compute a counter string in binary format for the given value.
   * 
   * @update	rickg 6June2000
   * 
   * @param   aValue  -- cardinal value of string
   * @param   aString -- will hold result
   */
  static void BinaryString(PRInt32 aValue,nsString& aString) {
	  static char	kBinarySet[]="01";

	  if (aValue<0)
		  aValue=65536-abs(aValue);
    SeriesString(aValue,aString,kBinarySet,0,2);
  }

  /**
   * Compute a counter string in hex format for the given value.
   * 
   * @update	rickg 6June2000
   * 
   * @param   aValue  -- cardinal value of string
   * @param   aString -- will hold result
   */
  static void HexString(PRInt32 aValue,nsString& aString) {
    static char	kHexSet[]="0123456789ABCDEF";

	  if (aValue<0)
		  aValue=65536-abs(aValue);
    SeriesString(aValue,aString,kHexSet,0,16);
  }

  /**
   * Compute a counter string in the roman style for the given value.
   * 
   * @update	rickg 6June2000
   * 
   * @param   aValue  -- cardinal value of string
   * @param   aString -- will hold result
   */
  static void RomanString(PRInt32 aValue,nsString& aString) {
	  static char digitsA[] = "ixcm";
	  static char digitsB[] = "vld?";

    aString.Truncate();
    if(aValue<0)
      aString.AppendWithConversion('-');

    aValue=abs(aValue);
	  char	decStr[20];
	  sprintf(decStr,"%d", aValue);

	  int len=strlen(decStr);
	  int romanPos=len;
    int digitPos=0;
    int n=0;

	  for(digitPos=0;digitPos<len;digitPos++) {
		  romanPos--;
		  switch(decStr[digitPos]) {
			  case	'0':	break;
			  case	'3':	aString.AppendWithConversion(digitsA[romanPos]);
			  case	'2':	aString.AppendWithConversion(digitsA[romanPos]);
			  case	'1':	aString.AppendWithConversion(digitsA[romanPos]);
				  break;
			  case	'4':	aString.AppendWithConversion(digitsA[romanPos]);
			  case 	'5':	case	'6':
			  case	'7':	case 	'8':	
          aString.AppendWithConversion(digitsB[romanPos]);
					for(n=0;n<(decStr[digitPos]-'5');n++)
            aString.AppendWithConversion(digitsA[romanPos]);
				  break;
			  case	'9':	          
          aString.AppendWithConversion(digitsA[romanPos]);
          aString.AppendWithConversion(digitsA[romanPos]);
					break;
		  }
	  }
  }

  /**
   * Compute a counter string in the alpha style for the given value.
   * 
   * @update	rickg 6June2000
   * 
   * @param   aValue  -- cardinal value of string
   * @param   aString -- will hold result
   */
  static void AlphaString(PRInt32 aValue,nsString& aString) {
	  static const char kAlphaSet[]="abcdefghijklmnopqrstuvwxyz";
	  
	  if (0<aValue)
      SeriesString(aValue-1,aString,kAlphaSet,-1,26);    
  }

  /**
   * Compute a counter string in the footnote style for the given value.
   * 
   * @update	rickg 6June2000
   * 
   * @param   aValue  -- cardinal value of string
   * @param   aString -- will hold result
   */
  static void FootnoteString(PRInt32 aValue,nsString& aString) {
    static char	kFootnoteSet[]="abcdefg";

	  int			seriesLen = strlen (kFootnoteSet) - 1;
	  int			count=0;
    int     repCount=0;
	  int			modChar=0;
	  
    aString.Truncate();

    aValue=abs(aValue);
	  repCount=((aValue-1)/seriesLen);
	  modChar=aValue-(repCount*seriesLen);
	  
    for(count=0;count<=repCount;count++) {
      aString.AppendWithConversion(kFootnoteSet[modChar]);
    }
  }

protected:

  const char* mUserSeries;
  eNumFormat  mFormat;
  PRBool      mCase;
  PRInt32     mValue;
  int         mUserBase;
};


/**
 * 
 * @update	gess 11May2000
 */
void nsDTDContext::AllocateCounters(void) {
  if(!mCounters) {
    mCounters = new PRInt32 [NS_HTML_TAG_MAX]; //in addition to reseting, you may need to allocate.
    ResetCounters();
  }
}

/**
 * 
 * @update	gess 11May2000
 */
void nsDTDContext::ResetCounters(void) {
  if(mCounters) {
    memset(mCounters,0,NS_HTML_TAG_MAX*sizeof(PRInt32));
  }
}

/**********************************************************
  @update: rickg  17May2000

  Call this to handle counter attributes:
    name="group"
    value="nnn"
    noincr="?"
    format="alpha|dec|footnote|hex|roman|spelled|talk"

  returns the newly incremented value for the (determined) group.
 **********************************************************/
PRInt32 nsDTDContext::IncrementCounter(eHTMLTags aTag,nsIParserNode& aNode,nsString& aResult) {

  PRInt32     result=0;

  PRInt32       theIndex=0;
  PRInt32       theNewValue=-1; //-1 is interpreted to mean "don't reset the counter sequence.
  PRInt32       theIncrValue=1; //this may get set to 0 if we see a "noincr" key.
  PRInt32       theCount=aNode.GetAttributeCount();
  CNamedEntity *theEntity=0;

  CAbacus::eNumFormat  theNumFormat=CAbacus::eDecimal;

  for(theIndex=0;theIndex<theCount;theIndex++){
    nsAutoString theKey(aNode.GetKeyAt(theIndex));
    const nsString& theValue=aNode.GetValueAt(theIndex);

    if(theKey.EqualsWithConversion("name",PR_TRUE)){
      theEntity=GetEntity(theValue);
      if(!theEntity) {
        theEntity=RegisterEntity(theValue,theValue);
        theEntity->mOrdinal=0;
      }
      aTag=eHTMLTag_userdefined;
    }
    else if(theKey.EqualsWithConversion("noincr",PR_TRUE)){
      theIncrValue=0;
    }
    else if(theKey.EqualsWithConversion("format",PR_TRUE)){
      PRUnichar theChar=theValue.CharAt(0);
      if('"'==theChar)
        theChar=theValue.CharAt(1);
      switch(theChar){
        case 'A': case 'a': theNumFormat=CAbacus::eAlpha;   break;
        case 'B': case 'b': theNumFormat=CAbacus::eBinary;  break;
        case 'D': case 'd': theNumFormat=CAbacus::eDecimal; break;
        case 'H': case 'h': theNumFormat=CAbacus::eHex;     break;
        case 'R': case 'r': theNumFormat=CAbacus::eRoman;   break;
        case 'S': case 's': theNumFormat=CAbacus::eSpelled; break;
        default:
          theNumFormat=CAbacus::eDecimal;
          break;
      }
      //determine numbering style
    }
    else if(theKey.EqualsWithConversion("value",PR_TRUE)){
      PRInt32 err=0;
      theNewValue=theValue.ToInteger(&err);
      if(!err) {

        theIncrValue=0;

        AllocateCounters();
        if(mCounters) {
          mCounters[aTag]=theNewValue;
        }
      }
      else theNewValue=-1;
    }
  }

  if(theEntity && (eHTMLTag_userdefined==aTag)) {
    result=theEntity->mOrdinal+=theIncrValue;
  }
  else {
    AllocateCounters();
    if(mCounters) {
      result=mCounters[aTag]+=theIncrValue;
    }
    else result=0;
  }
  CAbacus::GetFormattedString(theNumFormat,result,aResult,0,0,0);

  return result;
}


/**
 * 
 * @update  gess7/9/98
 */
PRBool nsDTDContext::HasOpenContainer(eHTMLTags aTag) const {
  PRInt32 theIndex=mStack.LastOf(aTag);
  return PRBool(-1<theIndex);
}

/**
 * 
 * @update  gess7/9/98
 */
void nsDTDContext::Push(const nsCParserNode* aNode,nsEntryStack* aStyleStack) {
  if(aNode) {

#ifdef  NS_DEBUG
    eHTMLTags theTag=(eHTMLTags)aNode->GetNodeType();
    int size=mStack.mCount;
    if(size< eMaxTags)
      mXTags[size]=theTag;
#endif
    mStack.Push(aNode,aStyleStack);
  }
}

/** 
 * @update  gess 11/11/99, 
 *          harishd 04/04/99
 */
nsCParserNode* nsDTDContext::Pop(nsEntryStack *&aChildStyleStack) {

  PRInt32         theSize=mStack.mCount;
  nsCParserNode*  result=0;

  if(0<theSize) {

#ifdef  NS_DEBUG
    if ((theSize>0) && (theSize <= eMaxTags))
      mXTags[theSize-1]=eHTMLTag_unknown;
#endif


    nsTagEntry* theEntry=mStack.EntryAt(mStack.mCount-1);
    if(theEntry) {
      aChildStyleStack=theEntry->mStyles;
    }

    result=mStack.Pop();
    theEntry->mParent=0;
  }

  return result;
}
 
/**
 * 
 * @update  harishd 04/07/00
 */

nsCParserNode* nsDTDContext::Pop() {
  nsEntryStack   *theTempStyleStack=0; // This has no use here...
  return Pop(theTempStyleStack);
}

/**
 * 
 * @update  gess7/9/98
 */
eHTMLTags nsDTDContext::First(void) const {
  return mStack.First();
}

/**
 * 
 * @update  gess7/9/98
 */
eHTMLTags nsDTDContext::TagAt(PRInt32 anIndex) const {
  return mStack.TagAt(anIndex);
}


/**
 * 
 * @update  gess7/9/98
 */
nsTagEntry* nsDTDContext::LastEntry(void) const {
  return mStack.EntryAt(mStack.mCount-1);
}

/**
 *  
 * @update  gess7/9/98
 */
eHTMLTags nsDTDContext::Last() const {
  return mStack.Last();
}


/**
 * 
 * @update  gess7/9/98
 */
nsEntryStack* nsDTDContext::GetStylesAt(PRInt32 anIndex) const {
  nsEntryStack* result=0;

  if(anIndex<mStack.mCount){
    nsTagEntry* theEntry=mStack.EntryAt(anIndex);
    if(theEntry) {
      result=theEntry->mStyles;
    }
  }
  return result;
}


/**
 * 
 * @update  gess 04/28/99
 */
void nsDTDContext::PushStyle(const nsCParserNode* aNode){

  nsTagEntry* theEntry=mStack.EntryAt(mStack.mCount-1);
  if(theEntry ) {
    nsEntryStack* theStack=theEntry->mStyles;
    if(!theStack) {
      theStack=theEntry->mStyles=new nsEntryStack();
    }
    if(theStack) {
      theStack->Push(aNode);
      mResidualStyleCount++;
    }
  } //if
}


/**
 * Call this when you have an EntryStack full of styles
 * that you want to push at this level.
 * 
 * @update  gess 04/28/99
 */
void nsDTDContext::PushStyles(nsEntryStack *aStyles){

  if(aStyles) {
    nsTagEntry* theEntry=mStack.EntryAt(mStack.mCount-1);
    if(theEntry ) {
      nsEntryStack* theStyles=theEntry->mStyles;
      if(!theStyles) {
        theEntry->mStyles=aStyles;

        PRUint32 scount=aStyles->mCount;
        PRUint32 sindex=0;

        theEntry=aStyles->mEntries;
        for(sindex=0;sindex<scount;sindex++){            
          theEntry->mParent=0;  //this tells us that the style is not open at any level
          theEntry++;
          mResidualStyleCount++;
        } //for

      }
      else {
        theStyles->Append(aStyles);
        //  Delete aStyles since it has been copied to theStyles...
        delete aStyles;
        aStyles=0;
      }
    } //if(theEntry )
    else if(mStack.mCount==0) {
      // If you're here it means that we have hit the rock bottom
      // ,of the stack, and there's no need to handle anymore styles.
      // Fix for bug 29048
      IF_DELETE(aStyles,mNodeAllocator);
    }
  }//if(aStyles)
}


/** 
 * 
 * @update  gess 04/28/99
 */
nsCParserNode* nsDTDContext::PopStyle(void){
  nsCParserNode *result=0;

  nsTagEntry *theEntry=mStack.EntryAt(mStack.mCount-1);
  if(theEntry && (theEntry->mNode)) {
    nsEntryStack* theStyleStack=theEntry->mParent;
    if(theStyleStack){
      result=theStyleStack->Pop();
      mResidualStyleCount--;
    }
  } //if
  return result;
}

/** 
 * 
 * @update  gess 04/28/99
 */
nsCParserNode* nsDTDContext::PopStyle(eHTMLTags aTag){

  PRInt32 theLevel=0;
  nsCParserNode* result=0;

  for(theLevel=mStack.mCount-1;theLevel>0;theLevel--) {
    nsEntryStack *theStack=mStack.mEntries[theLevel].mStyles;
    if(theStack) {
      if(aTag==theStack->Last()) {
        result=theStack->Pop();
        mResidualStyleCount--;
        break; // Fix bug 50710 - Stop after finding a style.
      } else {
        // NS_ERROR("bad residual style entry");
      }
    } 
  }

  return result;
}

/** 
 * 
 * This is similar to popstyle, except that it removes the
 * style tag given from anywhere in the style stack, and
 * not just at the top.
 *
 * @update  gess 01/26/00
 */
void nsDTDContext::RemoveStyle(eHTMLTags aTag){
  
  PRInt32 theLevel=mStack.mCount;
  
  while (theLevel) {
    nsEntryStack *theStack=GetStylesAt(--theLevel);
    if (theStack) {
      PRInt32 index=theStack->mCount;
      while (index){
        nsTagEntry *theEntry=theStack->EntryAt(--index);
        if (aTag==(eHTMLTags)theEntry->mNode->GetNodeType()) {
          mResidualStyleCount--;
          nsCParserNode* result=theStack->Remove(index,aTag);
          IF_FREE(result, mNodeAllocator);  
          return;
        } 
      }
    } 
  }
}

/**
 * This gets called when the parser module is getting unloaded
 * 
 * @return  nada
 */
void nsDTDContext::ReleaseGlobalObjects(){
}


/**************************************************************
  Now define the nsTokenAllocator class...
 **************************************************************/

static const size_t  kTokenBuckets[]       ={sizeof(CStartToken),sizeof(CAttributeToken),sizeof(CCommentToken),sizeof(CEndToken)};
static const PRInt32 kNumTokenBuckets      = sizeof(kTokenBuckets) / sizeof(size_t);
static const PRInt32 kInitialTokenPoolSize = NS_SIZE_IN_HEAP(sizeof(CToken)) * 200;

/**
 * 
 * @update  gess7/25/98
 * @param 
 */
nsTokenAllocator::nsTokenAllocator() {

  MOZ_COUNT_CTOR(nsTokenAllocator);

  mArenaPool.Init("TokenPool", kTokenBuckets, kNumTokenBuckets, kInitialTokenPoolSize);

#ifdef NS_DEBUG
  int i=0;
  for(i=0;i<eToken_last-1;i++) {
    mTotals[i]=0;
  }
#endif

}

/**
 * Destructor for the token factory
 * @update  gess7/25/98
 */
nsTokenAllocator::~nsTokenAllocator() {

  MOZ_COUNT_DTOR(nsTokenAllocator);

}

class CTokenFinder: public nsDequeFunctor{
public:
  CTokenFinder(CToken* aToken) {mToken=aToken;}
  virtual void* operator()(void* anObject) {
    if(anObject==mToken) {
      return anObject;
    }
    return 0;
  }
  CToken* mToken;
};

/**
 * Let's get this code ready to be reused by all the contexts.
 * 
 * @update	rickg 12June2000
 * @param   aType -- tells you the type of token to create
 * @param   aTag  -- tells you the type of tag to init with this token
 * @param   aString -- gives a default string value for the token
 *
 * @return  ptr to new token (or 0).
 */
CToken* nsTokenAllocator::CreateTokenOfType(eHTMLTokenTypes aType,eHTMLTags aTag, const nsAReadableString& aString) {

  CToken* result=0;

#ifdef  NS_DEBUG
    mTotals[aType-1]++;
#endif
  switch(aType){
    case eToken_start:            result=new(mArenaPool) CStartToken(aString, aTag); break;
    case eToken_end:              result=new(mArenaPool) CEndToken(aString, aTag); break;
    case eToken_comment:          result=new(mArenaPool) CCommentToken(aString); break;
    case eToken_entity:           result=new(mArenaPool) CEntityToken(aString); break;
    case eToken_whitespace:       result=new(mArenaPool) CWhitespaceToken(aString); break;
    case eToken_newline:          result=new(mArenaPool) CNewlineToken(); break;
    case eToken_text:             result=new(mArenaPool) CTextToken(aString); break;
    case eToken_attribute:        result=new(mArenaPool) CAttributeToken(aString); break;
    case eToken_script:           result=new(mArenaPool) CScriptToken(aString); break;
    case eToken_style:            result=new(mArenaPool) CStyleToken(aString); break;
    case eToken_instruction:      result=new(mArenaPool) CInstructionToken(aString); break;
    case eToken_cdatasection:     result=new(mArenaPool) CCDATASectionToken(aString); break;
    case eToken_error:            result=new(mArenaPool) CErrorToken(); break;
    case eToken_doctypeDecl:      result=new(mArenaPool) CDoctypeDeclToken(aString); break;
    case eToken_markupDecl:       result=new(mArenaPool) CMarkupDeclToken(aString); break;
      default:
        NS_ASSERTION(PR_FALSE, "nsDTDUtils::CreateTokenOfType: illegal token type"); 
        break;
  }

  return result;
}

/**
 * Let's get this code ready to be reused by all the contexts.
 * 
 * @update	rickg 12June2000
 * @param   aType -- tells you the type of token to create
 * @param   aTag  -- tells you the type of tag to init with this token
 *
 * @return  ptr to new token (or 0).
 */
CToken* nsTokenAllocator::CreateTokenOfType(eHTMLTokenTypes aType,eHTMLTags aTag) {

  CToken* result=0;

#ifdef  NS_DEBUG
    mTotals[aType-1]++;
#endif
  switch(aType){
    case eToken_start:            result=new(mArenaPool) CStartToken(aTag); break;
    case eToken_end:              result=new(mArenaPool) CEndToken(aTag); break;
    case eToken_comment:          result=new(mArenaPool) CCommentToken(); break;
    case eToken_attribute:        result=new(mArenaPool) CAttributeToken(); break;
    case eToken_entity:           result=new(mArenaPool) CEntityToken(); break;
    case eToken_whitespace:       result=new(mArenaPool) CWhitespaceToken(); break;
    case eToken_newline:          result=new(mArenaPool) CNewlineToken(); break;
    case eToken_text:             result=new(mArenaPool) CTextToken(); break;
    case eToken_script:           result=new(mArenaPool) CScriptToken(); break;
    case eToken_style:            result=new(mArenaPool) CStyleToken(); break;
    case eToken_instruction:      result=new(mArenaPool) CInstructionToken(); break;
    case eToken_cdatasection:     result=new(mArenaPool) CCDATASectionToken(); break;
    case eToken_error:            result=new(mArenaPool) CErrorToken(); break;
    case eToken_doctypeDecl:      result=new(mArenaPool) CDoctypeDeclToken(aTag); break;
    case eToken_markupDecl:       result=new(mArenaPool) CMarkupDeclToken(); break;
    default:
      NS_ASSERTION(PR_FALSE, "nsDTDUtils::CreateTokenOfType: illegal token type"); 
      break;
   }

  return result;
}

#ifdef DEBUG_TRACK_NODES 

static nsCParserNode* gAllNodes[100];
static int gAllNodeCount=0;

int FindNode(nsCParserNode *aNode) {
  int theIndex=0;
  for(theIndex=0;theIndex<gAllNodeCount;theIndex++) {
    if(gAllNodes[theIndex]==aNode) {
      return theIndex;
    }
  }
  return -1;
}

void AddNode(nsCParserNode *aNode) {
  if(-1==FindNode(aNode)) {
    gAllNodes[gAllNodeCount++]=aNode;
  }
  else {
    //you tried to recycle a node twice!
  }
}

void RemoveNode(nsCParserNode *aNode) {
  int theIndex=FindNode(aNode);
  if(-1<theIndex) {
    gAllNodes[theIndex]=gAllNodes[--gAllNodeCount];
  }
}

#endif 


#ifdef HEAP_ALLOCATED_NODES
nsNodeAllocator::nsNodeAllocator():mSharedNodes(0){
#ifdef DEBUG_TRACK_NODES
  mCount=0;
#endif
#else 
  static const size_t  kNodeBuckets[]       ={sizeof(nsCParserNode)};
  static const PRInt32 kNumNodeBuckets      = sizeof(kNodeBuckets) / sizeof(size_t);
  static const PRInt32 kInitialNodePoolSize = NS_SIZE_IN_HEAP(sizeof(nsCParserNode)) * 50;
nsNodeAllocator::nsNodeAllocator() {
  mNodePool.Init("NodePool", kNodeBuckets, kNumNodeBuckets, kInitialNodePoolSize);
#endif
  MOZ_COUNT_CTOR(nsNodeAllocator);
}
  
nsNodeAllocator::~nsNodeAllocator() {
  MOZ_COUNT_DTOR(nsNodeAllocator);

#ifdef HEAP_ALLOCATED_NODES
  nsCParserNode* theNode=0;

  while((theNode=(nsCParserNode*)mSharedNodes.Pop())){
#ifdef DEBUG_TRACK_NODES
    RemoveNode(theNode);
#endif
    ::operator delete(theNode); 
    theNode=nsnull;
  }
#ifdef DEBUG_TRACK_NODES
  if(mCount) {
    printf("**************************\n");
    printf("%i out of %i nodes leaked!\n",gAllNodeCount,mCount);
    printf("**************************\n");
  }
#endif
#endif
}
  
nsCParserNode* nsNodeAllocator::CreateNode(CToken* aToken,PRInt32 aLineNumber,nsTokenAllocator* aTokenAllocator) {
  nsCParserNode* result=0;

#ifdef HEAP_ALLOCATED_NODES
#if 0
  if(gAllNodeCount!=mSharedNodes.GetSize()) {
    int x=10; //this is very BAD!
  }
#endif

  result=NS_STATIC_CAST(nsCParserNode*,mSharedNodes.Pop());
  if(result) {
    result->Init(aToken,aLineNumber,aTokenAllocator,this);
  }
  else{
    result=nsCParserNode::Create(aToken,aLineNumber,aTokenAllocator,this);
#ifdef DEBUG_TRACK_NODES
    mCount++;
    AddNode(NS_STATIC_CAST(nsCParserNode*,result));
#endif
    IF_HOLD(result);
  }
#else
  result=nsCParserNode::Create(aToken,aLineNumber,aTokenAllocator,this);
  IF_HOLD(result);
#endif
  return result;
}

#ifdef DEBUG
void DebugDumpContainmentRules(nsIDTD& theDTD,const char* aFilename,const char* aTitle) {
#ifdef RICKG_DEBUG

#include <fstream.h>

  const char* prefix="     ";
  fstream out(aFilename,ios::out);
  out << "==================================================" << endl;
  out << aTitle << endl;
  out << "==================================================";
  int i,j=0;
  int written;
  for(i=1;i<eHTMLTag_text;i++){
    const char* tag=nsHTMLTags::GetCStringValue((eHTMLTags)i);
    out << endl << endl << "Tag: <" << tag << ">" << endl;
    out << prefix;
    written=0;
    if(theDTD.IsContainer(i)) {
      for(j=1;j<eHTMLTag_text;j++){
        if(15==written) {
          out << endl << prefix;
          written=0;
        }
        if(theDTD.CanContain(i,j)){
          tag=nsHTMLTags::GetCStringValue((eHTMLTags)j);
          if(tag) {
            out<< tag << ", ";
            written++;
          }
        }
      }//for
    }
    else out<<"(not container)" << endl;
  }
#endif
}
#endif


/*************************************************************************
 *  The table lookup technique was adapted from the algorithm described  *
 *  by Avram Perez, Byte-wise CRC Calculations, IEEE Micro 3, 40 (1983). *
 *************************************************************************/

#define POLYNOMIAL 0x04c11db7L

static PRBool crc_table_initialized;
static PRUint32 crc_table[256];

static void gen_crc_table() {
 /* generate the table of CRC remainders for all possible bytes */
  int i, j;  
  PRUint32 crc_accum;
  for ( i = 0;  i < 256;  i++ ) { 
    crc_accum = ( (unsigned long) i << 24 );
    for ( j = 0;  j < 8;  j++ ) { 
      if ( crc_accum & 0x80000000L )
        crc_accum = ( crc_accum << 1 ) ^ POLYNOMIAL;
      else crc_accum = ( crc_accum << 1 ); 
    }
    crc_table[i] = crc_accum; 
  }
  return; 
}

PRUint32 AccumulateCRC(PRUint32 crc_accum, char *data_blk_ptr, int data_blk_size)  {
  if (!crc_table_initialized) {
    gen_crc_table();
    crc_table_initialized = PR_TRUE;
  }

 /* update the CRC on the data block one byte at a time */
  int i, j;
  for ( j = 0;  j < data_blk_size;  j++ ) { 
    i = ( (int) ( crc_accum >> 24) ^ *data_blk_ptr++ ) & 0xff;
    crc_accum = ( crc_accum << 8 ) ^ crc_table[i]; 
  }
  return crc_accum; 
}

/**************************************************************
  This defines the topic object used by the observer service.
  The observerService uses a list of these, 1 per topic when
  registering tags.
 **************************************************************/
NS_IMPL_ISUPPORTS1(nsObserverEntry, nsIObserverEntry)

nsObserverEntry::nsObserverEntry(const nsAString& aTopic) : mTopic(aTopic) 
{
  NS_INIT_ISUPPORTS();
  nsCRT::zero(mObservers,sizeof(mObservers));
}

nsObserverEntry::~nsObserverEntry() {
  for (PRInt32 i = 0; i <= NS_HTML_TAG_MAX; i++){
    if (mObservers[i]) {
      PRInt32 count = mObservers[i]->Count();
      for (PRInt32 j = 0; j < count; j++) {
        nsISupports* obs = (nsISupports*)mObservers[i]->ElementAt(j);
        NS_IF_RELEASE(obs);
      }
      delete mObservers[i];
    }
  }
}

NS_IMETHODIMP
nsObserverEntry::Notify(nsIParserNode* aNode,
                        nsIParser* aParser,
                        nsISupports* aWebShell) 
{
  NS_ENSURE_ARG_POINTER(aNode);
  NS_ENSURE_ARG_POINTER(aParser);

  nsresult result = NS_OK;
  eHTMLTags theTag = (eHTMLTags)aNode->GetNodeType();
 
  if (theTag <= NS_HTML_TAG_MAX) {
    nsVoidArray*  theObservers = mObservers[theTag];
    if (theObservers) {
      nsAutoString      theCharsetValue;
      nsCharsetSource   theCharsetSource;
      aParser->GetDocumentCharset(theCharsetValue,theCharsetSource);

      PRInt32 theAttrCount = aNode->GetAttributeCount(); 
      PRInt32 theObserversCount = theObservers->Count(); 
      if (0 < theObserversCount){
        nsStringArray keys(theAttrCount+4), values(theAttrCount+4);

        // XXX this and the following code may be a performance issue.
        // Every key and value is copied and added to an voidarray (causing at
        // least 2 allocations for mImpl, usually more, plus at least 1 per
        // string (total = 2*(keys+3) + 2(or more) array allocations )).
        PRInt32 index;
        for (index = 0; index < theAttrCount; index++) {
          keys.AppendString(aNode->GetKeyAt(index));
          values.AppendString(aNode->GetValueAt(index));
        } 

        nsAutoString intValue;

        keys.AppendString(NS_LITERAL_STRING("charset")); 
        values.AppendString(theCharsetValue);       
      
        keys.AppendString(NS_LITERAL_STRING("charsetSource")); 
        intValue.AppendInt(PRInt32(theCharsetSource),10);
        values.AppendString(intValue); 

        keys.AppendString(NS_LITERAL_STRING("X_COMMAND"));
        values.AppendString(NS_LITERAL_STRING("text/html")); 

        nsCOMPtr<nsIChannel> channel;
        aParser->GetChannel(getter_AddRefs(channel));

        for (index=0;index<theObserversCount;index++) {
          nsIElementObserver* observer = NS_STATIC_CAST(nsIElementObserver*,theObservers->ElementAt(index));
          if (observer) {
            result = observer->Notify(aWebShell, channel,
                                      NS_ConvertASCIItoUCS2(nsHTMLTags::GetStringValue(theTag)).get(),
                                      &keys, &values);
            if (NS_FAILED(result)) {
              break;
            }
          }
        } 
      } 
    }
  }
  return result;
}

PRBool 
nsObserverEntry::Matches(const nsAString& aString) {
  PRBool result = aString.Equals(mTopic);
  return result;
}

nsresult
nsObserverEntry::AddObserver(nsIElementObserver *aObserver,
                             eHTMLTags aTag) 
{
  if (aObserver) {
    if (!mObservers[aTag]) {
      mObservers[aTag] = new nsAutoVoidArray();
      if (!mObservers[aTag]) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
    NS_ADDREF(aObserver);
    mObservers[aTag]->AppendElement(aObserver);
  }
  return NS_OK;
}

void 
nsObserverEntry::RemoveObserver(nsIElementObserver *aObserver)
{
  for (PRInt32 i=0; i <= NS_HTML_TAG_MAX; i++){
    if (mObservers[i]) {
      nsISupports* obs = aObserver;
      PRBool removed = mObservers[i]->RemoveElement(obs);
      if (removed) {
        NS_RELEASE(obs);
      }
    }
  }
}
