/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */ 
       
  
#include "nsDTDUtils.h" 
#include "CNavDTD.h" 
#include "nsIParserNode.h" 
#include "nsParserNode.h" 

#include "nsIObserverService.h"
#include "nsIServiceManager.h"

MOZ_DECL_CTOR_COUNTER(nsEntryStack);
MOZ_DECL_CTOR_COUNTER(nsDTDContext);
MOZ_DECL_CTOR_COUNTER(nsTokenAllocator);
MOZ_DECL_CTOR_COUNTER(CNodeRecycler);
MOZ_DECL_CTOR_COUNTER(CObserverService); 
 

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
void nsEntryStack::Push(const nsIParserNode* aNode,nsEntryStack* aStyleStack) {
  if(aNode) {

    EnsureCapacityFor(mCount+1);

    ((nsCParserNode*)aNode)->mUseCount++;

    mEntries[mCount].mTag=(eHTMLTags)aNode->GetNodeType();
    mEntries[mCount].mNode=(nsIParserNode*)aNode;
    mEntries[mCount].mParent=aStyleStack;
    mEntries[mCount++].mStyles=0;
  }
}


/**
 * This method inserts the given node onto the front of this stack
 *
 * @update  gess 11/10/99
 */
void nsEntryStack::PushFront(const nsIParserNode* aNode,nsEntryStack* aStyleStack) {
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
    mEntries[0].mNode=(nsIParserNode*)aNode;
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
nsIParserNode* nsEntryStack::Remove(PRInt32 anIndex,eHTMLTags aTag) {
  nsIParserNode* result=0;

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
nsIParserNode* nsEntryStack::Pop(void) {

  nsIParserNode* result=0;

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
nsIParserNode* nsEntryStack::NodeAt(PRInt32 anIndex) const {
  nsIParserNode* result=0;
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

CNodeRecycler   *nsDTDContext::gNodeRecycler=0;

/**
 * 
 * @update	gess 04.21.2000
 */
nsDTDContext::nsDTDContext() : mStack(), mEntities(0){

  MOZ_COUNT_CTOR(nsDTDContext);
  mResidualStyleCount=0;
  mContextTopIndex=0;
  mTableStates=0;
  mHadBody=PR_FALSE; 
  mHadFrameset=PR_TRUE;
  mTransitional=PR_FALSE;
  ResetCounters();
  mHadDocTypeDecl=PR_FALSE;
  mHasOpenHead=PR_FALSE;
  mTokenAllocator=0;

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

    const PRUnichar *theBuf=aName.GetUnicode();
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
void nsDTDContext::ResetCounters(void) {
  memset(mCounters,0,sizeof(mCounters));
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
PRInt32 nsDTDContext::IncrementCounter(eHTMLTags aTag,nsCParserNode& aNode,nsString& aResult) {

  PRInt32     result=0;

  PRInt32       theIndex=0;
  PRInt32       theNewValue=-1; //-1 is interpreted to mean "don't reset the counter sequence.
  PRInt32       theIncrValue=1; //this may get set to 0 if we see a "noincr" key.
  PRInt32       theCount=aNode.GetAttributeCount();
  CNamedEntity *theEntity=0;

  CAbacus::eNumFormat  theNumFormat=CAbacus::eDecimal;

  for(theIndex=0;theIndex<theCount;theIndex++){
    const nsString& theKey=aNode.GetKeyAt(theIndex);
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
        mCounters[aTag]=theNewValue;
      }
      else theNewValue=-1;
    }
  }

  if(theEntity && (eHTMLTag_userdefined==aTag)) {
    result=theEntity->mOrdinal+=theIncrValue;
  }
  else result=mCounters[aTag]+=theIncrValue;
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
void nsDTDContext::Push(const nsIParserNode* aNode,nsEntryStack* aStyleStack) {
  if(aNode) {

#ifdef  NS_DEBUG
    eHTMLTags theTag=(eHTMLTags)aNode->GetNodeType();
    int size=mStack.mCount;
    if(size< eMaxTags)
      mXTags[size]=theTag;
#endif
    mStack.Push((nsIParserNode*)aNode,aStyleStack);
  }
}

/** 
 * @update  gess 11/11/99, 
 *          harishd 04/04/99
 */
nsIParserNode* nsDTDContext::Pop(nsEntryStack *&aChildStyleStack) {

  PRInt32         theSize=mStack.mCount;
  nsIParserNode*  result=0;

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

nsIParserNode* nsDTDContext::Pop() {
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
void nsDTDContext::PushStyle(const nsIParserNode* aNode){

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
      gNodeRecycler->RecycleNode((nsCParserNode*)aStyles->Pop());
      delete aStyles;
      aStyles=0;
    }
  }//if(aStyles)
}


/** 
 * 
 * @update  gess 04/28/99
 */
nsIParserNode* nsDTDContext::PopStyle(void){
  nsIParserNode *result=0;

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
nsIParserNode* nsDTDContext::PopStyle(eHTMLTags aTag){

  PRInt32 theLevel=0;
  nsIParserNode* result=0;

  for(theLevel=mStack.mCount-1;theLevel>0;theLevel--) {
    nsEntryStack *theStack=mStack.mEntries[theLevel].mStyles;
    if(theStack) {
      if(aTag==theStack->Last()) {
        result=theStack->Pop();
        mResidualStyleCount--;
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
nsIParserNode* nsDTDContext::RemoveStyle(eHTMLTags aTag){

  PRInt32 theLevel=0;
  nsIParserNode* result=0;

  for(theLevel=mStack.mCount-1;theLevel>0;theLevel--) {
    nsEntryStack *theStack=mStack.mEntries[theLevel].mStyles;
    if(theStack) {
      if(aTag==theStack->Last()) {
        result=theStack->Pop();
        mResidualStyleCount--;
      } else {
        // NS_ERROR("bad residual style entry");
      }
    } 
  }

  return result;
}

/**
 * 
 * @update  harishd 04/10/00
 */
nsresult nsDTDContext::GetNodeRecycler(CNodeRecycler*& aNodeRecycler){
  nsresult result=NS_OK;
  if(!gNodeRecycler) {
    gNodeRecycler=new CNodeRecycler();
    if(gNodeRecycler==0) result=NS_ERROR_OUT_OF_MEMORY;
  }
  aNodeRecycler=gNodeRecycler;
  return result;
}

/**
 * 
 * @update  rickg 16June2000
 */
void nsDTDContext::RecycleNode(nsCParserNode* aNode) {
  nsresult result=NS_OK;

  if(aNode) {
    if(!gNodeRecycler)
      result=nsDTDContext::GetNodeRecycler(gNodeRecycler);

    if(NS_SUCCEEDED(result)) {
      gNodeRecycler->RecycleNode(aNode);
    }
    else {
      delete aNode;
    }
  }
}


/**
 * This gets called when the parser module is getting unloaded
 * 
 * @return  nada
 */
void nsDTDContext::ReleaseGlobalObjects(){
  if(gNodeRecycler) {
    delete gNodeRecycler;
    gNodeRecycler=0;
  }
}


/**************************************************************
  Now define the nsTokenAllocator class...
 **************************************************************/

static const size_t kBucketSizes[]    ={sizeof(CStartToken),sizeof(CAttributeToken),sizeof(CCommentToken),sizeof(CEndToken)};
static const PRInt32 kNumBuckets      = sizeof(kBucketSizes) / sizeof(size_t);
static const PRInt32 kInitialPoolSize = NS_SIZE_IN_HEAP(sizeof(CToken)) * 1536;

/**
 * 
 * @update  gess7/25/98
 * @param 
 */
nsTokenAllocator::nsTokenAllocator() {

  MOZ_COUNT_CTOR(nsTokenAllocator);

  mArenaPool.Init("TheTokenPool", kBucketSizes, kNumBuckets, kInitialPoolSize);

#ifdef NS_DEBUG
  int i=0;
  for(i=0;i<eToken_last-1;i++) {
    mTotals[i]=0;
#endif
  }
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
CToken* nsTokenAllocator::CreateTokenOfType(eHTMLTokenTypes aType,eHTMLTags aTag, const nsString& aString) {

  CToken* result=0;

#ifdef  NS_DEBUG
    mTotals[aType-1]++;
#endif
  switch(aType){
    case eToken_start:            result=new(mArenaPool) CStartToken(aTag); break;
    case eToken_end:              result=new(mArenaPool) CEndToken(aTag); break;
    case eToken_comment:          result=new(mArenaPool) CCommentToken(); break;
    case eToken_entity:           result=new(mArenaPool) CEntityToken(aString); break;
    case eToken_whitespace:       result=new(mArenaPool) CWhitespaceToken(); break;
    case eToken_newline:          result=new(mArenaPool) CNewlineToken(); break;
    case eToken_text:             result=new(mArenaPool) CTextToken(aString); break;
    case eToken_attribute:        result=new(mArenaPool) CAttributeToken(aString); break;
    case eToken_script:           result=new(mArenaPool) CScriptToken(); break;
    case eToken_style:            result=new(mArenaPool) CStyleToken(); break;
    case eToken_skippedcontent:   result=new(mArenaPool) CSkippedContentToken(aString); break;
    case eToken_instruction:      result=new(mArenaPool) CInstructionToken(); break;
    case eToken_cdatasection:     result=new(mArenaPool) CCDATASectionToken(); break;
    case eToken_error:            result=new(mArenaPool) CErrorToken(); break;
    case eToken_doctypeDecl:      result=new(mArenaPool) CDoctypeDeclToken(); break;
      default:
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
    case eToken_text:             result=new(mArenaPool) CTextToken(NS_ConvertToString("")); break;
    case eToken_script:           result=new(mArenaPool) CScriptToken(); break;
    case eToken_style:            result=new(mArenaPool) CStyleToken(); break;
    case eToken_skippedcontent:   result=new(mArenaPool) CSkippedContentToken(NS_ConvertToString("")); break;
    case eToken_instruction:      result=new(mArenaPool) CInstructionToken(); break;
    case eToken_cdatasection:     result=new(mArenaPool) CCDATASectionToken(); break;
    case eToken_error:            result=new(mArenaPool) CErrorToken(); break;
    case eToken_doctypeDecl:      result=new(mArenaPool) CDoctypeDeclToken(aTag); break;
    default:
      break;
   }

  return result;
}

/**
 * Let's get this code ready to be reused by all the contexts.
 * 
 * @update	harishd 08/04/00
 * @param   aType -- tells you the type of token to create
 * @param   aTag  -- tells you the type of tag to init with this token
 *
 * @return  ptr to new token (or 0).
 */
CToken* nsTokenAllocator::CreateRTFTokenOfType(eRTFTokenTypes aType,eRTFTags aTag) {

  CToken* result=0;

  switch(aType){
    case eRTFToken_controlword:   result=new(mArenaPool) CRTFControlWord(aTag); break;
    case eRTFToken_content:       result=new(mArenaPool) CRTFContent();         break;
    default: 
      break;
   }
  
  return result;
}


CNodeRecycler::CNodeRecycler(): mSharedNodes(0) {

  MOZ_COUNT_CTOR(CNodeRecycler);

#ifdef NS_DEBUG
  gNodeCount=0;
#endif
}

CNodeRecycler::~CNodeRecycler() {

  MOZ_COUNT_DTOR(CNodeRecycler);

  nsCParserNode* theNode=0;

#ifdef NS_DEBUG
#if 1
  PRInt32 count=gNodeCount-mSharedNodes.GetSize();
  if(count) {
    printf("%i of %i nodes leaked!\n",count,gNodeCount);
  }
#endif
#endif

  while((theNode=(nsCParserNode*)mSharedNodes.Pop())){
    delete theNode;
  }
}

void CNodeRecycler::RecycleNode(nsCParserNode* aNode) {
  
  if(aNode && (!aNode->mUseCount)) {
        
    IF_FREE(aNode->mToken);

    CToken* theToken=0;
    while((theToken=(CToken*)aNode->PopAttributeToken())){
      IF_FREE(theToken);
    }
    mSharedNodes.Push(aNode);
  }
}

nsCParserNode* CNodeRecycler::CreateNode(void) {

  nsCParserNode* result=0;
  if(0<mSharedNodes.GetSize()) {
    result=(nsCParserNode*)mSharedNodes.Pop();
  }
  else{
    result=new nsCParserNode();
#ifdef NS_DEBUG
#if 1
    gNodeCount++;
#endif
#endif
  }
  return result;
}

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
  Define the nsIElementObserver release class...
 **************************************************************/
class nsObserverReleaser: public nsDequeFunctor{
public:
  virtual void* operator()(void* anObject) {
    nsIElementObserver* theObserver= (nsIElementObserver*)anObject;
    NS_RELEASE(theObserver);
    return 0;
  }
};

/**************************************************************
  This defines the topic object used by the observer service.
  The observerService uses a list of these, 1 per topic when
  registering tags.
 **************************************************************/

nsObserverTopic::nsObserverTopic(const nsString& aTopic) : mTopic(aTopic),
                                                           mKeys(0),
                                                           mValues(0) {

   nsCRT::zero(mObservers,sizeof(mObservers));
   mCharsetKey.AssignWithConversion("charset");
   mSourceKey.AssignWithConversion("charsetSource");
   mDTDKey.AssignWithConversion("X_COMMAND");
}

nsObserverTopic::~nsObserverTopic() {
  nsObserverReleaser theReleaser;

  PRInt32 theIndex=0;
  for(theIndex=0;theIndex<=NS_HTML_TAG_MAX;theIndex++){
    if(mObservers[theIndex]){
      mObservers[theIndex]->ForEach(theReleaser);
      delete mObservers[theIndex];
      mObservers[theIndex]=0;
    }
  }
}

PRBool nsObserverTopic::Matches(const nsString& aString) {
  PRBool result=aString.Equals(mTopic);
  return result;
}

nsDeque* nsObserverTopic::GetObserversForTag(eHTMLTags aTag) {
  if(aTag <= NS_HTML_TAG_MAX) {
    return mObservers[aTag];
  }
  return 0;
}

void nsObserverTopic::RegisterObserverForTag(nsIElementObserver *anObserver,eHTMLTags aTag) {
  if(anObserver) {
    if(mObservers[aTag] == nsnull) {
       mObservers[aTag] = new nsDeque(0);
    }
    NS_ADDREF(anObserver);
    mObservers[aTag]->Push(anObserver);
  }
}

/**
 * This method will notify observers registered for specific tags.
 * 
 * @update harishd 08/29/99
 * @param  aTag            -  The tag for which observers could be waiting for.
 * @param  aNode           -  
 * @param  aUniqueID       -  The document ID.
 * @param  aDTD            -  The current DTD.
 * @param  aCharsetValue   -
 * @param  aCharsetSource  -
 * @return if SUCCESS return NS_OK else return ERROR code.
 */
nsresult nsObserverTopic::Notify(eHTMLTags aTag,nsIParserNode& aNode,void* aUniqueID,nsIParser* aParser) {
  nsresult  result=NS_OK;

  nsDeque*  theDeque=GetObserversForTag(aTag);
  if(theDeque){ 

    nsAutoString      theCharsetValue;
    nsCharsetSource   theCharsetSource;
    aParser->GetDocumentCharset(theCharsetValue,theCharsetSource);

    PRInt32 theAttrCount =aNode.GetAttributeCount(); 
    PRUint32 theDequeSize=theDeque->GetSize(); 
    if(0<theDequeSize){
      mKeys.Empty();
      mValues.Empty();
      int index = 0; 
      for(index=0; index<theAttrCount; index++) {
        mKeys.Push((PRUnichar*)aNode.GetKeyAt(index).GetUnicode()); 
        mValues.Push((PRUnichar*)aNode.GetValueAt(index).GetUnicode()); 
      } 

      nsAutoString intValue;
      
      // Add pseudo attribute in the end

      mKeys.Push((PRUnichar*)mCharsetKey.GetUnicode()); 
      mValues.Push((PRUnichar*)theCharsetValue.GetUnicode());
      
        
      mKeys.Push((PRUnichar*)mSourceKey.GetUnicode()); 
      intValue.AppendInt(PRInt32(theCharsetSource),10);
      mValues.Push((PRUnichar*)intValue.GetUnicode());

      mKeys.Push((PRUnichar*)mDTDKey.GetUnicode());
      mValues.Push((PRUnichar*)mTopic.GetUnicode());

      nsAutoString theTagStr; theTagStr.AssignWithConversion(nsHTMLTags::GetStringValue(aTag));
      nsObserverNotifier theNotifier(theTagStr.GetUnicode(),(nsISupports*)aUniqueID,&mKeys,&mValues);
      theDeque->FirstThat(theNotifier); 
      result=theNotifier.mResult; 
     }//if 
  } 
  return result;
}


/******************************************************************************
  This class is used to store ref's to tag observers during the parse phase.
  Note that for simplicity, this is a singleton that is constructed in the
  CNavDTD and shared for the duration of the application session. Later on it
  might be nice to use a more dynamic approach that would permit observers to
  come and go on a document basis.

  I changed the observerservice to store topics so that we can distinguish 
  observers by topic. Up till now, they've all just be thrown into the same
  observer list, which was wrong. This fixes bug #28825.
 ******************************************************************************/

CObserverService::CObserverService() : mTopics(0) {

  MOZ_COUNT_CTOR(CObserverService);

  nsAutoString theHTMLTopic; theHTMLTopic.AssignWithConversion(kHTMLTextContentType);
  RegisterObservers(theHTMLTopic);

  nsAutoString theXMLTopic; theXMLTopic.AssignWithConversion(kXMLTextContentType);  //use the mimetype for the topic
  RegisterObservers(theXMLTopic);

}


CObserverService::~CObserverService() {

  MOZ_COUNT_DTOR(CObserverService);

  nsObserverTopic *theTopic=(nsObserverTopic*)mTopics.Pop();
  while(theTopic) {
    delete theTopic;
    theTopic=(nsObserverTopic*)mTopics.Pop();
  }
}

nsObserverTopic* CObserverService::GetTopic(const nsString& aTopic) {
  PRInt32 theIndex=0;
  nsObserverTopic *theTopic=(nsObserverTopic*)mTopics.ObjectAt(theIndex++);

  while(theTopic) {
    if(theTopic->Matches(aTopic))
      return theTopic;
    theTopic=(nsObserverTopic*)mTopics.ObjectAt(theIndex++);
  }
  return 0;
}

nsObserverTopic* CObserverService::CreateTopic(const nsString& aTopic) {
  nsObserverTopic* theTopic=new nsObserverTopic(aTopic);
  mTopics.Push(theTopic);
  return theTopic;
}

/**
 * This method will maintain lists of observers registered for specific tags.
 * 
 * @update rickg 03.23.2000
 * @param  aTopic  -  The topic under which observers register for.
 * @return if SUCCESS return NS_OK else return ERROR code.
 */

void CObserverService::RegisterObservers(const nsString& aTopic) {
  nsresult result = NS_OK;
  nsIObserverService* theObserverService = nsnull;
  result = nsServiceManager::GetService(NS_OBSERVERSERVICE_PROGID, NS_GET_IID(nsIObserverService),
                                      (nsISupports**) &theObserverService, nsnull);
  if(result == NS_OK){
    nsIEnumerator* theEnum = nsnull;
    result = theObserverService->EnumerateObserverList(aTopic.GetUnicode(), &theEnum);
    nsServiceManager::ReleaseService(NS_OBSERVERSERVICE_PROGID, theObserverService);

    if(result == NS_OK) {
      nsCOMPtr<nsIElementObserver> theElementObserver;
      nsISupports *inst = nsnull;

      nsObserverTopic *theTopic=0;

      for (theEnum->First(); theEnum->IsDone() != NS_OK; theEnum->Next()) {
        result = theEnum->CurrentItem(&inst);
        if (NS_SUCCEEDED(result)) {
          theElementObserver = do_QueryInterface(inst, &result);
          NS_RELEASE(inst);
        }
        if (result == NS_OK) {
          const char* theTagStr = nsnull;
          PRUint32 theTagIndex = 0;
          theTagStr = theElementObserver->GetTagNameAt(theTagIndex);
          while (theTagStr != nsnull) {
            eHTMLTags theTag = nsHTMLTags::LookupTag(nsCAutoString(theTagStr));
            if((eHTMLTag_userdefined!=theTag) && (theTag <= NS_HTML_TAG_MAX)){
            
              theTopic=GetTopic(aTopic);
              if(!theTopic)
                theTopic=CreateTopic(aTopic);
              if(theTopic) {
                theTopic->RegisterObserverForTag(theElementObserver,theTag);
              }

            }
            theTagIndex++;
            theTagStr = theElementObserver->GetTagNameAt(theTagIndex);
          }
        }
      }
    }
    NS_IF_RELEASE(theEnum);
  }
}

/**
 * This method will maintain lists of observers registered for specific tags.
 * 
 * @update rickg 03.23.2000
 * @param  aTopic  -  The topic under which observers register for.
 * @return if SUCCESS return NS_OK else return ERROR code.
 */

void CObserverService::UnregisterObservers(const nsString& aTopic) {
}

/**
 * This method will notify observers registered for specific tags.
 * 
 * @update rickg 03.23.2000
 * @param  aTag            -  The tag for which observers could be waiting for.
 * @param  aNode           -  
 * @param  aUniqueID       -  The document ID.
 * @param  aDTD            -  The current DTD.
 * @param  aCharsetValue   -
 * @param  aCharsetSource  -
 * @return if SUCCESS return NS_OK else return ERROR code.
 */
nsresult CObserverService::Notify(  eHTMLTags aTag,
                                    nsIParserNode& aNode,
                                    void* aUniqueID, 
                                    const nsString& aTopic,
                                    nsIParser* aParser) {
  nsresult  result=NS_OK;
  nsObserverTopic *theTopic=GetTopic(aTopic);
  if(theTopic) {
    result=theTopic->Notify(aTag,aNode,aUniqueID,aParser);
  }
  return result;
}

/**
 * This method will look for the list of observers registered for 
 * a specific tag.
 * 
 * @update rickg 03.23.2000
 * @param aTag - The tag for which observers could be waiting for.
 * @return if FOUND return "observer list" else return nsnull;
 *
 */

nsDeque* CObserverService::GetObserversForTagInTopic(eHTMLTags aTag,const nsString& aTopic) {
  nsObserverTopic *theTopic=GetTopic(aTopic);
  if(theTopic) {
    return theTopic->GetObserversForTag(aTag);
  }
  return 0;
}
