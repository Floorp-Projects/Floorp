/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

/* -*- Mode: C++; tab-width: 4 -*-
 *  JulianString.cpp: implementation of the JulianString class
 *  Copyright © 1998 Netscape Communications Corporation, all rights reserved.
 *  Steve Mansour <sman@netscape.com>
 */

#ifndef MOZ_TREX
#include "xp.h"
#include "fe_proto.h"
#endif

#include "nspr.h"
#ifdef NSPR20
#include "plstr.h"
#endif

#if 0
/* Breaks SINIX5.4 build */
#include <npapi.h>
#endif
#include "jdefines.h"
#include "julnstr.h"

#ifndef MOZ_TREX
#include "xp.h"
#include "fe_proto.h"
#include "julianform.h"
#include "xpgetstr.h"
#define WANT_ENUM_STRING_IDS
#include "allxpstr.h"

#define PL_strncpy strncpy
#define PR_Free XP_FREE
#define PR_Malloc XP_ALLOC
#endif

#ifndef XP_STRCASECMP
    #define XP_STRCASECMP strcasecmp
#endif

#ifdef LIBJULIAN
#ifndef XP_MAC
    #define XP_ASSERT(a)
#endif
    #define NOT_NULL(X) X
#endif

#ifndef MOZ_TREX
extern "C" XP_Bool bCallbacksSet;
extern "C" pJulian_Form_Callback_Struct JulianForm_CallBacks;
#endif

/*
 *  Stolen routines (from netparse.c) to keep the containing dll free of dependencies...
 */
static char *PRIVATE_NET_EscapeBytes (const char * str, int32 len, int mask, int32 * out_len);
static int PRIVATE_NET_UnEscapeCnt (char * str);
#define URL_XALPHAS     (unsigned char) 1
#define URL_XPALPHAS    (unsigned char) 2
#define URL_PATH        (unsigned char) 4
#define HEX_ESCAPE '%'
#define UNHEX(C) \
    ((C >= '0' && C <= '9') ? C - '0' : \
    ((C >= 'A' && C <= 'F') ? C - 'A' + 10 : \
    ((C >= 'a' && C <= 'f') ? C - 'a' + 10 : 0)))

/**
 *  Empty constructor:  JulianString x;
 */
JulianString::JulianString()
{
    m_iStrlen = 0;
    m_HashCode = 0;
    m_iGrowBy = 32;
    m_pBuf = new char[ m_iBufSize = m_iGrowBy ];
    m_pBuf[0] = 0;
}

/**
 *  JulianString x("hello, world");
 */
JulianString::JulianString(const char* p, size_t iGrowBy)
{
    m_iStrlen = XP_STRLEN(p);
    m_pBuf = new char[ m_iBufSize = m_iStrlen + 1 + iGrowBy ];
    XP_MEMCPY(m_pBuf,p,m_iStrlen);
    m_pBuf[m_iStrlen] = 0;
    m_HashCode = 0;
    m_iGrowBy = iGrowBy;
}

/**
 *  JulianString x("hello, world");
 *  JulianString y(x);
 */
JulianString::JulianString( const JulianString& s, size_t iGrowBy )
{
    m_pBuf = new char[ m_iBufSize = s.m_iBufSize ];
    m_iStrlen = s.m_iStrlen;
    XP_MEMCPY(m_pBuf,s.m_pBuf,m_iBufSize);
    m_HashCode = 0;
    m_iGrowBy = iGrowBy;
}

/**
 *  JulianString x(2048);
 */
JulianString::JulianString(size_t iSize, size_t iGrowBy)
{
    m_pBuf = new char[ m_iBufSize = iSize ];
    m_iStrlen = 0;
    *m_pBuf = 0;
    m_HashCode = 0;
    m_iGrowBy = iGrowBy;
}

JulianString::~JulianString()
{
    delete [] m_pBuf;
}

/**
 * return the character at the supplied index. If 
 * index is negative, the characters are indexed from the right
 * side of the string.  That is:
 *
 * <pre>
 *   JulianString x("012345");
 *   x[2] returns '2'
 *   x[-1] returns '5'
 * </pre>
 *
 * If iIndex is > than the string length, the last character
 * will be returned. If iIndex is < the negative of the last
 * character, the first character will be returned.
 *
 * @param iIndex  index of first character of the sub
 * @return        a character
 */
