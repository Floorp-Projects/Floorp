/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott Collins <scc@mozilla.org> (original author)
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
  #define NS_NAMED_MULTILINE_LITERAL_STRING(n,s)  const nsDependentString n(NS_REINTERPRET_CAST(const nsAString::char_type*, s), PRUint32((sizeof(s)/sizeof(wchar_t))-1))
  typedef nsDependentString nsLiteralString;
#else
  #define NS_LL(s)                                s
  #define NS_MULTILINE_LITERAL_STRING(s)          NS_ConvertASCIItoUTF16(s, PRUint32(sizeof(s)-1))
  #define NS_MULTILINE_LITERAL_STRING_INIT(n,s)   n(s, PRUint32(sizeof(s)-1))
  #define NS_NAMED_MULTILINE_LITERAL_STRING(n,s)  const NS_ConvertASCIItoUTF16 n(s, PRUint32(sizeof(s)-1))
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

#define NS_LITERAL_CSTRING(s)                     NS_STATIC_CAST(const nsAFlatCString&, nsDependentCString(s, PRUint32(sizeof(s)-1)))
#define NS_LITERAL_CSTRING_INIT(n,s)              n(s, PRUint32(sizeof(s)-1))
#define NS_NAMED_LITERAL_CSTRING(n,s)             const nsDependentCString n(s, PRUint32(sizeof(s)-1))

typedef nsDependentCString nsLiteralCString;

#endif /* !defined(nsLiteralString_h___) */
