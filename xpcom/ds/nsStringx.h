#ifndef _NSCSTRING_CLASSES_
#define _NSCSTRING_CLASSES_

#include "nsString2x.h"


/***************************************************************************
 *
 *  Now the basic nsCString class, which uses nsSymmetricStringImpl as a
 *  base so that it gets all the char behaviors, and conversion functionality
 *  via the AltCharType.
 *
 ***************************************************************************/

class nsCString : public nsStringImpl<char> {
public:
  
  nsCString() : nsStringImpl<char>() {
  }
  
  nsCString(const nsCString& aString,PRInt32 aLength=-1) : nsStringImpl<char>() {
    Assign(aString,aLength);
  }
  
  nsCString(const char* aString, PRInt32 aLength=-1) : nsStringImpl<char>(aString,aLength) {
  }

  //call this version for string ptrs that match the compatible chartype
  nsCString(const PRUnichar* aString, PRInt32 aLength=-1) : nsStringImpl<char>(){
    Assign(aString,aLength);
  }

  //call this version for string ptrs that match the compatible chartype
  nsCString(const nsStringImpl<PRUnichar>& aString, PRInt32 aLength=-1) : nsStringImpl<char>(){
    Assign(aString,aLength);
  }

  virtual ~nsCString() { }
  
  nsCString& operator=(const nsCString& aString) {
    nsStringImpl<char>::operator=(aString);
    return *this;
  }

  nsCString& operator=(const char* aString) {
    nsStringImpl<char>::operator=(aString);
    return *this;
  } 

  nsCString& operator=(const nsStringImpl<PRUnichar>& aString) {
    Assign(aString);
    return *this;
  }

  nsCString& operator=(const PRUnichar* aString) {
    Assign(aString);
    return *this;
  }

  //******************************************
  // Here are the accessor methods...
  //******************************************

  char*     GetBuffer() {return mStringValue.mBuffer;}  //this needs to be cleaned up soon.
  virtual   PRUnichar  operator[](PRUint32 aOffset)  {return mStringValue[aOffset];}


  //******************************************
  // Here are the mutation methods...
  //******************************************