char JulianString::operator[](int32 iIndex)
{
  if (iIndex >= 0)
  {
    if (iIndex < (int32) m_iStrlen)
      return m_pBuf[iIndex];
    iIndex = m_iStrlen-1;
  }
  else
  {
    iIndex = m_iStrlen + iIndex;
  }
  if (iIndex < 0)
    iIndex = 0;
  return m_pBuf[iIndex];
}

/**
 * return a JulianString that is the substring defined by 
 * the two supplied indeces. If either index is negative
 * the return value is an empty string.  If i is greater
 * than j the return value is an empty string. If i is
 * past the end of the string the return value is an
 * empty string. If j is past the end of the string then
 * the return value is the string from i to the end of
 * the string. Examples:
 *
 * <pre>
 *   JulianString x("012345");
 *   x.indexSubstr(2,4) returns a JulianString containing "234"
 * </pre>
 *
 * @param i   index of first character of the sub
 * @param j   index of the last character of the sub
 * @return    a JulianString
 */
JulianString JulianString::indexSubstr(int32 i, int32 j)
{
  if ( i < 0 || j < 0 )
    return "";

  if ( i > j )
    return "";

  if (i+1 > (int32)m_iStrlen)
    return "";
  
  if (j+1 > (int32)m_iStrlen)
    j = m_iStrlen -1;

  return (*this)(i,j-i+1);
}


/**
 * create a substring from the supplied index and count. If the
 * index is negative, the characters are indexed from the right
 * side of the string.  That is:
 *
 * <pre>
 *   JulianString x("012345");
 *   x(2,3) returns a JulianString containing "234"
 *   x(-2,2) returns a JulianString containing "45"
 *
 * @param iIndex  index of first character of the sub
 * @param iCount  number of characters to copy
 * @return        a substring
 */
JulianString JulianString::operator()(int32 iIndex, int32 iCount)
{
    /*
     * Validate args...
     */
    if (iIndex >= 0)
    {
        XP_ASSERT(iIndex < m_iStrlen);
        if ((long)iIndex >= (long)m_iStrlen)
            iIndex = m_iStrlen - 1;
        XP_ASSERT((long)(iCount + iIndex) <= (long)m_iStrlen);
        if ((long)(iIndex + iCount) > (long)m_iStrlen)
            iCount = m_iStrlen - iIndex;
    }
    else
    {
        XP_ASSERT(iIndex > -(long)m_iStrlen);
        if ((long)iIndex <= -(long)m_iStrlen)
            iIndex = 1 - m_iStrlen;
        XP_ASSERT( 0 >= iCount + iIndex);
        if (0 < iCount + iIndex )
            iCount = - iIndex;
    }

    /*
     * extract the substring...
     */
    char* s1 = m_pBuf;
    if (iIndex < 0)
        iIndex += m_iStrlen;

    int32 iNumLeft = m_iStrlen - iIndex;
    if (iCount > iNumLeft || iCount < 0)
        iCount = iNumLeft;

    JulianString Str(iCount + 1);

    if (Str.IsValid())
    {
        XP_MEMCPY(Str.m_pBuf,s1 + iIndex, iCount);
        Str.m_pBuf[iCount] = 0;
        Str.m_iStrlen = iCount;
    }

    return Str.GetBuffer();
}

/**
 *  Replace the substring from index m to n with the supplied
 *  string. Any index less than 0 will be snapped to 0. Any
 *  index > than the last index in the current string will be
 *  snapped to the last index.  If the current string is empty
 *  or uninitialized, it is simply set to the value of *p.
 *  If m > n the values are swapped.
 *
 *  @param m  starting index of substring
 *  @param n  ending index of substring
 *  @param p  string to insert
 *  @return   *this
 */
