/*****************************************************************************************************************
 *  
 * This template provides the basic implementation for all the original
 * nsString classes (nsString, nsCString) and serve as the basis for
 * thier autostring forms.
 *
 * DEVELOPMENT NOTES:
 *
 *  0. Note that I've explictly created interfaces for both the nsString(s), 
 *     and for the nsReadableString (ABT). This will allow the nsString
 *     form to be optimized (as in the original), while the nsReadable
 *     interfaces can use a new, more generic approach (including iterators).
 *     SEE Append() as an example.
 * 
 *  1. Flesh out more of the nsReadableString and nsReadableStringIterator interfaces.
 *     Note the use of the readableStringIterator in the version of Append() that
 *     accepts an nsReadableString&. This is because the readableString may have 
 *     almost any form (e.g. segmentString), and we can't rely upon a linear
 *     layout of buffer memory as we can with nsString class deriviates.
 *
 *     I fully expect this pattern to get replaced with a legitimate form.
 *
 *
 *  2. I don't really think it's necessary to go crazy with virtual methods
 *     but I've done so for now as a matter of consistency. Once this file
 *     gets further along, and I have a workable nsAutoString, then I'll start
 *     backing off from virtual methods whereever possible. Please be patient.
 *
 *  3. I'm being lazy for the time being, and NOT encoding the buffer-ownership
 *     flag into the high-order bit of the length field. This needs to be done
 *     "real soon" but it's not worth doing (today) while I'm still designing.
 *
 *  4. I explictly implemented the orthodox canonical form.
 *
 *  5. Before anyone get's too critical of this design, note that I'm using
 *     the dual-class template form to make interoperability between char* 
 *     and prunichar* obvious. I recognize that it's better to switch to 
 *     an nsReadableString interface, but I've done this for now ONLY to
 *     make the conversion from our existing nsString class easier. Once
 *     I get over a few hurdles there, I'll re-examine this approach.
 *     Once again: please be patient (I'm only 5 hours into this effort).
 *
 *  6. I've quickly hooked up some of the low level routines (in the 
 *     file nsBufferManager.h). Don't expect it all to work just yet -- 
 *     they're mostly placeholders. You CAN already construct and copy
 *     construct an nsString, nsCString and the autostring forms.
 *
 *  7. Note that the basic nsString interfaces (append, assign, etc) are not
 *     perfectly equivalent to the originals. All I tried to do was capture
 *     the gist of the originals to flesh out these interfaces. 
 *
 *  8. Next Up: Mapping of more nsString functions (insert, delete, the
 *              boolean operators, the searching methods, etc.
 *
 *        *** NOTE: I've begun to map the existing nsString functions --
 *                  most are empty covers, to be used as a placeholder.
 *
 *  9. After I'm done with #8, I'll split this implementation into 2 files.
 * 
 * FILE NOTES:
 *
 *  02.29.2000: Original files (rickg)
 *  03.02.2000: Expand the API's to be a bit more realistic (rickg): 
 *                 1. Flesh out the interface to be compatible with old library 
 *                 2. Enable both nsString-based interfaces and nsReadableString interfaces 
 *                 3. Build enough infrastructure to test nsString(s) and nsImmutableString constructions.
 *  
 *****************************************************************************************************************/


#ifndef NS_STRINGIMPL_
#define NS_STRINGIMPL_

#include "nsStringValue.h"
#include "nsBufferManager.h"


/***************************************************************************
 *
 *  The following is the basic interface nsString searching...
 *
 *  We will merge this into nsReadableString very soon, but I've kept it
 *  seperate until I can map and factor the original nsString methods.
 *
 ***************************************************************************/
class nsSearchableString {
public:

  //We need to decide how deep (and generic) this API can go...

};

/***************************************************************************
 *
 *  This isn't intended as a real class, but only to illustrate that for 
 *  nsReadableString, we want a common access pattern to get at the underlying
 *  buffer. Iterators make perfect sense, but it will take a real implementation.
 *
 *  Note: This class is intended to replace direct calls to GetBuffer()
 *  in cases where iteration on an nsReadableString is taking place. 
 *
 ***************************************************************************/
class nsReadableStringIterator {
public:
  nsReadableStringIterator() { }
};

/***************************************************************************
 *
 *  The following is the basic interface anyone who wants to be passed
 *  as a string should define.
 *
 ***************************************************************************/
