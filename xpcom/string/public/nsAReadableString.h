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
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Scott Collins <scc@mozilla.org> (original author)
 */

/* nsAReadableString.h --- a compatibility header for clients still using the names |nsAReadable[C]String| et al */

#ifndef nsAReadableString_h___
#define nsAReadableString_h___

#ifndef nsAString_h___
#include "nsAString.h"
#endif

typedef const nsAString   nsAReadableString;
typedef const nsACString  nsAReadableCString;

#ifndef nsLiteralString_h___
#include "nsLiteralString.h"
#endif

#ifndef nsPromiseSubstring_h___
#include "nsPromiseSubstring.h"
#endif

#ifndef nsPromiseFlatString_h___
#include "nsPromiseFlatString.h"
#endif

#ifdef NEED_CPP_TEMPLATE_CAST_TO_BASE
#define NS_READABLE_CAST(CharT, expr)  (NS_STATIC_CAST(const nsStringTraits<CharT>::abstract_string_type&, (expr)))
#else
#define NS_READABLE_CAST(CharT, expr)  (expr)
#endif

#endif // !defined(nsAReadableString_h___)