JulianString& JulianString::Replace(int32 m, int32 n, char* p)
{
  /*
   * A few sanity checks first...
   */
  if (m_iStrlen <= 0)
  {
    (*this) = p;
    return *this;
  }
  if (m < 0)
    m = 0;
  if (n < 0)
    n = 0;
  if ((size_t)(m+1) >= m_iStrlen)
    m = m_iStrlen - 1;
  if ((size_t)(n+1) >= m_iStrlen)
    n = m_iStrlen - 1;
  if (m > n)
  {
    int32 t = m;
    m = n;
    n = t;
  }

  /*
   * Replace from m to n with p
   */
  JulianString s = "";
  JulianString stemp;

  if (m > 0)
    s = Left(m);
  s += p;
  stemp = Right(m_iStrlen - n - 1); /* fixed warning */
  s += stemp;

  (*this) = s;
  return *this;
}

/**
 *  Replace the substring from index m to n with the supplied
 *  string. Any index less than 0 will be snapped to 0. Any
 *  index > than the last index in the current string will be
 *  snapped to the last index.  If the current string is empty
 *  or uninitialized, it is simply set to the value of *p.
 *  If m > n the values are swapped.
 *
 *  @param m  starting index of substring
 *  @param n  ending index of substring
 *  @param p  string to insert
 *  @return   *this
 */
JulianString& JulianString::Replace(int32 m, int32 n, JulianString& p)
{
  return Replace(m,n, p.GetBuffer());
}

/**
 * return the leftmost n characters as a substring. If
 * n is < 0 it is the same as calling Right(n).
 *
 * Examples:
 *
 * <pre>
 *   JulianString x("012345");
 *   x.Left(4) returns a JulianString containing "0123"
 *   x.Left(-4) returns a JulianString containing "2345"
 * </pre>
 *
 * @param n   number of characters to return
 * @return    a JulianString
 */
JulianString JulianString::Left(int32 n)
{
  if (n < 0)
    return Right(-n);
  return (*this)(0,n);
}

/**
 * return the rightmost n characters as a substring. If
 * n is < 0 it is the same as calling Left(n).
 *
 * Examples:
 *
 * <pre>
 *   JulianString x("012345");
 *   x.Right(4) returns a JulianString containing "2345"
 *   x.Right(-4) returns a JulianString containing "0123"
 * </pre>
 *
 * @param n   number of characters to return
 * @return    a JulianString
 */
JulianString JulianString::Right(int32 n)
{
  if (n < 0)
    return Left(-n);
  if (0 >= m_iStrlen)
    return "";
  if ((size_t)n > m_iStrlen )
    n = m_iStrlen;

  return (*this)(m_iStrlen-n,n);
}

/**
 *  Copy the supplied null terminated string into *this.
 *  @param s  the character array to be copied.
 *  @return   a reference to *this
 */
JulianString& JulianString::operator=(const char* s)
{
    m_iStrlen = XP_STRLEN(s);
    size_t iLen = m_iStrlen + 1;
    if (iLen > m_iBufSize)
        ReallocTo(iLen);
    if (IsValid())
    {
        XP_MEMCPY(m_pBuf,s,iLen);
        m_HashCode = 0;
    }
    return *this;
}

/**
 *  Copy the supplied JulianString into *this.
 *  @param s  the string to be copied.
 *  @return   a reference to *this
 */
const JulianString& JulianString::operator=(const JulianString& s)
{
    if (s.m_iStrlen + 1 > m_iBufSize)
        ReallocTo(s.m_iStrlen+1);
    XP_MEMCPY(m_pBuf,s.m_pBuf,s.m_iStrlen + 1 );
    m_iStrlen = s.m_iStrlen;
    m_HashCode = 0;
    return *this;
}

/**
 *  Verify that there is enough room to hold iLen bytes in
 *  the current buffer. If not, reallocate the buffer. This
 *  routine does not copy the old contents into the new buffer.
 *
 *  @param iLen  the size of the buffer needed
 */
void JulianString::ReallocTo(size_t iLen)
{
    if (m_iBufSize >= iLen)
        return;
    delete [] m_pBuf;
    m_pBuf = new char[m_iBufSize = iLen + m_iGrowBy];
}

/**
 *  Verify that there is enough room to hold iLen bytes in
 *  the current buffer. If not, reallocate the buffer. Copy
 *  the old contents into the new buffer.
 *
 *  @param iLen  the size of the buffer needed
 */
