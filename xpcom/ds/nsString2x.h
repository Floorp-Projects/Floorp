#ifndef _NSSTRING_CLASSES_
#define _NSSTRING_CLASSES_

#include "nsStringImpl.h"
#include "nsAutoStringImpl.h"


/***************************************************************************
 *
 *  Now the basic nsCString class, which uses nsSymmetricStringImpl as a
 *  base so that it gets all the char behaviors, and conversion functionality
 *  via the AltCharType.
 *
 ***************************************************************************/

class nsString : public nsStringImpl<PRUnichar> {
public:
  
  nsString() : nsStringImpl<PRUnichar>() {
  }
  
  nsString(const nsString& aString,PRInt32 aLength=-1) : nsStringImpl<PRUnichar>() {
    Assign(aString,aLength);
  }

  
  nsString(const PRUnichar* aString, PRInt32 aLength=-1) : nsStringImpl<PRUnichar>(aString,aLength) {
  }

  //call this version for string ptrs that match the alternate chartype
  nsString(const char* aString, PRInt32 aLength=-1) : nsStringImpl<PRUnichar>(){
    Assign(aString,aLength);
  }

  //call this version for string ptrs that match the alternate chartype
  nsString(const nsStringImpl<char>& aString, PRInt32 aLength=-1) : nsStringImpl<PRUnichar>(){
    Assign(aString,aLength);
  }

  virtual ~nsString() { }
  
  nsString& operator=(const nsString& aString) {
    nsStringImpl<PRUnichar>::operator=(aString);
    return *this;
  }

  nsString& operator=(const nsStringImpl<PRUnichar>& aString) {
    Assign(aString);
    return *this;
  }

  nsString& operator=(const PRUnichar* aString) {
    nsStringImpl<PRUnichar>::operator=(aString);
    return *this;
  } 

  nsString& operator=(const char* aString) {
    Assign(aString);
    return *this;
  }

  //******************************************
  // Here are the accessor methods...
  //******************************************

  PRUnichar*  GetUnicode() {return mStringValue.mBuffer;}  //this needs to be cleaned up soon.
  virtual     PRUnichar  operator[](PRUint32 aOffset)  {return mStringValue[aOffset];}


  //******************************************
  // Here are the mutation methods...
  //******************************************

    //assign from a stringimpl
  nsresult Assign(const nsString& aString,PRInt32 aLength=-1) {
    Truncate();
    return Append(aString);
  }

    //assign from a stringimpl<PRUnichar>
  nsresult Assign(const nsStringImpl<char>& aString,PRInt32 aLength=-1) {
    Truncate();
    return Append(aString,aLength);
  }

    //assign from a compatible string pointer
  virtual nsresult Assign(const char* aString,PRInt32 aLength=-1) {
    Truncate();
    return Append(aString,aLength);
  }

    //assign from a stringimpl
  nsresult Append(const nsString& aString,PRInt32 aLength=-1) {
    SVAppend< PRUnichar, PRUnichar> (mStringValue,aString.mStringValue,0,aString.mStringValue.mLength);
    return NS_OK;
  }
    //append from a stringimpl
  virtual nsresult Append(const nsStringImpl<char> &aString,PRInt32 aLength=-1) {
    SVAppend< PRUnichar, char> (mStringValue,aString.mStringValue,0,aString.mStringValue.mLength);
    return NS_OK;
  }

    //append from a type compatible string pointer
  virtual nsresult Append(const char* aString,PRInt32 aLength=-1) {
    nsresult result=NS_OK;
    if(aString) {
      nsStringValueImpl<char> theTempString(const_cast<char*>(aString),aLength);
      SVAppend< PRUnichar, char > (mStringValue,theTempString,0,aLength);
    }
    return result;
  }


    //***************************************
    //  Here come a wad of insert methods...
    //***************************************

  nsString& Insert(const char* aChar,PRUint32 anOffset,PRInt32 aCount=-1){
    return *this;
  }

  nsString& Insert(char aChar,PRUint32 anOffset){
    return *this;
  }


    //*******************************************
    //  Here come inplace replacement  methods...
    //*******************************************


  nsString& ReplaceSubstring(const char* aTarget,const PRUnichar* aNewValue) {
    return *this;
  }

    //*******************************************
    //  Here come the stripchar methods...
    //*******************************************

  nsString& StripChar(char aChar,PRInt32 anOffset=0){
    return *this;
  }

    //*******************************************
    //  Here come the operator+=() methods...
    //*******************************************

  nsString& operator+=(const char* aUCString) {
    Append(aUCString);
    return *this;
  }
  
  nsString& operator+=(const char aChar){
    // Append(aChar);
    return *this;
  }


  /***********************************
    Comparison methods...
   ***********************************/

  PRBool operator==(const char* aString) const {return Equals(aString);}

  PRBool operator!=(const char* aString) const {return PRBool(Compare(aString)!=0);}

  PRBool operator<(const char* aString) const {return PRBool(Compare(aString)<0);}

  PRBool operator>(const char* aString) const {return PRBool(Compare(aString)>0);}

