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

/* nsStringTraits.h --- declares specialized ``traits'' classes which allow templatized functions and classes to select appropriate non-template types */

#ifndef nsStringTraits_h___
#define nsStringTraits_h___

#ifndef nsStringFwd_h___
#include "nsStringFwd.h"
#endif

#ifndef nscore_h___
#include "nscore.h"
#endif




  /**
   *
   */

template <class CharT>
struct nsStringTraits
  {
    typedef nsAString             abstract_string_type;
    typedef nsAPromiseString      abstract_promise_type;
    typedef nsAFlatString         abstract_flat_type;
    typedef const nsLocalString   literal_string_type;
  };

#if 0
  // for lame compilers, put these declarations into the general case
  //  so we only need to specialize for |char|
NS_SPECIALIZE_TEMPLATE
struct nsStringTraits<PRUnichar>
  {
    typedef nsAString             abstract_string_type;
    typedef nsAPromiseString      abstract_promise_type;
    typedef nsAFlatString         abstract_flat_type;
    typedef const nsLocalString   literal_string_type;
  };
#endif

NS_SPECIALIZE_TEMPLATE
struct nsStringTraits<char>
  {
    typedef nsACString            abstract_string_type;
    typedef nsAPromiseCString     abstract_promise_type;
    typedef nsAFlatCString        abstract_flat_type;
    typedef const nsLocalCString  literal_string_type;
  };


#endif /* !defined(nsStringTraits_h___) */