void JulianString::Realloc(size_t iLen)
{
    if (m_iBufSize >= iLen)
        return;
    char* p = new char[m_iBufSize = iLen + m_iGrowBy];
    if (p)
    {
        XP_MEMCPY(p,m_pBuf,m_iStrlen);
        p[m_iStrlen] = 0;
    }
    else
    {
        m_iBufSize = m_iStrlen = 0;
    }
    delete [] m_pBuf;
    m_pBuf = p;
    m_HashCode = 0;
}

/**
 *  When the string length changes, presumably through
 *  external use of GetBuffer(), the internal string length
 *  is set by calling this method.  Also, the hash code is
 *  reset since we have no idea what was done to the string.
 */
void JulianString::DoneWithBuffer()
{
    m_iStrlen = XP_STRLEN(m_pBuf);
    m_HashCode = 0;
}

/**
 *  Find the first occurrence of a string within this string
 *  @param p  the string to search for
 *  @param i  the offset within this string to begin the search
 *  @return   the index where the string was found.  -1 means
 *            the string was not found.
 */
int32 JulianString::Find(const char* p, int32 i) const
{
  char* pRet = XP_STRSTR(&m_pBuf[i],p);
  if (0 == pRet)
    return -1;
  return (int32)(i + pRet - &m_pBuf[i]);
}

/**
 *  Find the first occurrence of a character within this string
 *  @param c  the character to search for
 *  @param i  the offset within this string to begin the search
 *  @return   the index where the string was found.  -1 means
 *            the string was not found.
 */
int32 JulianString::Find(char c, int32 i) const
{
  char* pRet = XP_STRCHR(&m_pBuf[i],c);
  if (0 == pRet)
    return -1;
  return (int32)(i + pRet - &m_pBuf[i]);
}

#ifdef MOZ_TREX
/**
 *  Find the last occurrence of a character within a string
 *  @param c  the character to search for
 *  @param i  index of the beginning of the string
 *  @return   the index where the string was found.  -1 means
 *            the string was not found.
 */
int32 JulianString::RFind(char c, int32 i) const
{
  char* pRet = PL_strrchr(&m_pBuf[i],c);
  if (0 == pRet)
    return -1;
  return (int32)(i + pRet - &m_pBuf[i]);
}

/**
 *  Find the last occurrence of a string within this string
 *  @param p  the string to search for
 *  @param i  the offset within this string to begin the search
 *  @return   the index where the string was found.  -1 means
 *            the string was not found.
 */
int32 JulianString::RFind(const char* p, int32 i) const
{
  char* pRet = PL_strrstr(&m_pBuf[i],p);
  if (0 == pRet)
    return -1;
  return (int32)(i + pRet - &m_pBuf[i]);
}
#endif

/**
 *  Concatenate two strings and return the result.
 *  @param s1  one of the strings to concatenate
 *  @param s2  the other string to concatenate
 *  @return    the concatenated string.
 */
JulianString JulianString::operator+(JulianString& s2)
{
    JulianString s(m_iStrlen + s2.m_iStrlen + 1);
    s = *this;
    s += s2;
    return s;
}

/**
 *  Append the supplied string to *this
 *  @param s  the string to append
 *  @return   a reference to *this
 */
JulianString& JulianString::operator+=(const char* s)
{
    size_t iLen = XP_STRLEN(s);
    Realloc(iLen + m_iStrlen + 1);
    if (IsValid())
    {
        XP_MEMCPY(&m_pBuf[m_iStrlen],s,iLen);
        m_iStrlen += iLen;
        m_pBuf[m_iStrlen] = 0;
    }
    else
    {
        m_iStrlen = m_iBufSize = 0;
    }
    m_HashCode = 0;
    return *this;
}

/**
 *  Append the supplied string to *this
 *  @param s  the string to append
 *  @return   a reference to *this
 */
JulianString& JulianString::operator+=(JulianString& s)
{
    Realloc(s.m_iStrlen + m_iStrlen + 1);
    XP_MEMCPY(&m_pBuf[m_iStrlen],s.m_pBuf,s.m_iStrlen);
    m_iStrlen += s.m_iStrlen;
    m_pBuf[m_iStrlen] = 0;
    m_HashCode = 0;
    return *this;
}

/**
 *  Prepend the supplied string to *this
 *  @param s  the string to append
 *  @return   a reference to *this
 */