class nsReadableString : public nsSearchableString {
public:
  virtual PRUint32  Length() =0;
  virtual size_t    GetCharSize() =0;

  virtual PRUnichar operator[](PRUint32) = 0;

  virtual nsReadableStringIterator* First() = 0; //These are here so that nsReadable strings can offer a standard mechanism for
  virtual nsReadableStringIterator* Last() = 0;  //callers to access underlying data. Feel free to replace with a better pattern.

//by moving to iterators, we can support segmented content models...
//virtual nsStringIterator  GetIterator() = 0;

//virtual PRBool    IsEmpty(void) = 0;

    //etc.
};


/***************************************************************************
 *
 *  Now the basic nsStringImpl implementation, that becomes the functional 
 *  base for genericized strings storing a given chartype.for nsString, 
 nsCStirng and the autoString pairs.
 *
 ***************************************************************************/

template <class CharType>
class nsStringImpl: public nsReadableString {
public:
  
  nsStringImpl() : mStringValue() {
  }
  
    //call this version for nsString,nsCString and the autostrings
  nsStringImpl(const nsStringImpl& aString,PRInt32 aLength=-1) : mStringValue() {
    Assign(aString,aLength);
  }

    //call this version for nsString,nsCString and the autostrings
  nsStringImpl(const CharType* aString,PRInt32 aLength=-1) : mStringValue() {
    Assign(aString,aLength);
  }

    //call this version for all other ABT versions of readable strings
  nsStringImpl(const nsReadableString &aString) : mStringValue() {
    Assign(aString);
  }

  virtual ~nsStringImpl() { }
  
  nsStringImpl& operator=(const nsStringImpl<CharType>& aString) {
    if(aString.mStringValue.mBuffer!=mStringValue.mBuffer) {
      Assign(aString);
    }
    return *this;
  }

  //******************************************
  // Here are the accessor methods...
  //******************************************

  virtual nsresult SetLength(PRUint32 aLength) {
    return NS_OK;
  }

  virtual nsresult SetCapacity(PRUint32 aCapacity) {
    return NS_OK;
  }

  operator CharType*() {return mStringValue.mBuffer;}

  PRUint32  Length() {return mStringValue.mLength;}  
  size_t    GetCharSize() {return sizeof(CharType);}
  PRBool    IsEmpty(void) {return PRBool(mStringValue.mLength==0);}
  PRUnichar operator[](PRUint32 aOffset)  {return mStringValue[aOffset];}
  PRBool    SetCharAt(PRUnichar aChar,PRUint32 anIndex) {return PR_FALSE;}

  PRUnichar CharAt(PRUint32 anIndex) const {return mStringValue[aOffset];}
  PRUnichar First(void) const {return mStringValue[0];} 
  PRUnichar Last(void) const {return mStringValue[mStringValue.mLength];}


    //these aren't the real deal, but serve as a placeholder for us to implement iterators.
  virtual nsReadableStringIterator* First() {return new nsReadableStringIterator(); }
  virtual nsReadableStringIterator* Last() {return new nsReadableStringIterator(); }

  PRInt32   CountChar(PRUnichar aChar) {  return 0; }
  void      DebugDump(void) const { }

  //******************************************
  // Here are the mutation methods...
  //******************************************

  virtual nsresult Truncate(PRUint32 offset=0) {
    if(mStringValue) {
      mStringValue[0]=0;
      mStringValue.mLength=0;
    }
    return NS_OK;
  }

    //assign from a stringimpl
  nsresult Assign(const nsStringImpl& aString) {
    nsresult result=NS_OK;
    if(mStringValue.mBuffer!=aString.mStringValue.mBuffer){
      Truncate();
      result=Append(aString);
    }
    return result;
  }

    //assign from a compatible string pointer
  virtual nsresult Assign(const CharType* aString,PRInt32 aLength=-1) {
    nsresult result=NS_OK;
    if(mStringValue.mBuffer!=aString){
      Truncate();
      result=Append(aString,aLength);
    }
    return result;
  }


    //assign from an nsReadableString (the ABT)
  virtual nsresult Assign(const nsReadableString &aString,PRInt32 aLength=-1) {
    nsresult result=NS_OK;
    Truncate();
    SVAppendReadable<CharType>(mStringValue,aString,0,aLength);
    return result;
  }

