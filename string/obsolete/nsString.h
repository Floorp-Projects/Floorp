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
 * LAST MODS:	gess 28Feb98
 * 
 * This very simple string class that knows how to do 
 * efficient (dynamic) resizing. It offers almost no
 * i18n support, and will undoubtedly have to be replaced.
 *
 */

#ifndef _NSSTRING
#define _NSSTRING


#include "prtypes.h"
#include "nscore.h"
#include "nsIAtom.h"
#include <iostream.h>
#include <stdio.h>


class NS_BASE nsString {
  public: 

                          nsString(const char* anISOLatin1="");
                          nsString(const nsString&);
                          nsString(const PRUnichar* aUnicode);    
  protected:
                          nsString(PRBool aSubclassBuffer); // special subclas constructor
  public:
  virtual                 ~nsString();

            PRInt32       Length() const { return mLength; }

            void          SetLength(PRInt32 aLength);
            void          Truncate(PRInt32 anIndex=0);
  virtual   void          EnsureCapacityFor(PRInt32 aNewLength);

  ///accessor methods
  //@{
            PRUnichar*    GetUnicode(void) const;
                          operator PRUnichar*() const;

#if 0
  // This is NOT allowed because it has to do a malloc to
  // create the iso-latin-1 version of the unicode string
                          operator char*() const;
#endif

            PRUnichar*    operator()() const;
            PRUnichar     operator()(PRInt32 i) const;
            PRUnichar&    operator[](PRInt32 i) const;
            PRUnichar&    CharAt(PRInt32 anIndex) const;
            PRUnichar&    First() const; 
            PRUnichar&    Last() const;

                //string creation methods...
            nsString      operator+(const nsString& aString);
            nsString      operator+(const char* anISOLatin1);
            nsString      operator+(char aChar);
            nsString      operator+(const PRUnichar* aBuffer);
            nsString      operator+(PRUnichar aChar);

            void          ToLowerCase();
            void          ToLowerCase(nsString& aString) const;
            void          ToUpperCase();
            void          ToUpperCase(nsString& aString) const;

            nsString*     ToNewString() const;
            char*         ToNewCString() const;

            char*         ToCString(char* aBuf,PRInt32 aBufLength) const;
            void          ToString(nsString& aString) const;

            PRUnichar*    ToNewUnicode() const;
            float         ToFloat(PRInt32* aErrorCode) const;
            PRInt32       ToInteger(PRInt32* aErrorCode) const;
  //@}

  ///string manipulation methods...                
  //@{
            nsString&     operator=(const nsString& aString);
            nsString&     operator=(const char* anISOLatin1);
            nsString&     operator=(char aChar);
            nsString&     operator=(const PRUnichar* aBuffer);
            nsString&     operator=(PRUnichar aChar);
            nsString&     SetString(const PRUnichar* aStr,PRInt32 aLength=-1);
            nsString&     SetString(const char* anISOLatin1,PRInt32 aLength=-1);

            nsString&     operator+=(const nsString& aString);
            nsString&     operator+=(const char* anISOLatin1);
            nsString&     operator+=(const PRUnichar* aBuffer);
            nsString&     operator+=(PRUnichar aChar);
            nsString&     Append(const nsString& aString,PRInt32 aLength=-1);
            nsString&     Append(const char* anISOLatin1,PRInt32 aLength=-1);
            nsString&     Append(char aChar);
            nsString&     Append(const PRUnichar* aBuffer,PRInt32 aLength=-1);
            nsString&     Append(PRUnichar aChar);
            nsString&     Append(PRInt32 aInteger,PRInt32 aRadix); //radix=8,10 or 16
            nsString&     Append(float aFloat);
                          
            PRInt32       Left(nsString& aCopy,PRInt32 aCount);
            PRInt32       Mid(nsString& aCopy,PRInt32 anOffset,PRInt32 aCount);
            PRInt32       Right(nsString& aCopy,PRInt32 aCount);
            PRInt32       Insert(nsString& aCopy,PRInt32 anOffset,PRInt32 aCount=-1);
            PRInt32       Insert(PRUnichar aChar,PRInt32 anOffset);

            nsString&     Cut(PRInt32 anOffset,PRInt32 aCount);
            nsString&     StripChars(const char* aSet);
            nsString&     StripWhitespace();
            nsString&     Trim( const char* aSet,
                                PRBool aEliminateLeading=PR_TRUE,
                                PRBool aEliminateTrailing=PR_TRUE);
            nsString&     CompressWhitespace( PRBool aEliminateLeading=PR_TRUE,
                                              PRBool aEliminateTrailing=PR_TRUE);
    static  PRBool        IsSpace(PRUnichar ch);
    static  PRBool        IsAlpha(PRUnichar ch);
  //@}