JulianString& JulianString::Prepend(const char* s)
{
    size_t iLen = XP_STRLEN(s);
    Realloc(iLen + m_iStrlen + 1);
    if (IsValid())
    {
        XP_MEMMOVE(&m_pBuf[iLen],m_pBuf,m_iStrlen);
        XP_MEMCPY(m_pBuf,s,iLen);
        m_iStrlen += iLen;
        m_pBuf[m_iStrlen] = 0;
    }
    else
    {
        m_iStrlen = m_iBufSize = 0;
    }
    m_HashCode = 0;
    return *this;
}

/**
 *  Prepend the supplied string to *this
 *  @param s  the string to append
 *  @return   a reference to *this
 */
JulianString& JulianString::Prepend(JulianString& s)
{
  return Prepend(s.m_pBuf);
}

/**
 *  compare the supplied string to *this
 *  @param p   the string to compare to *this
 *  @return    -1 if the supplied string is lexicographically
 *             less than *this, 0 if they are equal, and 1 if
 *             the supplied string is greater than *this.
 */
int32 JulianString::CompareTo(const char* p, XP_Bool iIgnoreCase) const
{
  if (iIgnoreCase)
#ifndef XP_MAC
    return strcasecomp(p,m_pBuf);
#else
    return XP_STRCMP(p,m_pBuf);
#endif
  else
    return XP_STRCMP(p,m_pBuf);
}

int32 JulianString::CompareTo(JulianString& s, XP_Bool iIgnoreCase) const
{
  if (iIgnoreCase)
#ifndef XP_MAC
    return strcasecomp(s.m_pBuf,m_pBuf);
#else
    return XP_STRCMP(s.m_pBuf,m_pBuf);
#endif
  else
    return XP_STRCMP(s.m_pBuf,m_pBuf);
}

/**
 *  Return the first index of the supplied string.
 *  @param p        the string to look for
 *  @param iOffset  offset within p to begin search
 *  @return         -1 if the string is not found
 *                  the offset if >= 0
 */
int32 JulianString::IndexOf(const char* p, int32 iOffset) const
{
    char* q = &m_pBuf[iOffset];
    int32 iLen = XP_STRLEN(p);
    char* pLoc = XP_STRSTR(q,p);
    return (0 == pLoc) ? -1 : pLoc - m_pBuf;
}

/**
 *  Starting at index i, find the first occurrence of any character
 *  in p.
 *  @param i  the offset within this string to begin the search
 *  @param p  pointer to a null terminated list of characters 
 *  @return   the index where the string was found.  -1 means
 *            the string was not found. NOTE: the index is relative
 *            to the begining of the string, not relative to i.
 */
int32 JulianString::Strpbrk( int32 i, const char *p ) const
{
  char* q = &m_pBuf[i];
  char* r = PL_strpbrk( q, p );
  if (0 == r)
    return -1;
  return (int32)(r - m_pBuf);
}

/**
 *  remove leading and trailing whitespace.
 */
void JulianString::Trim()
{
    char* pHead = m_pBuf;
    char* pTail;

    if (0 == m_iStrlen)
        return;

    for ( pHead = m_pBuf; *pHead && XP_IS_SPACE(*pHead); ++pHead )
        /* do nothing */ ;

    for ( pTail = &m_pBuf[m_iStrlen-1];
          pTail > pHead && XP_IS_SPACE(*pTail);
          --pTail )
        /* do nothing */ ;

    pTail[1] = 0;
    m_iStrlen = 1 + pTail - pHead;
    if (m_pBuf != pHead)
    {
        XP_MEMMOVE(m_pBuf,pHead,m_iStrlen);
    }
    m_pBuf[m_iStrlen] = 0;
    m_HashCode = 0;
}

/**
 *  Upshift the string.  The upshifting is based on toupper()
 *  in <ctype.h>
 */
void JulianString::ToUpper()
{
    char* p;
    for ( p = m_pBuf; *p; ++p )
        *p = (char) toupper(*p);
    m_HashCode = 0;
}

/**
 *  Downshift the string.  The downshifting is based on tolower()
 *  in <ctype.h>
 */
void JulianString::ToLower()
{
    char* p;
    for ( p = m_pBuf; *p; ++p )
        *p = (char) tolower(*p);
    m_HashCode = 0;
}