  PRBool operator<=(const char* aString) const {return PRBool(Compare(aString)<=0);}

  PRBool operator>=(const char* aString) const {return PRBool(Compare(aString)>=0);}


  PRInt32 Compare(const char* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;
  
  PRBool  Equals(const char* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;
  PRBool  Equals(const char* s1, const char* s2,PRBool aIgnoreCase=PR_FALSE) const;


  /***************************************
    These are string searching methods...
   ***************************************/

  PRInt32 FindChar(char aChar) {
    return -1;
  }

  PRInt32 Find(const nsStringImpl<char>& aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const {
    return -1;
  }

  PRInt32 Find(const char* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const{
    return -1;
  }
    
  PRInt32 FindCharInSet(const char* aString,PRInt32 anOffset=-1) const{
    return -1;
  }

  PRInt32 FindChar(char aChar,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const {
    return -1;
  }

  PRInt32 RFindChar(char aChar,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const{
    return -1;
  }

  PRInt32 RFind(const nsStringImpl<char>& aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const{
    return -1;
  }

  PRInt32 RFind(const char* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const{
    return -1;
  }
    
  PRInt32 RFindCharInSet(const nsStringImpl<char>& aString,PRInt32 anOffset=-1) const {
    return -1;
  }

  PRInt32 RFindCharInSet(const char* aString,PRInt32 anOffset=-1) const{
    return -1;
  }

};


/***************************************************************************
 *
 *  Now the basic nsCString class, which uses nsSymmetricStringImpl as a
 *  base so that it gets all the char behaviors, and conversion functionality
 *  via the AltCharType.
 *
 ***************************************************************************/

class nsAutoString : public nsAutoStringImpl<PRUnichar>  {
public:
  
  nsAutoString() : nsAutoStringImpl<PRUnichar>() {
  }
  
  nsAutoString(const nsAutoString& aString,PRInt32 aLength=-1) : nsAutoStringImpl<PRUnichar>(aString,aLength) {
    //Assign(aString,aLength);
  }
  
  nsAutoString(const PRUnichar* aString, PRInt32 aLength=-1) : nsAutoStringImpl<PRUnichar>(aString,aLength) {
  }

  //call this version for string ptrs that match the internal chartype
  nsAutoString(const char* aString, PRInt32 aLength=-1) : nsAutoStringImpl<PRUnichar>() {
    Assign(aString,aLength);
  }

  //call this version for string ptrs that match the internal chartype
  nsAutoString(const nsStringImpl<PRUnichar>& aString, PRInt32 aLength=-1) : nsAutoStringImpl<PRUnichar>() {
  }

  virtual ~nsAutoString() { }
  
  nsAutoString& operator=(const nsAutoString& aString) {
    nsStringImpl<PRUnichar>::operator=(aString);
    return *this;
  }

  nsAutoString& operator=(const char* aString) {
    Assign(aString);
    return *this;
  } 

  nsAutoString& operator=(const PRUnichar* aString) {
    nsStringImpl<PRUnichar>::operator=(aString);
    return *this;
  } 

  nsAutoString& operator=(const nsStringImpl<PRUnichar>& aString) {
    Assign(aString);
    return *this;
  }


  //******************************************
  // Here are the mutation methods...
  //******************************************

    //assign from a stringimpl
  nsresult Assign(const nsString& aString,PRInt32 aLength=-1) {
    Truncate();
    return Append(aString);
  }

    //assign from a stringimpl<PRUnichar>
  nsresult Assign(const nsStringImpl<PRUnichar>& aString,PRInt32 aLength=-1) {
    Truncate();
    return Append(aString,aLength);
  }

    //assign from a compatible string pointer
  virtual nsresult Assign(const PRUnichar* aString,PRInt32 aLength=-1) {
    Truncate();
    return Append(aString,aLength);
  }

    //assign from a stringimpl
  nsresult Append(const nsString& aString,PRInt32 aLength=-1) {
    SVAppend< PRUnichar, PRUnichar> (mStringValue,aString.mStringValue,0,aString.mStringValue.mLength);
    return NS_OK;
  }
    //append from a stringimpl
  virtual nsresult Append(const nsStringImpl<char> &aString,PRInt32 aLength=-1) {
    SVAppend< PRUnichar,char > (mStringValue,aString.mStringValue,0,aString.mStringValue.mLength);
    return NS_OK;
  }

    //append from a type compatible string pointer
  virtual nsresult Append(const char* aString,PRInt32 aLength=-1) {
    nsresult result=NS_OK;
    if(aString) {
      nsStringValueImpl<char> theTempString(const_cast<char*>(aString),aLength);
      SVAppend< PRUnichar, char > (mStringValue,theTempString,0,aLength);
    }
    return result;
  }


  /***********************************
    These come from stringsearcher
   ***********************************/

  virtual PRInt32 Find(const char* aTarget) {
    return -1;
  }

  virtual PRInt32 FindChar(PRUnichar aChar) {
    return -1;
  }

};


#endif