    //assign from a stringimpl
  nsresult Assign(const nsCString& aString,PRInt32 aLength=-1) {
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
  nsresult Append(const nsCString& aString,PRInt32 aLength=-1) {
    SVAppend< char, char> (mStringValue,aString.mStringValue,0,aString.mStringValue.mLength);
    return NS_OK;
  }
    //append from a stringimpl
  virtual nsresult Append(const nsStringImpl<PRUnichar> &aString,PRInt32 aLength=-1) {
    SVAppend< char, PRUnichar> (mStringValue,aString.mStringValue,0,aString.mStringValue.mLength);
    return NS_OK;
  }

    //append from a type compatible string pointer
  virtual nsresult Append(const PRUnichar* aString,PRInt32 aLength=-1) {
    nsresult result=NS_OK;
    if(aString) {
      nsStringValueImpl<PRUnichar> theTempString(const_cast<PRUnichar*>(aString),aLength);
      SVAppend< char, PRUnichar > (mStringValue,theTempString,0,aLength);
    }
    return result;
  }


    //***************************************
    //  Here come a wad of insert methods...
    //***************************************

  nsCString& Insert(const PRUnichar* aChar,PRUint32 anOffset,PRInt32 aCount=-1){
    return *this;
  }

  nsCString& Insert(PRUnichar aChar,PRUint32 anOffset){
    return *this;
  }


    //*******************************************
    //  Here come inplace replacement  methods...
    //*******************************************


  nsCString& ReplaceSubstring(const PRUnichar* aTarget,const PRUnichar* aNewValue) {
    return *this;
  }

    //*******************************************
    //  Here come the stripchar methods...
    //*******************************************

  nsCString& StripChar(PRUnichar aChar,PRInt32 anOffset=0){
    return *this;
  }

    //*******************************************
    //  Here come the operator+=() methods...
    //*******************************************

  nsCString& operator+=(const PRUnichar* aUCString) {
    Append(aUCString);
    return *this;
  }
  
  nsCString& operator+=(const PRUnichar aChar){
    // Append(aChar);
    return *this;
  }


  /***********************************
    Comparison methods...
   ***********************************/

  PRBool operator==(const PRUnichar* aString) const {return Equals(aString);}

  PRBool operator!=(const PRUnichar* aString) const {return PRBool(Compare(aString)!=0);}

  PRBool operator<(const PRUnichar* aString) const {return PRBool(Compare(aString)<0);}

  PRBool operator>(const PRUnichar* aString) const {return PRBool(Compare(aString)>0);}

  PRBool operator<=(const PRUnichar* aString) const {return PRBool(Compare(aString)<=0);}

  PRBool operator>=(const PRUnichar* aString) const {return PRBool(Compare(aString)>=0);}


  PRInt32 Compare(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;
  
  PRBool  Equals(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;
  PRBool  Equals(const PRUnichar* s1, const PRUnichar* s2,PRBool aIgnoreCase=PR_FALSE) const;


  /***************************************
    These are string searching methods...
   ***************************************/

  PRInt32 FindChar(PRUnichar aChar) {
    return -1;
  }

  PRInt32 Find(const nsStringImpl<PRUnichar>& aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const {
    return -1;
  }

  PRInt32 Find(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const{
    return -1;
  }
    
  PRInt32 FindCharInSet(const PRUnichar* aString,PRInt32 anOffset=-1) const{
    return -1;
  }

  PRInt32 FindChar(PRUnichar aChar,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const {
    return -1;
  }

  PRInt32 RFindChar(PRUnichar aChar,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const{
    return -1;
  }

  PRInt32 RFind(const nsStringImpl<PRUnichar>& aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const{
    return -1;
  }

  PRInt32 RFind(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const{
    return -1;
  }
    
  PRInt32 RFindCharInSet(const nsStringImpl<PRUnichar>& aString,PRInt32 anOffset=-1) const {
    return -1;
  }

  PRInt32 RFindCharInSet(const PRUnichar* aString,PRInt32 anOffset=-1) const{
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

class nsCAutoString : public nsAutoStringImpl<char>  {
public:
  
  nsCAutoString() : nsAutoStringImpl<char>() {
  }
  
  nsCAutoString(const nsCAutoString& aString,PRInt32 aLength=-1) : nsAutoStringImpl<char>(aString,aLength) {
    //Assign(aString,aLength);
  }
  
  nsCAutoString(const char* aString, PRInt32 aLength=-1) : nsAutoStringImpl<char>(aString,aLength) {
  }

  //call this version for string ptrs that match a compatible chartype
  nsCAutoString(const PRUnichar* aString, PRInt32 aLength=-1) : nsAutoStringImpl<char>() {
    Assign(aString,aLength);
  }

  //call this version for string ptrs that match the compatible chartype
  nsCAutoString(const nsStringImpl<PRUnichar>& aString, PRInt32 aLength=-1) : nsAutoStringImpl<char>() {
  }

  virtual ~nsCAutoString() { }
  
  nsCAutoString& operator=(const nsCAutoString& aString) {
    nsStringImpl<char>::operator=(aString);
    return *this;
  }

  nsCAutoString& operator=(const char* aString) {
    nsStringImpl<char>::operator=(aString);
    return *this;
  } 

  nsCAutoString& operator=(const PRUnichar* aString) {
    Assign(aString);
    return *this;
  } 

  nsCAutoString& operator=(const nsStringImpl<PRUnichar>& aString) {
    Assign(aString);
    return *this;
  }

  //******************************************
  // Here are the accessor methods...
  //******************************************

  char*     GetBuffer() {return mStringValue.mBuffer;}  //this needs to be cleaned up soon.
  virtual   PRUnichar  operator[](PRUint32 aOffset)  {return mStringValue[aOffset];}


  //******************************************
  // Here are the mutation methods...
  //******************************************

    //assign from a stringimpl
  nsresult Assign(const nsCString& aString,PRInt32 aLength=-1) {
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
  nsresult Append(const nsCString& aString,PRInt32 aLength=-1) {
    SVAppend< char, char> (mStringValue,aString.mStringValue,0,aString.mStringValue.mLength);
    return NS_OK;
  }
    //append from a stringimpl
  virtual nsresult Append(const nsStringImpl<PRUnichar> &aString,PRInt32 aLength=-1) {
    SVAppend< char, PRUnichar> (mStringValue,aString.mStringValue,0,aString.mStringValue.mLength);
    return NS_OK;
  }

    //append from a type compatible string pointer
  virtual nsresult Append(const PRUnichar* aString,PRInt32 aLength=-1) {
    nsresult result=NS_OK;
    if(aString) {
      nsStringValueImpl<PRUnichar> theTempString(const_cast<PRUnichar*>(aString),aLength);
      SVAppend< char, PRUnichar > (mStringValue,theTempString,0,aLength);
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