/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
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
#include <iostream.h>
#include <xp_mcom.h>
#include "jdefines.h"
#include "julnstr.h"

#include "xp.h"
#include "fe_proto.h"
#include "julianform.h"

#include "xpgetstr.h"
#define WANT_ENUM_STRING_IDS

#ifndef XP_STRCASECMP
    #define XP_STRCASECMP strcasecmp
#endif

#ifdef LIBJULIAN
    #define XP_ASSERT(a) 
    #define NOT_NULL(X)	X
#endif

extern "C" XP_Bool bCallbacksSet;
extern "C" pJulian_Form_Callback_Struct JulianForm_CallBacks;

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
JulianString::JulianString(char* p, size_t iGrowBy)
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
 * create a substring from the supplied index and count. If the
 * index is negative, the characters are indexed from the right
 * side of the string.  That is:
 *
 * <pre>
 *   JulianString x("12345");
 *   x(2,3) returns a JulianString containing "234"
 *   x(-2,2) returns a JulianString containing "45"
 * </pre>
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

    JulianString* pStr = new JulianString(iCount + 1);
    if (pStr->IsValid())
    {
        XP_MEMCPY(pStr->m_pBuf,s1 + iIndex, iCount);
        pStr->m_pBuf[iCount] = 0;
    }
    return *pStr;
}

/**
 *  Copy the supplied null terminated string into *this.
 *  @param s  the character array to be copied.
 *  @return   a reference to *this
 */
JulianString& JulianString::operator=(char* s)
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
    char *p = new char[m_iBufSize = iLen + m_iGrowBy];
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
 *  Concatenate two strings and return the result.
 *  @param s1  one of the strings to concatenate
 *  @param s2  the other string to concatenate
 *  @return    the concatenated string.
 */
JulianString operator+(JulianString& s1, JulianString& s2)
{
    JulianString s(s1.m_iStrlen + s2.m_iStrlen + 1);
    s = s1;
    s += s2;
    return s;
}

/**
 *  Append the supplied string to *this
 *  @param s  the string to append
 *  @return   a reference to *this
 */
JulianString& JulianString::operator+=(char* s)
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
 *  compare the supplied string to *this
 *  @param p   the string to compare to *this
 *  @return    -1 if the supplied string is lexicographically
 *             less than *this, 0 if they are equal, and 1 if
 *             the supplied string is greater than *this.
 */
int32 JulianString::CompareTo(char* p)
{
    return XP_STRCMP(p,m_pBuf);
}

int32 JulianString::CompareTo(JulianString& s)
{
    return CompareTo(s.m_pBuf); 
}

/**
 *  Return the first index of the supplied string.
 *  @param p        the string to look for
 *  @param iOffset  offset within p to begin search
 *  @return         -1 if the string is not found
 *                  the offset if >= 0
 */
int32 JulianString::IndexOf(char* p, int32 iOffset)
{
    char* q = &m_pBuf[iOffset];
    int32 iLen = XP_STRLEN(p);
    char *pLoc = XP_STRSTR(q,p);
    return (0 == pLoc) ? -1 : pLoc - m_pBuf;
}

/**
 *  remove leading and trailing whitespace.
 */
void JulianString::Trim()
{
    char *pHead = m_pBuf;
    char *pTail;

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
    char *p;
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
	char *p;
    for ( p = m_pBuf; *p; ++p )
        *p = (char) tolower(*p);
    m_HashCode = 0;
}

/**
 *  Get String from Application resources
 */
char * JulianString::GetString(int i)
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

/**
 *  compare this string to a char*
 *  @param p   the string to compare to *this
 *  @return    1 if they are equal, 0 if not.
 */
int operator==(JulianString& x, char* p)
{
    return XP_STRCMP(x.m_pBuf,p) == 0; 
}

/**
 *  compare this string to another
 *  @param p   the JulianString to compare to *this
 *  @return    1 if they are equal, 0 if not.
 */
int operator==(JulianString& x, JulianString& y)
{
    return XP_STRCMP(x.m_pBuf,y.m_pBuf) == 0; 
}

/**
 *  compare this string to a char*
 *  @param p   the string to compare to *this
 *  @return    0 if they are equal, 1 if not.
 */
int operator!=(JulianString& x, char* y)
{
    return XP_STRCMP(x.m_pBuf,y) != 0; 
}

/**
 *  compare this string to a char*
 *  @param p   the string to compare to *this
 *  @return    0 if they are equal, 1 if not.
 */
int operator!=(JulianString& x, JulianString& y)
{
    return XP_STRCMP(x.m_pBuf,y.m_pBuf) != 0; 
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
        char *p = m_pBuf;
        char *pLimit = &m_pBuf[m_iStrlen];

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
#ifdef XP_WIN32
PUBLIC int 
strcasecomp (const char* one, const char *two)
{
	const char *pA;
	const char *pB;

	for(pA=one, pB=two; *pA && *pB; pA++, pB++) 
	  {
	    int tmp = tolower(*pA) - tolower(*pB);
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
