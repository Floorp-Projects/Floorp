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

#ifndef nsStringIteratorUtils_h___
#define nsStringIteratorUtils_h___

template <class Iterator>
inline
PRBool
SameFragment( const Iterator& lhs, const Iterator& rhs )
  {
    return lhs.fragment().mStart == rhs.fragment().mStart;
  }

template <class CharT> class nsReadingIterator;

  // NOTE: need to break iterators out into their own file (as with many classes here), need
  //  these routines, but can't currently |#include "nsReadableUtils.h"|, this hack is bad
  //  but we need it to get OS2 building again.  Fix by splitting things into different files.
NS_COM size_t Distance( const nsReadingIterator<PRUnichar>&, const nsReadingIterator<PRUnichar>& );
NS_COM size_t Distance( const nsReadingIterator<char>&, const nsReadingIterator<char>& );


#endif /* !defined(nsStringIteratorUtils_h___) */