  ///searching methods...
  //@{
            PRInt32       Find(const char* anISOLatin1) const;
            PRInt32       Find(const PRUnichar* aString) const;
            PRInt32       Find(PRUnichar aChar,PRInt32 offset=0) const;
            PRInt32       Find(const nsString& aString) const;
            PRInt32       FindFirstCharInSet(const char* anISOLatin1Set,PRInt32 offset=0) const;
            PRInt32       FindFirstCharInSet(nsString& aString,PRInt32 offset=0) const;
            PRInt32       FindLastCharInSet(const char* anISOLatin1Set,PRInt32 offset=0) const;
            PRInt32       FindLastCharInSet(nsString& aString,PRInt32 offset=0) const;
            PRInt32       RFind(const char* anISOLatin1,PRBool aIgnoreCase=PR_FALSE) const;
            PRInt32       RFind(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE) const;
            PRInt32       RFind(const nsString& aString,PRBool aIgnoreCase=PR_FALSE) const;
            PRInt32       RFind(PRUnichar aChar,PRBool aIgnoreCase=PR_FALSE) const;
  //@}

  ///comparision methods...
  //@{
    virtual PRInt32       Compare(const nsString &S,PRBool aIgnoreCase=PR_FALSE) const;
    virtual PRInt32       Compare(const char *anISOLatin1,PRBool aIgnoreCase=PR_FALSE) const;
    virtual PRInt32       Compare(const PRUnichar *aString,PRBool aIgnoreCase=PR_FALSE) const;

            PRInt32       operator==(const nsString &S) const;
            PRInt32       operator==(const char *anISOLatin1) const;
            PRInt32       operator==(const PRUnichar* aString) const;
            PRInt32       operator!=(const nsString &S) const;
            PRInt32       operator!=(const char *anISOLatin1) const;
            PRInt32       operator!=(const PRUnichar* aString) const;
            PRInt32       operator<(const nsString &S) const;
            PRInt32       operator<(const char *anISOLatin1) const;
            PRInt32       operator<(const PRUnichar* aString) const;
            PRInt32       operator>(const nsString &S) const;
            PRInt32       operator>(const char *anISOLatin1) const;
            PRInt32       operator>(const PRUnichar* aString) const;
            PRInt32       operator<=(const nsString &S) const;
            PRInt32       operator<=(const char *anISOLatin1) const;
            PRInt32       operator<=(const PRUnichar* aString) const;
            PRInt32       operator>=(const nsString &S) const;
            PRInt32       operator>=(const char *anISOLatin1) const;
            PRInt32       operator>=(const PRUnichar* aString) const;

            PRBool        Equals(const nsString& aString) const;
            PRBool        Equals(const char* anISOLatin1) const;   
            PRBool        Equals(const nsIAtom *aAtom) const;
            PRBool        Equals(const PRUnichar* s1, const PRUnichar* s2) const;

            PRBool        EqualsIgnoreCase(const nsString& aString) const;
            PRBool        EqualsIgnoreCase(const char* anISOLatin1) const;
            PRBool        EqualsIgnoreCase(const nsIAtom *aAtom) const;
            PRBool        EqualsIgnoreCase(const PRUnichar* s1, const PRUnichar* s2) const;
  //@}

            static void   SelfTest();
    virtual void          DebugDump(ostream& aStream) const;

  protected:

typedef PRUnichar chartype;

            chartype*       mStr;
            PRInt32         mLength;
            PRInt32         mCapacity;
            static PRInt32  mInstanceCount;
};

extern NS_BASE int fputs(const nsString& aString, FILE* out);

//----------------------------------------------------------------------

/**
 * A version of nsString which is designed to be used as an automatic
 * variable.  It attempts to operate out of a fixed size internal
 * buffer until too much data is added; then a dynamic buffer is
 * allocated and grown as necessary.
 */
// XXX template this with a parameter for the size of the buffer?
class NS_BASE nsAutoString : public nsString {
public:
                nsAutoString();
                nsAutoString(const nsString& other);
                nsAutoString(const nsAutoString& other);
                nsAutoString(PRUnichar aChar);
                nsAutoString(const char* isolatin1);
                nsAutoString(const PRUnichar* us, PRInt32 uslen = -1);
  virtual       ~nsAutoString();

  static  void  SelfTest();

protected:
  virtual void EnsureCapacityFor(PRInt32 aNewLength);

  PRUnichar mBuf[32];

private:
  // XXX these need writing I suppose
  nsAutoString& operator=(const nsAutoString& other);
};

#endif

