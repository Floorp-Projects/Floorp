/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Scott Collins <scc@mozilla.org> (original author)
 */

#ifndef nsLiteralString_h___
#define nsLiteralString_h___

#ifndef nscore_h___
#include "nscore.h"
#endif

#ifndef nsDependentString_h___
#include "nsDependentString.h"
#endif


#if 0
inline
const nsDependentString
literal_string( const nsAString::char_type* aPtr )
  {
    return nsDependentString(aPtr);
  }

inline
const nsDependentString
literal_string( const nsAString::char_type* aPtr, PRUint32 aLength )
  {
    return nsDependentString(aPtr, aLength);
  }

inline
const nsDependentCString
literal_string( const nsACString::char_type* aPtr )
  {
    return nsDependentCString(aPtr);
  }

inline
const nsDependentCString
literal_string( const nsACString::char_type* aPtr, PRUint32 aLength )
  {
    return nsDependentCString(aPtr, aLength);
  }
#endif

#ifdef HAVE_CPP_2BYTE_WCHAR_T
  #define NS_LL(s)                                L##s
  #define NS_MULTILINE_LITERAL_STRING(s)          nsDependentString(NS_REINTERPRET_CAST(const nsAString::char_type*, s), PRUint32((sizeof(s)/sizeof(wchar_t))-1))
  #define NS_MULTILINE_LITERAL_STRING_INIT(n,s)   n(NS_REINTERPRET_CAST(const nsAString::char_type*, s), PRUint32((sizeof(s)/sizeof(wchar_t))-1))
  #define NS_NAMED_MULTILINE_LITERAL_STRING(n,s)  nsDependentString n(NS_REINTERPRET_CAST(const nsAString::char_type*, s), PRUint32((sizeof(s)/sizeof(wchar_t))-1))
  typedef nsDependentString nsLiteralString;
#else
  #define NS_LL(s)                                s
  #define NS_MULTILINE_LITERAL_STRING(s)          NS_ConvertASCIItoUTF16(s, PRUint32(sizeof(s)-1))
  #define NS_MULTILINE_LITERAL_STRING_INIT(n,s)   n(s, PRUint32(sizeof(s)-1))
  #define NS_NAMED_MULTILINE_LITERAL_STRING(n,s)  NS_ConvertASCIItoUTF16 n(s, PRUint32(sizeof(s)-1))
  typedef NS_ConvertASCIItoUTF16 nsLiteralString;
#endif

/*
 * Macro arguments used in concatenation or stringification won't be expanded.
 * Therefore, in order for |NS_L(FOO)| to work as expected (which is to expand
 * |FOO| before doing whatever |NS_L| needs to do to it) a helper macro needs
 * to be inserted in between to allow the macro argument to expand.
 * See "3.10.6 Separate Expansion of Macro Arguments" of the CPP manual for a
 * more accurate and precise explanation.
 */

#define NS_L(s)                                   NS_LL(s)

#define NS_LITERAL_STRING(s)                      NS_STATIC_CAST(const nsAFlatString&, NS_MULTILINE_LITERAL_STRING(NS_LL(s)))
#define NS_LITERAL_STRING_INIT(n,s)               NS_MULTILINE_LITERAL_STRING_INIT(n, NS_LL(s))
#define NS_NAMED_LITERAL_STRING(n,s)              NS_NAMED_MULTILINE_LITERAL_STRING(n, NS_LL(s))

#define NS_LITERAL_CSTRING(s)                     nsDependentCString(s, PRUint32(sizeof(s)-1))
#define NS_LITERAL_CSTRING_INIT(n,s)              n(s, PRUint32(sizeof(s)-1))
#define NS_NAMED_LITERAL_CSTRING(n,s)             nsDependentCString n(s, PRUint32(sizeof(s)-1))

typedef nsDependentCString nsLiteralCString;

#endif /* !defined(nsLiteralString_h___) */