#ifndef MOZ_TREX
/**
 *  Get String from Application resources
 */
char*  JulianString::GetString(int i)
{
  if (PR_TRUE == bCallbacksSet)
    return ((*JulianForm_CallBacks->GetString)(i));
  else
    return NULL ;

}

/**
 *  Load String from Application resources into this JulianString
 */
JulianString& JulianString::LoadString(int i)
{
  if (PR_TRUE == bCallbacksSet)
  {
    JulianString aString((*JulianForm_CallBacks->GetString)(i)) ;
    *this = aString ;
  }

  return (*this) ;

}
#endif

#if 0
/*Doesn't exist on BSD_3861.1 && SunOS4.1.3_U1  out for now*/
/*
 *  stdout-type output
 */
ostream& operator<<(ostream& s, JulianString& x)
{
    return s << x.m_pBuf;
}

/*
 *  stdin-type input.  we don't really need this method...
 */
istream& operator>>(istream& s, JulianString& x)
{
    char sBuf[1024];    /* DANGER! fixed size buffer... */
    s >> sBuf;
    x = sBuf;
    return s;
}
#endif

/**
 *  compare this string to a const char*
 *  @param p   the string to compare to *this
 *  @return    1 if they are equal, 0 if not.
 */
int JulianString::operator==(const char* p)
{
    return XP_STRCMP(m_pBuf,p) == 0;
}

/**
 *  compare this string to another
 *  @param p   the JulianString to compare to *this
 *  @return    1 if they are equal, 0 if not.
 */
int JulianString::operator==(JulianString& y)
{
    return XP_STRCMP(m_pBuf,y.m_pBuf) == 0;
}

/**
 *  netlib has a general function (netlib\modules\liburl\src\escape.c)
 *  which does url encoding. Since netlib is not yet available for raptor,
 *  the following will suffice. Convert space to +, don't convert alphanumeric,
 *  conver each non alphanumeric char to %XY where XY is the hexadecimal 
 *  equavalent of the binary representation of the character. 
 */
void JulianString::URLEncode()
{
  if (m_iStrlen <= 0)
        return;

  PRInt32 iNewLen=0;
  char* pSave = m_pBuf;
  char* p = PRIVATE_NET_EscapeBytes(m_pBuf, m_iStrlen, URL_XPALPHAS, &iNewLen);
  if (iNewLen > 0)
  {
    if (m_iBufSize < (size_t)(iNewLen + 1))
    {
      delete [] m_pBuf;
      m_iBufSize = iNewLen + 1;
      char *m_pBuf = new char[ m_iBufSize ];
      pSave = m_pBuf;
    }
    m_pBuf = pSave;
    PL_strncpy(m_pBuf,p,m_iBufSize-1);
    m_pBuf[m_iBufSize-1] = 0;
  }
  PR_Free(p);
}

/**
 *  URLDecode
 *  @param p   the string to compare to *this
 *  @return    0 if they are equal, 1 if not.
 */
void JulianString::URLDecode()
{
  PRIVATE_NET_UnEscapeCnt(m_pBuf);
}


/**
 *  compare this string to a char*
 *  @param p   the string to compare to *this
 *  @return    0 if they are equal, 1 if not.
 */
int JulianString::operator!=(const char* y)
{
    return XP_STRCMP(m_pBuf,y) != 0;
}

/**
 *  compare this string to a char*
 *  @param p   the string to compare to *this
 *  @return    0 if they are equal, 1 if not.
 */
int JulianString::operator!=(JulianString& y)
{
    return XP_STRCMP(m_pBuf,y.m_pBuf) != 0;
}

/**
 * return the hash code. Calculate if needed.
 * @return the hash code.
 */
int32 JulianString::HashCode()
{
    if (0 == m_iStrlen)
        return 0;

    if (0 == m_HashCode)
    {
        int32 iInc = (m_iStrlen >= 128) ? m_iStrlen/64 : 1;
        char* p = m_pBuf;
        char* pLimit = &m_pBuf[m_iStrlen];

        while ( p < pLimit )
        {
            m_HashCode = (m_HashCode * 37) + ((int32) *p);
            p += iInc;
        }

        if (0 == m_HashCode)
            ++m_HashCode;
    }

    return m_HashCode;
}