    //append from a stringimpl
  virtual nsresult Append(const nsStringImpl<CharType> &aString,PRInt32 aLength=-1) {
    SVAppend< CharType, CharType > (mStringValue,aString.mStringValue,0,aString.mStringValue.mLength);
    return NS_OK;
  }

    //append from a type compatible string pointer
  virtual nsresult Append(const CharType* aString,PRInt32 aLength=-1) {
    nsresult result=NS_OK;
    if(aString) {
      nsStringValueImpl<CharType> theStringValue(const_cast<CharType*>(aString),aLength);
      SVAppend< CharType, CharType > (mStringValue,theStringValue,0,aLength);
    }
    return result;
  }

    //append from an nsReadableString (the ABT)
  virtual nsresult Append(const nsReadableString &aString,PRInt32 aLength=-1) {
    SVAppendReadable<CharType> (mStringValue,aString,0,aLength);
    return NS_OK;
  }

    //append an integer
  virtual nsresult Append(PRInt32 aInteger,PRInt32 aRadix=10) {
    nsresult result=NS_OK;
    return result;
  }

    //Append a float
  virtual nsresult Append(float aFloat) {
    nsresult result=NS_OK;
    return result;
  }


    //***************************************
    //  Here come a few deletion methods...
    //***************************************

  nsStringImpl<CharType>& Cut(PRUint32 anOffset,PRInt32 aCount) {
    return *this;
  }

  nsStringImpl<CharType>& Trim(const char* aSet,PRBool aEliminateLeading=PR_TRUE,PRBool aEliminateTrailing=PR_TRUE,PRBool aIgnoreQuotes=PR_FALSE) {
    return *this;
  }

  nsStringImpl<CharType>& CompressSet(const char* aSet, PRUnichar aChar,PRBool aEliminateLeading=PR_TRUE,PRBool aEliminateTrailing=PR_TRUE){
    return *this;
  }

  nsStringImpl<CharType>& CompressWhitespace( PRBool aEliminateLeading=PR_TRUE,PRBool aEliminateTrailing=PR_TRUE){
    return *this;
  }


    //***************************************
    //  Here come a wad of insert methods...
    //***************************************

  nsStringImpl<CharType>& Insert(const nsStringImpl<CharType>& aCopy,PRUint32 anOffset,PRInt32 aCount=-1) {
    return *this;
  }

  nsStringImpl<CharType>& Insert(const CharType* aChar,PRUint32 anOffset,PRInt32 aCount=-1){
    return *this;
  }

  nsStringImpl<CharType>& Insert(CharType aChar,PRUint32 anOffset){
    return *this;
  }

  
    //*******************************************
    //  Here come inplace replacement  methods...
    //*******************************************

  nsStringImpl<CharType>& ReplaceChar(PRUnichar anOldChar,PRUnichar aNewChar){
    return *this;
  }

  nsStringImpl<CharType>& ReplaceChar(const char* aSet,PRUnichar aNewChar){
    return *this;
  }

  nsStringImpl<CharType>& ReplaceSubstring(  const nsStringImpl<CharType>& aTarget,
                                             const nsStringImpl<CharType>& aNewValue){
    return *this;
  }

  nsStringImpl<CharType>& ReplaceSubstring(const CharType* aTarget,const CharType* aNewValue) {
    return *this;
  }

    //*******************************************
    //  Here come the stripchar methods...
    //*******************************************

  nsStringImpl<CharType>& StripChars(const char* aSet){
    return *this;
  }

  nsStringImpl<CharType>& StripChar(CharType aChar,PRInt32 anOffset=0){
    return *this;
  }

  nsStringImpl<CharType>& StripChar(PRInt32 anInt,PRInt32 anOffset=0) {return StripChar((PRUnichar)anInt,anOffset); }

  nsStringImpl<CharType>& StripWhitespace() {
    return *this;
  }

    //**************************************************
    //  Here are some methods that extract substrings...
    //**************************************************

  PRUint32 Left(nsStringImpl<CharType>& aCopy,PRInt32 aCount) const {
  }

  PRUint32 Mid(nsStringImpl<CharType>& aCopy,PRUint32 anOffset,PRInt32 aCount) const {
  }

  PRUint32 Right(nsStringImpl<CharType>& aCopy,PRInt32 aCount) const {
  }