/*
 * Lifted from xp_str.c since we are in DLL ...
 */
#if defined(XP_WIN32) || defined(MOZ_TREX)
PUBLIC int
strcasecomp (const char* one, const char* two)
{
    const char* pA;
    const char* pB;

    for(pA=one, pB=two; *pA && *pB; pA++, pB++)
      {
#ifdef MOZ_TREX
        int tmp = tolower(*pA) - tolower(*pB);
#else
        int tmp = XP_TO_LOWER(*pA) - XP_TO_LOWER(*pB);
#endif
        if (tmp)
            return tmp;
      }
    if (*pA)
        return 1;
    if (*pB)
        return -1;
    return 0;
}
#endif





/*
 *  This code was lifted from netparse.c. We don't want a dependency
 *  on the net code in this class 
 */


/* encode illegal characters into % escaped hex codes.
 *
 * mallocs and returns a string that must be freed
 */
static 
int netCharType[256] =
/*    Bit 0           xalpha          -- the alphas
**    Bit 1           xpalpha         -- as xalpha but 
**                             converts spaces to plus and plus to %20
**    Bit 3 ...       path            -- as xalphas but doesn't escape '/'
*/
    /*   0 1 2 3 4 5 6 7 8 9 A B C D E F */
    {    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,     /* 0x */
               0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,       /* 1x */
               0,0,0,0,0,0,0,0,0,0,7,4,0,7,7,4,       /* 2x   !"#$%&'()*+,-./  */
         7,7,7,7,7,7,7,7,7,7,0,0,0,0,0,0,     /* 3x  0123456789:;<=>?  */
           0,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,   /* 4x  @ABCDEFGHIJKLMNO  */
           /* bits for '@' changed from 7 to 0 so '@' can be escaped   */
           /* in usernames and passwords in publishing.                */
           7,7,7,7,7,7,7,7,7,7,7,0,0,0,0,7,   /* 5X  PQRSTUVWXYZ[\]^_  */
           0,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,   /* 6x  `abcdefghijklmno  */
           7,7,7,7,7,7,7,7,7,7,7,0,0,0,0,0,   /* 7X  pqrstuvwxyz{\}~  DEL */
               0, };

#define IS_OK(C) (netCharType[((unsigned int) (C))] & (mask))

static char *
PRIVATE_NET_EscapeBytes (const char * str, int32 len, int mask, int32 * out_len)
{
    unsigned char *src;
    unsigned char *dst;
    char        *result;
    int32       i, extra = 0;
    char        *hexChars = "0123456789ABCDEF";

      if(!str)
              return(0);

      src = (unsigned char *) str;
    for(i = 0; i < len; i++)
        {
        if (!IS_OK(src[i]))
            extra+=2; /* the escape, plus an extra byte for each nibble */
        }

    if(!(result = (char *) PR_Malloc(len + extra + 1)))
         return(0);
 
     dst = (unsigned char *) result;
     for(i = 0; i < len; i++)
        {
              unsigned char c = src[i];
              if (IS_OK(c))
                {
                      *dst+= c;
                }
              else if(mask == URL_XPALPHAS && c == ' ')
                {
                      *dst++ = '+'; /* convert spaces to pluses */
                }
              else 
                {
                      *dst++ = HEX_ESCAPE;
                      *dst++ = hexChars[c >> 4];              /* high nibble */
                      *dst++ = hexChars[c & 0x0f];    /* low nibble */
                }
        }

    *dst = '\0';     /* tack on eos */
      if(out_len)
              *out_len = dst - (unsigned char *) result;
    return result;
}

static int
PRIVATE_NET_UnEscapeCnt (char * str)
{
    register char *src = str;
    register char *dst = str;

    while(*src)
        if (*src != HEX_ESCAPE)
        {
            *dst++ = *src++;
        } else {
            src++; /* walk over escape */
            if (*src)
            {
            *dst = UNHEX(*src) << 4;
            src++;
            }
            if (*src)
            {
            *dst = (*dst + UNHEX(*src));
            src++;
            }
            dst++;
        }

    *dst = 0;

    return (int)(dst - str);

} /* PRIVATE_NET_UnEscapeCnt */