    //*******************************************
    //  Here come the operator+=() methods...
    //*******************************************

  nsStringImpl<CharType>& operator+=(const nsStringImpl<CharType>& aString){
    Append(aString,aString.mStringValue.mLength);
    return *this;
  }

  nsStringImpl<CharType>& operator+=(const CharType* aString) {
    Append(aString);
    return *this;
  }
  
  nsStringImpl<CharType>& operator+=(const CharType aChar) {
    
    return *this;
  }

  
  nsStringImpl<CharType>& operator+=(const int anInt){
    Append(anInt,10);
    return *this;
  }

  void ToLowerCase() {
  }

  void ToLowerCase(nsStringImpl<CharType> &aString) const {
  }

  void ToUpperCase() {
  }

  void ToUpperCase(nsStringImpl<CharType> &aString) const {
  }

  /***********************************
    Comparison methods...
   ***********************************/

  PRBool operator==(const nsStringImpl<CharType>& aString) const {return Equals(aString);}      
  PRBool operator==(const CharType* aString) const {return Equals(aString);}

  PRBool operator!=(const nsStringImpl<CharType>& aString) const {return PRBool(Compare(aString)!=0);}
  PRBool operator!=(const CharType* aString) const {return PRBool(Compare(aString)!=0);}

  PRBool operator<(const nsStringImpl<CharType>& aString) const {return PRBool(Compare(aString)<0);}
  PRBool operator<(const CharType* aString) const {return PRBool(Compare(aString)<0);}

  PRBool operator>(const nsStringImpl<CharType>& aString) const {return PRBool(Compare(aString)>0);}
  PRBool operator>(const CharType* aString) const {return PRBool(Compare(aString)>0);}

  PRBool operator<=(const nsStringImpl<CharType>& aString) const {return PRBool(Compare(aString)<=0);}
  PRBool operator<=(const CharType* aString) const {return PRBool(Compare(aString)<=0);}

  PRBool operator>=(const nsStringImpl<CharType>& aString) const {return PRBool(Compare(aString)>=0);}
  PRBool operator>=(const CharType* aString) const {return PRBool(Compare(aString)>=0);}


  PRInt32 Compare(const nsStringImpl<CharType>& aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;
  PRInt32 Compare(const CharType* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;
  
  PRBool  Equals(const nsStringImpl<CharType> &aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;
  PRBool  Equals(const CharType* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;
  // PRBool  Equals(/*FIX: const */nsIAtom* anAtom,PRBool aIgnoreCase) const;   


  /***************************************
    These are string searching methods...
   ***************************************/

  PRInt32 FindChar(PRUnichar aChar) {
    return -1;
  }

  PRInt32 Find(const nsStringImpl<CharType>& aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const {
    return -1;
  }

  PRInt32 Find(const CharType* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const {
    return -1;
  }

    
  PRInt32 FindCharInSet(const nsStringImpl<CharType>& aString,PRInt32 anOffset=-1) const{
    return -1;
  }

  PRInt32 FindCharInSet(const CharType* aString,PRInt32 anOffset=-1) const{
    return -1;
  }

  PRInt32 FindChar(CharType aChar,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const {
    return -1;
  }

  PRInt32 RFindChar(CharType aChar,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const{
    return -1;
  }

  PRInt32 RFind(const nsStringImpl<CharType>& aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const{
    return -1;
  }

  PRInt32 RFind(const CharType* aCString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const{
    return -1;
  }
    
  PRInt32 RFindCharInSet(const nsStringImpl<CharType>& aString,PRInt32 anOffset=-1) const {
    return -1;
  }

  PRInt32 RFindCharInSet(const CharType* aString,PRInt32 anOffset=-1) const{
    return -1;
  }

  /***************************************
    These convert to a different type
   ***************************************/

  char* ToNewCString() const {
  }

  char* ToNewUTF8String() const {
  }

  PRUnichar* ToNewUnicode() const {
  }

  char* ToCString(char* aBuf,PRUint32 aBufLength,PRUint32 anOffset=0) const {
  }

  float ToFloat(PRInt32* aErrorCode) const {
  }

  PRInt32 ToInteger(PRInt32* aErrorCode,PRUint32 aRadix=kRadix10) const{
  }

  nsStringValueImpl<CharType>  mStringValue;
};



#endif
